/*
*	vk_renderer.cpp
*/

#include "vk_renderer.h"


static bool rr_vk = Renderer::Register(R_Vulkan, &VkRenderer::createInstance);


/*
*	Since i'm currently using Qt , will attempt to use stub functions that can be implemented by another loader later to get Vk*
*
*/






bool VkRenderer::Init()
{
	if (!(m_window = reiApp->vkWindow())) {
		qFatal(__FUNCTION__"(): failed to get VulkanWindow!");
		return false;
	}
	else qDebug(__FUNCTION__"()");

	QVulkanInstance* inst = m_window->vulkanInstance();


	VkDevice dev = m_window->device();
	m_devFuncs = inst->deviceFunctions(dev);

	const VkPhysicalDeviceLimits* pdevLimits = &m_window->physicalDeviceProperties()->limits;
	const VkDeviceSize uniAlign = pdevLimits->minUniformBufferOffsetAlignment;

	qDebug("uniform buffer offset alignment is %u", (uint)uniAlign);

	const int concurrentFrameCount = m_window->concurrentFrameCount();


	//////////// T E M P




	VkShaderModule vertShader = loadShader("./shader.vert.spv");
	VkShaderModule fragShader = loadShader("./shader.frag.spv");

	m_vertShaders.push_back(vertShader);
	m_fragShaders.push_back(fragShader);

	buildPipeForState(defState().raw);


	return true;
}



bool VkRenderer::Process(TA_context* ctx)
{
	if (ctx->rend.isRenderFramebuffer)
	{
	//	RenderFramebuffer();
		return true;
	}

	if (!ta_parse_vdrc(ctx))
		return false;


	return true;
}


extern bool Vk_StartFrame;
bool VkRenderer::RenderLastFrame()
{
//	qDebug() << __FUNCTION__"()";
	return true;
}

bool VkRenderer::Render()
{
#if 1
	if (!Vk_StartFrame)
		return true;
	else
		Vk_StartFrame = false;
#endif


//	if (!_pvrrc)		// apparently this is somehow ok considering   _pvrrc = DequeueRender();  is called _after_ RenderLastFrame()
//		return false;	//		die(__FUNCTION__"() _pvrrc is null!");

	qDebug() << __FUNCTION__"() --GOOD";

	VkDevice dev = m_window->device();

	const QSize sz = m_window->swapChainImageSize();

	// Add the necessary barriers and do the host-linear -> device-optimal copy, if not yet done.
//	ensureTexture();

	VkClearColorValue clearColor = { { 0, 0, 0, 1 } };
	VkClearDepthStencilValue clearDS = { 1, 0 };
	VkClearValue clearValues[2];
	memset(clearValues, 0, sizeof(clearValues));
	clearValues[0].color = clearColor;
	clearValues[1].depthStencil = clearDS;

	VkRenderPassBeginInfo rpBeginInfo;
	memset(&rpBeginInfo, 0, sizeof(rpBeginInfo));
	rpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rpBeginInfo.renderPass	= m_window->defaultRenderPass();
	rpBeginInfo.framebuffer	= m_window->currentFramebuffer();
	rpBeginInfo.renderArea.extent.width	= sz.width();
	rpBeginInfo.renderArea.extent.height= sz.height();
	rpBeginInfo.clearValueCount	= 2;
	rpBeginInfo.pClearValues	= clearValues;

	VkCommandBuffer cb = m_window->currentCommandBuffer();
	m_devFuncs->vkCmdBeginRenderPass(cb, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);



	////// **FIXME**
	VkDescriptorSet descSet = nullptr;	//// *FIXME******

//	m_devFuncs->vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipes[defState().raw].second, 0, 1, &descSet, 0, nullptr);


	void* pmVB = vtxBuffer.map();
	void* pmIB = idxBuffer.map();

#if 1
	memcpy(pmVB, pvrrc.verts.head(), pvrrc.verts.bytes());
	memcpy(pmIB, pvrrc.idx.head(),   pvrrc.idx.bytes());
#else
	Vertex* pV = (Vertex*)pmVB;
	u32* pIdx = (u32*)pmIB;

	pV[0].x = 0.0f;		pV[0].y = -0.5f;	pV[0].z = 0.5f;		((u32*)pV[0].col)[0] = 0xFF0000FF;
	pV[1].x = 0.5f;		pV[1].y = 0.5f;		pV[1].z = 0.5f;		((u32*)pV[1].col)[0] = 0x00FF00FF;
	pV[2].x = -0.5f;	pV[2].y = 0.5f;		pV[2].z = 0.5f;		((u32*)pV[2].col)[0] = 0xFF00FF00;
	pV[3].x = 0.5f;		pV[3].y = 0.9f;		pV[3].z = 0.5f;		((u32*)pV[3].col)[0] = 0xc0c000FF;

	for (u32 pi = 0; pi < 4; pi++) {
		pV[pi].x = 333.f * (0.5f + pV[pi].x) + 100.f;
		pV[pi].y = 333.f * (0.5f + pV[pi].y) + 100.f;
		printf("Vertex[%d] %.3f, %.3f \n", pi, pV[pi].x, pV[pi].y);
	}


#endif
	VkDeviceSize vbOffset = 0;
	VkBuffer buffers[] = { vtxBuffer._get() };
	m_devFuncs->vkCmdBindVertexBuffers(cb, 0, 1, buffers /*vtxBuffer.get()*/, &vbOffset);
	m_devFuncs->vkCmdBindIndexBuffer(cb, idxBuffer._get(), 0, VK_INDEX_TYPE_UINT32);



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




	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if 0

	u64 pstate = defState().raw;

	m_devFuncs->vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipes[pstate].first);	//


	m_devFuncs->vkCmdDraw(cb, 3, 1, 0, 0);


