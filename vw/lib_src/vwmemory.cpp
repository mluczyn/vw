#include "vwmemory.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
 
uint32_t findMemoryType(vk::PhysicalDevice physicalDevice, vk::MemoryRequirements memoryRequirements, vk::MemoryPropertyFlags requiredProperties)
{
	vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();
	for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
	{
		if ((memoryRequirements.memoryTypeBits >> i & 1) && ((memProperties.memoryTypes[i].propertyFlags & requiredProperties) == requiredProperties))
			return i;
	}
	throw std::runtime_error("vwMemory: Failed to find memory type with required property flags!");
}

vw::Buffer::Buffer(vw::Device& device, vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags requiredProperties) : vw::Buffer(device, device.getPhysicalDevice(), size, usage, device.getQueueFamilyIndices(vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute | vk::QueueFlagBits::eTransfer), requiredProperties)
{
	//TODO: Give ownership to queue families based on buffer usage
}

vw::Buffer::Buffer(vk::Device device, vk::PhysicalDevice physicalDevice, vk::DeviceSize size, vk::BufferUsageFlags usage, std::vector<uint32_t> queueFamilies, vk::MemoryPropertyFlags requiredProperties) : deviceHandle(device), bufferSize(size)
{
	vk::BufferCreateInfo bufferCreateInfo;
	bufferCreateInfo.size = size;
	bufferCreateInfo.usage = usage;
	bufferCreateInfo.sharingMode = (queueFamilies.size() == 1) ? vk::SharingMode::eExclusive : vk::SharingMode::eConcurrent;
	if (queueFamilies.size() > 1)
	{
		bufferCreateInfo.queueFamilyIndexCount = queueFamilies.size();
		bufferCreateInfo.pQueueFamilyIndices = queueFamilies.data();
	}
	vk::Buffer::operator=(device.createBuffer(bufferCreateInfo));

	vk::MemoryRequirements memRequirements = device.getBufferMemoryRequirements(*this);
	uint32_t memTypeIndex = findMemoryType(physicalDevice, memRequirements, requiredProperties);

	vk::MemoryAllocateInfo memAllocInfo;
	memAllocInfo.allocationSize = memRequirements.size;
	memAllocInfo.memoryTypeIndex = memTypeIndex;
	bufferMemory = device.allocateMemory(memAllocInfo);
	device.bindBufferMemory(*this, bufferMemory, 0);
}

vw::Buffer::~Buffer()
{
	deviceHandle.freeMemory(bufferMemory);
	deviceHandle.destroyBuffer(*this);
}

vw::StagingBuffer::StagingBuffer(vw::Device& device, vk::DeviceSize size) : vw::Buffer(device, size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent), deviceRef(device)
{
}

void vw::StagingBuffer::loadData(void* data)
{
	void* bufferData;
	deviceRef.mapMemory(bufferMemory, (vk::DeviceSize)0, bufferSize, vk::MemoryMapFlags(), &bufferData);
	memcpy(bufferData, data, static_cast<size_t>(bufferSize));
	deviceRef.unmapMemory(bufferMemory);
}

vw::CommandBuffer vw::StagingBuffer::copyToBuffer(vk::Buffer dstBuffer, std::vector<vk::BufferCopy> regions)
{
	vw::CommandBuffer cmdBuffer = deviceRef.createCommandBuffer(vk::QueueFlagBits::eTransfer, vk::CommandBufferLevel::ePrimary);
	cmdBuffer.begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
	cmdBuffer.copyBuffer(*this, dstBuffer, regions);
	cmdBuffer.end();
	return cmdBuffer;
}

vw::CommandBuffer vw::StagingBuffer::copyToImage(vk::Image dstImage, std::vector<vk::BufferImageCopy> regions)
{
	vw::CommandBuffer cmdBuffer = deviceRef.createCommandBuffer(vk::QueueFlagBits::eTransfer, vk::CommandBufferLevel::ePrimary);
	cmdBuffer.begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
	cmdBuffer.copyBufferToImage(*this, dstImage, vk::ImageLayout::eTransferDstOptimal, regions);
	cmdBuffer.end();
	return cmdBuffer;
}

