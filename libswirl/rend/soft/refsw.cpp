#include "refrend_base.inl"

union mem128i {
    uint8_t m128i_u8[16];
    int8_t m128i_i8[16];
    int16_t m128i_i16[8];
    int32_t m128i_i32[4];
    uint32_t m128i_u32[4];
};

static __forceinline int iround(float x)
{
    return _mm_cvtt_ss2si(_mm_load_ss(&x));
}

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
            w = texture->width - 1;
            h = texture->height - 1;
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

static cMutex texture_lock;

struct refsw;
struct FpuEntry;

union Color {
    u32 raw;
    u8 rgba[4];
    struct {
        u8 r;
        u8 g;
        u8 b;
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
    DECL_ALIGN(32) u32 render_buffer[MAX_RENDER_PIXELS * 6]; //param pointers + depth1 + depth2 + stencil + acum 1 + acum 2
    unique_ptr<PixelPipeline> pixelPipeline;

    u8* vram;
    refsw(u8* vram, PixelPipeline* pixelPipeline) : vram(vram), pixelPipeline(pixelPipeline) {
        
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

    void PeelBuffers(u32 paramValue, float depthValue, u32 stencilValue)
    {
        auto zb = reinterpret_cast<float*>(render_buffer + DEPTH1_BUFFER_PIXEL_OFFSET);
        auto zb2 = reinterpret_cast<float*>(render_buffer + DEPTH2_BUFFER_PIXEL_OFFSET);
        auto stencil = reinterpret_cast<u32*>(render_buffer + STENCIL_BUFFER_PIXEL_OFFSET);
        auto pb = reinterpret_cast<parameter_tag_t*>(render_buffer + PARAM_BUFFER_PIXEL_OFFSET);

        for (int i = 0; i < MAX_RENDER_PIXELS; i++) {
            zb2[i] = zb[i];     // keep old ZB for reference
            zb[i] = depthValue;    // set the "closest" test to furthest value possible
            stencil[i] = stencilValue;
            pb[i] = paramValue;
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

        for (int y = 0; y < 32; y++) {
            for (int x = 0; x < 32; x++) {
                PixelFlush_tsp(this, tileX + x, tileY + y, (u8*)rb,  *(f32*)&rb[DEPTH1_BUFFER_PIXEL_OFFSET],  *(parameter_tag_t*)&rb[PARAM_BUFFER_PIXEL_OFFSET]);
                rb++;
            }
        }
    }

    parameter_tag_t AddFpuEntry(DrawParameters* params, Vertex* vtx, RenderMode render_mode)
    {
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

        return fpu_entires.size();
    }

    void ClearFpuEntries() {
        fpu_entires.clear();
    }

    // Lookup/create cached TSP parameters, and call PixelFlush_tsp
    static bool PixelFlush_tsp(refsw* backend, float x, float y, u8 *rb, float invW, parameter_tag_t tag)
    {
        if (tag == 0)
            return false;

        auto entry = &backend->fpu_entires[tag-1];
        
        return entry->tsp(entry, x, y, 1/invW, rb);
    }

    // Used by layer peeling to determine end of processing
    int PixelsDrawn;

    // Rasterize a single triangle to ISP (or ISP+TSP for PT)
    void RasterizeTriangle(RenderMode render_mode, DrawParameters* params, parameter_tag_t tag, int vertex_offset, const Vertex& v1, const Vertex& v2, const Vertex& v3, const Vertex* v4, RECT* area)
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
                    u32 mode = vertex_offset ^ params->isp.CullMode & 1;

                    if (
                        (mode == 0 && area < 0) ||
                        (mode == 1 && area > 0)) {
                        return;
                    }
                }
            }
        }

        // Bounding rectangle
        int minx = iround(mmin(X1, X2, X3, area->left));
        int miny = iround(mmin(Y1, Y2, Y3, area->top));

        int spanx = iround(mmax(X1, X2, X3, area->right - 1)) - minx + 1;
        int spany = iround(mmax(Y1, Y2, Y3, area->bottom - 1)) - miny + 1;

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


        float hs12 = C1 + DX12 * miny - DY12 * minx;
        float hs23 = C2 + DX23 * miny - DY23 * minx;
        float hs31 = C3 + DX31 * miny - DY31 * minx;
        float hs41 = C4 + DX41 * miny - DY41 * minx;


        u8* cb_y = (u8*)render_buffer;
        cb_y += (miny - area->top) * stride_bytes + (minx - area->left) * 4;

        PlaneStepper3 Z;
        Z.Setup(v1, v2, v3, v1.z, v2.z, v3.z);

        float y_ps = miny;
        float minx_ps = minx;

        auto pixelFlush = pixelPipeline->GetIsp(render_mode, params->isp);

        // Loop through pixels
        for (int y = spany; y > 0; y -= 1)
        {
            float Xhs12 = hs12;
            float Xhs23 = hs23;
            float Xhs31 = hs31;
            float Xhs41 = hs41;

            u8* cb_x = cb_y;
            float x_ps = minx_ps;
            for (int x = spanx; x > 0; x -= 1)
            {
                Xhs12 -= DY12;
                Xhs23 -= DY23;
                Xhs31 -= DY31;
                Xhs41 -= DY41;

                bool inTriangle = Xhs12 > 0 && Xhs23 > 0 && Xhs31 > 0 && Xhs41 > 0;

                if (inTriangle)
                {
                    float invW = Z.Ip(x_ps, y_ps);
                    pixelFlush(this, x_ps, y_ps, invW, cb_x, tag);
                }

                cb_x += 4;
                x_ps = x_ps + 1;
            }
        next_y:
            hs12 += DX12;
            hs23 += DX23;
            hs31 += DX31;
            hs41 += DX41;
            cb_y += stride_bytes;
            y_ps = y_ps + 1;
        }
    }

    vector<FpuEntry> fpu_entires;
    
    virtual u8* GetColorOutputBuffer() {
        return reinterpret_cast<u8*>(render_buffer + ACCUM1_BUFFER_PIXEL_OFFSET);
    }

    void operator delete(void* p) {
        _mm_free(p);
    }
};

#if FEAT_TA == TA_LLE

#include "refsw_pixel.inl"

Renderer* rend_refsw(u8* vram) {
    return new refrend(vram, [=]() { 
        return (RefRendInterface*) new(_mm_malloc(sizeof(refsw), 32)) ::refsw(vram, new RefPixelPipeline());
    });
}


static auto refrend = RegisterRendererBackend(rendererbackend_t{ "refsw", "RefSW", 0, rend_refsw });
#endif