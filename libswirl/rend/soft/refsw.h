#pragma once
/*
	This file is part of libswirl
*/
#include "license/bsd"

#include "refrend_base.h"
#include "hw/pvr/ta_ctx.h" // for Vertex
#include "rend/gles/gles.h"


#define MAX_RENDER_WIDTH 32
#define MAX_RENDER_HEIGHT 32
#define MAX_RENDER_PIXELS (MAX_RENDER_WIDTH * MAX_RENDER_HEIGHT)

#define STRIDE_PIXEL_OFFSET MAX_RENDER_WIDTH

#define PARAM_BUFFER_PIXEL_OFFSET   0
#define DEPTH1_BUFFER_PIXEL_OFFSET  (MAX_RENDER_PIXELS*1)
#define DEPTH2_BUFFER_PIXEL_OFFSET  (MAX_RENDER_PIXELS*2)
#define STENCIL_BUFFER_PIXEL_OFFSET (MAX_RENDER_PIXELS*3)
#define ACCUM1_BUFFER_PIXEL_OFFSET  (MAX_RENDER_PIXELS*4)
#define ACCUM2_BUFFER_PIXEL_OFFSET  (MAX_RENDER_PIXELS*5)

/*
    Surface equation solver
*/
struct PlaneStepper3
{
    float ddx, ddy;
    float c;

    void Setup(const Vertex& v1, const Vertex& v2, const Vertex& v3, float v1_a, float v2_a, float v3_a)
    {
        float Aa = ((v3_a - v1_a) * (v2.y - v1.y) - (v2_a - v1_a) * (v3.y - v1.y));
        float Ba = ((v3.x - v1.x) * (v2_a - v1_a) - (v2.x - v1.x) * (v3_a - v1_a));

        float C = ((v2.x - v1.x) * (v3.y - v1.y) - (v3.x - v1.x) * (v2.y - v1.y));
        
        ddx = -Aa / C;
        ddy = -Ba / C;

        c = (v1_a - ddx * v1.x - ddy * v1.y);
    }

    __forceinline float Ip(float x, float y) const
    {
        return x * ddx + y * ddy + c;
    }

    __forceinline float Ip(float x, float y, float W) const
    {
        return Ip(x, y) * W;
    }
};

/*
    Interpolation helper
*/
struct IPs3
{
    PlaneStepper3 U;
    PlaneStepper3 V;
    PlaneStepper3 Col[4];
    PlaneStepper3 Ofs[4];

    void Setup(DrawParameters* params, text_info* texture, const Vertex& v1, const Vertex& v2, const Vertex& v3)
    {
        u32 w = 0, h = 0;
        if (texture) {
            w = texture->width;
            h = texture->height;
        }

        U.Setup(v1, v2, v3, v1.u * w * v1.z, v2.u * w * v2.z, v3.u * w * v3.z);
        V.Setup(v1, v2, v3, v1.v * h * v1.z, v2.v * h * v2.z, v3.v * h * v3.z);
        if (params->isp.Gouraud) {
            for (int i = 0; i < 4; i++)
                Col[i].Setup(v1, v2, v3, v1.col[i] * v1.z, v2.col[i] * v2.z, v3.col[i] * v3.z);

            for (int i = 0; i < 4; i++)
                Ofs[i].Setup(v1, v2, v3, v1.spc[i] * v1.z, v2.spc[i] * v2.z, v3.spc[i] * v3.z);
        } else {
            for (int i = 0; i < 4; i++)
                Col[i].Setup(v1, v2, v3, v3.col[i] * v1.z, v3.col[i] * v2.z, v3.col[i] * v3.z);

            for (int i = 0; i < 4; i++)
                Ofs[i].Setup(v1, v2, v3, v3.spc[i] * v1.z, v3.spc[i] * v2.z, v3.spc[i] * v3.z);
        }
    }
};

struct refsw;
struct FpuEntry;

union Color {
    u32 raw;
    u8 bgra[4];
    struct {
        u8 b;
        u8 g;
        u8 r;
        u8 a;
    };
};

struct PixelPipeline {
    // lookup tables
    typedef void(*IspFn)(refsw* backend, float x, float y, float invW, u8 *pb, parameter_tag_t tag);
    typedef bool(*TspFn)(const FpuEntry *entry, float x, float y, float invW, u8 *pb);
    typedef Color(*TextureFetchFn)(const text_info *texture, float u, float v);
    typedef Color(*ColorCombinerFn)(Color base, Color textel, Color offset);
    typedef bool(*BlendingUnitFn)(Color* cb, Color col);

    virtual IspFn GetIsp(RenderMode render_mode, ISP_TSP isp)= 0;
    virtual TspFn GetTsp(ISP_TSP isp, TSP tsp)= 0;
    virtual TextureFetchFn GetTextureFetch(TSP tsp)= 0;
    virtual ColorCombinerFn GetColorCombiner(ISP_TSP isp, TSP tsp)= 0;
    virtual BlendingUnitFn GetBlendingUnit(RenderMode render_mode, TSP tsp) = 0;

    virtual ~PixelPipeline() { }
};

// Used for deferred TSP processing lookups
struct FpuEntry
{
    IPs3 ips;
    DrawParameters params;
    text_info texture;
    PixelPipeline::TspFn tsp;
    PixelPipeline::TextureFetchFn textureFetch;
    PixelPipeline::ColorCombinerFn colorCombiner;
    PixelPipeline::BlendingUnitFn blendingUnit;
};

struct refsw : RefRendInterface
{
    vector<FpuEntry> fpu_entires;
    map<u32, parameter_tag_t> fpu_entires_lookup;

    // Used by layer peeling to determine end of processing
    int PixelsDrawn;
};