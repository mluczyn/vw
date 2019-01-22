#include "vkcore.h"

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t obj, size_t location, int32_t code, const char* layerPrefix, const char* msg, void* userData)
{
	std::cerr << layerPrefix << ":" << msg << std::endl;
	return VK_FALSE;
}

VkResult CreateDebugReportCallbackEXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugReportCallbackEXT* pCallback) {
	auto func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pCallback);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void DestroyDebugReportCallbackEXT(VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks* pAllocator) {
	auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
	if (func != nullptr) {
		func(instance, callback, pAllocator);
	}
}

vw::Instance::Instance(std::string appName, uint32_t version, vw::ValidationMode validationMode, std::vector<const char*> platformExtensions) : activeValidationMode(validationMode)
{
	if (!checkValidationLayerSupport(validationMode))
		throw std::runtime_error("VwInstance: Validation mode not available!");
	if (!checkExtensionSupport(platformExtensions))
		throw std::runtime_error("VwInstance: Extensions not available!");

	vk::ApplicationInfo appInfo;
	appInfo.pApplicationName = appName.c_str();
	appInfo.applicationVersion = version;
	appInfo.pEngineName = appName.append(" Engine").c_str();
	appInfo.engineVersion = version;
	appInfo.apiVersion = VK_API_VERSION_1_0;

	vk::InstanceCreateInfo createInfo;
	createInfo.pApplicationInfo = &appInfo;

	if (validationMode != release)
	{
		platformExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
		createInfo.enabledLayerCount = (uint32_t)ValidationModeLayers[validationMode].size();
		createInfo.ppEnabledLayerNames = ValidationModeLayers[validationMode].data();
	}

	createInfo.enabledExtensionCount = (uint32_t)platformExtensions.size();
	createInfo.ppEnabledExtensionNames = platformExtensions.data();

	instance = vk::createInstance(createInfo);

	if (validationMode != release)
	{
		VkDebugReportCallbackCreateInfoEXT debugInfo = {};
		debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
		debugInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
		debugInfo.pfnCallback = debugCallback;

		if (CreateDebugReportCallbackEXT((VkInstance)instance, &debugInfo, nullptr, &callback) != VK_SUCCESS) {
			throw std::runtime_error("VwInstance: Failed to create callback!");
		}
	}
}

vw::Instance::operator vk::Instance()
{
	return instance;
}

vw::Instance::~Instance()
{
	DestroyDebugReportCallbackEXT((VkInstance)instance, callback, nullptr);
	instance.destroy();
}

