#include "refrend_base.inl"

#include <CL/cl.h>

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

//return true if any is positive
static __forceinline bool EvalHalfSpaceAll(float cp12, float cp23, float cp31)
{
    // test all line equations
    bool svt = cp12 >= 0;
    svt &= cp23 >= 0;
    svt &= cp31 >= 0;

    return svt;
}

/*
    Surface equation solver
*/
struct PlaneStepper3
{
    cl_float ddx, ddy;
    cl_float c;

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

    __forceinline float InStep(float bas) const
    {
        return bas + ddx;
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

    void Setup(text_info* texture, const Vertex& v1, const Vertex& v2, const Vertex& v3)
    {
        u32 w = 0, h = 0;
        if (texture) {
            w = texture->width - 1;
            h = texture->height - 1;
        }

        U.Setup(v1, v2, v3, v1.u * w * v1.z, v2.u * w * v2.z, v3.u * w * v3.z);
        V.Setup(v1, v2, v3, v1.v * h * v1.z, v2.v * h * v2.z, v3.v * h * v3.z);
        
        for (int i = 0; i < 4; i++)
            Col[i].Setup(v1, v2, v3, v1.col[i], v2.col[i], v3.col[i]);

        for (int i = 0; i < 4; i++)
            Ofs[i].Setup(v1, v2, v3, v1.spc[i], v2.spc[i], v3.spc[i]);
    }
};

struct TriangleSetup {
    cl_float4 ddx;
    cl_float4 ddy;
    cl_float4 c;
    PlaneStepper3 Z;
    parameter_tag_t tag;
};


#define TPL_DECL_pixel template<RenderMode render_mode, bool pp_UseAlpha, bool pp_Texture, bool pp_IgnoreTexA, int pp_ShadInstr, bool pp_Offset >

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

#define OPENCL_DEFINE(definition) "#define " #definition "\n"
#define OPENCL_SHADER(name, delcs, ...) static const char* name = delcs #__VA_ARGS__

#define COMMON_OPENCL_DEFINES \
    OPENCL_DEFINE(MAX_RENDER_WIDTH 32) \
    OPENCL_DEFINE(MAX_RENDER_HEIGHT 32) \
    OPENCL_DEFINE(MAX_RENDER_PIXELS (MAX_RENDER_WIDTH * MAX_RENDER_HEIGHT)) \
    \
    OPENCL_DEFINE(STRIDE_PIXEL_OFFSET MAX_RENDER_WIDTH) \
    \
    OPENCL_DEFINE(PARAM_BUFFER_PIXEL_OFFSET 0) \
    OPENCL_DEFINE(DEPTH1_BUFFER_PIXEL_OFFSET (MAX_RENDER_PIXELS*1)) \
    OPENCL_DEFINE(DEPTH2_BUFFER_PIXEL_OFFSET (MAX_RENDER_PIXELS*2)) \
    OPENCL_DEFINE(STENCIL_BUFFER_PIXEL_OFFSET (MAX_RENDER_PIXELS*3)) \
    OPENCL_DEFINE(ACCUM1_BUFFER_PIXEL_OFFSET (MAX_RENDER_PIXELS*4)) \
    OPENCL_DEFINE(ACCUM2_BUFFER_PIXEL_OFFSET (MAX_RENDER_PIXELS*5))

OPENCL_SHADER(openclPixelPipeline,
OPENCL_DEFINE(RENDER_MODE %d)
COMMON_OPENCL_DEFINES
,
    typedef char  s8;
    typedef uchar u8;
    typedef int   s32;
    typedef uint  u32;
    typedef float f32;

    typedef u32 parameter_tag_t;

    typedef enum {
        RM_OPAQUE,
        RM_PUNCHTHROUGH,
        RM_TRANSLUCENT,
        RM_MODIFIER,
    } RenderMode_cl;

    typedef struct
    {
        float ddx, ddy;
        float c;
    } PlaneStepper3;

    typedef struct
    {
        PlaneStepper3 U;
        PlaneStepper3 V;
        PlaneStepper3 Col[4];
        PlaneStepper3 Ofs[4];
    } IPs3;

    typedef struct {
        float4 ddx;
        float4 ddy;
        float4 c;
        PlaneStepper3 Z;
        parameter_tag_t tag;
    } TriangleSetup;

    typedef struct {
        __global u32* pdata;
        u32 width;
        u32 height;
        u32 textype; // 0 565, 1 1555, 2 4444
    } text_info;

    typedef struct
    {
        u32 isp;
        u32 tsp;
        u32 tcw;
        u32 tsp2;
        u32 tcw2;
    } DrawParameters;

    typedef struct
    {
        IPs3 ips;
        DrawParameters params;
        text_info texture;
    } FpuEntry;

    float Interpolate(PlaneStepper3* cl, float x, float y) {
        return cl->ddx * x + cl->ddy*y + cl->c;
    }

    float InterpolateGlobal(__global PlaneStepper3* cl, float x, float y) {
        return cl->ddx * x + cl->ddy*y + cl->c;
    }

    bool ShadePixel(float x, float y, __global u8* pb, parameter_tag_t tag, __global const FpuEntry* fpuEntries) {

        __global uint* cb = (__global uint*)&pb[ACCUM1_BUFFER_PIXEL_OFFSET * 4];
        
        if (tag) {
            *cb = tag*0x07050301;
        }
        return true;
    }

    void RasterizePixel(float x, float y, __global u8* pb, PlaneStepper3* Z, parameter_tag_t tag, __global s32* PixelsDrawn, __global const FpuEntry* fpuEntries)
    {
        float invW = Interpolate(Z, x, y);
        __global float* zb = (__global float*)&pb [DEPTH1_BUFFER_PIXEL_OFFSET * 4];
        __global float* zb2 = (__global float*)&pb[DEPTH2_BUFFER_PIXEL_OFFSET * 4];
        __global uint* stencil = (__global uint*)&pb[STENCIL_BUFFER_PIXEL_OFFSET * 4];

        switch (RENDER_MODE)
        {
            // OPAQ
            case RM_OPAQUE:
            {
                // Z pre-pass only
                if (invW < *zb)
                    return;

                *zb = invW;
                *(__global parameter_tag_t *)pb = tag;
            }
            break;

            case RM_MODIFIER:
            {
                // Flip on Z pass
                if (invW < *zb)
                    return;

                *stencil ^= 0b10;
            }
            break;

            // PT
            case RM_PUNCHTHROUGH:
            {
                // Z + TSP syncronized for alpha test
                if (invW < *zb)
                    return;

                if (ShadePixel(x, y, pb, tag, fpuEntries))
                {
                    *zb = invW;
                    *(__global parameter_tag_t *)pb = tag;
                }
            }
            break;

            // Layer Peeling. zb2 holds the reference depth, zb is used to find closest to reference
            case RM_TRANSLUCENT:
            {
                if (invW <= *zb2)
                    return;

                if (invW > *zb)
                    return;

                atomic_inc(PixelsDrawn);

                *zb = invW;
                *(__global parameter_tag_t *)pb = tag;
            }
            break;
        }
    }

    __kernel void ClearBuffers(__global float* render_buffer, u32 paramValue, float depthValue, u32 stencilValue)
    {
        __global f32* zb = (__global f32*)(render_buffer + DEPTH1_BUFFER_PIXEL_OFFSET);
        __global u32* stencil = (__global u32*)(render_buffer + STENCIL_BUFFER_PIXEL_OFFSET);
        __global parameter_tag_t* pb= (__global parameter_tag_t*)(render_buffer + PARAM_BUFFER_PIXEL_OFFSET);

        int i = get_global_id(0);

        zb[i] = depthValue;
        stencil[i] = stencilValue;
        pb[i] = paramValue;
    }

    __kernel void PeelBuffers(__global float* render_buffer, u32 paramValue, float depthValue, u32 stencilValue)
    {
        __global f32* zb = (__global f32*)(render_buffer + DEPTH1_BUFFER_PIXEL_OFFSET);
        __global f32* zb2 = (__global f32*)(render_buffer + DEPTH2_BUFFER_PIXEL_OFFSET);
        
        __global u32* stencil = (__global u32*)(render_buffer + STENCIL_BUFFER_PIXEL_OFFSET);
        __global parameter_tag_t* pb= (__global parameter_tag_t*)(render_buffer + PARAM_BUFFER_PIXEL_OFFSET);

        int i = get_global_id(0);
        
        zb2[i] = zb[i];     // keep old ZB for reference
        zb[i] = depthValue;    // set the "closest" test to furthest value possible
        stencil[i] = stencilValue;
        pb[i] = paramValue;
    }

    __kernel void SummarizeStencilOr(__global float* render_buffer) {
        __global u32* stencil = (__global u32*)(render_buffer + STENCIL_BUFFER_PIXEL_OFFSET);

        int i = get_global_id(0);
        
        // post movdol merge INSIDE
        stencil[i] |= (stencil[i] >>1);
    }

    __kernel void SummarizeStencilAnd(__global float* render_buffer) {
        __global u32* stencil = (__global u32*)(render_buffer + STENCIL_BUFFER_PIXEL_OFFSET);

        int i = get_global_id(0);
        
        // post movdol merge OUTSIDE
        stencil[i] &= (stencil[i] >>1);
    }

    __kernel void RasterizeTriangle(__global float* render_buffer, TriangleSetup setup, float tileX, float tileY, __global s32* pixelsDrawn, __global const FpuEntry* fpuEntries) {
        int x = get_global_id(0);
        int y = get_global_id(1);
        int offset = y * MAX_RENDER_WIDTH + x;
        
        float screenX = tileX + x;
        float screenY = tileY + y;

        float4 planes = setup.ddx * tileX + setup.ddy * tileY + setup.c;
        
        if (planes.x >= 0 && planes.y >= 0 && planes.z >= 0 && planes.w >= 0) {
            //pixel is inside
            RasterizePixel(screenX, screenY, (__global u8*)render_buffer, &setup.Z, setup.tag, pixelsDrawn, fpuEntries);
        }
    }

    __kernel void ShadePixels(__global float* render_buffer, __global const FpuEntry* fpuEntries, float tileX, float tileY)
    {
            __global u32* pb = (__global u32*)&render_buffer[PARAM_BUFFER_PIXEL_OFFSET];
            __global u32* cb = (__global u32*)&render_buffer[ACCUM1_BUFFER_PIXEL_OFFSET];

            int x = get_global_id(0);
            int y = get_global_id(1);

            int offset = y * MAX_RENDER_WIDTH + x;

            parameter_tag_t tag = pb[offset];
            if (tag != 0){
                ShadePixel(tileX + x, tileY + y, (__global u8*)&pb[offset], tag, fpuEntries);
            }
            //lookup tag         
            // call PixelFlush_tsp_impl
    }


#if 0
    static bool PixelFlush_tsp_impl(const text_info *texture, float x, float y, u8 *pb, IPs3 &ip)
    {
        auto zb = (float *)&pb[DEPTH1_BUFFER_PIXEL_OFFSET * 4];
        auto stencil = (u32 *)&pb[STENCIL_BUFFER_PIXEL_OFFSET * 4];
        auto cb = &pb[ACCUM1_BUFFER_PIXEL_OFFSET * 4];

        float invW = *zb;

        float u = ip.U.Ip(x, y);
        float v = ip.V.Ip(x, y);

        u = u / invW;
        v = v / invW;

        u8 rv[4];
        {
            {
                u32 mult = 256;
                if (*stencil & 1) {
                    mult = FPU_SHAD_SCALE.scale_factor;
                }

                rv[0] = ip.Col[2].Ip(x, y) * mult / 256;
                rv[1] = ip.Col[1].Ip(x, y) * mult / 256;
                rv[2] = ip.Col[0].Ip(x, y) * mult / 256;
                rv[3] = ip.Col[3].Ip(x, y) * mult / 256;    
            }
            

            if (!pp_UseAlpha)
            {
                rv[3] = 255;
            }

            if (pp_Texture)
            {

                int ui = u * 256;
                int vi = v * 256;
                mem128i px = ((mem128i *)texture->pdata)[((ui >> 8) % texture->width + (vi >> 8) % texture->height * texture->width)];

                int ublend = ui & 255;
                int vblend = vi & 255;
                int nublend = 255 - ublend;
                int nvblend = 255 - vblend;

                u8 textel[4];

                for (int i = 0; i < 4; i++)
                {
                    textel[i] =
                        (px.m128i_u8[0 + i] * ublend * vblend) / 65536 +
                        (px.m128i_u8[4 + i] * nublend * vblend) / 65536 +
                        (px.m128i_u8[8 + i] * ublend * nvblend) / 65536 +
                        (px.m128i_u8[12 + i] * nublend * nvblend) / 65536;
                };

                if (pp_IgnoreTexA)
                {
                    textel[3] = 255;
                }

                if (pp_ShadInstr == 0)
                {
                    //color.rgb = texcol.rgb;
                    //color.a = texcol.a;

                    memcpy(rv, textel, sizeof(rv));
                }
                else if (pp_ShadInstr == 1)
                {
                    //color.rgb *= texcol.rgb;
                    //color.a = texcol.a;
                    for (int i = 0; i < 3; i++)
                    {
                        rv[i] = textel[i] * rv[i] / 256;
                    }

                    rv[3] = textel[3];
                }
                else if (pp_ShadInstr == 2)
                {
                    //color.rgb=mix(color.rgb,texcol.rgb,texcol.a);
                    u8 tb = textel[3];
                    u8 cb = 255 - tb;

                    for (int i = 0; i < 3; i++)
                    {
                        rv[i] = (textel[i] * tb + rv[i] * cb) / 256;
                    }

                    //rv[3] is not affected
                }
                else if (pp_ShadInstr == 3)
                {
                    //color*=texcol
                    for (int i = 0; i < 4; i++)
                    {
                        rv[i] = textel[i] * rv[i] / 256;
                    }
                }

                if (pp_Offset)
                {
                    //add offset

                    u32 mult = 256;
                    if (*stencil & 1) {
                        mult = FPU_SHAD_SCALE.scale_factor;
                    }

                    rv[0] += ip.Ofs[2].Ip(x, y) * mult / 256;
                    rv[1] += ip.Ofs[1].Ip(x, y) * mult / 256;
                    rv[2] += ip.Ofs[0].Ip(x, y) * mult / 256;
                    rv[3] += ip.Ofs[3].Ip(x, y);
                }
            }
        }



        if (render_mode == RM_PUNCHTHROUGH)
        {
            if (rv[3] < PT_ALPHA_REF)
            {
                memcpy(cb, rv, 4);
                return true;
            }
            else
            {
                return false;
            }
        }
        else if (render_mode == RM_TRANSLUCENT)
        {
            u8 *fb = (u8 *)cb;
            u8 src_blend = rv[3];
            u8 dst_blend = 255 - rv[3];
            for (int j = 0; j < 3; j++)
            {
                rv[j] = (rv[j] * src_blend) / 256 + (fb[j] * dst_blend) / 256;
            }

            memcpy(cb, rv, 4);
            return true;
        }
        else
        {
            memcpy(cb, rv, 4);
            return true;
        }
    }
#endif
);

struct refcl : refrend
{
    DECL_ALIGN(32) u32 render_buffer[MAX_RENDER_PIXELS * 6]; //param pointers + depth1 + depth2 + stencil + acum 1 + acum 2