vw::TransferSrc::TransferSrc(vw::Device& device) : vw::ImageBase(device)
{
	usageFlags |= vk::ImageUsageFlagBits::eTransferSrc;
}

vw::CommandBuffer vw::TransferSrc::copyToImage(vk::Image dstImage, vk::ImageLayout dstLayout, std::vector<vk::ImageCopy>& regions, bool oneTime)
{
	auto cmdBuffer = deviceRef.createCommandBuffer(vk::QueueFlagBits::eTransfer, vk::CommandBufferLevel::ePrimary);
	cmdBuffer.begin(oneTime ? vk::CommandBufferUsageFlagBits::eOneTimeSubmit : vk::CommandBufferUsageFlagBits::eSimultaneousUse);
	cmdBuffer.copyImage(*this, currentLayout, dstImage, dstLayout, regions);
	cmdBuffer.end();
	return cmdBuffer;
}

void vw::TransferSrc::copyToImage(vk::CommandBuffer cmdBuffer, vk::Image dstImage, vk::ImageLayout dstLayout, std::vector<vk::ImageCopy>& regions)
{
	cmdBuffer.copyImage(*this, currentLayout, dstImage, dstLayout, regions);
}

vw::TransferDst::TransferDst(vw::Device& device) : vw::ImageBase(device)
{
	usageFlags |= vk::ImageUsageFlagBits::eTransferDst;
}

vw::CommandBuffer vw::TransferDst::copyFromImage(vk::Image srcImage, vk::ImageLayout srcLayout, std::vector<vk::ImageCopy>& regions, bool oneTime)
{
	auto cmdBuffer = deviceRef.createCommandBuffer(vk::QueueFlagBits::eTransfer, vk::CommandBufferLevel::ePrimary);
	cmdBuffer.begin(oneTime ? vk::CommandBufferUsageFlagBits::eOneTimeSubmit : vk::CommandBufferUsageFlagBits::eSimultaneousUse);
	cmdBuffer.copyImage(srcImage, srcLayout, *this, currentLayout, regions);
	cmdBuffer.end();
	return cmdBuffer;
}

vw::ImageBase::ImageBase(vw::Device& device) : deviceRef(device)
{
}

vw::ImageBase::~ImageBase()
{
	for (auto& view : imageViews)
		deviceRef.destroyImageView(view);
	if (imageMemory)
		deviceRef.freeMemory(imageMemory);
	if(image)
		deviceRef.destroyImage(image);
}

vw::ImageBase::operator vk::Image()
{
	return image;
}

vk::Format vw::ImageBase::getFormat()
{
	return imgFormat;
}

static std::map<vk::ImageLayout, vk::AccessFlags> mapImageLayoutToAccess = 
{
	{vk::ImageLayout::eUndefined, vk::AccessFlagBits()},
	{vk::ImageLayout::eTransferDstOptimal, vk::AccessFlagBits::eTransferWrite}
};

static std::map<vk::ImageLayout, vk::PipelineStageFlags> mapImageLayoutToStage =
{
	{vk::ImageLayout::eUndefined, vk::PipelineStageFlagBits::eTopOfPipe},
	{vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits::eTransfer}
};

vw::CommandBuffer vw::ImageBase::transitionLayout(vk::ImageLayout newLayout)
{
	vk::ImageMemoryBarrier imageBarrier;
	imageBarrier.oldLayout = currentLayout;
	imageBarrier.newLayout = newLayout;
	imageBarrier.image = image;
	imageBarrier.subresourceRange = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
	imageBarrier.srcAccessMask = mapImageLayoutToAccess[currentLayout];
	imageBarrier.dstAccessMask = mapImageLayoutToAccess[newLayout];

	auto cmdBuffer = deviceRef.createCommandBuffer(vk::QueueFlagBits::eTransfer, vk::CommandBufferLevel::ePrimary);
	cmdBuffer.begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
	cmdBuffer.pipelineBarrier(mapImageLayoutToStage[currentLayout], mapImageLayoutToStage[newLayout], vk::DependencyFlags(), {}, {}, { imageBarrier });
	cmdBuffer.end();
	currentLayout = newLayout;
	return cmdBuffer;
}