#else
	RenderPass previous_pass = { 0 };
	int render_pass_count = pvrrc.render_passes.used();

	for (int render_pass = 0; render_pass < render_pass_count; render_pass++)
	{
		const RenderPass& current_pass = pvrrc.render_passes.head()[render_pass];

		auto first = previous_pass.op_count;
		auto count = current_pass.op_count - previous_pass.op_count;

		PolyParam* params = &pvrrc.global_param_op.head()[first];
		auto Type = ListType_Opaque;
		auto pass = render_pass;

		while (count-- > 0)
		{
			if ((params->count < 3) || 
				((pass != 0) && (Type == ListType_Translucent && params->tsp.SrcInstr == 0 && params->tsp.DstInstr == 1))) {
				params++;
				continue;
			}
			//gl4ShaderUniforms.poly_number = params - gply.head();
			//SetGPState<Type, SortingEnabled>(params, pass);

#if 1
			u64 pstate = pp2ps(*params).raw;
			if (!m_pipes.contains(pstate)) {
				qDebug() << "@@@@@@@ Building Pipeline for state: " << hex << pstate;
				buildPipeForState(pstate);
				if (!m_pipes.contains(pstate))
					die("fml , pipe burst?");
			}
#else
			u64 pstate = defState().raw;
#endif

			m_devFuncs->vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipes[pstate].first);	//


			m_devFuncs->vkCmdDraw(cb, params->count, 1, params->first, 0);
			

			params++;
		}

		//DrawList<ListType_Opaque, false>(pvrrc.global_param_op, previous_pass.op_count, current_pass.op_count - previous_pass.op_count, 0);
		//DrawList<ListType_Punch_Through, false>(pvrrc.global_param_pt, previous_pass.pt_count, current_pass.pt_count - previous_pass.pt_count, 0);

		// Modifier volumes
		//if (settings.rend.ModifierVolumes)
		//	DrawModVols(previous_pass.mvo_count, current_pass.mvo_count - previous_pass.mvo_count);


		previous_pass = current_pass;
	}

	//Opaque
	//DrawList<ListType_Opaque, false>(pvrrc.global_param_op, previous_pass.op_count, current_pass.op_count - previous_pass.op_count, 1);


	//we want at least 1 PParam
#endif
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



	m_devFuncs->vkCmdEndRenderPass(cb);
	m_window->frameReady();
	return true;
}

void VkRenderer::Present()
{
	m_window->requestUpdate();
}



void VkRenderer::Resize(int w, int h) {}

void VkRenderer::Term()
{
	VkDevice dev = m_window->device();

	/// **FIXME** these can be destroyed if/after all pipelines have been created ///
	for (auto vs : m_vertShaders)
		if (vs)	m_devFuncs->vkDestroyShaderModule(dev, vs, nullptr);

	for (auto fs : m_fragShaders)
		if (fs)	m_devFuncs->vkDestroyShaderModule(dev, fs, nullptr);

	for (auto p : m_pipes) {
		m_devFuncs->vkDestroyPipeline(dev, p.first, nullptr);
		m_devFuncs->vkDestroyPipelineLayout(dev, p.second, nullptr);
	}


	m_pipes.clear();
}


