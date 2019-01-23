#include "vwrender.h"

vw::RenderPass::RenderPass(vk::Device device, std::vector<vk::Format> attachmentFormats, std::vector<vk::ImageLayout> attachmentOutputLayouts, std::vector<vw::SubpassDescription> subpasses) : deviceHandle(device)
{

	subpassCount = subpasses.size();

	std::vector<vk::AttachmentDescription> attachmentDescriptions(attachmentFormats.size());
	vk::AttachmentDescription attachmentDescBase;
	attachmentDescBase.samples = vk::SampleCountFlagBits::e1;
	attachmentDescBase.loadOp = vk::AttachmentLoadOp::eClear;
	attachmentDescBase.storeOp = vk::AttachmentStoreOp::eStore;
	attachmentDescBase.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
	attachmentDescBase.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	attachmentDescBase.initialLayout = vk::ImageLayout::eUndefined;
	for (size_t i = 0; i < attachmentFormats.size(); ++i)
	{
		attachmentDescriptions[i] = attachmentDescBase;
		attachmentDescriptions[i].format = attachmentFormats[i];
		attachmentDescriptions[i].finalLayout = attachmentOutputLayouts[i];
	}

	std::vector<std::vector<vk::AccessFlagBits>> attachmentAccesses(attachmentFormats.size());
	//eIndexRead is used to indicate that the attachment is not used in the subpass
	for (size_t i = 0; i < attachmentFormats.size(); ++i)
		attachmentAccesses[i].resize(subpasses.size(), vk::AccessFlagBits::eIndexRead);

	//determine subpass image layouts for input/color/depthstencil attachments
	std::vector<std::vector<vk::AttachmentReference>> colorAttachments(subpasses.size()), depthAttachments(subpasses.size()), inputAttachments(subpasses.size());
	for (size_t i = 0; i < subpasses.size(); ++i)
	{
		for (auto attachmentIndex : subpasses[i].colorAttachments)
		{
			attachmentAccesses[attachmentIndex][i] = vk::AccessFlagBits::eColorAttachmentWrite;
			colorAttachments[i].push_back({ attachmentIndex, vk::ImageLayout::eColorAttachmentOptimal });
		}
		for (auto attachmentIndex : subpasses[i].depthStencilAttachments)
		{
			attachmentAccesses[attachmentIndex][i] = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
			depthAttachments[i].push_back({ attachmentIndex, vk::ImageLayout::eDepthStencilAttachmentOptimal });
		}
		for (auto attachmentIndex : subpasses[i].inputAttachments)
		{
			vk::AccessFlagBits lastWrite = vk::AccessFlagBits::eColorAttachmentWrite;
			uint32_t checkSubpass = i - 1;
			while (checkSubpass >= 0)
			{
				if (attachmentAccesses[attachmentIndex][checkSubpass] & (vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite))
				{
					lastWrite = attachmentAccesses[attachmentIndex][checkSubpass];
				}
			}

			bool depthStencilRead = lastWrite == vk::AccessFlagBits::eDepthStencilAttachmentWrite;
			attachmentAccesses[attachmentIndex][i] = vk::AccessFlagBits::eInputAttachmentRead;
			depthAttachments[i].push_back({ attachmentIndex, depthStencilRead ? vk::ImageLayout::eDepthStencilReadOnlyOptimal : vk::ImageLayout::eShaderReadOnlyOptimal });
		}
	}

	//deternmine preserved attachments
	std::vector<std::vector<uint32_t>> preservedAttachments(subpasses.size());
	for (uint32_t i = 0; i < attachmentFormats.size(); ++i)
	{
		bool upcomingRead = false;
		for (uint32_t j = subpasses.size(); j > 0; --j)
		{
			if (attachmentAccesses[i][j-1] == vk::AccessFlagBits::eInputAttachmentRead)
				upcomingRead = true;
			else if (attachmentAccesses[i][j-1] & (vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite))
				upcomingRead = false;
			else
				if (upcomingRead)
				{
					preservedAttachments[j-1].push_back(i);
				}
		}
	}

	std::vector<vk::SubpassDescription> subpassDescriptions(subpasses.size());
	for (uint32_t i = 0; i < subpasses.size(); ++i)
	{
		subpassDescriptions[i].pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
		subpassDescriptions[i].inputAttachmentCount = inputAttachments[i].size();
		subpassDescriptions[i].pInputAttachments = inputAttachments[i].data();
		subpassDescriptions[i].colorAttachmentCount = colorAttachments[i].size();
		subpassDescriptions[i].pColorAttachments = colorAttachments[i].data();
		subpassDescriptions[i].pDepthStencilAttachment = depthAttachments[i].data();
		subpassDescriptions[i].preserveAttachmentCount = preservedAttachments[i].size();
		subpassDescriptions[i].pPreserveAttachments = preservedAttachments[i].data();
	}

	size_t externDependencyCount = 0;
	for (auto& subpass : subpasses)
		externDependencyCount += (subpass.preDependencies.size() + subpass.postDependencies.size());

	std::vector<vk::SubpassDependency> subpassDependencies(externDependencyCount);
	size_t dependencyIterator = 0;

	//determine external dependencies
	for (size_t i = 0; i < subpasses.size(); ++i)
	{
		for (auto& preDependency : subpasses[i].preDependencies)
		{
			subpassDependencies[dependencyIterator].srcSubpass = VK_SUBPASS_EXTERNAL;
			subpassDependencies[dependencyIterator].dstSubpass = i;
			subpassDependencies[dependencyIterator].srcStageMask = preDependency.srcStageMask;
			subpassDependencies[dependencyIterator].dstStageMask = preDependency.dstStageMask;
			subpassDependencies[dependencyIterator].srcAccessMask = preDependency.srcAccessMask;
			subpassDependencies[dependencyIterator].dstAccessMask = preDependency.dstAccessMask;
			subpassDependencies[dependencyIterator++].dependencyFlags = preDependency.flags;
		}
		for (auto& postDependency : subpasses[i].postDependencies)
		{
			subpassDependencies[dependencyIterator].srcSubpass = i;
			subpassDependencies[dependencyIterator].dstSubpass = VK_SUBPASS_EXTERNAL;
			subpassDependencies[dependencyIterator].srcStageMask = postDependency.srcStageMask;
			subpassDependencies[dependencyIterator].dstStageMask = postDependency.dstStageMask;
			subpassDependencies[dependencyIterator].srcAccessMask = postDependency.srcAccessMask;
			subpassDependencies[dependencyIterator].dstAccessMask = postDependency.dstAccessMask;
			subpassDependencies[dependencyIterator++].dependencyFlags = postDependency.flags;
		}
	}

	//determine internal dependencies
	for(uint32_t i = 0; i < attachmentFormats.size(); ++i)
	{
		vk::AccessFlagBits lastAccessFlag, currentAccessFlag;
		uint32_t lastAccessSubpass, currentAccessSubpass;
		bool lastAccessAvailable = false;
		for (uint32_t j = 0; j < subpasses.size() - 1; ++j)
			if (attachmentAccesses[i][j] != vk::AccessFlagBits::eIndexRead)
			{
				currentAccessSubpass = j;
				currentAccessFlag = attachmentAccesses[i][j];

				if (lastAccessAvailable)
					if (currentAccessFlag != lastAccessFlag)
					{
						vk::SubpassDependency dependency;
						dependency.dependencyFlags = vk::DependencyFlagBits::eByRegion;
						dependency.srcSubpass = lastAccessSubpass;
						dependency.dstSubpass = currentAccessSubpass;
						dependency.srcStageMask = mapAccessStage[lastAccessFlag];
						dependency.dstStageMask = mapAccessStage[currentAccessFlag];
						dependency.srcAccessMask = lastAccessFlag;
						dependency.dstAccessMask = currentAccessFlag;
						subpassDependencies.push_back(dependency);
					}

				lastAccessFlag = currentAccessFlag;
				lastAccessSubpass = currentAccessSubpass;
				lastAccessAvailable = true;
			}
	}

	vk::RenderPassCreateInfo renderPassCreateInfo;
	renderPassCreateInfo.attachmentCount = attachmentDescriptions.size();
	renderPassCreateInfo.pAttachments = attachmentDescriptions.data();
	renderPassCreateInfo.subpassCount = subpassDescriptions.size();
	renderPassCreateInfo.pSubpasses = subpassDescriptions.data();
	renderPassCreateInfo.dependencyCount = subpassDependencies.size();
	renderPassCreateInfo.pDependencies = subpassDependencies.data();
	
	renderPass = deviceHandle.createRenderPass(renderPassCreateInfo);

	std::vector<vw::GraphicsPipelineSettings*> pipelineSettings(subpasses.size());
	for (size_t i = 0; i < subpasses.size(); ++i)
		pipelineSettings[i] = subpasses[i].pipelineSettings;

	createPipelines(pipelineSettings);
}
 
