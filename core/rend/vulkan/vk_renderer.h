/*
*	vk_renderer.h
*/
#pragma once

#include "vk_base.h"
#include "hw/pvr/Renderer_if.h"





union PipeState
{
	struct {

		u32 dstBlend : 3;	// TSP - High 6 bits
		u32 srcBlend : 3;


		u32 disWriteZ : 1;	// ISP_TSP - high 5 bits
		u32 cullMode : 2;
		u32 depthMode : 3;
	};
	u64 raw;
};

inline static PipeState defState()
{
	PipeState ps;
	ps.raw = 0;

	ps.dstBlend = 1;	// _ONE
	ps.srcBlend = 1;	// _ONE

	ps.disWriteZ = 0; // pp.isp.ZWriteDis;
	ps.cullMode  = 0; // pp.isp.CullMode;
	ps.depthMode = VK_COMPARE_OP_LESS_OR_EQUAL;

	return ps;
}

inline static PipeState pp2ps(PolyParam& pp)
{
	static PipeState ps;

	ps.raw = 0;

	///// for now use bitstruct to copy, optimize later and test results vs

	ps.dstBlend  = pp.tsp.DstInstr;
	ps.srcBlend  = pp.tsp.SrcInstr;

	ps.disWriteZ = pp.isp.ZWriteDis;
	ps.cullMode  = pp.isp.CullMode;
	ps.depthMode = pp.isp.DepthMode;



	return ps;
}


typedef QPair<VkPipeline, VkPipelineLayout> RPipe_T;
typedef QHash<quint64, RPipe_T> RPipeMap;

class VkRenderer
	: Renderer
//	, public QVulkanDeviceFunctions
{
	// temp , use Qt //
//	VkDevice m_dev;

	VulkanWindow* m_window;
	QVulkanDeviceFunctions* m_devFuncs;

	RPipeMap m_pipes;
	VkCommandPool m_cmdPool;
	VulkanBuffer vtxBuffer, idxBuffer;
	QVector<VkShaderModule> m_vertShaders, m_fragShaders;


public:

	VkRenderer()
		: m_window(nullptr)
		, m_cmdPool(nullptr)
		, m_devFuncs(nullptr)
		, vtxBuffer(MB(64), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
		, idxBuffer(MB(4),  VK_BUFFER_USAGE_INDEX_BUFFER_BIT)
	{

	}

	// Renderer Interface //

	static Renderer* createInstance() {
		return new VkRenderer;
	}


	bool Init();

	void Resize(int w, int h);

	void Term();

	bool Process(TA_context* ctx);
	bool Render();
	bool RenderLastFrame();

	void Present();

	void DrawOSD(bool clear_screen);

	u32 GetTexture(TSP tsp, TCW tcw);

	void Invalidate(vram_block* bl);






private:


	VkPipeline buildPipeForState(u64 pstate);

	VkShaderModule loadShader(const std::string path);



};
