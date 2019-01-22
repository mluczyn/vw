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

	vw::Swapchain swapchain(device, device.getPhysicalDevice(), window.getSurface());
	auto swapImages = swapchain.getImageViews();
	vk::Extent2D screenExtent = swapchain.getExtent();
	vw::Semaphore nextImageAquired(device);

	vw::SubpassDescription subpass;
	subpass.colorAttachments = { 0 };
	subpass.attachmentBlendModes = { vw::BlendMode::disabled };

	vk::SubpassDependency dependency;
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	dependency.srcAccessMask = vk::AccessFlagBits::eMemoryRead;
	dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eColorAttachmentRead;

	vw::GraphicsPipelineSettings graphicsPipelineConfig;
	graphicsPipelineConfig.addShaderStages({ vertexShader, fragmentShader});

	vw::RenderPass renderPass(device, { swapchain.getImageFormat() }, { vk::ImageLayout::ePresentSrcKHR }, { subpass }, { dependency }, { graphicsPipelineConfig });

	std::vector<std::shared_ptr<vw::Framebuffer>> framebuffers(swapImages.size());
	for (size_t i = 0; i < swapImages.size(); ++i)
		framebuffers[i] = std::make_shared<vw::Framebuffer>(device, renderPass, screenExtent, std::vector<vk::ImageView>{ swapImages[i] });

	vk::ClearValue clearValue;
	clearValue.color.setFloat32({ 0.0f, 0.0f, 0.0f, 1.0f });

	auto cmdBuffers = device.createCommandBufferSet(swapImages.size(), vk::QueueFlagBits::eGraphics, vk::CommandBufferLevel::ePrimary);
	for (size_t i = 0; i < swapImages.size(); ++i)
	{
		cmdBuffers[i]->begin(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
		framebuffers[i]->beginRenderPass(*cmdBuffers[i], { clearValue }, true);

		cmdBuffers[i]->bindPipeline(vk::PipelineBindPoint::eGraphics, renderPass.getSubpassPipeline(0));
		cmdBuffers[i]->setScissor(0, { vk::Rect2D(0, screenExtent) });
		cmdBuffers[i]->setViewport(0, { vk::Viewport(0, 0, (float)screenExtent.width, (float)screenExtent.height, 0.0f, 1.0f) });
		cmdBuffers[i]->draw(3, 1, 0, 0);

		cmdBuffers[i]->endRenderPass();
		cmdBuffers[i]->end();

		cmdBuffers[i]->setWaitConditions({ nextImageAquired }, { vk::PipelineStageFlagBits::eColorAttachmentOutput });
	}

	while (!window.shouldClose())
	{
		glfwPollEvents();
		uint32_t imageIndex = swapchain.getNextImageIndex(nextImageAquired);
		cmdBuffers[imageIndex]->submit();
		swapchain.present(imageIndex, { *cmdBuffers[imageIndex] });
	}
	device.waitIdle();
	return 0;
}
