#pragma once
#include <cstring>
#include <string>
#include <vector>
#include <functional>
#include <iostream>
#include <vulkan\vulkan.hpp>
#include "vwutils.h"
#include "vwrender.h"

namespace vw
{
	enum ValidationMode
	{
		release,
		debug
	};

	const std::vector<const char*> ValidationModeLayers[] =
	{
		{},
		{ "VK_LAYER_LUNARG_standard_validation"}
	};
	
	class Semaphore : public vk::Semaphore
	{
	public:
		Semaphore(vk::Device device);
		~Semaphore();
	private:
		vk::Device deviceHandle;
	};
	
	class Fence : public vk::Fence
	{
	public:
		Fence(vk::Device device);
		~Fence();
	private:
		vk::Device deviceHandle;
	};

	class CommandBuffer : public vk::CommandBuffer, public vw::Semaphore
	{
	public:
		CommandBuffer(vk::Device device, vk::CommandPool commandPool, vk::CommandBufferLevel level, std::function<vk::Queue()> queueRequestFunc);
		CommandBuffer(vk::Device device, vk::CommandBuffer bufferHandle, std::function<vk::Queue()> queueRequestFunc); //External alloc/dealloc
		CommandBuffer(vw::CommandBuffer&& other);
		void begin(vk::CommandBufferUsageFlags usageFlags = vk::CommandBufferUsageFlagBits());
		void setWaitConditions(std::vector<vk::Semaphore> waitSemaphores, std::vector<vk::PipelineStageFlags> waitStages);
		void submit();
		void submit(vk::Semaphore triggerSemaphore);
		void submitAndSync();
		~CommandBuffer();
		CommandBuffer& operator=(vw::CommandBuffer&& other);
	private:
		vk::Device deviceHandle;
		vk::CommandPool poolHandle;
		std::function<vk::Queue()> requestQueue;
		std::vector<vk::Semaphore> wSemaphores;
		std::vector<vk::PipelineStageFlags> wStages;
	};
	
	class CommandBufferSet : public std::vector<std::unique_ptr<vw::CommandBuffer>>
	{
	public:
		CommandBufferSet(size_t count, vk::Device device, vk::CommandPool commandPool, vk::CommandBufferLevel level, std::function<vk::Queue()> queueRequestFunc);
		~CommandBufferSet();
	private:
		vk::Device deviceHandle;
		std::vector<vk::CommandBuffer> buffers;
		vk::CommandPool poolHandle;
	};

	class Device : public vk::Device
	{
	public:
		Device(vk::Instance instance, vk::PhysicalDevice physicalDevice, bool presentationSupport, bool enableValidation, std::vector<const char*> extensions);
		vk::PhysicalDevice getPhysicalDevice() { return physicalDeviceHandle; };
		std::vector<uint32_t> getQueueFamilyIndices(vk::QueueFlags flagMask);
		vw::CommandBuffer createCommandBuffer(vk::QueueFlags flags, vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary);
		vw::CommandBufferSet createCommandBufferSet(uint32_t count, vk::QueueFlags flags, vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary);
		void waitIdle();
		~Device();
	private:
		std::function<vk::Queue()> createQueueRequest(uint32_t queueFamilyIndex);
		uint32_t findQueueFamily(vk::QueueFlags flags);
		vk::PhysicalDevice physicalDeviceHandle;
		vk::PhysicalDeviceFeatures deviceFeatures;
	 
		std::vector<vk::QueueFamilyProperties> queueFamilies;
		std::vector<vk::Queue> queues;
		std::vector<vk::CommandPool> commandPools;
	};

	class Instance
	{
	public:
		Instance(std::string appName, uint32_t version, vw::ValidationMode validationMode, std::vector<const char*> platformExtensions);
		operator vk::Instance();
		vw::Device createDevice(vk::QueueFlags requiredQueueFlags, VkSurfaceKHR requiredSurfaceSupport, std::vector<const char*> requiredExtensions);
		~Instance();
	private:
		bool checkValidationLayerSupport(vw::ValidationMode mode);
		bool checkExtensionSupport(std::vector<const char*> extensions);

		vw::ValidationMode activeValidationMode;
		vk::Instance instance;
		VkDebugReportCallbackEXT callback;
	};
}