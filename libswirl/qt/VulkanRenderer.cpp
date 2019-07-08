/*
*	qt/VulkanRenderer.cpp
*/
#include <QVulkanFunctions>
#include <QtConcurrent>
#include <QTime>

#include "Application.h"
#include "VulkanWindow.h"
#include "VulkanRenderer.h"



VulkanRenderer::VulkanRenderer(VulkanWindow* w) //, int initialCount)
	: QVulkanWindowRenderer()
	, m_window(w)
{

}




static float vertexData[] = { // Y up, front = CW
	 // x, y, z, u, v
	 -1, -1, 0, 0, 1,
	 -1,  1, 0, 0, 0,
	  1, -1, 0, 1, 1,
	  1,  1, 0, 1, 0
};

static const int UNIFORM_DATA_SIZE = 16 * sizeof(float);

static inline VkDeviceSize aligned(VkDeviceSize v, VkDeviceSize byteAlign)
{
	return (v + byteAlign - 1) & ~(byteAlign - 1);
}

bool Vk_StartFrame = false;


void VulkanRenderer::startNextFrame()
{
	qDebug() << __FUNCTION__"()";

	Vk_StartFrame = true;
#if 0
	VkDevice dev = m_window->device();
	VkCommandBuffer cb = m_window->currentCommandBuffer();
	const QSize sz = m_window->swapChainImageSize();
	// Add the necessary barriers and do the host-linear -> device-optimal copy, if not yet done.
	ensureTexture();

	VkClearColorValue clearColor = { { 0, 0, 0, 1 } };
	VkClearDepthStencilValue clearDS = { 1, 0 };
	VkClearValue clearValues[2];
	memset(clearValues, 0, sizeof(clearValues));
	clearValues[0].color = clearColor;
	clearValues[1].depthStencil = clearDS;

	VkRenderPassBeginInfo rpBeginInfo;
	memset(&rpBeginInfo, 0, sizeof(rpBeginInfo));
	rpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rpBeginInfo.renderPass = m_window->defaultRenderPass();
	rpBeginInfo.framebuffer = m_window->currentFramebuffer();
	rpBeginInfo.renderArea.extent.width = sz.width();
	rpBeginInfo.renderArea.extent.height = sz.height();
	rpBeginInfo.clearValueCount = 2;
	rpBeginInfo.pClearValues = clearValues;
	VkCommandBuffer cmdBuf = m_window->currentCommandBuffer();
	m_devFuncs->vkCmdBeginRenderPass(cmdBuf, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	quint8* p;
	VkResult err = m_devFuncs->vkMapMemory(dev, m_bufMem, m_uniformBufInfo[m_window->currentFrame()].offset,
		UNIFORM_DATA_SIZE, 0, reinterpret_cast<void**>(&p));
	if (err != VK_SUCCESS)
		qFatal("Failed to map memory: %d", err);
	QMatrix4x4 m = m_proj;
	m.rotate(m_rotation, 0, 0, 1);
	memcpy(p, m.constData(), 16 * sizeof(float));
	m_devFuncs->vkUnmapMemory(dev, m_bufMem);

	// Not exactly a real animation system, just advance on every frame for now.
	m_rotation += 1.0f;

	m_devFuncs->vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
	m_devFuncs->vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1,
		&m_descSet[m_window->currentFrame()], 0, nullptr);
	VkDeviceSize vbOffset = 0;
	m_devFuncs->vkCmdBindVertexBuffers(cb, 0, 1, &m_buf, &vbOffset);

	VkViewport viewport;
	viewport.x = viewport.y = 0;
	viewport.width = sz.width();
	viewport.height = sz.height();
	viewport.minDepth = 0;
	viewport.maxDepth = 1;
	m_devFuncs->vkCmdSetViewport(cb, 0, 1, &viewport);

	VkRect2D scissor;
	scissor.offset.x = scissor.offset.y = 0;
	scissor.extent.width = viewport.width;
	scissor.extent.height = viewport.height;
	m_devFuncs->vkCmdSetScissor(cb, 0, 1, &scissor);

	m_devFuncs->vkCmdDraw(cb, 4, 1, 0, 0);

	m_devFuncs->vkCmdEndRenderPass(cmdBuf);
	m_window->frameReady();
	m_window->requestUpdate(); // render continuously, throttled by the presentation rate

#endif
}