    struct {   
        cl_platform_id platform_id = NULL;
        cl_device_id device_id = NULL;
        cl_context context = NULL;
        cl_command_queue command_queue = NULL;
        cl_mem render_buffer = NULL;
        int    fpu_entries_count = 0;
        int    fpu_entries_size = 0;
        cl_mem fpu_entries = NULL;
        cl_program program[RM_COUNT] = {NULL};
        cl_kernel shadePixelsKernel[RM_COUNT] = {NULL};
    } cl;
    
    refcl(u8* vram) : refrend(vram) {

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
    void RenderParamTags(RenderMode rm, int tileXi, int tileYi) {
        
        SyncFpuEntries();

        clEnqueueWriteBuffer(cl.command_queue, cl.render_buffer, CL_TRUE, 0, sizeof(render_buffer), render_buffer, 0, NULL, NULL);

        cl_float tileX = tileXi;
        cl_float tileY = tileYi;

        //__kernel void ShadePixels(__global float* render_buffer, __global const FpuEntry* fpuEntries, float tileX, float tileY)
        clSetKernelArg(cl.shadePixelsKernel[rm], 0, sizeof(cl_mem), (void *)&cl.render_buffer);
        clSetKernelArg(cl.shadePixelsKernel[rm], 1, sizeof(cl_mem), (void *)&cl.fpu_entries);
        clSetKernelArg(cl.shadePixelsKernel[rm], 2, sizeof(tileX), (void *)&tileX);
        clSetKernelArg(cl.shadePixelsKernel[rm], 3, sizeof(tileY), (void *)&tileY);
 
        size_t worksize[] = { MAX_RENDER_WIDTH, MAX_RENDER_HEIGHT };
        clEnqueueNDRangeKernel( cl.command_queue, cl.shadePixelsKernel[rm], 2, NULL, worksize, 0, 0, 0, 0 );

        /*
        auto pb = reinterpret_cast<parameter_tag_t*>(render_buffer + PARAM_BUFFER_PIXEL_OFFSET);

        for (int y = 0; y < 32; y++) {
            for (int x = 0; x < 32; x++) {
                PixelFlush_tsp(tileX + x, tileY + y, (u8*)&render_buffer[x + y * MAX_RENDER_WIDTH], *pb++);
            }
        }
        */

       clEnqueueReadBuffer(cl.command_queue, cl.render_buffer, CL_TRUE, 0, sizeof(render_buffer), render_buffer, 0, NULL, NULL);
    }

    parameter_tag_t AddFpuEntry(DrawParameters* params, Vertex* vtx, RenderMode render_mode)
    {
        FpuEntry entry;
        entry.params = *params;
        // generate
        if (entry.params.isp.Texture)
        {
            entry.texture = raw_GetTexture(vram, entry.params.tsp, entry.params.tcw);
        }

        entry.ips.Setup(&entry.texture, vtx[0], vtx[1], vtx[2]);

        fpu_entires.push_back(entry);

        return fpu_entires.size();
    }

    void ClearFpuEntries() {
        fpu_entires.clear();
    }

    // Depth processing for a pixel -- render_mode 0: OPAQ, 1: PT, 2: TRANS
    template <RenderMode render_mode>
    void PixelFlush_isp(float x, float y, u8 *pb, PlaneStepper3 &Z, parameter_tag_t tag)
    {
        float invW = Z.Ip(x, y);
        auto zb = (float*)&pb [DEPTH1_BUFFER_PIXEL_OFFSET * 4];
        auto zb2 = (float*)&pb[DEPTH2_BUFFER_PIXEL_OFFSET * 4];
        auto stencil = (u32*)&pb[STENCIL_BUFFER_PIXEL_OFFSET * 4];

#if 0 // make sure we're writting to the right place
    verify((uintptr_t) pb >= (uintptr_t)&render_buffer[0]);

    verify((uintptr_t)pb < ((uintptr_t)&render_buffer[0] + sizeof(render_buffer)));


    verify((uintptr_t)zb >= (uintptr_t)&render_buffer[0]);

    verify((uintptr_t)zb < ((uintptr_t)&render_buffer[0] + sizeof(render_buffer)));
#endif

        switch (render_mode)
        {
            // OPAQ
            case RM_OPAQUE:
            {
                // Z pre-pass only
                if (invW < *zb)
                    return;

                *zb = invW;
                *(parameter_tag_t *)pb = tag;
            }
            break;

            case RM_MODIFIER:
            {
                // Flip on Z pass
                if (invW < *zb)
                    return;

                *stencil ^= 0b10;
            }
            break;

            // PT
            case RM_PUNCHTHROUGH:
            {
                // Z + TSP syncronized for alpha test
                if (invW < *zb)
                    return;

                if (true)
                {
                    *zb = invW;
                    *(parameter_tag_t *)pb = tag;
                }
            }
            break;

            // Layer Peeling. zb2 holds the reference depth, zb is used to find closest to reference
            case RM_TRANSLUCENT:
            {
                if (invW <= *zb2)
                    return;

                if (invW > *zb)
                    return;

                PixelsDrawn++;

                *zb = invW;
                *(parameter_tag_t *)pb = tag;
            }
            break;
        }
    }

    #define RasterizeTriangleImpl(mode) \
    void RasterizeTriangle_##mode(DrawParameters* params, parameter_tag_t tag, int vertex_offset, const Vertex& v1, const Vertex& v2, const Vertex& v3, RECT* area) { \
        RasterizeTriangle<RM_##mode>(params, tag, vertex_offset, v1, v2, v3, area); \
    }

