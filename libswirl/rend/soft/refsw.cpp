/*
	This file is part of libswirl

   Implementes the Reference SoftWare renderer (RefRendInterface) backend for refrend_base.
   This includes buffer operations and rasterization

   Pixel operations are in refsw_pixel.cpp (PixelPipeline interface)
*/
#include "license/bsd"

#include <cmath>
#include <float.h>

#include "refsw.h"
#include "refsw_pixel.h"

#include <mmintrin.h>
#include <xmmintrin.h>
#include <emmintrin.h>
#include <smmintrin.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
static __forceinline int iround(float x)
{
    return _mm_cvtt_ss2si(_mm_load_ss(&x));
}
#pragma clang diagnostic pop

static float mmin(float a, float b, float c, float d)
{
    float rv = min(a, b);
    rv = min(c, rv);
    return max(d, rv);
}

static float mmax(float a, float b, float c, float d)
{
    float rv = max(a, b);
    rv = max(c, rv);
    return min(d, rv);
}

static cMutex texture_lock;

struct refsw_impl : refsw
{
    DECL_ALIGN(32) u32 render_buffer[MAX_RENDER_PIXELS * 6]; //param pointers + depth1 + depth2 + stencil + acum 1 + acum 2
    unique_ptr<PixelPipeline> pixelPipeline;

    u8* vram;
    refsw_impl(u8* vram, PixelPipeline* pixelPipeline) : vram(vram), pixelPipeline(pixelPipeline) {
        
    }

    bool Init() {
        return true;
    }

    void ClearBuffers(u32 paramValue, float depthValue, u32 stencilValue)
    {
        auto zb = reinterpret_cast<float*>(render_buffer + DEPTH1_BUFFER_PIXEL_OFFSET);
        auto stencil = reinterpret_cast<u32*>(render_buffer + STENCIL_BUFFER_PIXEL_OFFSET);
        auto pb = reinterpret_cast<parameter_tag_t*>(render_buffer + PARAM_BUFFER_PIXEL_OFFSET);

        for (int i = 0; i < MAX_RENDER_PIXELS; i++) {
            zb[i] = depthValue;
            stencil[i] = stencilValue;
            pb[i] = paramValue;
        }
    }

    void ClearParamBuffer(u32 paramValue) {
        auto pb = reinterpret_cast<parameter_tag_t*>(render_buffer + PARAM_BUFFER_PIXEL_OFFSET);

        for (int i = 0; i < MAX_RENDER_PIXELS; i++) {
            pb[i] = paramValue;
        }
    }

    void PeelBuffers(float depthValue, u32 stencilValue)
    {
        auto zb = reinterpret_cast<float*>(render_buffer + DEPTH1_BUFFER_PIXEL_OFFSET);
        auto zb2 = reinterpret_cast<float*>(render_buffer + DEPTH2_BUFFER_PIXEL_OFFSET);
        auto stencil = reinterpret_cast<u32*>(render_buffer + STENCIL_BUFFER_PIXEL_OFFSET);

        for (int i = 0; i < MAX_RENDER_PIXELS; i++) {
            zb2[i] = zb[i];     // keep old ZB for reference
            zb[i] = depthValue;    // set the "closest" test to furthest value possible
            stencil[i] = stencilValue;
        }
    }

    void SummarizeStencilOr() {
        auto stencil = reinterpret_cast<u32*>(render_buffer + STENCIL_BUFFER_PIXEL_OFFSET);

        // post movdol merge INSIDE
        for (int i = 0; i < MAX_RENDER_PIXELS; i++) {
            stencil[i] |= (stencil[i] >>1);
        }
    }

    void SummarizeStencilAnd() {
        auto stencil = reinterpret_cast<u32*>(render_buffer + STENCIL_BUFFER_PIXEL_OFFSET);

        // post movdol merge OUTSIDE
        for (int i = 0; i < MAX_RENDER_PIXELS; i++) {
            stencil[i] &= (stencil[i] >>1);
        }
    }

    void ClearPixelsDrawn()
    {
        PixelsDrawn = 0;
    }

    u32 GetPixelsDrawn()
    {
        return PixelsDrawn;
    }

