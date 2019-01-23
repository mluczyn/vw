#pragma once
#include "vkcore.h"
namespace vw
{
	class Buffer : public vk::Buffer
	{
	public:
		Buffer(vw::Device& device, vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags requiredProperties);
		Buffer(vk::Device device, vk::PhysicalDevice physicalDevice, vk::DeviceSize size, vk::BufferUsageFlags usage, std::vector<uint32_t> queueFamilies, vk::MemoryPropertyFlags requiredProperties);
		~Buffer();
	protected:
		vk::DeviceMemory bufferMemory;
		vk::DeviceSize bufferSize;
		vk::Device deviceHandle;
	};

	class StagingBuffer : public vw::Buffer
	{
	public:
		StagingBuffer(vw::Device& device, vk::DeviceSize size);
		void loadData(void* data);
		vw::CommandBuffer copyToBuffer(vk::Buffer dstBuffer, std::vector<vk::BufferCopy> regions);
		vw::CommandBuffer copyToImage(vk::Image dstImage, std::vector<vk::BufferImageCopy> regions);
	private:
		vw::Device& deviceRef;
	};

	enum BufferStagingMode
	{
		DymanicStaging,
		StaticStaging
	};

	class VertexBuffer : public vw::Buffer
	{
	public:
		VertexBuffer(vw::Device& device, vk::DeviceSize size);
		vw::CommandBuffer loadData(void* data, std::vector<vk::BufferCopy> regions);
	private:
		std::unique_ptr<vw::StagingBuffer> stagingBuffer;
		vw::Device& deviceRef;
	};

	class ImageBase
	{
	public:
		ImageBase(vw::Device& device);
		~ImageBase();
		operator vk::Image();
		vk::Format getFormat();
		vw::CommandBuffer transitionLayout(vk::ImageLayout newLayout);
		void transitionLayout(vk::CommandBuffer cmdBuffer, vk::ImageLayout newLayout);
		vk::ImageView createView(vk::ImageAspectFlags aspectFlags);
	protected:
		void createImage();
		
		vk::ImageUsageFlags usageFlags;
		uint32_t imgWidth = 0, imgHeight = 0;
		vk::Format imgFormat = vk::Format::eUndefined;
		vk::ImageLayout currentLayout;
		vk::ImageTiling imgTiling = vk::ImageTiling::eOptimal;

		vw::Device& deviceRef;
	private:
		vk::Image image;
		vk::DeviceMemory imageMemory;
		std::vector<vk::ImageView> imageViews;
	};

	
	class TransferSrc : public virtual ImageBase
	{
	public:
		TransferSrc(vw::Device& device);
		void copyToImage(vk::CommandBuffer cmdBuffer, vk::Image dstImage, vk::ImageLayout dstLayout, std::vector<vk::ImageCopy> regions);
	};

	class TransferDst : public virtual ImageBase
	{
	public:
		TransferDst(vw::Device& device);
	};

	class ColorAttachment : public virtual ImageBase
	{
	public:
		ColorAttachment(vw::Device& device);
	};
	
	template<vk::ImageType imageType, class... Uses>
	class Image : public virtual ImageBase, public Uses...
	{
	public:
		Image(vw::Device& device, uint32_t width, uint32_t height, vk::Format format, vk::ImageLayout initialLayout = vk::ImageLayout::eGeneral) : ImageBase(device), Uses(device)...
		{
			imgWidth = width;
			imgHeight = height;
			imgFormat = format;
			currentLayout = initialLayout;
			createImage();
		}
	};
}