void VulkanRenderer::initResources()
{
	//if (DBG)
		qDebug(__FUNCTION__"()");


#if 0
	QVulkanInstance* inst = m_window->vulkanInstance();

	VkDevice dev = m_window->device();
	m_devFuncs = inst->deviceFunctions(dev);

	const VkPhysicalDeviceLimits* pdevLimits = &m_window->physicalDeviceProperties()->limits;
	const VkDeviceSize uniAlign = pdevLimits->minUniformBufferOffsetAlignment;

	qDebug("uniform buffer offset alignment is %u", (uint)uniAlign);

	const int concurrentFrameCount = m_window->concurrentFrameCount();
	VkBufferCreateInfo bufInfo;
	memset(&bufInfo, 0, sizeof(bufInfo));
	bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	// Our internal layout is vertex, uniform, uniform, ... with each uniform buffer start offset aligned to uniAlign.
	const VkDeviceSize vertexAllocSize = aligned(sizeof(vertexData), uniAlign);
	const VkDeviceSize uniformAllocSize = aligned(UNIFORM_DATA_SIZE, uniAlign);
	bufInfo.size = vertexAllocSize + concurrentFrameCount * uniformAllocSize;
	bufInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

	VkResult err = m_devFuncs->vkCreateBuffer(dev, &bufInfo, nullptr, &m_buf);
	if (err != VK_SUCCESS)
		qFatal("Failed to create buffer: %d", err);

	VkMemoryRequirements memReq;
	m_devFuncs->vkGetBufferMemoryRequirements(dev, m_buf, &memReq);

	VkMemoryAllocateInfo memAllocInfo = {
		VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		nullptr,
		memReq.size,
		m_window->hostVisibleMemoryIndex()
	};

	err = m_devFuncs->vkAllocateMemory(dev, &memAllocInfo, nullptr, &m_bufMem);
	if (err != VK_SUCCESS)
		qFatal("Failed to allocate memory: %d", err);

	err = m_devFuncs->vkBindBufferMemory(dev, m_buf, m_bufMem, 0);
	if (err != VK_SUCCESS)
		qFatal("Failed to bind buffer memory: %d", err);

	quint8* p;
	err = m_devFuncs->vkMapMemory(dev, m_bufMem, 0, memReq.size, 0, reinterpret_cast<void**>(&p));
	if (err != VK_SUCCESS)
		qFatal("Failed to map memory: %d", err);
	memcpy(p, vertexData, sizeof(vertexData));
	QMatrix4x4 ident;
	memset(m_uniformBufInfo, 0, sizeof(m_uniformBufInfo));
	for (int i = 0; i < concurrentFrameCount; ++i) {
		const VkDeviceSize offset = vertexAllocSize + i * uniformAllocSize;
		memcpy(p + offset, ident.constData(), 16 * sizeof(float));
		m_uniformBufInfo[i].buffer = m_buf;
		m_uniformBufInfo[i].offset = offset;
		m_uniformBufInfo[i].range = uniformAllocSize;
	}
	m_devFuncs->vkUnmapMemory(dev, m_bufMem);

	VkVertexInputBindingDescription vertexBindingDesc = {
		0, // binding
		5 * sizeof(float),
		VK_VERTEX_INPUT_RATE_VERTEX
	};
	VkVertexInputAttributeDescription vertexAttrDesc[] = {
		{ // position
			0, // location
			0, // binding
			VK_FORMAT_R32G32B32_SFLOAT,
			0
		},
		{ // texcoord
			1,
			0,
			VK_FORMAT_R32G32_SFLOAT,
			3 * sizeof(float)
		}
	};

	VkPipelineVertexInputStateCreateInfo vertexInputInfo;
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.pNext = nullptr;
	vertexInputInfo.flags = 0;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &vertexBindingDesc;
	vertexInputInfo.vertexAttributeDescriptionCount = 2;
	vertexInputInfo.pVertexAttributeDescriptions = vertexAttrDesc;


	// Sampler.
	VkSamplerCreateInfo samplerInfo;
	memset(&samplerInfo, 0, sizeof(samplerInfo));
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_NEAREST;
	samplerInfo.minFilter = VK_FILTER_NEAREST;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.maxAnisotropy = 1.0f;
	err = m_devFuncs->vkCreateSampler(dev, &samplerInfo, nullptr, &m_sampler);
	if (err != VK_SUCCESS)
		qFatal("Failed to create sampler: %d", err);


#if 1

	// Texture.
	if (!createTexture(QStringLiteral("./tex512.png")))
		qFatal("Failed to create texture");

#endif

	// Set up descriptor set and its layout.
	VkDescriptorPoolSize descPoolSizes[2] = {
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, uint32_t(concurrentFrameCount) },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, uint32_t(concurrentFrameCount) }
	};
	VkDescriptorPoolCreateInfo descPoolInfo;
	memset(&descPoolInfo, 0, sizeof(descPoolInfo));
	descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descPoolInfo.maxSets = concurrentFrameCount;
	descPoolInfo.poolSizeCount = 2;
	descPoolInfo.pPoolSizes = descPoolSizes;
	err = m_devFuncs->vkCreateDescriptorPool(dev, &descPoolInfo, nullptr, &m_descPool);
	if (err != VK_SUCCESS)
		qFatal("Failed to create descriptor pool: %d", err);

	VkDescriptorSetLayoutBinding layoutBinding[2] =
	{
		{
			0, // binding
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			1, // descriptorCount
			VK_SHADER_STAGE_VERTEX_BIT,
			nullptr
		},
		{
			1, // binding
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			1, // descriptorCount
			VK_SHADER_STAGE_FRAGMENT_BIT,
			nullptr
		}
	};
	VkDescriptorSetLayoutCreateInfo descLayoutInfo = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		2, // bindingCount
		layoutBinding
	};
	err = m_devFuncs->vkCreateDescriptorSetLayout(dev, &descLayoutInfo, nullptr, &m_descSetLayout);
	if (err != VK_SUCCESS)
		qFatal("Failed to create descriptor set layout: %d", err);

	for (int i = 0; i < concurrentFrameCount; ++i) {
		VkDescriptorSetAllocateInfo descSetAllocInfo = {
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			nullptr,
			m_descPool,
			1,
			&m_descSetLayout
		};
		err = m_devFuncs->vkAllocateDescriptorSets(dev, &descSetAllocInfo, &m_descSet[i]);
		if (err != VK_SUCCESS)
			qFatal("Failed to allocate descriptor set: %d", err);

		VkWriteDescriptorSet descWrite[2];
		memset(descWrite, 0, sizeof(descWrite));
		descWrite[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descWrite[0].dstSet = m_descSet[i];
		descWrite[0].dstBinding = 0;
		descWrite[0].descriptorCount = 1;
		descWrite[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descWrite[0].pBufferInfo = &m_uniformBufInfo[i];

		VkDescriptorImageInfo descImageInfo = {
			m_sampler,
			m_texView,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};

		descWrite[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descWrite[1].dstSet = m_descSet[i];
		descWrite[1].dstBinding = 1;
		descWrite[1].descriptorCount = 1;
		descWrite[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descWrite[1].pImageInfo = &descImageInfo;

		m_devFuncs->vkUpdateDescriptorSets(dev, 2, descWrite, 0, nullptr);
	}

	// Pipeline cache
	VkPipelineCacheCreateInfo pipelineCacheInfo;
	memset(&pipelineCacheInfo, 0, sizeof(pipelineCacheInfo));
	pipelineCacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	err = m_devFuncs->vkCreatePipelineCache(dev, &pipelineCacheInfo, nullptr, &m_pipelineCache);
	if (err != VK_SUCCESS)
		qFatal("Failed to create pipeline cache: %d", err);

	// Pipeline layout
	VkPipelineLayoutCreateInfo pipelineLayoutInfo;
	memset(&pipelineLayoutInfo, 0, sizeof(pipelineLayoutInfo));
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &m_descSetLayout;
	err = m_devFuncs->vkCreatePipelineLayout(dev, &pipelineLayoutInfo, nullptr, &m_pipelineLayout);
	if (err != VK_SUCCESS)
		qFatal("Failed to create pipeline layout: %d", err);

#if 1

	// Shaders
	VkShaderModule vertShaderModule = createShader(QStringLiteral("./shader.vert.spv"));
	VkShaderModule fragShaderModule = createShader(QStringLiteral("./shader.frag.spv"));

#endif

	// Graphics pipeline
	VkGraphicsPipelineCreateInfo pipelineInfo;
	memset(&pipelineInfo, 0, sizeof(pipelineInfo));
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

	VkPipelineShaderStageCreateInfo shaderStages[2] = {
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_VERTEX_BIT,
			vertShaderModule,
			"main",
			nullptr
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			fragShaderModule,
			"main",
			nullptr
		}
	};
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;

	pipelineInfo.pVertexInputState = &vertexInputInfo;

	VkPipelineInputAssemblyStateCreateInfo ia;
	memset(&ia, 0, sizeof(ia));
	ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	pipelineInfo.pInputAssemblyState = &ia;

	// The viewport and scissor will be set dynamically via vkCmdSetViewport/Scissor.
	// This way the pipeline does not need to be touched when resizing the window.
	VkPipelineViewportStateCreateInfo vp;
	memset(&vp, 0, sizeof(vp));
	vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	vp.viewportCount = 1;
	vp.scissorCount = 1;
	pipelineInfo.pViewportState = &vp;

	VkPipelineRasterizationStateCreateInfo rs;
	memset(&rs, 0, sizeof(rs));
	rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rs.polygonMode = VK_POLYGON_MODE_FILL;
	rs.cullMode = VK_CULL_MODE_BACK_BIT;
	rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rs.lineWidth = 1.0f;
	pipelineInfo.pRasterizationState = &rs;

	VkPipelineMultisampleStateCreateInfo ms;
	memset(&ms, 0, sizeof(ms));
	ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	pipelineInfo.pMultisampleState = &ms;

	VkPipelineDepthStencilStateCreateInfo ds;
	memset(&ds, 0, sizeof(ds));
	ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	ds.depthTestEnable = VK_TRUE;
	ds.depthWriteEnable = VK_TRUE;
	ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	pipelineInfo.pDepthStencilState = &ds;

	VkPipelineColorBlendStateCreateInfo cb;
	memset(&cb, 0, sizeof(cb));
	cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	// assume pre-multiplied alpha, blend, write out all of rgba
	VkPipelineColorBlendAttachmentState att;
	memset(&att, 0, sizeof(att));
	att.colorWriteMask = 0xF;
	att.blendEnable = VK_TRUE;
	att.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	att.colorBlendOp = VK_BLEND_OP_ADD;
	att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	att.alphaBlendOp = VK_BLEND_OP_ADD;
	cb.attachmentCount = 1;
	cb.pAttachments = &att;
	pipelineInfo.pColorBlendState = &cb;

	VkDynamicState dynEnable[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dyn;
	memset(&dyn, 0, sizeof(dyn));
	dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dyn.dynamicStateCount = sizeof(dynEnable) / sizeof(VkDynamicState);
	dyn.pDynamicStates = dynEnable;
	pipelineInfo.pDynamicState = &dyn;

	pipelineInfo.layout = m_pipelineLayout;
	pipelineInfo.renderPass = m_window->defaultRenderPass();

	err = m_devFuncs->vkCreateGraphicsPipelines(dev, m_pipelineCache, 1, &pipelineInfo, nullptr, &m_pipeline);
	if (err != VK_SUCCESS)
		qFatal("Failed to create graphics pipeline: %d", err);

	if (vertShaderModule)
		m_devFuncs->vkDestroyShaderModule(dev, vertShaderModule, nullptr);
	if (fragShaderModule)
		m_devFuncs->vkDestroyShaderModule(dev, fragShaderModule, nullptr);



#endif


}



void VulkanRenderer::initSwapChainResources()
{
	qDebug() << __FUNCTION__"()";
#if 0
	m_proj = m_window->clipCorrectionMatrix();
	const QSize sz = m_window->swapChainImageSize();
	m_proj.perspective(45.0f, sz.width() / (float)sz.height(), 0.01f, 1000.0f);
	markViewProjDirty();
#endif
}

void VulkanRenderer::releaseSwapChainResources()
{
	qDebug() << __FUNCTION__"()";
#if 0
	// It is important to finish the pending frame right here since this is the
	// last opportunity to act with all resources intact.
	m_frameWatcher.waitForFinished();
	// Cannot count on the finished() signal being emitted before returning
	// from here.
	if (m_framePending) {
		m_framePending = false;
		m_window->frameReady();
	}
#endif
}

void VulkanRenderer::releaseResources()
{
	qDebug() << __FUNCTION__"()";
	//if (DBG)
	//m_pipelinesFuture.waitForFinished();

	VkDevice dev = m_window->device();

#if 0
	if (m_itemMaterial.descSetLayout) {
		m_devFuncs->vkDestroyDescriptorSetLayout(dev, m_itemMaterial.descSetLayout, nullptr);
		m_itemMaterial.descSetLayout = VK_NULL_HANDLE;
	}

	if (m_itemMaterial.descPool) {
		m_devFuncs->vkDestroyDescriptorPool(dev, m_itemMaterial.descPool, nullptr);
		m_itemMaterial.descPool = VK_NULL_HANDLE;
	}

	if (m_itemMaterial.pipeline) {
		m_devFuncs->vkDestroyPipeline(dev, m_itemMaterial.pipeline, nullptr);
		m_itemMaterial.pipeline = VK_NULL_HANDLE;
	}

	if (m_itemMaterial.pipelineLayout) {
		m_devFuncs->vkDestroyPipelineLayout(dev, m_itemMaterial.pipelineLayout, nullptr);
		m_itemMaterial.pipelineLayout = VK_NULL_HANDLE;
	}
	if (m_pipelineCache) {
		m_devFuncs->vkDestroyPipelineCache(dev, m_pipelineCache, nullptr);
		m_pipelineCache = VK_NULL_HANDLE;
	}

	if (m_blockVertexBuf) {
		m_devFuncs->vkDestroyBuffer(dev, m_blockVertexBuf, nullptr);
		m_blockVertexBuf = VK_NULL_HANDLE;
	}


	if (m_instBufMem) {
		m_devFuncs->vkFreeMemory(dev, m_instBufMem, nullptr);
		m_instBufMem = VK_NULL_HANDLE;
	}


#endif
}











#if 0

VkShaderModule VulkanRenderer::createShader(const QString& name)
{
	QFile file(name);
	if (!file.open(QIODevice::ReadOnly)) {
		qWarning("Failed to read shader %s", qPrintable(name));
		return VK_NULL_HANDLE;
	}
	QByteArray blob = file.readAll();
	file.close();

	VkShaderModuleCreateInfo shaderInfo;
	memset(&shaderInfo, 0, sizeof(shaderInfo));
	shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderInfo.codeSize = blob.size();
	shaderInfo.pCode = reinterpret_cast<const uint32_t*>(blob.constData());
	VkShaderModule shaderModule;
	VkResult err = m_devFuncs->vkCreateShaderModule(m_window->device(), &shaderInfo, nullptr, &shaderModule);
	if (err != VK_SUCCESS) {
		qWarning("Failed to create shader module: %d", err);
		return VK_NULL_HANDLE;
	}

	return shaderModule;
}





/*
*	This is a rather large pile of horseshit , ridiculous amt of code to handle loading textures...
*/


bool VulkanRenderer::createTexture(const QString& name)
{
	QImage img(name);
	if (img.isNull()) {
		qWarning("Failed to load image %s", qPrintable(name));
		return false;
	}

	// Convert to byte ordered RGBA8. Use premultiplied alpha, see pColorBlendState in the pipeline.
	img = img.convertToFormat(QImage::Format_RGBA8888_Premultiplied);

	QVulkanFunctions* f = m_window->vulkanInstance()->functions();
	VkDevice dev = m_window->device();

	const bool srgb = QCoreApplication::arguments().contains(QStringLiteral("--srgb"));
	if (srgb)
		qDebug("sRGB swapchain was requested, making texture sRGB too");

	m_texFormat = srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;

	// Now we can either map and copy the image data directly, or have to go
	// through a staging buffer to copy and convert into the internal optimal
	// tiling format.
	VkFormatProperties props;
	f->vkGetPhysicalDeviceFormatProperties(m_window->physicalDevice(), m_texFormat, &props);
	const bool canSampleLinear = (props.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
	const bool canSampleOptimal = (props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
	if (!canSampleLinear && !canSampleOptimal) {
		qWarning("Neither linear nor optimal image sampling is supported for RGBA8");
		return false;
	}

	static bool alwaysStage = qEnvironmentVariableIntValue("QT_VK_FORCE_STAGE_TEX");

	if (canSampleLinear && !alwaysStage) {
		if (!createTextureImage(img.size(), &m_texImage, &m_texMem,
			VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_SAMPLED_BIT,
			m_window->hostVisibleMemoryIndex()))
			return false;

		if (!writeLinearImage(img, m_texImage, m_texMem))
			return false;

		m_texLayoutPending = true;
	}
	else {
		if (!createTextureImage(img.size(), &m_texStaging, &m_texStagingMem,
			VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
			m_window->hostVisibleMemoryIndex()))
			return false;

		if (!createTextureImage(img.size(), &m_texImage, &m_texMem,
			VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			m_window->deviceLocalMemoryIndex()))
			return false;

		if (!writeLinearImage(img, m_texStaging, m_texStagingMem))
			return false;

		m_texStagingPending = true;
	}

	VkImageViewCreateInfo viewInfo;
	memset(&viewInfo, 0, sizeof(viewInfo));
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = m_texImage;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = m_texFormat;
	viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
	viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
	viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
	viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.levelCount = viewInfo.subresourceRange.layerCount = 1;

	VkResult err = m_devFuncs->vkCreateImageView(dev, &viewInfo, nullptr, &m_texView);
	if (err != VK_SUCCESS) {
		qWarning("Failed to create image view for texture: %d", err);
		return false;
	}

	m_texSize = img.size();

	return true;
}

bool VulkanRenderer::createTextureImage(const QSize& size, VkImage* image, VkDeviceMemory* mem,
	VkImageTiling tiling, VkImageUsageFlags usage, uint32_t memIndex)
{
	VkDevice dev = m_window->device();

	VkImageCreateInfo imageInfo;
	memset(&imageInfo, 0, sizeof(imageInfo));
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = m_texFormat;
	imageInfo.extent.width = size.width();
	imageInfo.extent.height = size.height();
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = tiling;
	imageInfo.usage = usage;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;

	VkResult err = m_devFuncs->vkCreateImage(dev, &imageInfo, nullptr, image);
	if (err != VK_SUCCESS) {
		qWarning("Failed to create linear image for texture: %d", err);
		return false;
	}

	VkMemoryRequirements memReq;
	m_devFuncs->vkGetImageMemoryRequirements(dev, *image, &memReq);

	if (!(memReq.memoryTypeBits & (1 << memIndex))) {
		VkPhysicalDeviceMemoryProperties physDevMemProps;
		m_window->vulkanInstance()->functions()->vkGetPhysicalDeviceMemoryProperties(m_window->physicalDevice(), &physDevMemProps);
		for (uint32_t i = 0; i < physDevMemProps.memoryTypeCount; ++i) {
			if (!(memReq.memoryTypeBits & (1 << i)))
				continue;
			memIndex = i;
		}
	}

	VkMemoryAllocateInfo allocInfo = {
		VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		nullptr,
		memReq.size,
		memIndex
	};
	qDebug("allocating %u bytes for texture image", uint32_t(memReq.size));

	err = m_devFuncs->vkAllocateMemory(dev, &allocInfo, nullptr, mem);
	if (err != VK_SUCCESS) {
		qWarning("Failed to allocate memory for linear image: %d", err);
		return false;
	}

	err = m_devFuncs->vkBindImageMemory(dev, *image, *mem, 0);
	if (err != VK_SUCCESS) {
		qWarning("Failed to bind linear image memory: %d", err);
		return false;
	}

	return true;
}

bool VulkanRenderer::writeLinearImage(const QImage& img, VkImage image, VkDeviceMemory memory)
{
	VkDevice dev = m_window->device();

	VkImageSubresource subres = {
		VK_IMAGE_ASPECT_COLOR_BIT,
		0, // mip level
		0
	};
	VkSubresourceLayout layout;
	m_devFuncs->vkGetImageSubresourceLayout(dev, image, &subres, &layout);

	uchar* p;
	VkResult err = m_devFuncs->vkMapMemory(dev, memory, layout.offset, layout.size, 0, reinterpret_cast<void**>(&p));
	if (err != VK_SUCCESS) {
		qWarning("Failed to map memory for linear image: %d", err);
		return false;
	}

	for (int y = 0; y < img.height(); ++y) {
		const uchar* line = img.constScanLine(y);
		memcpy(p, line, img.width() * 4);
		p += layout.rowPitch;
	}

	m_devFuncs->vkUnmapMemory(dev, memory);
	return true;
}

void VulkanRenderer::ensureTexture()
{
	if (!m_texLayoutPending && !m_texStagingPending)
		return;

	Q_ASSERT(m_texLayoutPending != m_texStagingPending);
	VkCommandBuffer cb = m_window->currentCommandBuffer();

	VkImageMemoryBarrier barrier;
	memset(&barrier, 0, sizeof(barrier));
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.levelCount = barrier.subresourceRange.layerCount = 1;

	if (m_texLayoutPending) {
		m_texLayoutPending = false;

		barrier.oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrier.image = m_texImage;

		m_devFuncs->vkCmdPipelineBarrier(cb,
			VK_PIPELINE_STAGE_HOST_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0, 0, nullptr, 0, nullptr,
			1, &barrier);
	}
	else {
		m_texStagingPending = false;

		barrier.oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.image = m_texStaging;
		m_devFuncs->vkCmdPipelineBarrier(cb,
			VK_PIPELINE_STAGE_HOST_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			0, 0, nullptr, 0, nullptr,
			1, &barrier);

		barrier.oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.image = m_texImage;
		m_devFuncs->vkCmdPipelineBarrier(cb,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			0, 0, nullptr, 0, nullptr,
			1, &barrier);

		VkImageCopy copyInfo;
		memset(&copyInfo, 0, sizeof(copyInfo));
		copyInfo.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyInfo.srcSubresource.layerCount = 1;
		copyInfo.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyInfo.dstSubresource.layerCount = 1;
		copyInfo.extent.width = m_texSize.width();
		copyInfo.extent.height = m_texSize.height();
		copyInfo.extent.depth = 1;
		m_devFuncs->vkCmdCopyImage(cb, m_texStaging, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			m_texImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyInfo);

		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrier.image = m_texImage;
		m_devFuncs->vkCmdPipelineBarrier(cb,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0, 0, nullptr, 0, nullptr,
			1, &barrier);
	}
}



void VulkanRenderer::createItemPipeline()
{
	VkDevice dev = m_window->device();

#if 0
	// Vertex layout.
	VkVertexInputBindingDescription vertexBindingDesc[] = {
		{
			0, // binding
			8 * sizeof(float),
			VK_VERTEX_INPUT_RATE_VERTEX
		},
		{
			1,
			6 * sizeof(float),
			VK_VERTEX_INPUT_RATE_INSTANCE
		}
	};
	VkVertexInputAttributeDescription vertexAttrDesc[] = {
		{ // position
			0, // location
			0, // binding
			VK_FORMAT_R32G32B32_SFLOAT,
			0 // offset
		},
		{ // normal
			1,
			0,
			VK_FORMAT_R32G32B32_SFLOAT,
			5 * sizeof(float)
		},
		{ // instTranslate
			2,
			1,
			VK_FORMAT_R32G32B32_SFLOAT,
			0
		},
		{ // instDiffuseAdjust
			3,
			1,
			VK_FORMAT_R32G32B32_SFLOAT,
			3 * sizeof(float)
		}
	};

	VkPipelineVertexInputStateCreateInfo vertexInputInfo;
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.pNext = nullptr;
	vertexInputInfo.flags = 0;
	vertexInputInfo.vertexBindingDescriptionCount = sizeof(vertexBindingDesc) / sizeof(vertexBindingDesc[0]);
	vertexInputInfo.pVertexBindingDescriptions = vertexBindingDesc;
	vertexInputInfo.vertexAttributeDescriptionCount = sizeof(vertexAttrDesc) / sizeof(vertexAttrDesc[0]);
	vertexInputInfo.pVertexAttributeDescriptions = vertexAttrDesc;

	// Descriptor set layout.
	VkDescriptorPoolSize descPoolSizes[] = {
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 2 }
	};
	VkDescriptorPoolCreateInfo descPoolInfo;
	memset(&descPoolInfo, 0, sizeof(descPoolInfo));
	descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descPoolInfo.maxSets = 1; // a single set is enough due to the dynamic uniform buffer
	descPoolInfo.poolSizeCount = sizeof(descPoolSizes) / sizeof(descPoolSizes[0]);
	descPoolInfo.pPoolSizes = descPoolSizes;

	VkResult err = m_devFuncs->vkCreateDescriptorPool(dev, &descPoolInfo, nullptr, &m_itemMaterial.descPool);
	if (err != VK_SUCCESS)
		qFatal("Failed to create descriptor pool: %d", err);

	VkDescriptorSetLayoutBinding layoutBindings[] =
	{
		{
			0, // binding
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
			1, // descriptorCount
			VK_SHADER_STAGE_VERTEX_BIT,
			nullptr
		},
		{
			1,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
			1,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			nullptr
		}
	};
	VkDescriptorSetLayoutCreateInfo descLayoutInfo = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		sizeof(layoutBindings) / sizeof(layoutBindings[0]),
		layoutBindings
	};
	err = m_devFuncs->vkCreateDescriptorSetLayout(dev, &descLayoutInfo, nullptr, &m_itemMaterial.descSetLayout);
	if (err != VK_SUCCESS)
		qFatal("Failed to create descriptor set layout: %d", err);

	VkDescriptorSetAllocateInfo descSetAllocInfo = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		nullptr,
		m_itemMaterial.descPool,
		1,
		&m_itemMaterial.descSetLayout
	};
	err = m_devFuncs->vkAllocateDescriptorSets(dev, &descSetAllocInfo, &m_itemMaterial.descSet);
	if (err != VK_SUCCESS)
		qFatal("Failed to allocate descriptor set: %d", err);

	// Graphics pipeline.
	VkPipelineLayoutCreateInfo pipelineLayoutInfo;
	memset(&pipelineLayoutInfo, 0, sizeof(pipelineLayoutInfo));
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &m_itemMaterial.descSetLayout;

	err = m_devFuncs->vkCreatePipelineLayout(dev, &pipelineLayoutInfo, nullptr, &m_itemMaterial.pipelineLayout);
	if (err != VK_SUCCESS)
		qFatal("Failed to create pipeline layout: %d", err);

	VkGraphicsPipelineCreateInfo pipelineInfo;
	memset(&pipelineInfo, 0, sizeof(pipelineInfo));
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

	VkPipelineShaderStageCreateInfo shaderStages[2] = {
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_VERTEX_BIT,
			m_itemMaterial.vs.data()->shaderModule,
			"main",
			nullptr
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			m_itemMaterial.fs.data()->shaderModule,
			"main",
			nullptr
		}
	};
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;

	pipelineInfo.pVertexInputState = &vertexInputInfo;

	VkPipelineInputAssemblyStateCreateInfo ia;
	memset(&ia, 0, sizeof(ia));
	ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	pipelineInfo.pInputAssemblyState = &ia;

	VkPipelineViewportStateCreateInfo vp;
	memset(&vp, 0, sizeof(vp));
	vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	vp.viewportCount = 1;
	vp.scissorCount = 1;
	pipelineInfo.pViewportState = &vp;

	VkPipelineRasterizationStateCreateInfo rs;
	memset(&rs, 0, sizeof(rs));
	rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rs.polygonMode = VK_POLYGON_MODE_FILL;
	rs.cullMode = VK_CULL_MODE_BACK_BIT;
	rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rs.lineWidth = 1.0f;
	pipelineInfo.pRasterizationState = &rs;

	VkPipelineMultisampleStateCreateInfo ms;
	memset(&ms, 0, sizeof(ms));
	ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	ms.rasterizationSamples = m_window->sampleCountFlagBits();
	pipelineInfo.pMultisampleState = &ms;

	VkPipelineDepthStencilStateCreateInfo ds;
	memset(&ds, 0, sizeof(ds));
	ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	ds.depthTestEnable = VK_TRUE;
	ds.depthWriteEnable = VK_TRUE;
	ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	pipelineInfo.pDepthStencilState = &ds;

	VkPipelineColorBlendStateCreateInfo cb;
	memset(&cb, 0, sizeof(cb));
	cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	VkPipelineColorBlendAttachmentState att;
	memset(&att, 0, sizeof(att));
	att.colorWriteMask = 0xF;
	cb.attachmentCount = 1;
	cb.pAttachments = &att;
	pipelineInfo.pColorBlendState = &cb;

	VkDynamicState dynEnable[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dyn;
	memset(&dyn, 0, sizeof(dyn));
	dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dyn.dynamicStateCount = sizeof(dynEnable) / sizeof(VkDynamicState);
	dyn.pDynamicStates = dynEnable;
	pipelineInfo.pDynamicState = &dyn;

	pipelineInfo.layout = m_itemMaterial.pipelineLayout;
	pipelineInfo.renderPass = m_window->defaultRenderPass();

	err = m_devFuncs->vkCreateGraphicsPipelines(dev, m_pipelineCache, 1, &pipelineInfo, nullptr, &m_itemMaterial.pipeline);
	if (err != VK_SUCCESS)
		qFatal("Failed to create graphics pipeline: %d", err);
#endif
}

void VulkanRenderer::createFloorPipeline()
{
	VkDevice dev = m_window->device();

	// Vertex layout.
	VkVertexInputBindingDescription vertexBindingDesc = {
		0, // binding
		3 * sizeof(float),
		VK_VERTEX_INPUT_RATE_VERTEX
	};
	VkVertexInputAttributeDescription vertexAttrDesc[] = {
		{ // position
			0, // location
			0, // binding
			VK_FORMAT_R32G32B32_SFLOAT,
			0 // offset
		},
	};

	VkPipelineVertexInputStateCreateInfo vertexInputInfo;
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.pNext = nullptr;
	vertexInputInfo.flags = 0;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &vertexBindingDesc;
	vertexInputInfo.vertexAttributeDescriptionCount = sizeof(vertexAttrDesc) / sizeof(vertexAttrDesc[0]);
	vertexInputInfo.pVertexAttributeDescriptions = vertexAttrDesc;

	// Do not bother with uniform buffers and descriptors, all the data fits
	// into the spec mandated minimum of 128 bytes for push constants.
	VkPushConstantRange pcr[] = {
		// mvp
		{
			VK_SHADER_STAGE_VERTEX_BIT,
			0,
			64
		},
		// color
		{
			VK_SHADER_STAGE_FRAGMENT_BIT,
			64,
			12
		}
	};

	VkPipelineLayoutCreateInfo pipelineLayoutInfo;
	memset(&pipelineLayoutInfo, 0, sizeof(pipelineLayoutInfo));
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.pushConstantRangeCount = sizeof(pcr) / sizeof(pcr[0]);
	pipelineLayoutInfo.pPushConstantRanges = pcr;

	VkResult err = m_devFuncs->vkCreatePipelineLayout(dev, &pipelineLayoutInfo, nullptr, &m_floorMaterial.pipelineLayout);
	if (err != VK_SUCCESS)
		qFatal("Failed to create pipeline layout: %d", err);

	VkGraphicsPipelineCreateInfo pipelineInfo;
	memset(&pipelineInfo, 0, sizeof(pipelineInfo));
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

	VkPipelineShaderStageCreateInfo shaderStages[2] = {
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_VERTEX_BIT,
			m_floorMaterial.vs.data()->shaderModule,
			"main",
			nullptr
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			m_floorMaterial.fs.data()->shaderModule,
			"main",
			nullptr
		}
	};
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;

	pipelineInfo.pVertexInputState = &vertexInputInfo;

	VkPipelineInputAssemblyStateCreateInfo ia;
	memset(&ia, 0, sizeof(ia));
	ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	pipelineInfo.pInputAssemblyState = &ia;

	VkPipelineViewportStateCreateInfo vp;
	memset(&vp, 0, sizeof(vp));
	vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	vp.viewportCount = 1;
	vp.scissorCount = 1;
	pipelineInfo.pViewportState = &vp;

	VkPipelineRasterizationStateCreateInfo rs;
	memset(&rs, 0, sizeof(rs));
	rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rs.polygonMode = VK_POLYGON_MODE_FILL;
	rs.cullMode = VK_CULL_MODE_BACK_BIT;
	rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rs.lineWidth = 1.0f;
	pipelineInfo.pRasterizationState = &rs;

	VkPipelineMultisampleStateCreateInfo ms;
	memset(&ms, 0, sizeof(ms));
	ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	ms.rasterizationSamples = m_window->sampleCountFlagBits();
	pipelineInfo.pMultisampleState = &ms;

	VkPipelineDepthStencilStateCreateInfo ds;
	memset(&ds, 0, sizeof(ds));
	ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	ds.depthTestEnable = VK_TRUE;
	ds.depthWriteEnable = VK_TRUE;
	ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	pipelineInfo.pDepthStencilState = &ds;

	VkPipelineColorBlendStateCreateInfo cb;
	memset(&cb, 0, sizeof(cb));
	cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	VkPipelineColorBlendAttachmentState att;
	memset(&att, 0, sizeof(att));
	att.colorWriteMask = 0xF;
	cb.attachmentCount = 1;
	cb.pAttachments = &att;
	pipelineInfo.pColorBlendState = &cb;

	VkDynamicState dynEnable[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dyn;
	memset(&dyn, 0, sizeof(dyn));
	dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dyn.dynamicStateCount = sizeof(dynEnable) / sizeof(VkDynamicState);
	dyn.pDynamicStates = dynEnable;
	pipelineInfo.pDynamicState = &dyn;

	pipelineInfo.layout = m_floorMaterial.pipelineLayout;
	pipelineInfo.renderPass = m_window->defaultRenderPass();

	err = m_devFuncs->vkCreateGraphicsPipelines(dev, m_pipelineCache, 1, &pipelineInfo, nullptr, &m_floorMaterial.pipeline);
	if (err != VK_SUCCESS)
		qFatal("Failed to create graphics pipeline: %d", err);
}











static float quadVert[] = { // Y up, front = CW
	-1, -1, 0,
	-1,  1, 0,
	 1, -1, 0,
	 1,  1, 0
};

#define DBG Q_UNLIKELY(m_window->isDebugEnabled())

const int MAX_INSTANCES = 16384;
const VkDeviceSize PER_INSTANCE_DATA_SIZE = 6 * sizeof(float); // instTranslate, instDiffuseAdjust

static inline VkDeviceSize aligned(VkDeviceSize v, VkDeviceSize byteAlign)
{
	return (v + byteAlign - 1) & ~(byteAlign - 1);
}

void VulkanRenderer::preInitResources()
{
	const QVector<int> sampleCounts = m_window->supportedSampleCounts();
	if (DBG)
		qDebug() << "Supported sample counts:" << sampleCounts;
	if (sampleCounts.contains(4)) {
		if (DBG)
			qDebug("Requesting 4x MSAA");
		m_window->setSampleCount(4);
	}
}

void VulkanRenderer::ensureBuffers()
{
	if (m_blockVertexBuf)
		return;

	VkDevice dev = m_window->device();
	const int concurrentFrameCount = m_window->concurrentFrameCount();

	// Vertex buffer for the block.
	VkBufferCreateInfo bufInfo;
	memset(&bufInfo, 0, sizeof(bufInfo));
	bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	const int blockMeshByteCount = m_blockMesh.data()->vertexCount * 8 * sizeof(float);
	bufInfo.size = blockMeshByteCount;
	bufInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	VkResult err = m_devFuncs->vkCreateBuffer(dev, &bufInfo, nullptr, &m_blockVertexBuf);
	if (err != VK_SUCCESS)
		qFatal("Failed to create vertex buffer: %d", err);

	VkMemoryRequirements blockVertMemReq;
	m_devFuncs->vkGetBufferMemoryRequirements(dev, m_blockVertexBuf, &blockVertMemReq);

	// Vertex buffer for the logo.
	const int logoMeshByteCount = m_logoMesh.data()->vertexCount * 8 * sizeof(float);
	bufInfo.size = logoMeshByteCount;
	bufInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	err = m_devFuncs->vkCreateBuffer(dev, &bufInfo, nullptr, &m_logoVertexBuf);
	if (err != VK_SUCCESS)
		qFatal("Failed to create vertex buffer: %d", err);

	VkMemoryRequirements logoVertMemReq;
	m_devFuncs->vkGetBufferMemoryRequirements(dev, m_logoVertexBuf, &logoVertMemReq);

	// Vertex buffer for the floor.
	bufInfo.size = sizeof(quadVert);
	err = m_devFuncs->vkCreateBuffer(dev, &bufInfo, nullptr, &m_floorVertexBuf);
	if (err != VK_SUCCESS)
		qFatal("Failed to create vertex buffer: %d", err);

	VkMemoryRequirements floorVertMemReq;
	m_devFuncs->vkGetBufferMemoryRequirements(dev, m_floorVertexBuf, &floorVertMemReq);

	// Uniform buffer. Instead of using multiple descriptor sets, we take a
	// different approach: have a single dynamic uniform buffer and specify the
	// active-frame-specific offset at the time of binding the descriptor set.
	bufInfo.size = (m_itemMaterial.vertUniSize + m_itemMaterial.fragUniSize) * concurrentFrameCount;
	bufInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	err = m_devFuncs->vkCreateBuffer(dev, &bufInfo, nullptr, &m_uniBuf);
	if (err != VK_SUCCESS)
		qFatal("Failed to create uniform buffer: %d", err);

	VkMemoryRequirements uniMemReq;
	m_devFuncs->vkGetBufferMemoryRequirements(dev, m_uniBuf, &uniMemReq);

	// Allocate memory for everything at once.
	VkDeviceSize logoVertStartOffset = aligned(0 + blockVertMemReq.size, logoVertMemReq.alignment);
	VkDeviceSize floorVertStartOffset = aligned(logoVertStartOffset + logoVertMemReq.size, floorVertMemReq.alignment);
	m_itemMaterial.uniMemStartOffset = aligned(floorVertStartOffset + floorVertMemReq.size, uniMemReq.alignment);
	VkMemoryAllocateInfo memAllocInfo = {
		VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		nullptr,
		m_itemMaterial.uniMemStartOffset + uniMemReq.size,
		m_window->hostVisibleMemoryIndex()
	};
	err = m_devFuncs->vkAllocateMemory(dev, &memAllocInfo, nullptr, &m_bufMem);
	if (err != VK_SUCCESS)
		qFatal("Failed to allocate memory: %d", err);

	err = m_devFuncs->vkBindBufferMemory(dev, m_blockVertexBuf, m_bufMem, 0);
	if (err != VK_SUCCESS)
		qFatal("Failed to bind vertex buffer memory: %d", err);
	err = m_devFuncs->vkBindBufferMemory(dev, m_logoVertexBuf, m_bufMem, logoVertStartOffset);
	if (err != VK_SUCCESS)
		qFatal("Failed to bind vertex buffer memory: %d", err);
	err = m_devFuncs->vkBindBufferMemory(dev, m_floorVertexBuf, m_bufMem, floorVertStartOffset);
	if (err != VK_SUCCESS)
		qFatal("Failed to bind vertex buffer memory: %d", err);
	err = m_devFuncs->vkBindBufferMemory(dev, m_uniBuf, m_bufMem, m_itemMaterial.uniMemStartOffset);
	if (err != VK_SUCCESS)
		qFatal("Failed to bind uniform buffer memory: %d", err);

	// Copy vertex data.
	quint8* p;
	err = m_devFuncs->vkMapMemory(dev, m_bufMem, 0, m_itemMaterial.uniMemStartOffset, 0, reinterpret_cast<void**>(&p));
	if (err != VK_SUCCESS)
		qFatal("Failed to map memory: %d", err);
	memcpy(p, m_blockMesh.data()->geom.constData(), blockMeshByteCount);
	memcpy(p + logoVertStartOffset, m_logoMesh.data()->geom.constData(), logoMeshByteCount);
	memcpy(p + floorVertStartOffset, quadVert, sizeof(quadVert));
	m_devFuncs->vkUnmapMemory(dev, m_bufMem);

	// Write descriptors for the uniform buffers in the vertex and fragment shaders.
	VkDescriptorBufferInfo vertUni = { m_uniBuf, 0, m_itemMaterial.vertUniSize };
	VkDescriptorBufferInfo fragUni = { m_uniBuf, m_itemMaterial.vertUniSize, m_itemMaterial.fragUniSize };

	VkWriteDescriptorSet descWrite[2];
	memset(descWrite, 0, sizeof(descWrite));
	descWrite[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descWrite[0].dstSet = m_itemMaterial.descSet;
	descWrite[0].dstBinding = 0;
	descWrite[0].descriptorCount = 1;
	descWrite[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	descWrite[0].pBufferInfo = &vertUni;

	descWrite[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descWrite[1].dstSet = m_itemMaterial.descSet;
	descWrite[1].dstBinding = 1;
	descWrite[1].descriptorCount = 1;
	descWrite[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	descWrite[1].pBufferInfo = &fragUni;

	m_devFuncs->vkUpdateDescriptorSets(dev, 2, descWrite, 0, nullptr);
}

void VulkanRenderer::ensureInstanceBuffer()
{
	if (m_instCount == m_preparedInstCount && m_instBuf)
		return;

	Q_ASSERT(m_instCount <= MAX_INSTANCES);

	VkDevice dev = m_window->device();

	// allocate only once, for the maximum instance count
	if (!m_instBuf) {
		VkBufferCreateInfo bufInfo;
		memset(&bufInfo, 0, sizeof(bufInfo));
		bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufInfo.size = MAX_INSTANCES * PER_INSTANCE_DATA_SIZE;
		bufInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

		// Keep a copy of the data since we may lose all graphics resources on
		// unexpose, and reinitializing to new random positions afterwards
		// would not be nice.
		m_instData.resize(bufInfo.size);

		VkResult err = m_devFuncs->vkCreateBuffer(dev, &bufInfo, nullptr, &m_instBuf);
		if (err != VK_SUCCESS)
			qFatal("Failed to create instance buffer: %d", err);

		VkMemoryRequirements memReq;
		m_devFuncs->vkGetBufferMemoryRequirements(dev, m_instBuf, &memReq);
		if (DBG)
			qDebug("Allocating %u bytes for instance data", uint32_t(memReq.size));

		VkMemoryAllocateInfo memAllocInfo = {
			VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			nullptr,
			memReq.size,
			m_window->hostVisibleMemoryIndex()
		};
		err = m_devFuncs->vkAllocateMemory(dev, &memAllocInfo, nullptr, &m_instBufMem);
		if (err != VK_SUCCESS)
			qFatal("Failed to allocate memory: %d", err);

		err = m_devFuncs->vkBindBufferMemory(dev, m_instBuf, m_instBufMem, 0);
		if (err != VK_SUCCESS)
			qFatal("Failed to bind instance buffer memory: %d", err);
	}

	if (m_instCount != m_preparedInstCount) {
		if (DBG)
			qDebug("Preparing instances %d..%d", m_preparedInstCount, m_instCount - 1);
		char* p = m_instData.data();
		p += m_preparedInstCount * PER_INSTANCE_DATA_SIZE;
		auto gen = [](float a, float b) { return float((qrand() % int(b - a + 1)) + a); };
		for (int i = m_preparedInstCount; i < m_instCount; ++i) {
			// Apply a random translation to each instance of the mesh.
			float t[] = { gen(-5, 5), gen(-4, 6), gen(-30, 5) };
			memcpy(p, t, 12);
			// Apply a random adjustment to the diffuse color for each instance. (default is 0.7)
			float d[] = { gen(-6, 3) / 10.0f, gen(-6, 3) / 10.0f, gen(-6, 3) / 10.0f };
			memcpy(p + 12, d, 12);
			p += PER_INSTANCE_DATA_SIZE;
		}
		m_preparedInstCount = m_instCount;
	}

	quint8* p;
	VkResult err = m_devFuncs->vkMapMemory(dev, m_instBufMem, 0, m_instCount * PER_INSTANCE_DATA_SIZE, 0,
		reinterpret_cast<void**>(&p));
	if (err != VK_SUCCESS)
		qFatal("Failed to map memory: %d", err);
	memcpy(p, m_instData.constData(), m_instData.size());
	m_devFuncs->vkUnmapMemory(dev, m_instBufMem);
}

void VulkanRenderer::getMatrices(QMatrix4x4* vp, QMatrix4x4* model, QMatrix3x3* modelNormal, QVector3D* eyePos)
{
	model->setToIdentity();
	if (m_useLogo)
		model->rotate(90, 1, 0, 0);
	model->rotate(m_rotation, 1, 1, 0);

	*modelNormal = model->normalMatrix();

	QMatrix4x4 view = m_cam.viewMatrix();
	*vp = m_proj * view;

	*eyePos = view.inverted().column(3).toVector3D();
}

void VulkanRenderer::writeFragUni(quint8* p, const QVector3D& eyePos)
{
	float ECCameraPosition[] = { eyePos.x(), eyePos.y(), eyePos.z() };
	memcpy(p, ECCameraPosition, 12);
	p += 16;

	// Material
	float ka[] = { 0.05f, 0.05f, 0.05f };
	memcpy(p, ka, 12);
	p += 16;

	float kd[] = { 0.7f, 0.7f, 0.7f };
	memcpy(p, kd, 12);
	p += 16;

	float ks[] = { 0.66f, 0.66f, 0.66f };
	memcpy(p, ks, 12);
	p += 16;

	// Light parameters
	float ECLightPosition[] = { m_lightPos.x(), m_lightPos.y(), m_lightPos.z() };
	memcpy(p, ECLightPosition, 12);
	p += 16;

	float att[] = { 1, 0, 0 };
	memcpy(p, att, 12);
	p += 16;

	float color[] = { 1.0f, 1.0f, 1.0f };
	memcpy(p, color, 12);
	p += 12; // next we have two floats which have an alignment of 4, hence 12 only

	float intensity = 0.8f;
	memcpy(p, &intensity, 4);
	p += 4;

	float specularExp = 150.0f;
	memcpy(p, &specularExp, 4);
	p += 4;
}


void VulkanRenderer::buildFrame()
{
	QMutexLocker locker(&m_guiMutex);

	ensureBuffers();
	ensureInstanceBuffer();
	m_pipelinesFuture.waitForFinished();

	VkCommandBuffer cb = m_window->currentCommandBuffer();
	const QSize sz = m_window->swapChainImageSize();

	VkClearColorValue clearColor = { { 0.67f, 0.84f, 0.9f, 1.0f } };
	VkClearDepthStencilValue clearDS = { 1, 0 };
	VkClearValue clearValues[3];
	memset(clearValues, 0, sizeof(clearValues));
	clearValues[0].color = clearValues[2].color = clearColor;
	clearValues[1].depthStencil = clearDS;

	VkRenderPassBeginInfo rpBeginInfo;
	memset(&rpBeginInfo, 0, sizeof(rpBeginInfo));
	rpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rpBeginInfo.renderPass = m_window->defaultRenderPass();
	rpBeginInfo.framebuffer = m_window->currentFramebuffer();
	rpBeginInfo.renderArea.extent.width = sz.width();
	rpBeginInfo.renderArea.extent.height = sz.height();
	rpBeginInfo.clearValueCount = m_window->sampleCountFlagBits() > VK_SAMPLE_COUNT_1_BIT ? 3 : 2;
	rpBeginInfo.pClearValues = clearValues;
	VkCommandBuffer cmdBuf = m_window->currentCommandBuffer();
	m_devFuncs->vkCmdBeginRenderPass(cmdBuf, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport = {
		0, 0,
		float(sz.width()), float(sz.height()),
		0, 1
	};
	m_devFuncs->vkCmdSetViewport(cb, 0, 1, &viewport);

	VkRect2D scissor = {
		{ 0, 0 },
		{ uint32_t(sz.width()), uint32_t(sz.height()) }
	};
	m_devFuncs->vkCmdSetScissor(cb, 0, 1, &scissor);

	buildDrawCallsForFloor();
	buildDrawCallsForItems();

	m_devFuncs->vkCmdEndRenderPass(cmdBuf);
}

void VulkanRenderer::buildDrawCallsForItems()
{
	VkDevice dev = m_window->device();
	VkCommandBuffer cb = m_window->currentCommandBuffer();

	m_devFuncs->vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_itemMaterial.pipeline);

	VkDeviceSize vbOffset = 0;
	m_devFuncs->vkCmdBindVertexBuffers(cb, 0, 1, m_useLogo ? &m_logoVertexBuf : &m_blockVertexBuf, &vbOffset);
	m_devFuncs->vkCmdBindVertexBuffers(cb, 1, 1, &m_instBuf, &vbOffset);

	// Now provide offsets so that the two dynamic buffers point to the
	// beginning of the vertex and fragment uniform data for the current frame.
	uint32_t frameUniOffset = m_window->currentFrame() * (m_itemMaterial.vertUniSize + m_itemMaterial.fragUniSize);
	uint32_t frameUniOffsets[] = { frameUniOffset, frameUniOffset };
	m_devFuncs->vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_itemMaterial.pipelineLayout, 0, 1,
		&m_itemMaterial.descSet, 2, frameUniOffsets);

	if (m_animating)
		m_rotation += 0.5;

	if (m_animating || m_vpDirty) {
		if (m_vpDirty)
			--m_vpDirty;
		QMatrix4x4 vp, model;
		QMatrix3x3 modelNormal;
		QVector3D eyePos;
		getMatrices(&vp, &model, &modelNormal, &eyePos);

		// Map the uniform data for the current frame, ignore the geometry data at
		// the beginning and the uniforms for other frames.
		quint8* p;
		VkResult err = m_devFuncs->vkMapMemory(dev, m_bufMem,
			m_itemMaterial.uniMemStartOffset + frameUniOffset,
			m_itemMaterial.vertUniSize + m_itemMaterial.fragUniSize,
			0, reinterpret_cast<void**>(&p));
		if (err != VK_SUCCESS)
			qFatal("Failed to map memory: %d", err);

		// Vertex shader uniforms
		memcpy(p, vp.constData(), 64);
		memcpy(p + 64, model.constData(), 64);
		const float* mnp = modelNormal.constData();
		memcpy(p + 128, mnp, 12);
		memcpy(p + 128 + 16, mnp + 3, 12);
		memcpy(p + 128 + 32, mnp + 6, 12);

		// Fragment shader uniforms
		p += m_itemMaterial.vertUniSize;
		writeFragUni(p, eyePos);

		m_devFuncs->vkUnmapMemory(dev, m_bufMem);
	}

	m_devFuncs->vkCmdDraw(cb, (m_useLogo ? m_logoMesh.data() : m_blockMesh.data())->vertexCount, m_instCount, 0, 0);
}

void VulkanRenderer::buildDrawCallsForFloor()
{
	VkCommandBuffer cb = m_window->currentCommandBuffer();

	m_devFuncs->vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_floorMaterial.pipeline);

	VkDeviceSize vbOffset = 0;
	m_devFuncs->vkCmdBindVertexBuffers(cb, 0, 1, &m_floorVertexBuf, &vbOffset);

	QMatrix4x4 mvp = m_proj * m_cam.viewMatrix() * m_floorModel;
	m_devFuncs->vkCmdPushConstants(cb, m_floorMaterial.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, 64, mvp.constData());
	float color[] = { 0.67f, 1.0f, 0.2f };
	m_devFuncs->vkCmdPushConstants(cb, m_floorMaterial.pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 64, 12, color);

	m_devFuncs->vkCmdDraw(cb, 4, 1, 0, 0);
}




#endif















