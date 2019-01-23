#include "vwpresent.h"

std::vector<const char*> vw::getPresentationExtensions()
{
	std::vector<const char*> extensions;
	uint32_t count;
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&count);
	extensions.insert(extensions.end(), &glfwExtensions[0], &glfwExtensions[count]);
	return extensions;
}

vw::Window::Window(vk::Instance instance, int width, int height, std::string title)
{
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, false);
	windowHandle = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
	glfwCreateWindowSurface((VkInstance)instance , windowHandle, nullptr, &surfaceHandle);
}

vw::Window::~Window()
{
	glfwDestroyWindow(windowHandle);
}

vk::SurfaceKHR vw::Window::getSurface()
{
	return vk::SurfaceKHR(surfaceHandle);
}

vw::Window::operator GLFWwindow*()
{
	return windowHandle;
}

bool vw::Window::shouldClose()
{
	return glfwWindowShouldClose(windowHandle);
}

vw::Semaphore::Semaphore(vk::Device device) : deviceHandle(device)
{
	vk::SemaphoreCreateInfo semaphoreCreateInfo;
	vk::Semaphore::operator = (device.createSemaphore(semaphoreCreateInfo));
}

vw::Semaphore::~Semaphore()
{
	deviceHandle.destroySemaphore(*this);
}

vw::Swapchain::Swapchain(vk::Device logicalDevice, vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface) : deviceHandle(logicalDevice)
{
	uint32_t queueFamilyCount = physicalDevice.getQueueFamilyProperties().size();
	std::vector<uint32_t> presentationQueueIndices;
	for (uint32_t i = 0; i < queueFamilyCount; ++i)
	{
		vk::Bool32 presentationSupport;
		physicalDevice.getSurfaceSupportKHR(i, surface, &presentationSupport);
		if (presentationSupport)
			presentationQueueIndices.push_back(i);
	}

	presentQueue = logicalDevice.getQueue(presentationQueueIndices[0], 0);

	//default surface format was checked earlier
	selectedFormat = { vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear };
	//FIFO is always supported therefore it is the default
	selectedMode = vk::PresentModeKHR::eFifo;

	surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);

	auto surfaceFormats = physicalDevice.getSurfaceFormatsKHR(surface);
	auto presentModes = physicalDevice.getSurfacePresentModesKHR(surface);
	for (auto presentMode : presentModes)
	{
		//Enable triple-buffering if available
		if (presentMode == vk::PresentModeKHR::eMailbox)
			selectedMode = presentMode;
	}

	vk::SwapchainCreateInfoKHR swapchainCreateInfo;
	swapchainCreateInfo.surface = surface;

	swapchainCreateInfo.minImageCount = (surfaceCapabilities.minImageCount == surfaceCapabilities.maxImageCount) ? surfaceCapabilities.minImageCount : surfaceCapabilities.minImageCount + 1;
	swapchainCreateInfo.imageFormat = selectedFormat.format;
	swapchainCreateInfo.imageColorSpace = selectedFormat.colorSpace;
	swapchainCreateInfo.imageExtent = surfaceCapabilities.currentExtent;
	swapchainCreateInfo.imageArrayLayers = 1;
	swapchainCreateInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
	swapchainCreateInfo.imageSharingMode = (presentationQueueIndices.size() == 1) ? vk::SharingMode::eExclusive : vk::SharingMode::eConcurrent;

	swapchainCreateInfo.queueFamilyIndexCount = (uint32_t)presentationQueueIndices.size();
	swapchainCreateInfo.pQueueFamilyIndices = presentationQueueIndices.data();

	swapchainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
	swapchainCreateInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
	swapchainCreateInfo.presentMode = selectedMode;
	swapchainCreateInfo.clipped = true;

	swapchain = deviceHandle.createSwapchainKHR(swapchainCreateInfo);

	swapchainImages = deviceHandle.getSwapchainImagesKHR(swapchain);
	swapchainImageViews.resize(swapchainImages.size());

	vk::ImageViewCreateInfo viewCreateInfo;
	viewCreateInfo.viewType = vk::ImageViewType::e2D;
	viewCreateInfo.format = selectedFormat.format;
	viewCreateInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	viewCreateInfo.subresourceRange.baseMipLevel = 0;
	viewCreateInfo.subresourceRange.levelCount = 1;
	viewCreateInfo.subresourceRange.baseArrayLayer = 0;
	viewCreateInfo.subresourceRange.layerCount = 1;
	for (size_t i = 0; i < swapchainImages.size(); ++i)
	{
		viewCreateInfo.image = swapchainImages[i];
		swapchainImageViews[i] = deviceHandle.createImageView(viewCreateInfo);
	}
}

void vw::Swapchain::present(uint32_t imageIndex, std::vector<vk::Semaphore> waitConditions)
{
	vk::PresentInfoKHR presentInfo;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapchain;
	presentInfo.pImageIndices = &imageIndex;

	presentInfo.waitSemaphoreCount = waitConditions.size();
	presentInfo.pWaitSemaphores = waitConditions.data();
	presentQueue.presentKHR(presentInfo);
}

void vw::Swapchain::presentAndSync(uint32_t imageIndex, std::vector<vk::Semaphore> waitConditions)
{
	present(imageIndex, waitConditions);
	presentQueue.waitIdle();
}

vk::Extent2D vw::Swapchain::getExtent()
{
	return surfaceCapabilities.currentExtent;
}

vk::Format vw::Swapchain::getImageFormat()
{
	return vk::Format::eB8G8R8A8Unorm;
}

vk::Image vw::Swapchain::getImage(uint32_t index)
{
	return swapchainImages[index];
}

std::vector<vk::ImageView> vw::Swapchain::getImageViews()
{
	return swapchainImageViews;
}

std::vector<vk::Image> vw::Swapchain::getImages()
{
	return swapchainImages;
}

uint32_t vw::Swapchain::getNextImageIndex(vk::Semaphore signaledSemaphore)
{
	uint32_t imageIndex;
	deviceHandle.acquireNextImageKHR(swapchain, UINT64_MAX, signaledSemaphore, nullptr, &imageIndex);
	return imageIndex;
}

vw::Swapchain::~Swapchain()
{
	for (auto imageView : swapchainImageViews)
		deviceHandle.destroyImageView(imageView);
	deviceHandle.destroySwapchainKHR(swapchain);
}