    RasterizeTriangleImpl(OPAQUE)
    RasterizeTriangleImpl(PUNCHTHROUGH)
    RasterizeTriangleImpl(TRANSLUCENT)
    RasterizeTriangleImpl(MODIFIER)

    #undef RasterizeTriangleImpl
    
    // Used by layer peeling to determine end of processing
    int PixelsDrawn;

    // Rasterize a single triangle to ISP (or ISP+TSP for PT)
    template<RenderMode render_mode>
    void RasterizeTriangle(DrawParameters* params, parameter_tag_t tag, int vertex_offset, const Vertex& v1, const Vertex& v2, const Vertex& v3, RECT* area)
    {
        const int stride_bytes = STRIDE_PIXEL_OFFSET * 4;
        //Plane equation

#define FLUSH_NAN(a) isnan(a) ? 0 : a

        const float Y1 = FLUSH_NAN(v1.y);
        const float Y2 = FLUSH_NAN(v2.y);
        const float Y3 = FLUSH_NAN(v3.y);

        const float X1 = FLUSH_NAN(v1.x);
        const float X2 = FLUSH_NAN(v2.x);
        const float X3 = FLUSH_NAN(v3.x);

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
        const float DX31 = sgn * (X3 - X1);

        const float DY12 = sgn * (Y1 - Y2);
        const float DY23 = sgn * (Y2 - Y3);
        const float DY31 = sgn * (Y3 - Y1);

        float C1 = DY12 * X1 - DX12 * Y1;
        float C2 = DY23 * X2 - DX23 * Y2;
        float C3 = DY31 * X3 - DX31 * Y3;


        float hs12 = C1 + DX12 * miny - DY12 * minx;
        float hs23 = C2 + DX23 * miny - DY23 * minx;
        float hs31 = C3 + DX31 * miny - DY31 * minx;


        u8* cb_y = (u8*)render_buffer;
        cb_y += (miny - area->top) * stride_bytes + (minx - area->left) * 4;

        PlaneStepper3 Z;
        Z.Setup(v1, v2, v3, v1.z, v2.z, v3.z);

        float y_ps = miny;
        float minx_ps = minx;

        // Loop through pixels
        for (int y = spany; y > 0; y -= 1)
        {
            float Xhs12 = hs12;
            float Xhs23 = hs23;
            float Xhs31 = hs31;
            u8* cb_x = cb_y;
            float x_ps = minx_ps;
            for (int x = spanx; x > 0; x -= 1)
            {
                Xhs12 -= DY12;
                Xhs23 -= DY23;
                Xhs31 -= DY31;

                bool inTriangle = EvalHalfSpaceAll(Xhs12, Xhs23, Xhs31);

                if (inTriangle)
                {
                    PixelFlush_isp<render_mode>(x_ps, y_ps, cb_x, Z, tag);
                }

                cb_x += 4;
                x_ps = x_ps + 1;
            }
        next_y:
            hs12 += DX12;
            hs23 += DX23;
            hs31 += DX31;
            cb_y += stride_bytes;
            y_ps = y_ps + 1;
        }
    }