void VkRenderer::DrawOSD(bool clear_screen) { }

u32 VkRenderer::GetTexture(TSP tsp, TCW tcw) { return 0; }

void VkRenderer::Invalidate(vram_block* bl)
{
	VkDevice dev = m_window->device();

}





/******************************************************************************************
*	Pipeline state creation routines
*/



#ifdef  PState
#undef  PState
#endif

#define PState	((PipeState*)&pstate)

#define PSFnAttr	__forceinline static void




PSFnAttr setRasterizationState(VkPipelineRasterizationStateCreateInfo& rs, u64 pstate)
{
#ifdef _DEBUG
	memset(&rs, 0, sizeof(rs));
#endif
	// ???
//	rs.depthClampEnable;

	rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rs.polygonMode	= VK_POLYGON_MODE_FILL;
	rs.cullMode		= VkCullModeFlags((PState->cullMode < 2) ? VK_CULL_MODE_NONE : (PState->cullMode-1));	// 0,1:NONE, FRONT, BACK
	rs.frontFace	= VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rs.lineWidth	= 1.0f;
	rs.rasterizerDiscardEnable = VK_FALSE;

	rs.depthBiasEnable			= VK_FALSE;	
	rs.depthBiasConstantFactor	= 0.0f; // Optional
	rs.depthBiasClamp			= 0.0f; // Optional
	rs.depthBiasSlopeFactor		= 0.0f; // Optional
}

PSFnAttr setMultisampleState(VkPipelineMultisampleStateCreateInfo& ms, u64 pstate)
{
#ifdef _DEBUG
	memset(&ms, 0, sizeof(ms));
#endif

	ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	ms.sampleShadingEnable		= VK_FALSE;
	ms.rasterizationSamples		= VK_SAMPLE_COUNT_1_BIT;	// m_window->sampleCountFlagBits();
	ms.minSampleShading			= 1.0f;		// Optional
	ms.pSampleMask				= nullptr;	// Optional
	ms.alphaToCoverageEnable	= VK_FALSE;	// Optional
	ms.alphaToOneEnable			= VK_FALSE;	// Optional
}


PSFnAttr setDepthStencilState(VkPipelineDepthStencilStateCreateInfo& ds, u64 pstate)
{
#ifdef _DEBUG
	memset(&ds, 0, sizeof(ds));
#endif

	ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	ds.depthTestEnable	= VK_TRUE;	// *FIXME*
	ds.depthWriteEnable	= PState->disWriteZ ? VK_FALSE : VK_TRUE;
	ds.depthCompareOp	= VkCompareOp(PState->depthMode);				// Matches Up

	//	ds.depthBoundsTestEnable = VK_FALSE;
	//	ds.stencilTestEnable
}

PSFnAttr setColorBlendAttState(VkPipelineColorBlendAttachmentState& cba, u64 pstate)
{
#ifdef _DEBUG
	memset(&cba, 0, sizeof(cba));
#endif

	const static VkBlendFactor DstBlend[] =	{
		VK_BLEND_FACTOR_ZERO,		VK_BLEND_FACTOR_ONE,
		VK_BLEND_FACTOR_SRC_COLOR,	VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
		VK_BLEND_FACTOR_SRC_ALPHA,	VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		VK_BLEND_FACTOR_DST_ALPHA,	VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
	};

	const static VkBlendFactor SrcBlend[] =	{
		VK_BLEND_FACTOR_ZERO,		VK_BLEND_FACTOR_ONE,
		VK_BLEND_FACTOR_DST_COLOR,	VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
		VK_BLEND_FACTOR_SRC_ALPHA,	VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		VK_BLEND_FACTOR_DST_ALPHA,	VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
	};

	//// *FIXME*  I didn't really check how this is going to work out,  I'd imagine they all happen the same : check ////
	//u32 IgnoreTexA : 1;
	//u32 UseAlpha : 1;


	cba.blendEnable = VK_FALSE;	// *FIXME*  VK_TRUE;
	cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT; // 0xF;

	cba.srcColorBlendFactor = SrcBlend[PState->srcBlend];
	cba.dstColorBlendFactor = DstBlend[PState->dstBlend];
	cba.colorBlendOp		= VK_BLEND_OP_ADD;

	//if (!tcw.useAlpha) att.colorWriteMask &= ~VK_COLOR_COMPONENT_A_BIT;
	//else
	cba.srcAlphaBlendFactor = SrcBlend[PState->srcBlend];
	cba.dstAlphaBlendFactor = DstBlend[PState->dstBlend];
	cba.alphaBlendOp		= VK_BLEND_OP_ADD;

#if 0
	VK_BLEND_OP_ADD = 0,
	VK_BLEND_OP_SUBTRACT = 1,
	VK_BLEND_OP_REVERSE_SUBTRACT = 2,
	VK_BLEND_OP_MIN = 3,
	VK_BLEND_OP_MAX = 4,
#endif
}