     // Render to ACCUM from TAG buffer
    // TAG holds references to triangles, ACCUM is the tile framebuffer
    void RenderParamTags(RenderMode rm, int tileX, int tileY) {

        auto rb = render_buffer;
        float halfpixel = HALF_OFFSET.tsp_pixel_half_offset ? 0.5f : 0;

        for (int y = 0; y < 32; y++) {
            for (int x = 0; x < 32; x++) {
                PixelFlush_tsp(this, tileX + x + halfpixel, tileY + y + halfpixel, (u8*)rb,  *(f32*)&rb[DEPTH1_BUFFER_PIXEL_OFFSET],  *(parameter_tag_t*)&rb[PARAM_BUFFER_PIXEL_OFFSET]);
                rb++;
            }
        }
    }

    parameter_tag_t AddFpuEntry(DrawParameters* params, Vertex* vtx, RenderMode render_mode, ISP_BACKGND_T_type core_tag)
    {
        auto cache = fpu_entires_lookup.find(core_tag.full);
        if (cache != fpu_entires_lookup.end()) {
            return cache->second;
        }

        FpuEntry entry;
        entry.params = *params;
        // generate
        if (entry.params.isp.Texture)
        {
            texture_lock.Lock();
            entry.texture = raw_GetTexture(vram, entry.params.tsp, entry.params.tcw);
            texture_lock.Unlock();
        }

        entry.ips.Setup(params, &entry.texture, vtx[0], vtx[1], vtx[2]);

        entry.tsp = pixelPipeline->GetTsp(entry.params.isp, entry.params.tsp);
        entry.textureFetch = pixelPipeline->GetTextureFetch(entry.params.tsp);
        entry.colorCombiner = pixelPipeline->GetColorCombiner(entry.params.isp, entry.params.tsp);
        entry.blendingUnit = pixelPipeline->GetBlendingUnit(render_mode, entry.params.tsp);

        fpu_entires.push_back(entry);

        parameter_tag_t tag = (parameter_tag_t)(fpu_entires.size() << 1);

        fpu_entires_lookup[core_tag.full] = tag;

        return tag;
    }

    void ClearFpuEntries() {
        fpu_entires.clear();
        fpu_entires_lookup.clear();
    }

    // Lookup/create cached TSP parameters, and call PixelFlush_tsp
    static bool PixelFlush_tsp(refsw* backend, float x, float y, u8 *rb, float invW, parameter_tag_t tag)
    {
        if (tag & TAG_INVALID)
            return false;

        auto entry = &backend->fpu_entires[(tag>>1) - 1];
        
        return entry->tsp(entry, x, y, 1/invW, rb);
    }