    // Used for deferred TSP processing lookups
    struct FpuEntry
    {
        IPs3 ips;
        DrawParameters params;
        text_info texture;
    };

    vector<FpuEntry> fpu_entires;
    
    virtual u8* GetColorOutputBuffer() {

        clFlush(cl.command_queue);
        clFinish(cl.command_queue);
        clEnqueueReadBuffer(cl.command_queue, cl.render_buffer, CL_TRUE, ACCUM1_BUFFER_PIXEL_OFFSET * 4, MAX_RENDER_PIXELS * 4, render_buffer + ACCUM1_BUFFER_PIXEL_OFFSET, 0, NULL, NULL);

        return reinterpret_cast<u8*>(render_buffer + ACCUM1_BUFFER_PIXEL_OFFSET);
    }

    void operator delete(void* p) {
        _mm_free(p);
    }

    void cl_check(int err, const char* err_string)
    {
        if (err != CL_SUCCESS)
        {
            printf("CL Error: %s (%d)\n", err_string, err);
            exit(-1);
        }
    }

    void clAllocFpuEntries() {
        cl_int err;

        if (cl.fpu_entries) {
            clReleaseMemObject(cl.fpu_entries);
        }

        cl.fpu_entries = clCreateBuffer(cl.context, CL_MEM_READ_WRITE, cl.fpu_entries_size * sizeof(FpuEntry), NULL, &err);
        cl_check( err, "clCreateBuffer" );
    }

