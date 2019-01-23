#include "vwpresent.h"
#include "vkcore.h"
#include "vwmemory.h"

int main()
{
	glfwInit();
	vw::Instance instance("test", VK_MAKE_VERSION(1, 0, 0), vw::ValidationMode::debug, vw::getPresentationExtensions());

	vw::Window window(instance, 800, 800, "window");

	vw::Device device = instance.createDevice(vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute, (VkSurfaceKHR)window.getSurface(), { "VK_KHR_swapchain" });

	std::string shaderPath = SHADER_DIR;
	vw::Shader vertexShader(device, vk::ShaderStageFlagBits::eVertex, shaderPath + "vert.spv");
	vw::Shader fragmentShader(device, vk::ShaderStageFlagBits::eFragment, shaderPath + "frag.spv");

	vw::GraphicsPipelineSettings graphicsPipelineConfig;
	graphicsPipelineConfig.addShaderStages({ vertexShader, fragmentShader });
	graphicsPipelineConfig.setBlendModes({ vw::BlendMode::disabled });

	vw::Swapchain swapchain(device, device.getPhysicalDevice(), window.getSurface());
	auto swapImages = swapchain.getImages();
	vk::Extent2D screenExtent = swapchain.getExtent();
	vw::Semaphore nextImageAquired(device);

	vw::ExternalDependency dependency;
	dependency.srcStageMask = vk::PipelineStageFlagBits::eTransfer;
	dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	dependency.srcAccessMask = vk::AccessFlagBits::eTransferRead;
	dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eColorAttachmentRead;

	vw::SubpassDescription subpass;
	subpass.colorAttachments = { 0 };
	subpass.attachmentBlendModes = { vw::BlendMode::disabled };
	subpass.preDependencies = { dependency};
	subpass.pipelineSettings = &graphicsPipelineConfig;

	vw::Image<vk::ImageType::e2D, vw::ColorAttachment, vw::TransferSrc> image(device, screenExtent.width, screenExtent.height, vk::Format::eR8G8B8A8Unorm);
	auto imageView = image.createView(vk::ImageAspectFlagBits::eColor);

	vw::RenderPass renderPass(device, { image.getFormat() }, { vk::ImageLayout::eColorAttachmentOptimal }, { subpass });

	vw::Framebuffer framebuffer(device, renderPass, screenExtent, { imageView });
 
	vk::ClearValue clearValue;
	clearValue.color.setFloat32({ 0.0f, 0.0f, 0.0f, 1.0f });

	auto renderCmdBuffer = device.createCommandBuffer(vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eTransfer);
	
	renderCmdBuffer.begin();
	framebuffer.beginRenderPass(renderCmdBuffer, { clearValue }, true);

	renderCmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, renderPass.getSubpassPipeline(0));
	renderCmdBuffer.setScissor(0, { vk::Rect2D(0, screenExtent) });
	renderCmdBuffer.setViewport(0, { vk::Viewport(0, 0, (float)screenExtent.width, (float)screenExtent.height, 0.0f, 1.0f) });
	renderCmdBuffer.draw(3, 1, 0, 0);

	renderCmdBuffer.endRenderPass();
	renderCmdBuffer.end();

	vk::ImageSubresourceLayers imageSubresources(vk::ImageAspectFlagBits::eColor, 0, 0, 1);
	vk::ImageCopy imageCopyRegion(imageSubresources, { 0, 0, 0 }, imageSubresources, { 0, 0, 0 }, { screenExtent.width, screenExtent.height, 1 });

	auto transferCmdBuffers = device.createCommandBufferSet(swapImages.size(), vk::QueueFlagBits::eTransfer);
	for (size_t i = 0; i < transferCmdBuffers.size(); ++i)
	{
		transferCmdBuffers[i]->begin();
		image.transitionLayout(*transferCmdBuffers[i], vk::ImageLayout::eTransferSrcOptimal);
		image.copyToImage(*transferCmdBuffers[i], swapImages[i], vk::ImageLayout::ePresentSrcKHR, {imageCopyRegion});
		image.transitionLayout(*transferCmdBuffers[i], vk::ImageLayout::eColorAttachmentOptimal);
		transferCmdBuffers[i]->end();
	}

	window.untilClosed([&]()
	{
		uint32_t imageIndex = swapchain.getNextImageIndex(nextImageAquired);
		renderCmdBuffer.submit();
		transferCmdBuffers[imageIndex]->setWaitConditions({ renderCmdBuffer, nextImageAquired }, { vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer });
		transferCmdBuffers[imageIndex]->submit();
		swapchain.present(imageIndex, { *transferCmdBuffers[imageIndex] });
	});
	device.waitIdle();
	return 0;
}