vw::RenderPass::~RenderPass()
{
	for (auto pipeline : pipelines)
		deviceHandle.destroyPipeline(pipeline);
	deviceHandle.destroyPipelineLayout(emptyLayout);
	deviceHandle.destroyRenderPass(renderPass);
}

vk::Pipeline vw::RenderPass::getSubpassPipeline(uint32_t subpassIndex)
{
	assert(subpassIndex < subpassCount);
	return pipelines[subpassIndex];
}

void vw::RenderPass::createPipelines(std::vector<vw::GraphicsPipelineSettings*>& pipelineSettings)
{
	vw::PipelineLayout emptyLayout(deviceHandle, {}, {});

	std::vector<vk::GraphicsPipelineCreateInfo> pipelineCreateInfos(subpassCount);
	std::vector<std::vector<vk::PipelineShaderStageCreateInfo>> shaderStageInfos(subpassCount);

	for (size_t i = 0; i < pipelineSettings.size(); ++i)
	{
		pipelineCreateInfos[i] = pipelineSettings[i]->operator vk::GraphicsPipelineCreateInfo();

		pipelineCreateInfos[i].stageCount = pipelineSettings[i]->shaderStages.size();
		shaderStageInfos[i].resize(pipelineCreateInfos[i].stageCount);
		for (size_t j = 0; j < pipelineSettings[i]->shaderStages.size(); ++j)
			shaderStageInfos[i][j] = pipelineSettings[i]->shaderStages[j].get().getShaderStageInfo();
		pipelineCreateInfos[i].pStages = shaderStageInfos[i].data();
		
		if (!pipelineSettings[i]->pipelineCreateInfo.layout)
			pipelineCreateInfos[i].layout = emptyLayout;
	
		pipelineCreateInfos[i].renderPass = renderPass;
		pipelineCreateInfos[i].subpass = i;
	}

	pipelines = deviceHandle.createGraphicsPipelines(vk::PipelineCache(), pipelineCreateInfos);
}