VkPipeline VkRenderer::buildPipeForState(u64 pstate)
{
	Vertex;
	/// Vertex Data Binding ///

#define V_F32	VK_FORMAT_R32_SFLOAT
#define V_Vec2	VK_FORMAT_R32G32_SFLOAT
#define V_Vec3	VK_FORMAT_R32G32B32_SFLOAT
#define V_Vec4	VK_FORMAT_R32G32B32A32_SFLOAT

	VkVertexInputBindingDescription vibd;
	vibd.binding		= 0;
	vibd.stride			= sizeof(Vertex);
	vibd.inputRate		= VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription viad[7];
	viad[0].binding		= vibd.binding;
	viad[0].location	= 0;
	viad[0].format		= V_Vec3;
	viad[0].offset		= offsetof(Vertex, x);
	
	viad[1].binding		= vibd.binding;
	viad[1].location	= 1;
	viad[1].format		= VK_FORMAT_R8G8B8A8_UNORM;
	viad[1].offset		= offsetof(Vertex, col[0]);

	viad[2].binding		= vibd.binding;
	viad[2].location	= 2;
	viad[2].format		= VK_FORMAT_R8G8B8A8_UNORM;
	viad[2].offset		= offsetof(Vertex, spc[0]);

	viad[3].binding		= vibd.binding;
	viad[3].location	= 3;
	viad[3].format		= V_Vec2;
	viad[3].offset		= offsetof(Vertex, u);

	// Two volumes format //

	viad[4].binding		= vibd.binding;
	viad[4].location	= 4;
	viad[4].format		= VK_FORMAT_R8G8B8A8_UNORM;
	viad[4].offset		= offsetof(Vertex, col1[0]);

	viad[5].binding		= vibd.binding;
	viad[5].location	= 5;
	viad[5].format		= VK_FORMAT_R8G8B8A8_UNORM;
	viad[5].offset		= offsetof(Vertex, spc1[0]);

	viad[6].binding		= vibd.binding;
	viad[6].location	= 6;
	viad[6].format		= V_Vec2;
	viad[6].offset		= offsetof(Vertex, u1);



	VkPipelineVertexInputStateCreateInfo vis = {};
	vis.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vis.pNext = nullptr;
	vis.flags = 0;
	vis.vertexBindingDescriptionCount = 1;
	vis.vertexAttributeDescriptionCount = 7;
	vis.pVertexBindingDescriptions = &vibd;
	vis.pVertexAttributeDescriptions = viad;



	VkPipelineInputAssemblyStateCreateInfo ias = {};
	memset(&ias, 0, sizeof(ias));
	ias.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	ias.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;	// VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY
	ias.primitiveRestartEnable = VK_FALSE;			// restartEnable=true breaks up strips with ~0 / (uSZ)-1
	

	VkPipelineRasterizationStateCreateInfo rs;
	setRasterizationState(rs, pstate);

	VkPipelineMultisampleStateCreateInfo ms;
	setMultisampleState(ms, pstate);

	VkPipelineDepthStencilStateCreateInfo dss;
	setDepthStencilState(dss, pstate);


	VkPipelineColorBlendAttachmentState cbas;
	setColorBlendAttState(cbas, pstate);

	VkPipelineColorBlendStateCreateInfo cbs;
	memset(&cbs, 0, sizeof(cbs));
	cbs.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	cbs.attachmentCount	= 1;
	cbs.pAttachments	= &cbas;
	cbs.logicOpEnable	= VK_FALSE;

	// The viewport and scissor will be set dynamically via vkCmdSetViewport/Scissor.
	// This way the pipeline does not need to be touched when resizing the window.
	VkPipelineViewportStateCreateInfo vs;
	memset(&vs, 0, sizeof(vs));
	vs.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	vs.viewportCount	= 1;
	vs.scissorCount		= 1;


	VkDynamicState dynEnable[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };	// _DEPTH_BIAS, _BLEND_CONSTANTS, _DEPTH_BOUNDS, _STENCIL_COMPARE_MASK, _STENCIL_WRITE_MASK, _EXCLUSIVE_SCISSOR_NV
	VkPipelineDynamicStateCreateInfo dyn;
	memset(&dyn, 0, sizeof(dyn));
	dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dyn.dynamicStateCount = sizeof(dynEnable) / sizeof(VkDynamicState);
	dyn.pDynamicStates = dynEnable;
	



	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount			= 0;		// Optional
	pipelineLayoutInfo.pSetLayouts				= nullptr;	// Optional
	pipelineLayoutInfo.pushConstantRangeCount	= 0;		// Optional
	pipelineLayoutInfo.pPushConstantRanges		= nullptr;	// Optional

	VkPipelineLayout pipeLayout = nullptr;
	if (VK_SUCCESS != m_devFuncs->vkCreatePipelineLayout(m_window->device(), &pipelineLayoutInfo, nullptr, &pipeLayout)) {
		throw std::runtime_error("failed to create pipeline layout!");
	}

	VkGraphicsPipelineCreateInfo pipelineInfo;
	memset(&pipelineInfo, 0, sizeof(pipelineInfo));
	pipelineInfo.sType		= VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

	pipelineInfo.layout		= pipeLayout;
	pipelineInfo.renderPass	= m_window->defaultRenderPass();
	pipelineInfo.subpass	= 0;

	// *FIXME* it's faster to create/switch a derived pipeline, TODO //
//	pipelineInfo.flags |= VK_PIPELINE_CREATE_DERIVATIVE_BIT;
	pipelineInfo.basePipelineIndex	= -1;				// Optional
	pipelineInfo.basePipelineHandle	= VK_NULL_HANDLE;	// Optional


	pipelineInfo.pVertexInputState		= &vis;
	pipelineInfo.pInputAssemblyState	= &ias;
	pipelineInfo.pRasterizationState	= &rs;
	pipelineInfo.pMultisampleState		= &ms;
	pipelineInfo.pDepthStencilState		= &dss;
	pipelineInfo.pColorBlendState		= &cbs;
	pipelineInfo.pDynamicState			= &dyn;
	pipelineInfo.pViewportState			= &vs;
	

	/////// *FIXME* ///////

	auto vertShader = m_vertShaders.back();
	auto fragShader = m_fragShaders.back();

	VkPipelineShaderStageCreateInfo shaderStages[2] = {
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,		0,
			VK_SHADER_STAGE_VERTEX_BIT,
			vertShader,	"main",	nullptr
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,		0,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			fragShader,	"main",	nullptr
		}
	};
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;

	VkPipeline pipeline = nullptr;

	if (VK_SUCCESS != m_devFuncs->vkCreateGraphicsPipelines(m_window->device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline)) {
		throw std::runtime_error("failed to create graphics pipeline!");
	}

	m_pipes[pstate] = RPipe_T(pipeline, pipeLayout);

	return pipeline;
}