    // Rasterize a single triangle to ISP (or ISP+TSP for PT)
    void RasterizeTriangle(RenderMode render_mode, DrawParameters* params, parameter_tag_t tag, int vertex_offset, const Vertex& v1, const Vertex& v2, const Vertex& v3, const Vertex* v4, taRECT* area)
    {
        const int stride_bytes = STRIDE_PIXEL_OFFSET * 4;
        //Plane equation

#define FLUSH_NAN(a) isnan(a) ? 0 : a

        const float Y1 = FLUSH_NAN(v1.y);
        const float Y2 = FLUSH_NAN(v2.y);
        const float Y3 = FLUSH_NAN(v3.y);
        const float Y4 = v4 ? FLUSH_NAN(v4->y) : 0;

        const float X1 = FLUSH_NAN(v1.x);
        const float X2 = FLUSH_NAN(v2.x);
        const float X3 = FLUSH_NAN(v3.x);
        const float X4 = v4 ? FLUSH_NAN(v4->x) : 0;

        int sgn = 1;

        // cull
        {
            //area: (X1-X3)*(Y2-Y3)-(Y1-Y3)*(X2-X3)
            float area = ((X1 - X3) * (Y2 - Y3) - (Y1 - Y3) * (X2 - X3));

            if (area > 0)
                sgn = -1;

            if (params->isp.CullMode != 0) {
                float abs_area = fabsf(area);

                if (abs_area < FPU_CULL_VAL)
                    return;

                if (params->isp.CullMode >= 2) {
                    u32 mode = vertex_offset ^ (params->isp.CullMode & 1);

                    if (
                        (mode == 0 && area < 0) ||
                        (mode == 1 && area > 0)) {
                        return;
                    }
                }
            }
        }

        // Bounding rectangle
        int minx = mmin(X1, X2, X3, area->left);
        int miny = mmin(Y1, Y2, Y3, area->top);

        int spanx = mmax(X1+1, X2+1, X3+1, area->right - 1) - minx + 1;
        int spany = mmax(Y1+1, Y2+1, Y3+1, area->bottom - 1) - miny + 1;

        //Inside scissor area?
        if (spanx < 0 || spany < 0)
            return;

        // Half-edge constants
        const float DX12 = sgn * (X1 - X2);
        const float DX23 = sgn * (X2 - X3);
        const float DX31 = v4 ? sgn * (X3 - X4) : sgn * (X3 - X1);
        const float DX41 = v4 ? sgn * (X4 - X1) : 0;

        const float DY12 = sgn * (Y1 - Y2);
        const float DY23 = sgn * (Y2 - Y3);
        const float DY31 = v4 ? sgn * (Y3 - Y4) : sgn * (Y3 - Y1);
        const float DY41 = v4 ? sgn * (Y4 - Y1) : 0;

        float C1 = DY12 * X1 - DX12 * Y1;
        float C2 = DY23 * X2 - DX23 * Y2;
        float C3 = DY31 * X3 - DX31 * Y3;
        float C4 = v4 ? DY41 * X4 - DX41 * Y4 : 1;


        u8* cb_y = (u8*)render_buffer;
        cb_y += (miny - area->top) * stride_bytes + (minx - area->left) * 4;

        PlaneStepper3 Z;
        Z.Setup(v1, v2, v3, v1.z, v2.z, v3.z);

        float halfpixel = HALF_OFFSET.fpu_pixel_half_offset ? 0.5f : 0;
        float y_ps = miny + halfpixel;
        float minx_ps = minx + halfpixel;

        auto pixelFlush = pixelPipeline->GetIsp(render_mode, params->isp);

        // Loop through pixels
        for (int y = spany; y > 0; y -= 1)
        {
            u8* cb_x = cb_y;
            float x_ps = minx_ps;
            for (int x = spanx; x > 0; x -= 1)
            {
                float Xhs12 = C1 + DX12 * y_ps - DY12 * x_ps;
                float Xhs23 = C2 + DX23 * y_ps - DY23 * x_ps;
                float Xhs31 = C3 + DX31 * y_ps - DY31 * x_ps;
                float Xhs41 = C4 + DX41 * y_ps - DY41 * x_ps;

                bool inTriangle = Xhs12 >= 0 && Xhs23 >= 0 && Xhs31 >= 0 && Xhs41 >= 0;

                if (inTriangle)
                {
                    float invW = Z.Ip(x_ps, y_ps);
                    pixelFlush(this, x_ps, y_ps, invW, cb_x, tag);
                }

                cb_x += 4;
                x_ps = x_ps + 1;
            }
        next_y:
            cb_y += stride_bytes;
            y_ps = y_ps + 1;
        }
    }
    
    virtual u8* GetColorOutputBuffer() {
        return reinterpret_cast<u8*>(render_buffer + ACCUM1_BUFFER_PIXEL_OFFSET);
    }

    virtual u8* DebugGetAllBuffers() {
        return reinterpret_cast<u8*>(render_buffer);
    }

    void operator delete(void* p) {
        _mm_free(p);
    }
};

#if FEAT_TA == TA_LLE

Renderer* rend_refsw(u8* vram) {
    return rend_refred_base(vram, [=]() { 
        return (RefRendInterface*) new(_mm_malloc(sizeof(refsw_impl), 32)) ::refsw_impl(vram, Create_RefPixelPipeline());
    });
}

static auto refrend = RegisterRendererBackend(rendererbackend_t{ "refsw", "RefSW", 0, rend_refsw });

Renderer* rend_refsw_debug(u8* vram) {
    return rend_refred_base(vram, [=]() { 
        return rend_refred_debug((RefRendInterface*) new(_mm_malloc(sizeof(refsw_impl), 32)) ::refsw_impl(vram, Create_RefPixelPipeline()));
    });
}

static auto refrend_debug = RegisterRendererBackend(rendererbackend_t{ "refsw-dbg", "RefSW Debug", 0, rend_refsw_debug });
#endif