vw::Framebuffer::Framebuffer(vk::Device device, vk::RenderPass renderPass, vk::Extent2D dimensions, std::vector<vk::ImageView> attachments) : deviceHandle(device), renderPassHandle(renderPass), frameExtent(dimensions)
{
	vk::FramebufferCreateInfo framebufferCreateInfo;
	framebufferCreateInfo.renderPass = renderPass;
	framebufferCreateInfo.attachmentCount = attachments.size();
	framebufferCreateInfo.pAttachments = attachments.data();
	framebufferCreateInfo.width = dimensions.width;
	framebufferCreateInfo.height = dimensions.height;
	framebufferCreateInfo.layers = 1;

	framebuffer = deviceHandle.createFramebuffer(framebufferCreateInfo);
}

vw::Framebuffer::~Framebuffer()
{
	if(framebuffer)
		deviceHandle.destroyFramebuffer(framebuffer);
}

void vw::Framebuffer::beginRenderPass(vk::CommandBuffer commandBuffer, std::vector<vk::ClearValue> clearValues, bool firstSubpassInline)
{
	vk::RenderPassBeginInfo renderPassBeginInfo;
	renderPassBeginInfo.framebuffer = framebuffer;
	renderPassBeginInfo.renderPass = renderPassHandle;
	renderPassBeginInfo.renderArea.extent = frameExtent;
	renderPassBeginInfo.clearValueCount = clearValues.size();
	renderPassBeginInfo.pClearValues = clearValues.data();

	vk::SubpassContents contents = firstSubpassInline ? vk::SubpassContents::eInline : vk::SubpassContents::eSecondaryCommandBuffers;
	commandBuffer.beginRenderPass(renderPassBeginInfo, contents);
}