VkShaderModule VkRenderer::loadShader(const std::string path)
{

#if 1
	std::vector<u8> data;

	FILE* f = fopen(path.c_str(), "rb");
	if (!f) return nullptr;

	fseek(f, 0, SEEK_END);
	size_t fsize = ftell(f);
	fseek(f, 0, SEEK_SET);

	data.resize(fsize);
	fread(&data[0], 1, fsize, f);
	fclose(f);

	const uint32_t* pData = reinterpret_cast<const uint32_t*>(&data[0]);
#else

	QFile file(name);
	if (!file.open(QIODevice::ReadOnly)) {
		qWarning("Failed to read shader %s", qPrintable(name));
		return VK_NULL_HANDLE;
	}
	QByteArray blob = file.readAll();
	file.close();

	size_t fsize = blob.size();
	const uint32_t* pData = reinterpret_cast<const uint32_t*>(blob.constData());
#endif



	VkShaderModuleCreateInfo shaderInfo;
	memset(&shaderInfo, 0, sizeof(shaderInfo));
	shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderInfo.codeSize = fsize;
	shaderInfo.pCode = pData;
	VkShaderModule shaderModule;
	VkResult err = m_devFuncs->vkCreateShaderModule(m_window->device(), &shaderInfo, nullptr, &shaderModule);
	if (err != VK_SUCCESS) {
		qWarning("Failed to create shader module: %d", err);
		return VK_NULL_HANDLE;
	}

	return shaderModule;
}



