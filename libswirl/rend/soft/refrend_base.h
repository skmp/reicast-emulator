#pragma once
/*
	This file is part of libswirl
*/
#include "license/bsd"

#include <functional>
#include "refrend_regs.h"
#include "hw/pvr/ta_structs.h" // for ISP_TSP and co
#include "hw/pvr/ta_ctx.h" // for Vertex

struct RegionArrayEntry {
    RegionArrayEntryControl control;
    ListPointer opaque;
    ListPointer opaque_mod;
    ListPointer trans;
    ListPointer trans_mod;
    ListPointer puncht;
};

struct DrawParameters
{
    ISP_TSP isp;
    TSP tsp;
    TCW tcw;
    TSP tsp2;
    TSP tcw2;
};

enum RenderMode {
    RM_OPAQUE,
    RM_PUNCHTHROUGH,
    RM_TRANSLUCENT,
    RM_MODIFIER,
    RM_COUNT
};


typedef u32 parameter_tag_t;

struct taRECT {
    int left, top, right, bottom;
};

struct RefRendInterface
{
    // backend specific init
    virtual bool Init() = 0;

    // Clear the buffers
    virtual void ClearBuffers(u32 paramValue, float depthValue, u32 stencilValue) = 0;

    // Clear and set DEPTH2 for peeling
    virtual void PeelBuffers(u32 paramValue, float depthValue, u32 stencilValue) = 0;

    // Summarize tile after rendering modvol (inside)
    virtual void SummarizeStencilOr() = 0;
    
    // Summarize tile after rendering modvol (outside)
    virtual void SummarizeStencilAnd() = 0;

    // Clear the pixel drawn counter
    virtual void ClearPixelsDrawn() = 0;

    // Get the pixel drawn counter. Used during layer peeling to determine when to stop processing
    virtual u32 GetPixelsDrawn() = 0;

    // Add an entry to the fpu parameters list
    virtual parameter_tag_t AddFpuEntry(DrawParameters* params, Vertex* vtx, RenderMode render_mode) = 0;

    // Clear the fpu parameters list
    virtual void ClearFpuEntries() = 0;

    // Get the final output of the 32x32 tile. Used to write to the VRAM framebuffer
    virtual u8* GetColorOutputBuffer() = 0;

    // Render to ACCUM from TAG buffer
    // TAG holds references to triangles, ACCUM is the tile framebuffer
    virtual void RenderParamTags(RenderMode rm, int tileX, int tileY) = 0;

    // RasterizeTriangle
    virtual void RasterizeTriangle(RenderMode render_mode, DrawParameters* params, parameter_tag_t tag, int vertex_offset, const Vertex& v1, const Vertex& v2, const Vertex& v3, const Vertex* v4, taRECT* area) = 0;

    virtual ~RefRendInterface() { };
};

Renderer* rend_refred_base(u8* vram, function<RefRendInterface*()> createBackend);

RefRendInterface*  rend_refred_debug(RefRendInterface* backend);
