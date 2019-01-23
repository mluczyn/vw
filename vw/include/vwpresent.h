#pragma once
#define GLFW_INCLUDE_VULCAN
#include <vector>
#include <string>
#include "vkcore.h"
#include <GLFW\glfw3.h>

namespace vw
{
	std::vector<const char*> getPresentationExtensions();

	class Window
	{
	public:
		Window(vk::Instance instance, int width, int height, std::string title);
		~Window();
		vk::SurfaceKHR getSurface();
		operator GLFWwindow*();
		bool shouldClose();
	private:
		GLFWwindow* windowHandle;
		VkSurfaceKHR surfaceHandle;
	};

	class Swapchain
	{
	public:
		Swapchain(vk::Device logicalDevice, vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface);
		void present(uint32_t imageIndex, std::vector<vk::Semaphore> waitConditions);
		void presentAndSync(uint32_t imageIndex, std::vector<vk::Semaphore> waitConditions);
		vk::Extent2D getExtent();
		vk::Format getImageFormat();
		vk::Image getImage(uint32_t index);
		std::vector<vk::ImageView> getImageViews();
		std::vector<vk::Image> getImages();
		uint32_t getNextImageIndex(vk::Semaphore signaledSemaphore);
		~Swapchain();
	private:
		vk::SwapchainKHR swapchain;
		std::vector<vk::Image> swapchainImages;
		std::vector<vk::ImageView> swapchainImageViews;

		vk::Device deviceHandle;
		vk::Queue presentQueue;
		vk::SurfaceCapabilitiesKHR surfaceCapabilities;
		vk::SurfaceFormatKHR selectedFormat;
		vk::PresentModeKHR selectedMode;
	};
}