#if 0


PipelineShader* GetProgram(u32 cp_AlphaTest, u32 pp_ClipTestMode,
	u32 pp_Texture, u32 pp_UseAlpha, u32 pp_IgnoreTexA, u32 pp_ShadInstr, u32 pp_Offset,
	u32 pp_FogCtrl, bool pp_Gouraud, bool pp_BumpMap, bool fog_clamping, bool trilinear)
{
	if (settings.rend.Rotate90 != gl.rotate90)
	{
		gl_delete_shaders();
		gl.rotate90 = settings.rend.Rotate90;
	}
	u32 rv = 0;

	rv |= pp_ClipTestMode;
	rv <<= 1; rv |= cp_AlphaTest;
	rv <<= 1; rv |= pp_Texture;
	rv <<= 1; rv |= pp_UseAlpha;
	rv <<= 1; rv |= pp_IgnoreTexA;
	rv <<= 2; rv |= pp_ShadInstr;
	rv <<= 1; rv |= pp_Offset;
	rv <<= 2; rv |= pp_FogCtrl;
	rv <<= 1; rv |= pp_Gouraud;
	rv <<= 1; rv |= pp_BumpMap;
	rv <<= 1; rv |= fog_clamping;
	rv <<= 1; rv |= trilinear;

	PipelineShader* shader = &gl.shaders[rv];
	if (shader->program == 0)
	{
		shader->cp_AlphaTest = cp_AlphaTest;
		shader->pp_ClipTestMode = pp_ClipTestMode - 1;
		shader->pp_Texture = pp_Texture;
		shader->pp_UseAlpha = pp_UseAlpha;
		shader->pp_IgnoreTexA = pp_IgnoreTexA;
		shader->pp_ShadInstr = pp_ShadInstr;
		shader->pp_Offset = pp_Offset;
		shader->pp_FogCtrl = pp_FogCtrl;
		shader->pp_Gouraud = pp_Gouraud;
		shader->pp_BumpMap = pp_BumpMap;
		shader->fog_clamping = fog_clamping;
		shader->trilinear = trilinear;
		CompilePipelineShader(shader);
	}

	return shader;
}




template <u32 Type, bool SortingEnabled>
__forceinline
void SetGPState(const PolyParam* gp, u32 cflip = 0)
{
	if (gp->pcw.Texture && gp->tsp.FilterMode > 1)
	{
		ShaderUniforms.trilinear_alpha = 0.25 * (gp->tsp.MipMapD & 0x3);
		if (gp->tsp.FilterMode == 2)
			// Trilinear pass A
			ShaderUniforms.trilinear_alpha = 1.0 - ShaderUniforms.trilinear_alpha;
	}
	else
		ShaderUniforms.trilinear_alpha = 1.f;

	bool color_clamp = gp->tsp.ColorClamp && (pvrrc.fog_clamp_min != 0 || pvrrc.fog_clamp_max != 0xffffffff);
	int fog_ctrl = settings.rend.Fog ? gp->tsp.FogCtrl : 2;

	CurrentShader = GetProgram(Type == ListType_Punch_Through ? 1 : 0,
		SetTileClip(gp->tileclip, -1) + 1,
		gp->pcw.Texture,
		gp->tsp.UseAlpha,
		gp->tsp.IgnoreTexA,
		gp->tsp.ShadInstr,
		gp->pcw.Offset,
		fog_ctrl,
		gp->pcw.Gouraud,
		gp->tcw.PixelFmt == PixelBumpMap,
		color_clamp,
		ShaderUniforms.trilinear_alpha != 1.f);



	template <u32 Type, bool SortingEnabled>
	void DrawList(const List<PolyParam> & gply, int first, int count)
	{
		PolyParam* params = &gply.head()[first];


		if (count == 0)
			return;
		//we want at least 1 PParam


		//set some 'global' modes for all primitives

		glcache.Enable(GL_STENCIL_TEST);
		glcache.StencilFunc(GL_ALWAYS, 0, 0);
		glcache.StencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

		while (count-- > 0)
		{
			if (params->count > 2) //this actually happens for some games. No idea why ..
			{
				SetGPState<Type, SortingEnabled>(params);
				glDrawElements(GL_TRIANGLE_STRIP, params->count, gl.index_type,
					(GLvoid*)(gl.get_index_size() * params->first)); glCheck();
			}

			params++;
		}
	}
#endif