    void SyncFpuEntries() {
        if (cl.fpu_entries_count < fpu_entires.size()) {

            if (cl.fpu_entries_size < fpu_entires.size()) {
                cl.fpu_entries_size = 16 + fpu_entires.size() * 2;
                clAllocFpuEntries();
                cl.fpu_entries_count = 0;
            }

            clEnqueueWriteBuffer(cl.command_queue, cl.fpu_entries, CL_TRUE, 0, fpu_entires.size() * sizeof(FpuEntry), &fpu_entires[0], 0, NULL, NULL);

            cl.fpu_entries_count = fpu_entires.size();
        }
    }

    bool Init()  {
        if (!refrend::Init())
            return false;

        cl_uint ret_num_devices;
        cl_uint ret_num_platforms;
        cl_int err;

        // Step 01: Get platform/device information
        err = clGetPlatformIDs( 1, &cl.platform_id, &ret_num_platforms );
        cl_check( err, "clGetPlatformIDs" );

        // Step 02: Get information about the device
        err = clGetDeviceIDs( cl.platform_id, CL_DEVICE_TYPE_GPU, 1, &cl.device_id, &ret_num_devices );
        cl_check( err, "clGetDeviceIDs" );
 
        // Step 03: Create OpenCL Context
        cl.context = clCreateContext( NULL, 1, &cl.device_id, NULL, NULL, &err );
        cl_check( err, "clCreateContext" );
   
        // Step 04: Create Command Queue
        cl.command_queue = clCreateCommandQueue( cl.context, cl.device_id, 0, &err );
        cl_check( err, "clCreateCommandQueue" );

        // Step 05: Create memory objects and tranfer the data to memory buffer
        cl.render_buffer = clCreateBuffer(cl.context, CL_MEM_READ_WRITE, sizeof(render_buffer), NULL, &err);
        
        //err = clEnqueueWriteBuffer(command_queue, Amobj, CL_TRUE, 0, 16*sizeof(float), A, 0, NULL, NULL);

        for (auto i = 0; i < RM_COUNT; i++) {
            char* source = new char[strlen(openclPixelPipeline) + 512];
            sprintf(source, openclPixelPipeline, i);

            cl.program[i] = clCreateProgramWithSource( cl.context, 1, (const char **) &source, 0, &err );
            cl_check( err, "clCreateProgramWithSource" );

            delete[] source;

            err = clBuildProgram( cl.program[i], 1, &cl.device_id, NULL, NULL, NULL );

            size_t ret_val_size;

            clGetProgramBuildInfo(cl.program[i], cl.device_id, CL_PROGRAM_BUILD_LOG, 0, NULL, &ret_val_size);

            if (ret_val_size) {
                auto build_log = new char[ret_val_size+1];

                clGetProgramBuildInfo(cl.program[i], cl.device_id, CL_PROGRAM_BUILD_LOG, ret_val_size, build_log, NULL);
                build_log[ret_val_size] = '\0';
                printf("OpenCL: %s\n", build_log);
                delete[] build_log;
            }

            cl_check( err, "clBuildProgram" );

            // Step 09: Create OpenCL Kernel
            cl.shadePixelsKernel[i] = clCreateKernel( cl.program[i], "ShadePixels", &err );
            cl_check( err, "clCreateKernel" );
        }

        return true;
    }

    ~refcl() {
        for (auto i = 0; i < RM_COUNT; i++) {
            if (cl.shadePixelsKernel[i]) clReleaseKernel(cl.shadePixelsKernel[i]);
            if (cl.program[i]) clReleaseProgram(cl.program[i]);
        }

        if (cl.render_buffer) clReleaseMemObject(cl.render_buffer);
        if (cl.fpu_entries) clReleaseMemObject(cl.fpu_entries);
        if (cl.command_queue) clReleaseCommandQueue(cl.command_queue);
        if (cl.context) clReleaseContext(cl.context);
    }
};

#if FEAT_TA == TA_LLE
Renderer* rend_refcl(u8* vram) {
    return new(_mm_malloc(sizeof(refcl), 32)) ::refcl(vram);
}

static auto refrend = RegisterRendererBackend(rendererbackend_t{ "refcl", "RefCL", 0, rend_refcl });
#endif