vw::Device vw::Instance::createDevice(vk::QueueFlags requiredQueueFlags, VkSurfaceKHR requiredSurfaceSupport, std::vector<const char*> requiredExtensions)
{
	auto physicalDevices = instance.enumeratePhysicalDevices();
	std::vector<vk::PhysicalDevice> capableDevices;
	vk::PhysicalDevice selectedDevice;
	vk::DeviceSize selectedMemorySize = 0;
	vk::PhysicalDeviceType selectedDeviceType = vk::PhysicalDeviceType::eOther;

	//Remove devices without required queue families, extensions or surface support
	for (auto physicalDevice : physicalDevices)
	{
		auto queueFamilies = physicalDevice.getQueueFamilyProperties();
		vk::QueueFlags availableFlags;
		for (auto queueFamily : queueFamilies)
			availableFlags |= queueFamily.queueFlags;

		uint32_t extensionCount = requiredExtensions.size();
		auto availableExtensions = physicalDevice.enumerateDeviceExtensionProperties();
		for (auto extensionR : requiredExtensions)
			for (auto extensionA : availableExtensions)
				if (strcmp(extensionR, extensionA.extensionName) == 0)
				{
					extensionCount--;
				}
		bool surfaceCompatible = true;
		if (requiredSurfaceSupport != 0)
		{
			surfaceCompatible = false;
			vk::SurfaceFormatKHR defaultFormat = { vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear };
			auto surfaceFormats = physicalDevice.getSurfaceFormatsKHR(vk::SurfaceKHR(requiredSurfaceSupport));
			for (auto surfaceFormat : surfaceFormats)
				surfaceCompatible |= (surfaceFormat == defaultFormat);

			auto presentModes = physicalDevice.getSurfacePresentModesKHR(vk::SurfaceKHR(requiredSurfaceSupport));
			if (presentModes.empty())
				surfaceCompatible = false;
		}
		if (((requiredQueueFlags & availableFlags) == requiredQueueFlags) && extensionCount == 0 && surfaceCompatible)
			capableDevices.push_back(physicalDevice);
	}

	if (capableDevices.size() == 0)
		throw std::runtime_error("VwInstance: Failed to find physical device with given requirements!");

	std::vector<vk::PhysicalDevice> discreteDevices;
	for (auto physicalDevice : capableDevices)
	{
		if (physicalDevice.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
			discreteDevices.push_back(physicalDevice);
	}
	if (discreteDevices.size())
		capableDevices = discreteDevices;

	for (auto physicalDevice : capableDevices)
	{
		auto memoryProperties = physicalDevice.getMemoryProperties();
		vk::DeviceSize localMemorySize = 0;
		for (size_t i = 0; i < memoryProperties.memoryHeapCount; ++i)
		{
			if ((memoryProperties.memoryHeaps[i].flags & vk::MemoryHeapFlagBits::eDeviceLocal))
				localMemorySize += memoryProperties.memoryHeaps[i].size;
		}
		physicalDevice.getProperties().deviceType;
		if ((localMemorySize > selectedMemorySize))
		{
			selectedDevice = physicalDevice;
			selectedMemorySize = localMemorySize;
		}
	}

	return vw::Device(instance, selectedDevice, (requiredSurfaceSupport != 0), (activeValidationMode != release), requiredExtensions);
}

bool vw::Instance::checkValidationLayerSupport(vw::ValidationMode mode)
{
	if (mode == release)
		return true;

	uint32_t requiredLayerCount = vw::ValidationModeLayers[mode].size();
	auto availableLayers = vk::enumerateInstanceLayerProperties();
	for (auto layerR : vw::ValidationModeLayers[mode])
	{
		for (auto layerA : availableLayers)
		{ 
			if (strcmp(layerR, layerA.layerName) == 0)
				requiredLayerCount--;
		}
	}

	return requiredLayerCount == 0;
}

bool vw::Instance::checkExtensionSupport(std::vector<const char*> extensions)
{
	uint32_t requiredExtensionCount = extensions.size();
	auto availableExtensions = vk::enumerateInstanceExtensionProperties();

	for (auto extensionR : extensions)
	{
		for (auto extensionA : availableExtensions)
		{
			if (strcmp(extensionR, extensionA.extensionName) == 0)
				requiredExtensionCount--;
		}
	}
	return requiredExtensionCount == 0;
}

vw::Device::Device(vk::Instance instance, vk::PhysicalDevice physicalDevice, bool presentationSupport, bool enableValidation, std::vector<const char*> extensions) : physicalDeviceHandle(physicalDevice)
{
	vk::DeviceCreateInfo logicalDeviceCreateInfo;

	//Create all available queues
	queueFamilies = physicalDevice.getQueueFamilyProperties();
	uint32_t queueFamilyCount = queueFamilies.size();
	std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos(queueFamilyCount);
	
	float queuePriorities[30];
	std::fill(queuePriorities, queuePriorities + 30, 1.0f);
	for (size_t i = 0; i < queueFamilyCount; ++i)
	{
		queueCreateInfos[i].queueFamilyIndex = i;
		queueCreateInfos[i].queueCount = 1;
		queueCreateInfos[i].pQueuePriorities = queuePriorities;
	}
	logicalDeviceCreateInfo.queueCreateInfoCount = queueFamilyCount;
	logicalDeviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();

	//Enable all available features
	deviceFeatures = physicalDevice.getFeatures();
	logicalDeviceCreateInfo.pEnabledFeatures = &deviceFeatures;

	logicalDeviceCreateInfo.enabledExtensionCount = (uint32_t)extensions.size();
	logicalDeviceCreateInfo.ppEnabledExtensionNames = extensions.data();

	//Device layers are depreciated
	logicalDeviceCreateInfo.enabledLayerCount = 0;

	vk::Device::operator = (physicalDevice.createDevice(logicalDeviceCreateInfo));
 
	//Get queue handles
	queues.resize(queueFamilyCount);
	for (size_t i = 0; i < queueFamilyCount; ++i)
		queues[i] = getQueue(i, 0);

	//Create command pools
	vk::CommandPoolCreateInfo poolCreateInfo;
	commandPools.resize(queueFamilyCount);
	for (size_t i = 0; i < queueFamilyCount; ++i)
	{
		poolCreateInfo.queueFamilyIndex = i;
		commandPools[i] = createCommandPool(poolCreateInfo);
	}
}
 
std::vector<uint32_t> vw::Device::getQueueFamilyIndices(vk::QueueFlags flagMask)
{
	std::vector<uint32_t> indices;
	for (uint32_t i = 0; i < queueFamilies.size(); ++i)
	{
		if ((queueFamilies[i].queueFlags & flagMask) == flagMask)
			indices.push_back(i);
	}
	return indices;
}

vw::CommandBuffer vw::Device::createCommandBuffer(vk::QueueFlags flags, vk::CommandBufferLevel level)
{
	uint32_t queueFamily = findQueueFamily(flags);
	return vw::CommandBuffer(*this, commandPools[queueFamily], level, createQueueRequest(queueFamily));
}

vw::CommandBufferSet vw::Device::createCommandBufferSet(uint32_t count, vk::QueueFlags flags, vk::CommandBufferLevel level)
{
	uint32_t queueFamily = findQueueFamily(flags);
	return vw::CommandBufferSet(count, *this, commandPools[queueFamily], level, createQueueRequest(queueFamily));
}

std::function<vk::Queue()> vw::Device::createQueueRequest(uint32_t queueFamilyIndex)
{
	return [queueFamilyIndex, this]() {return queues[queueFamilyIndex]; };
}

//Finds queue family with matching flags, otherwise return largest queue family with compatible flags
uint32_t vw::Device::findQueueFamily(vk::QueueFlags flags)
{
	uint32_t selectedFamily = queueFamilies.size(), queueCount = 0;
	for (uint32_t i = 0; i < queueFamilies.size(); ++i)
	{
		if (flags == queueFamilies[i].queueFlags)
			return i;
		else if (((flags & queueFamilies[i].queueFlags) == flags) && (queueCount < queueFamilies[i].queueCount))
		{
			selectedFamily = i;
			queueCount = queueFamilies[i].queueCount;
		}
	}
	if (selectedFamily == queueFamilies.size())
		throw std::runtime_error("VwDevice: Failed to find compatible queue family!");
	return selectedFamily;
}

void vw::Device::waitIdle()
{
	for (auto queue : queues)
		queue.waitIdle();
}

vw::Device::~Device()
{
	for (auto& pool : commandPools)
		destroyCommandPool(pool);
	destroy();
}

vw::CommandBuffer::CommandBuffer(vk::Device device, vk::CommandPool commandPool, vk::CommandBufferLevel level, std::function<vk::Queue()> queueRequestFunc) : deviceHandle(device), poolHandle(commandPool), requestQueue(queueRequestFunc), vw::Semaphore(device)
{
	vk::CommandBufferAllocateInfo allocateInfo;
	allocateInfo.commandPool = poolHandle;
	allocateInfo.commandBufferCount = 1;
	allocateInfo.level = level;
	device.allocateCommandBuffers(&allocateInfo, dynamic_cast<vk::CommandBuffer*>(this));
}

vw::CommandBuffer::CommandBuffer(vk::Device device, vk::CommandBuffer bufferHandle, std::function<vk::Queue()> queueRequestFunc) : deviceHandle(device), vk::CommandBuffer(bufferHandle), requestQueue(queueRequestFunc), vw::Semaphore(device)
{
}

vw::CommandBuffer::CommandBuffer(vw::CommandBuffer && other) : vw::Semaphore(other.deviceHandle)
{
	deviceHandle = other.deviceHandle;
	poolHandle = other.poolHandle;
	requestQueue = other.requestQueue;
	wSemaphores = other.wSemaphores;
	wStages = other.wStages;
	other.poolHandle = nullptr;
}

void vw::CommandBuffer::begin(vk::CommandBufferUsageFlags usageFlags)
{
	vk::CommandBufferBeginInfo beginInfo(usageFlags);
    vk::CommandBuffer::begin(beginInfo);
}

void vw::CommandBuffer::setWaitConditions(std::vector<vk::Semaphore> waitSemaphores, std::vector<vk::PipelineStageFlags> waitStages)
{
	wSemaphores = waitSemaphores;
	wStages = waitStages;
}

void vw::CommandBuffer::submit()
{
	vk::SubmitInfo submitInfo;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = this;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = static_cast<vk::Semaphore*>(this);
	submitInfo.waitSemaphoreCount = wSemaphores.size();
	submitInfo.pWaitSemaphores = wSemaphores.data();
	submitInfo.pWaitDstStageMask = wStages.data();
	requestQueue().submit( submitInfo, vk::Fence());
}

void vw::CommandBuffer::submit(vk::Semaphore triggerSemaphore)
{
	std::vector<vk::Semaphore> tSemaphores = { static_cast<vk::Semaphore>(*this) };
	if (triggerSemaphore)
		tSemaphores.push_back(triggerSemaphore);

	vk::SubmitInfo submitInfo;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = this;
	submitInfo.signalSemaphoreCount = tSemaphores.size();
	submitInfo.pSignalSemaphores = tSemaphores.data();
	submitInfo.waitSemaphoreCount = wSemaphores.size();
	submitInfo.pWaitSemaphores = wSemaphores.data();
	submitInfo.pWaitDstStageMask = wStages.data();
	requestQueue().submit({ submitInfo }, vk::Fence());
}

void vw::CommandBuffer::submitAndSync()
{
	vk::SubmitInfo submitInfo;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = this;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = static_cast<vk::Semaphore*>(this);
	submitInfo.waitSemaphoreCount = wSemaphores.size();
	submitInfo.pWaitSemaphores = wSemaphores.data();
	submitInfo.pWaitDstStageMask = wStages.data();

	vk::Queue queue = requestQueue();
	queue.submit({ submitInfo }, vk::Fence());
	queue.waitIdle();
}

vw::CommandBuffer::~CommandBuffer()
{
	if (poolHandle)
		deviceHandle.freeCommandBuffers(poolHandle, { *this });
}

vw::CommandBuffer & vw::CommandBuffer::operator=(vw::CommandBuffer && other)
{
	deviceHandle = other.deviceHandle;
	poolHandle = other.poolHandle;
	requestQueue = other.requestQueue;
	wSemaphores = other.wSemaphores;
	wStages = other.wStages;
	other.poolHandle = nullptr;
	return *this;
}

vw::Fence::Fence(vk::Device device) : deviceHandle(device)
{
	vk::FenceCreateInfo fenceInfo;
	vk::Fence::operator = (deviceHandle.createFence(fenceInfo));
}

vw::Fence::~Fence()
{
	deviceHandle.destroyFence(*this);
}

vw::CommandBufferSet::CommandBufferSet(size_t count, vk::Device device, vk::CommandPool commandPool, vk::CommandBufferLevel level, std::function<vk::Queue()> queueRequestFunc) : deviceHandle(device), poolHandle(commandPool)
{
	this->resize(count);
	buffers.resize(count);

	vk::CommandBufferAllocateInfo allocateInfo;
	allocateInfo.commandBufferCount = count;
	allocateInfo.level = level;
	allocateInfo.commandPool = poolHandle;

	deviceHandle.allocateCommandBuffers(&allocateInfo, buffers.data());
	
	size_t i = 0;
	for (auto& ptr : *this)
		ptr = std::make_unique<vw::CommandBuffer>(deviceHandle, buffers[i++], queueRequestFunc);
}

vw::CommandBufferSet::~CommandBufferSet()
{
	deviceHandle.freeCommandBuffers(poolHandle, this->size(), buffers.data());
}