vw::GraphicsPipelineSettings::GraphicsPipelineSettings()
{
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.vertexAttributeDescriptionCount = 0;

	inputAssemblyInfo.topology = vk::PrimitiveTopology::eTriangleList;
	inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;
 
	/*
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = 0.0f;
	viewport.height = 0.0f;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	scissor.offset = { 0, 0 };
	scissor.extent = { 0, 0 };
	*/
 
	viewportInfo.viewportCount = 1;
	viewportInfo.scissorCount = 1;
	
	rasterizerInfo.depthClampEnable = VK_FALSE;
	rasterizerInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizerInfo.polygonMode = vk::PolygonMode::eFill;
	rasterizerInfo.cullMode = vk::CullModeFlagBits::eBack;
	rasterizerInfo.frontFace = vk::FrontFace::eClockwise;
	rasterizerInfo.depthBiasEnable = VK_FALSE;
	rasterizerInfo.lineWidth = 1.0f;

	multisamplingInfo.rasterizationSamples = vk::SampleCountFlagBits::e1;
	multisamplingInfo.sampleShadingEnable = VK_FALSE;

	colorBlendInfo.logicOpEnable = VK_FALSE;

	depthStencilInfo.depthTestEnable = VK_FALSE;
	depthStencilInfo.depthWriteEnable = VK_FALSE;
	depthStencilInfo.depthCompareOp = vk::CompareOp::eLess;
	depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
	depthStencilInfo.stencilTestEnable = VK_FALSE;
	depthStencilInfo.minDepthBounds = 0.0f;
	depthStencilInfo.maxDepthBounds = 1.0f;

	vk::DynamicState dynamicStateArray[] = 
	{
		vk::DynamicState::eViewport,
		vk::DynamicState::eScissor
	};

	dynamicStates.insert(dynamicStates.end(), dynamicStateArray, dynamicStateArray+2);
	dynamicStateInfo.dynamicStateCount = dynamicStates.size();
	dynamicStateInfo.pDynamicStates = dynamicStates.data();
	
	pipelineCreateInfo.flags = vk::PipelineCreateFlagBits::eAllowDerivatives;
	pipelineCreateInfo.pVertexInputState = &vertexInputInfo;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyInfo;
	pipelineCreateInfo.pViewportState = &viewportInfo;
	pipelineCreateInfo.pRasterizationState = &rasterizerInfo;
	pipelineCreateInfo.pMultisampleState = &multisamplingInfo;
	pipelineCreateInfo.pDepthStencilState = &depthStencilInfo;
	pipelineCreateInfo.pColorBlendState = &colorBlendInfo;
	pipelineCreateInfo.pDynamicState = &dynamicStateInfo;
}

vw::GraphicsPipelineSettings::operator vk::GraphicsPipelineCreateInfo()
{
	colorBlendInfo.attachmentCount = colorBlendAttachmentStates.size();
	colorBlendInfo.pAttachments = colorBlendAttachmentStates.data();
	pipelineCreateInfo.pColorBlendState = &colorBlendInfo;

	return pipelineCreateInfo;
}

void vw::GraphicsPipelineSettings::addShaderStages(std::vector<std::reference_wrapper<vw::Shader>> shaders)
{
	shaderStages.insert(shaderStages.end(), shaders.begin(), shaders.end());
	pipelineCreateInfo.stageCount = shaderStages.size();
}

void vw::GraphicsPipelineSettings::setBlendModes(std::vector<vw::BlendMode> blendModes)
{
	colorBlendAttachmentStates.clear();
	for (auto blendMode : blendModes)
		colorBlendAttachmentStates.push_back(mapBlendMode[blendMode]);
}

void vw::GraphicsPipelineSettings::setLayout(vk::PipelineLayout pipelineLayout)
{
	pipelineCreateInfo.layout = pipelineLayout;
}

vw::PipelineLayout::PipelineLayout(vk::Device device, std::vector<vk::DescriptorSetLayout> setLayouts, std::vector<vk::PushConstantRange> pushConstants) : deviceHandle(device)
{
	vk::PipelineLayoutCreateInfo layoutCreateInfo;
	layoutCreateInfo.setLayoutCount = setLayouts.size();
	layoutCreateInfo.pSetLayouts = setLayouts.data();
	layoutCreateInfo.pushConstantRangeCount = pushConstants.size();
	layoutCreateInfo.pPushConstantRanges = pushConstants.data();
	layout = deviceHandle.createPipelineLayout(layoutCreateInfo);
}

vw::PipelineLayout::~PipelineLayout()
{
	deviceHandle.destroyPipelineLayout(layout);
}

vw::PipelineLayout::operator vk::PipelineLayout()
{
	return layout;
}
