#pragma once
#include<vulkan\vulkan.hpp>
#include <map>
#include <memory>
#include "vwshader.hpp"

namespace vw
{
	enum BlendMode
	{
		disabled
	};

	struct SubpassDescription
	{
		std::vector<uint32_t> inputAttachments;
		std::vector<uint32_t> colorAttachments;
		std::vector<vw::BlendMode> attachmentBlendModes;
		std::vector<uint32_t> depthStencilAttachments;
	};

	static std::map<vk::AccessFlagBits, vk::PipelineStageFlagBits> mapAccessStage =
	{
		{vk::AccessFlagBits::eColorAttachmentWrite, vk::PipelineStageFlagBits::eColorAttachmentOutput},
		{vk::AccessFlagBits::eDepthStencilAttachmentWrite, vk::PipelineStageFlagBits::eLateFragmentTests},
		{vk::AccessFlagBits::eInputAttachmentRead, vk::PipelineStageFlagBits::eFragmentShader}
	};

	static const vk::ColorComponentFlags ColorComponentsRGBA = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
	static std::map<vw::BlendMode, vk::PipelineColorBlendAttachmentState> mapBlendMode =
	{
		{disabled, vk::PipelineColorBlendAttachmentState(0, vk::BlendFactor::eZero, vk::BlendFactor::eZero, vk::BlendOp::eAdd, vk::BlendFactor::eZero,
		vk::BlendFactor::eZero, vk::BlendOp::eAdd, vw::ColorComponentsRGBA)}
	};

	class Framebuffer
	{
	public:
		Framebuffer(vk::Device device, vk::RenderPass renderPass, vk::Extent2D dimensions, std::vector<vk::ImageView> attachments);
		~Framebuffer();
		void beginRenderPass(vk::CommandBuffer commandBuffer, std::vector<vk::ClearValue> clearValues, bool firstSubpassInline);
	private:
		vk::Framebuffer framebuffer;
		vk::RenderPass renderPassHandle;
		vk::Extent2D frameExtent;
		vk::Device deviceHandle;
	};

	struct GraphicsPipelineSettings
	{
	public:
		GraphicsPipelineSettings();
		//Renderpass/subpass and shader module info is not filled in
		operator vk::GraphicsPipelineCreateInfo();
		void addShaderStages(std::vector<vk::PipelineShaderStageCreateInfo> shaders);
		void setBlendModes(std::vector<vw::BlendMode> blendModes);
		void setLayout(vk::PipelineLayout pipelineLayout);

		vk::GraphicsPipelineCreateInfo pipelineCreateInfo;
		std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;

		vk::PipelineVertexInputStateCreateInfo vertexInputInfo;

		vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo;

		vk::PipelineViewportStateCreateInfo viewportInfo;
		vk::Viewport viewport;
		vk::Rect2D scissor;

		vk::PipelineRasterizationStateCreateInfo rasterizerInfo;

		vk::PipelineMultisampleStateCreateInfo multisamplingInfo;

		vk::PipelineDepthStencilStateCreateInfo depthStencilInfo;

		vk::PipelineColorBlendStateCreateInfo colorBlendInfo;
		std::vector<vk::PipelineColorBlendAttachmentState> colorBlendAttachmentStates;

		vk::PipelineDynamicStateCreateInfo dynamicStateInfo;

		std::vector<vk::DynamicState> dynamicStates;
	};

	class PipelineLayout
	{
	public:
		PipelineLayout(vk::Device device, std::vector<vk::DescriptorSetLayout> setLayouts, std::vector<vk::PushConstantRange> pushConstants);
		~PipelineLayout();
		operator vk::PipelineLayout();
	private:
		vk::Device deviceHandle;
		vk::PipelineLayout layout;
	};

	class RenderPass
	{
	public:
		RenderPass(vk::Device device, std::vector<vk::Format> attachmentFormats, std::vector<vk::ImageLayout> attachmentOutputLayouts, std::vector<vw::SubpassDescription> subpasses, std::vector<vk::SubpassDependency> externalDependencies, std::vector<vw::GraphicsPipelineSettings> pipelineSettings);
		~RenderPass();
		operator vk::RenderPass() { return renderPass; };
		vk::Pipeline getSubpassPipeline(uint32_t subpassIndex);
	private:
		void createPipelines(std::vector<vw::GraphicsPipelineSettings>& pipelineSettings);
		vk::RenderPass renderPass; 
		uint32_t subpassCount;
		std::vector<vk::Pipeline> pipelines;
		vk::PipelineLayout emptyLayout;
		vk::Device deviceHandle;
	};
}