void vw::ImageBase::transitionLayout(vk::CommandBuffer cmdBuffer, vk::ImageLayout newLayout)
{
	vk::ImageMemoryBarrier imageBarrier;
	imageBarrier.oldLayout = currentLayout;
	imageBarrier.newLayout = newLayout;
	imageBarrier.image = image;
	imageBarrier.subresourceRange = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
	imageBarrier.srcAccessMask = mapImageLayoutToAccess[currentLayout];
	imageBarrier.dstAccessMask = mapImageLayoutToAccess[newLayout];

	cmdBuffer.pipelineBarrier(mapImageLayoutToStage[currentLayout], mapImageLayoutToStage[newLayout], vk::DependencyFlags(), {}, {}, { imageBarrier });
 
	currentLayout = newLayout;
}

void vw::ImageBase::createImage()
{
	vk::ImageCreateInfo imageCreateInfo;
	imageCreateInfo.imageType = vk::ImageType::e2D;
	imageCreateInfo.extent = { imgWidth, imgHeight, 1 };
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.format = imgFormat;
	imageCreateInfo.tiling = imgTiling;
	imageCreateInfo.initialLayout = currentLayout;
	imageCreateInfo.usage = usageFlags;
	imageCreateInfo.samples = vk::SampleCountFlagBits::e1;

	auto queueFamilies = deviceRef.getQueueFamilyIndices(vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute | vk::QueueFlagBits::eTransfer);
	imageCreateInfo.sharingMode = (queueFamilies.size() > 1) ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive;
	imageCreateInfo.queueFamilyIndexCount = queueFamilies.size();
	imageCreateInfo.pQueueFamilyIndices = queueFamilies.data();

	image = deviceRef.createImage(imageCreateInfo);

	auto memRequirements = deviceRef.getImageMemoryRequirements(image);
	auto memTypeIndex = findMemoryType(deviceRef.getPhysicalDevice(), memRequirements, vk::MemoryPropertyFlagBits::eDeviceLocal);
	imageMemory = deviceRef.allocateMemory({ memRequirements.size, memTypeIndex });
	deviceRef.bindImageMemory(image, imageMemory, 0);

}

vk::ImageView vw::ImageBase::createView(vk::ImageAspectFlags aspectFlags)
{
	vk::ImageViewCreateInfo viewCreateInfo;
	viewCreateInfo.image = image;
	viewCreateInfo.viewType = vk::ImageViewType::e2D;
	viewCreateInfo.format = imgFormat;
	viewCreateInfo.subresourceRange = { aspectFlags, 0, 1, 0, 1 };
	auto view = deviceRef.createImageView(viewCreateInfo);
	imageViews.push_back(view);
	return view;
}

static std::map<uint32_t, vk::Format> mapNumChannelsToFormat = 
{
	{1, vk::Format::eR8Unorm},
	{2, vk::Format::eR8G8Unorm},
	{3, vk::Format::eR8G8B8Unorm},
	{4, vk::Format::eR8G8B8A8Unorm}
};
vw::VertexBuffer::VertexBuffer(vw::Device & device, vk::DeviceSize size) : deviceRef(device), vw::Buffer(device, size, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal)
{
	stagingBuffer = std::make_unique<vw::StagingBuffer>(device, size);
}

vw::CommandBuffer vw::VertexBuffer::loadData(void* data, std::vector<vk::BufferCopy> regions)
{
	stagingBuffer->loadData(data);
	return stagingBuffer->copyToBuffer(*this, regions);
}

vw::ColorAttachment::ColorAttachment(vw::Device& device) : ImageBase(device)
{
	usageFlags |= vk::ImageUsageFlagBits::eColorAttachment;
}
