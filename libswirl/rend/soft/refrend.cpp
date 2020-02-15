/*
	This file is part of libswirl
*/
#include "license/bsd"

/*
    REFSW: Reference-style software rasterizer

    An attempt to model CLX2's CORE/SPG/RAMDAC at the lowest functional level

    Rasterizer structure
    ===

    Reads tile lists in CORE format, generated from a LLE TA implementation or software running from sh4,
    renders them in 32x32 tiles, reads out to VRAM and displays framebuffer from VRAM.

    CORE high level overview
    ===

    CORE Renders based on the REGION ARRAY, which is a flag-terminated list of tiles. Each RegionArrayEntry
    contains the TILE x/y position, control flags for Z clear/Write out/Presort and pointers to OBJECT LISTS.

    OBJECT LISTS are inline linked lists containing ObjectListEntries. Each ObjectListEntry has a small
    descriptor for the entry type and vertex size, and a pointer to the OBJECT DATA.

    OBJECT DATA contains the PARAMETERS for the OBJECT (ISP, TSP, TCW, optional TSP2 and TCW2) and vertixes.

    There are 3 OBJECT DATA TYPES
    - Triangle Strips (PARAMETERS, up to 8 VTXs) x 1
    - Triangle Arrays (PARAMETERS, 3 vtx) x Num_of_primitives
    - Quad Arrays (PARAMETERS, 4 vtx) x Num_of_primitives

    CORE renders the OBJECTS to its internal TILE BUFFERS, scales and filters the output (SCL)
    and writes out to VRAM.

    CORE Rendering details
    ===

    CORE has four main components, FPU (triangle setup) ISP (Rasterization, depth, stencil), TSP (Texutre + Shading)
    and SCL (tile writeout + scaling). There are three color rendering modes: DEPTH FIRST, DEPTH + COLOR and LAYER PEELING.

    OPAQUE OBJECTS are rendered using the DEPTH FIRST mode.
    PUNCH THROUGH OBJECTS are rendered using the DEPTH + COLOR mode.
    TRANSPARENT OBJECTS are rendered using either the DEPTH + COLOR mode or the LAYER PEELING mode.
    
    DEPTH FIRST mode
    ---
    OBJECTS are first rendered by ISP in the depth and tag buffers, 32 pixels (?) at a time. then the SPAN SORTER collects spans with the
    same tag and sends them to TSP for shading processing, one pixel at a time.

    DEPTH + COLOR mode
    ---
    OBJECTS are rendered by ISP and TSP at the same time, one pixel (?) at a time. ALPHA TEST feedback from TSP modifies the Z-write behavior.

    LAYER PEELING mode
    ---

    OBJECTS are first rendered by ISP in the depth and tag buffers, using a depth pass and a depth test buffer. SPAN SORTER collects spans with
    the same tag and sends them to TSP for shading processing. The process repeats itself until all layers have been indepedently rendered. On
    each pass, only the pixels with the lowest depth value that pass the depth pass buffer are rendered.
*/


#include <omp.h>
#include "hw/pvr/Renderer_if.h"
#include "hw/pvr/pvr_mem.h"
#include "oslib/oslib.h"
#include "rend/TexCache.h"

#include <mmintrin.h>
#include <xmmintrin.h>
#include <emmintrin.h>
#include <smmintrin.h>

#include <cmath>
#include <float.h>

#include "rend/gles/gles.h"
#include "gui/gui.h"

extern u32 decoded_colors[3][65536];

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

static DECL_ALIGN(32) u32 render_buffer[MAX_RENDER_PIXELS * 6]; //param pointers + depth1 + depth2 + stencil + acum 1 + acum 2


#pragma pack(push, 1) 
union RegionArrayEntryControl {
    struct {
        u32 res0 : 2;
        u32 tilex : 6;
        u32 tiley : 6;
        u32 res1 : 14;
        u32 no_writeout : 1;
        u32 pre_sort : 1;
        u32 z_keep : 1;
        u32 last_region : 1;
    };
    u32 full;
};

typedef u32 pvr32addr_t;

union ListPointer {
    struct
    {
        u32 pad0 : 2;
        u32 ptr_in_words : 22;
        u32 pad1 : 7;
        u32 empty : 1;
    };
    u32 full;
};

union ObjectListEntry {
    struct {
        u32 pad0 : 31;
        u32 is_not_triangle_strip : 1;
    };

    struct {
        u32 pad1 : 29;
        u32 type : 3;
    };

    struct {
        u32 param_offs_in_words : 21;
        u32 skip : 3;
        u32 shadow : 1;
        u32 mask : 6;
    } tstrip;

    struct {
        u32 param_offs_in_words : 21;
        u32 skip : 3;
        u32 shadow : 1;
        u32 prims : 4;
    } tarray;

    struct {
        u32 param_offs_in_words : 21;
        u32 skip : 3;
        u32 shadow : 1;
        u32 prims : 4;
    } qarray;

    struct {
        u32 pad3 : 2;
        u32 next_block_ptr_in_words : 22;
        u32 pad4 : 4;
        u32 end_of_list : 1;
    } link;

    u32 full;
};

#pragma pack(pop)

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
};

#if HOST_OS != OS_WINDOWS
struct RECT {
    int left, top, right, bottom;
};

#include     <X11/Xlib.h>
#endif

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


#define TPL_DECL_pixel template<RenderMode render_mode, bool pp_UseAlpha, bool pp_Texture, bool pp_IgnoreTexA, int pp_ShadInstr, bool pp_Offset >

#define TPL_PRMS_pixel <render_mode, pp_UseAlpha, pp_Texture, pp_IgnoreTexA, pp_ShadInstr, pp_Offset >

struct refrend;

// lookup tables
typedef bool(*PixelFlush_tspFn)(const text_info *texture, float x, float y, u8 *pb, IPs3 &ip);
static PixelFlush_tspFn PixelFlush_tspFns[3][2][2][2][4][2];

#if HOST_OS == OS_WINDOWS
static BITMAPINFOHEADER bi = { sizeof(BITMAPINFOHEADER), 0, 0, 1, 32, BI_RGB };
#endif

bool gles_init();

typedef u32 parameter_tag_t;

/*
    Main renderer class
*/
struct refrend : Renderer
{
    u8* vram;

    // Used for deferred TSP processing lookups
    struct FpuEntry
    {
        IPs3 ips;
        DrawParameters params;
        text_info texture;
        PixelFlush_tspFn shade;
    };

    vector<FpuEntry> fpu_entires;

    refrend(u8* vram) : vram(vram) { }

    // Texture and shade a pixel
    TPL_DECL_pixel 
    static bool PixelFlush_tsp(const text_info *texture, float x, float y, u8 *pb, IPs3 &ip)
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

        entry.shade = PixelFlush_tspFns[render_mode][entry.params.tsp.UseAlpha][entry.params.isp.Texture][entry.params.tsp.IgnoreTexA][entry.params.tsp.ShadInstr][entry.params.isp.Offset];

        fpu_entires.push_back(entry);

        return fpu_entires.size();
    }

    // Lookup/create cached TSP parameters, and call PixelFlush_tsp
    bool PixelFlush_tsp(float x, float y, u8 *pb, parameter_tag_t tag)
    {
        if (tag == 0)
            return false;

        auto entry = &fpu_entires[tag-1];
        
        return entry->shade(&entry->texture, x, y, pb, entry->ips);
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

                if (PixelFlush_tsp(x, y, pb, tag))
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

    // Used by layer peeling to determine end of processing
    int PixelsDrawn;

    // Rasterize a single triangle to ISP (or ISP+TSP for PT)
    template<RenderMode render_mode>
    void RasterizeTriangle(DrawParameters* params, parameter_tag_t tag, int vertex_offset, const Vertex& v1, const Vertex& v2, const Vertex& v3, u32* colorBuffer, RECT* area)
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


        u8* cb_y = (u8*)colorBuffer;
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

    template<RenderMode render_mode>
    void RendTriangle(DrawParameters* params, parameter_tag_t tag, int vertex_offset, const Vertex& v1, const Vertex& v2, const Vertex& v3, u32* colorBuffer, RECT* area)
    {
        RasterizeTriangle<render_mode>(params, tag, vertex_offset, v1, v2, v3, colorBuffer, area);

        if (render_mode == RM_MODIFIER)
        {          
            u32* stencil = &colorBuffer[STENCIL_BUFFER_PIXEL_OFFSET];

            if (params->isp.modvol.VolumeMode == 1 ) 
            {
                // post movdol merge INSIDE
                for (int i = 0; i < MAX_RENDER_PIXELS; i++) {
                    stencil[i] |= (stencil[i] >>1);
                }
            }
            else if (params->isp.modvol.VolumeMode == 2) 
            {
                // post movdol merge OUTSIDE
                for (int i = 0; i < MAX_RENDER_PIXELS; i++) {
                    stencil[i] &= (stencil[i] >>1);
                }
            }
        }
    }

    void SetFBScale(float x, float y)
    {
        fb_scale_x = x;
        fb_scale_y = y;
    }

    virtual bool Process(TA_context* ctx) {

        // no-op for now
        return true;
    }


    // called on vblank
    virtual bool RenderFramebuffer() {
        Present();
        return false;
    }

    // Render to ACCUM from TAG buffer
    // TAG holds references to triangles, ACCUM is the tile framebuffer
    void RenderParamTags(int tileX, int tileY) {

        auto pb = reinterpret_cast<parameter_tag_t*>(render_buffer + PARAM_BUFFER_PIXEL_OFFSET);

        for (int y = 0; y < 32; y++) {
            for (int x = 0; x < 32; x++) {
                PixelFlush_tsp(tileX + x, tileY + y, (u8*)&render_buffer[x + y * MAX_RENDER_WIDTH], *pb++);
            }
        }
    }

    u32 ReadRegionArrayEntry(u32 base, RegionArrayEntry* entry) 
    {
        bool fmt_v1 = (((FPU_PARAM_CFG >> 21) & 1) == 0);

        entry->control.full     = vri(vram, base);
        entry->opaque.full      = vri(vram, base + 4);
        entry->opaque_mod.full  = vri(vram, base + 8);
        entry->trans.full       = vri(vram, base + 12);
        entry->trans_mod.full   = vri(vram, base + 16);

        if (fmt_v1)
        {
            entry->puncht.full = 0x80000000;
            return 5 * 4;
        }
        else
        {
            entry->puncht.full = vri(vram, base + 20);
            return 6 * 4;
        }
    }

    f32 f16(u16 v)
    {
        u32 z=v<<16;
        return *(f32*)&z;
    }

	#define vert_packed_color_(to,src) \
		{ \
		u32 t=src; \
		to[2] = (u8)(t);t>>=8;\
		to[1] = (u8)(t);t>>=8;\
		to[0] = (u8)(t);t>>=8;\
		to[3] = (u8)(t);      \
		}

    //decode a vertex in the native pvr format
    void decode_pvr_vertex(DrawParameters* params, pvr32addr_t ptr,Vertex* cv)
    {
        //XYZ
        //UV
        //Base Col
        //Offset Col

        //XYZ are _allways_ there :)
        cv->x=vrf(vram, ptr);ptr+=4;
        cv->y=vrf(vram, ptr);ptr+=4;
        cv->z=vrf(vram, ptr);ptr+=4;

        if (params->isp.Texture)
        {	//Do texture , if any
            if (params->isp.UV_16b)
            {
                u32 uv=vri(vram, ptr);
                cv->u = f16((u16)uv);
                cv->v = f16((u16)(uv >> 16));
                ptr+=4;
            }
            else
            {
                cv->u=vrf(vram, ptr);ptr+=4;
                cv->v=vrf(vram, ptr);ptr+=4;
            }
        }

        //Color
        u32 col=vri(vram, ptr);ptr+=4;
        vert_packed_color_(cv->col,col);
        if (params->isp.Offset)
        {
            //Intensity color (can be missing too ;p)
            u32 col=vri(vram, ptr);ptr+=4;
            vert_packed_color_(cv->spc,col);
        }
    }

    // decode an object (params + vertexes)
    u32 decode_pvr_vetrices(DrawParameters* params, pvr32addr_t base, u32 skip, u32 shadow, Vertex* vtx, int count)
    {
        bool PSVM=FPU_SHAD_SCALE.intensity_shadow == 0;

        if (!PSVM) {
            shadow = 0; // no double volume stuff
        }

        params->isp.full=vri(vram, base);
        params->tsp.full=vri(vram, base+4);
        params->tcw.full=vri(vram, base+8);

        base += 12;
        if (shadow) {
            params->tsp2.full=vri(vram, base+0);
            params->tcw2.full=vri(vram, base+4);
            base += 8;
        }

        for (int i = 0; i < count; i++) {
            decode_pvr_vertex(params,base, &vtx[i]);
            base += (3 + skip * (shadow+1)) * 4;
        }

        return base;
    }


    // render a triangle strip object list entry
    template<RenderMode render_mode>
    void RenderTriangleStrip(ObjectListEntry obj, RECT* rect)
    {
        Vertex vtx[8];
        DrawParameters params;

        u32 param_base = PARAM_BASE & 0xF00000;

        decode_pvr_vetrices(&params, param_base + obj.tstrip.param_offs_in_words * 4, obj.tstrip.skip, obj.tstrip.shadow, vtx, 8);


        for (int i = 0; i < 6; i++)
        {
            if (obj.tstrip.mask & (1 << (5-i)))
            {

                parameter_tag_t tag = AddFpuEntry(&params, &vtx[i], render_mode);

                RendTriangle<render_mode>(&params, tag, i&1, vtx[i+0], vtx[i+1], vtx[i+2], render_buffer, rect);
            }
        }
    }


    // render a triangle array object list entry
    template<RenderMode render_mode>
    void RenderTriangleArray(ObjectListEntry obj, RECT* rect)
    {
        auto triangles = obj.tarray.prims + 1;
        u32 param_base = PARAM_BASE & 0xF00000;


        u32 param_ptr = param_base + obj.tarray.param_offs_in_words * 4;
        
        for (int i = 0; i<triangles; i++)
        {
            DrawParameters params;
            Vertex vtx[3];

            param_ptr = decode_pvr_vetrices(&params, param_ptr, obj.tarray.skip, obj.tarray.shadow, vtx, 3);
            
            parameter_tag_t tag = 0;
            if (render_mode != RM_MODIFIER)
            {
                tag = AddFpuEntry(&params, &vtx[0], render_mode);
            }

            RendTriangle<render_mode>(&params, tag, 0, vtx[0], vtx[1], vtx[2], render_buffer, rect);
        }
    }

    // render a quad array object list entry
    template<RenderMode render_mode>
    void RenderQuadArray(ObjectListEntry obj, RECT* rect)
    {
        auto quads = obj.qarray.prims + 1;
        u32 param_base = PARAM_BASE & 0xF00000;


        u32 param_ptr = param_base + obj.qarray.param_offs_in_words * 4;
        
        for (int i = 0; i<quads; i++)
        {
            DrawParameters params;
            Vertex vtx[4];

            param_ptr = decode_pvr_vetrices(&params, param_ptr, obj.qarray.skip, obj.qarray.shadow, vtx, 4);
            
            parameter_tag_t tag = AddFpuEntry(&params, &vtx[0], render_mode);

            //TODO: FIXME
            RendTriangle<render_mode>(&params, tag, 0, vtx[0], vtx[1], vtx[2], render_buffer, rect);
        }
    }

    // Render an object list
    template<RenderMode render_mode>
    void RenderObjectList(pvr32addr_t base, RECT* rect)
    {
        ObjectListEntry obj;

        for (;;) {
            obj.full = vri(vram, base);
            base += 4;

            if (!obj.is_not_triangle_strip) {
                RenderTriangleStrip<render_mode>(obj, rect);
            } else {
                switch(obj.type) {
                    case 0b111: // link
                        if (obj.link.end_of_list)
                            return;

                            base = obj.link.next_block_ptr_in_words * 4;
                        break;

                    case 0b100: // triangle array
                        RenderTriangleArray<render_mode>(obj, rect);
                        break;
                    
                    case 0b101: // quad array
                        RenderQuadArray<render_mode>(obj, rect);
                        break;

                    default:
                        printf("RenderObjectList: Not handled object type: %d\n", obj.type);
                }
            }
        }
    }

    // Render a frame
    // Called on START_RENDER write
    virtual bool RenderPVR() {

        u32 base = REGION_BASE;

        RegionArrayEntry entry;
        int tilenum = 1;

        // Parse region array
        do {
            base += ReadRegionArrayEntry(base, &entry);

            RECT rect;
            rect.top = entry.control.tiley * 32;
            rect.left = entry.control.tilex * 32;
            
            rect.bottom = rect.top + 32;
            rect.right = rect.left + 32;

            // Tile needs clear?
            if (!entry.control.z_keep) {
                
                // Clear Z
                auto zb = reinterpret_cast<float*>(render_buffer + DEPTH1_BUFFER_PIXEL_OFFSET);

                for (int i = 0; i < MAX_RENDER_PIXELS; i++) {
                    zb[i] = ISP_BACKGND_D.f;
                }

                // Clear PB
                auto pb = reinterpret_cast<parameter_tag_t*>(render_buffer + PARAM_BUFFER_PIXEL_OFFSET);

                DrawParameters params;
                Vertex vtx[8];
                decode_pvr_vetrices(&params, PARAM_BASE + ISP_BACKGND_T.tag_address * 4, ISP_BACKGND_T.skip, ISP_BACKGND_T.shadow, vtx, 8);

                auto tag = AddFpuEntry(&params, &vtx[ISP_BACKGND_T.tag_offset], RM_OPAQUE);

                for (int i = 0; i < MAX_RENDER_PIXELS; i++) {
                    pb[i] = tag;
                }

                // Clear stencil
                auto stencil = reinterpret_cast<u32*>(render_buffer + STENCIL_BUFFER_PIXEL_OFFSET);

                memset(stencil, 0, MAX_RENDER_PIXELS * 4);
            }

            // Render OPAQ to TAGS          
            if (!entry.opaque.empty) {
                RenderObjectList<RM_OPAQUE>(entry.opaque.ptr_in_words * 4, &rect);
            }

            // render PT to TAGS
            if (!entry.puncht.empty) {
                RenderObjectList<RM_PUNCHTHROUGH>(entry.puncht.ptr_in_words * 4, &rect);
            }

            //TODO: Render OPAQ modvols
            if (!entry.opaque_mod.empty) {
                RenderObjectList<RM_MODIFIER>(entry.opaque_mod.ptr_in_words * 4, &rect);
            }


            // Render TAGS to ACCUM
            RenderParamTags(rect.left, rect.top);

            

            // layer peeling rendering
            if (!entry.trans.empty) {

                do {
                    PixelsDrawn = 0;

                    // clear tags
                    memset(render_buffer, 0, MAX_RENDER_PIXELS * 4);

                    //copy depth test to depth pass buffer, clear test buffer

                    auto zb = reinterpret_cast<float*>(render_buffer + DEPTH1_BUFFER_PIXEL_OFFSET);
                    auto zb2 = reinterpret_cast<float*>(render_buffer + DEPTH2_BUFFER_PIXEL_OFFSET);

                    for (int i = 0; i < MAX_RENDER_PIXELS; i++) {
                        zb2[i] = zb[i];     // keep old ZB for reference
                        zb[i] = FLT_MAX;    // set the "closest" test to furthest value possible
                    }

                    // clear stencil
                    auto stencil = reinterpret_cast<u32*>(render_buffer + STENCIL_BUFFER_PIXEL_OFFSET);
                    memset(stencil, 0, MAX_RENDER_PIXELS * 4);


                    // render to TAGS
                    RenderObjectList<RM_TRANSLUCENT>(entry.trans.ptr_in_words * 4, &rect);

                    if (!entry.trans_mod.empty) {
                        RenderObjectList<RM_MODIFIER>(entry.trans_mod.ptr_in_words * 4, &rect);
                    }

                    // render TAGS to ACCUM
                    RenderParamTags(rect.left, rect.top);
                } while (PixelsDrawn != 0);
            }


            // Copy to vram
            if (!entry.control.no_writeout) {

                auto field = SCALER_CTL.fieldselect;
                auto interlace = SCALER_CTL.interlace;
                
                auto base = (interlace && field) ? FB_W_SOF2 : FB_W_SOF1;

                // very few configurations supported here
                verify(SCALER_CTL.hscale == 0);
                verify(SCALER_CTL.interlace == 0); // write both SOFs
                auto vscale = SCALER_CTL.vscalefactor;
                verify(vscale == 0x401 || vscale == 0x400 || vscale == 0x800);

                auto fb_packmode = FB_W_CTRL.fb_packmode;
                verify(fb_packmode == 0x1); // 565 RGB16

                auto bpp = 2;
                auto offset_bytes = entry.control.tilex * 32 * bpp + entry.control.tiley * 32  * FB_W_LINESTRIDE.stride * 8;

                for (int y = 0; y<32; y++) {
                    //auto base = (y&1) ? FB_W_SOF2 : FB_W_SOF1;
                    auto dst = base + offset_bytes + (y) * FB_W_LINESTRIDE.stride * 8;
                    auto src = reinterpret_cast<u8*>(render_buffer + y * STRIDE_PIXEL_OFFSET + ACCUM1_BUFFER_PIXEL_OFFSET);

                    for (int x = 0; x<32; x++) {
                        auto pixel = (((src[0] >> 3) & 0x1F) << 11) | (((src[1] >> 2) & 0x3F) << 5) | ((src[2] >> 3) & 0x1F);
                        pvr_write_area1_16(vram, dst, pixel);
                        
                        dst += bpp;
                        src+=4; // skip alpha
                    }    
                }
            }

        } while (!entry.control.last_region);

        // clear the tsp cache
        fpu_entires.clear();

        return false;
    }

#if HOST_OS == OS_WINDOWS
    HWND hWnd;
    HBITMAP hBMP = 0, holdBMP;
    HDC hmem;
#endif


    virtual bool Init() {

        gles_init();

#if HOST_OS == OS_WINDOWS
        hWnd = (HWND)libPvr_GetRenderTarget();

        bi.biWidth = 640;
        bi.biHeight = 480;

        RECT rect;

        GetClientRect(hWnd, &rect);

        HDC hdc = GetDC(hWnd);

        FillRect(hdc, &rect, (HBRUSH)(COLOR_BACKGROUND));

        bi.biSizeImage = bi.biWidth * bi.biHeight * 4;

        hBMP = CreateCompatibleBitmap(hdc, bi.biWidth, bi.biHeight);
        hmem = CreateCompatibleDC(hdc);
        holdBMP = (HBITMAP)SelectObject(hmem, hBMP);
        ReleaseDC(hWnd, hdc);
#endif
#define REP_16(x) ((x)* 16 + (x))
#define REP_32(x) ((x)* 8 + (x)/4)
#define REP_64(x) ((x)* 4 + (x)/16)

        for (int c = 0; c < 65536; c++) {
            //565
            decoded_colors[0][c] = 0xFF000000 | (REP_32((c >> 11) % 32) << 16) | (REP_64((c >> 5) % 64) << 8) | (REP_32((c >> 0) % 32) << 0);
            //1555
            decoded_colors[1][c] = ((c >> 0) % 2 * 255 << 24) | (REP_32((c >> 11) % 32) << 16) | (REP_32((c >> 6) % 32) << 8) | (REP_32((c >> 1) % 32) << 0);
            //4444
            decoded_colors[2][c] = (REP_16((c >> 0) % 16) << 24) | (REP_16((c >> 12) % 16) << 16) | (REP_16((c >> 8) % 16) << 8) | (REP_16((c >> 4) % 16) << 0);
        }


#define PixelFlush_tsp refrend::PixelFlush_tsp
        {
            PixelFlush_tspFns[0][0][1][0][0][0] = &PixelFlush_tsp<RM_OPAQUE, 0, 1, 0, 0, 0>;
            PixelFlush_tspFns[0][0][1][0][0][1] = &PixelFlush_tsp<RM_OPAQUE, 0, 1, 0, 0, 1>;
            PixelFlush_tspFns[0][0][1][0][1][0] = &PixelFlush_tsp<RM_OPAQUE, 0, 1, 0, 1, 0>;
            PixelFlush_tspFns[0][0][1][0][1][1] = &PixelFlush_tsp<RM_OPAQUE, 0, 1, 0, 1, 1>;
            PixelFlush_tspFns[0][0][1][0][2][0] = &PixelFlush_tsp<RM_OPAQUE, 0, 1, 0, 2, 0>;
            PixelFlush_tspFns[0][0][1][0][2][1] = &PixelFlush_tsp<RM_OPAQUE, 0, 1, 0, 2, 1>;
            PixelFlush_tspFns[0][0][1][0][3][0] = &PixelFlush_tsp<RM_OPAQUE, 0, 1, 0, 3, 0>;
            PixelFlush_tspFns[0][0][1][0][3][1] = &PixelFlush_tsp<RM_OPAQUE, 0, 1, 0, 3, 1>;
            PixelFlush_tspFns[0][0][1][1][0][0] = &PixelFlush_tsp<RM_OPAQUE, 0, 1, 1, 0, 0>;
            PixelFlush_tspFns[0][0][1][1][0][1] = &PixelFlush_tsp<RM_OPAQUE, 0, 1, 1, 0, 1>;
            PixelFlush_tspFns[0][0][1][1][1][0] = &PixelFlush_tsp<RM_OPAQUE, 0, 1, 1, 1, 0>;
            PixelFlush_tspFns[0][0][1][1][1][1] = &PixelFlush_tsp<RM_OPAQUE, 0, 1, 1, 1, 1>;
            PixelFlush_tspFns[0][0][1][1][2][0] = &PixelFlush_tsp<RM_OPAQUE, 0, 1, 1, 2, 0>;
            PixelFlush_tspFns[0][0][1][1][2][1] = &PixelFlush_tsp<RM_OPAQUE, 0, 1, 1, 2, 1>;
            PixelFlush_tspFns[0][0][1][1][3][0] = &PixelFlush_tsp<RM_OPAQUE, 0, 1, 1, 3, 0>;
            PixelFlush_tspFns[0][0][1][1][3][1] = &PixelFlush_tsp<RM_OPAQUE, 0, 1, 1, 3, 1>;
            PixelFlush_tspFns[0][0][0][0][0][0] = &PixelFlush_tsp<RM_OPAQUE, 0, 0, 0, 0, 0>;
            PixelFlush_tspFns[0][0][0][0][0][1] = &PixelFlush_tsp<RM_OPAQUE, 0, 0, 0, 0, 1>;
            PixelFlush_tspFns[0][0][0][0][1][0] = &PixelFlush_tsp<RM_OPAQUE, 0, 0, 0, 1, 0>;
            PixelFlush_tspFns[0][0][0][0][1][1] = &PixelFlush_tsp<RM_OPAQUE, 0, 0, 0, 1, 1>;
            PixelFlush_tspFns[0][0][0][0][2][0] = &PixelFlush_tsp<RM_OPAQUE, 0, 0, 0, 2, 0>;
            PixelFlush_tspFns[0][0][0][0][2][1] = &PixelFlush_tsp<RM_OPAQUE, 0, 0, 0, 2, 1>;
            PixelFlush_tspFns[0][0][0][0][3][0] = &PixelFlush_tsp<RM_OPAQUE, 0, 0, 0, 3, 0>;
            PixelFlush_tspFns[0][0][0][0][3][1] = &PixelFlush_tsp<RM_OPAQUE, 0, 0, 0, 3, 1>;
            PixelFlush_tspFns[0][0][0][1][0][0] = &PixelFlush_tsp<RM_OPAQUE, 0, 0, 1, 0, 0>;
            PixelFlush_tspFns[0][0][0][1][0][1] = &PixelFlush_tsp<RM_OPAQUE, 0, 0, 1, 0, 1>;
            PixelFlush_tspFns[0][0][0][1][1][0] = &PixelFlush_tsp<RM_OPAQUE, 0, 0, 1, 1, 0>;
            PixelFlush_tspFns[0][0][0][1][1][1] = &PixelFlush_tsp<RM_OPAQUE, 0, 0, 1, 1, 1>;
            PixelFlush_tspFns[0][0][0][1][2][0] = &PixelFlush_tsp<RM_OPAQUE, 0, 0, 1, 2, 0>;
            PixelFlush_tspFns[0][0][0][1][2][1] = &PixelFlush_tsp<RM_OPAQUE, 0, 0, 1, 2, 1>;
            PixelFlush_tspFns[0][0][0][1][3][0] = &PixelFlush_tsp<RM_OPAQUE, 0, 0, 1, 3, 0>;
            PixelFlush_tspFns[0][0][0][1][3][1] = &PixelFlush_tsp<RM_OPAQUE, 0, 0, 1, 3, 1>;
            PixelFlush_tspFns[0][1][1][0][0][0] = &PixelFlush_tsp<RM_OPAQUE, 1, 1, 0, 0, 0>;
            PixelFlush_tspFns[0][1][1][0][0][1] = &PixelFlush_tsp<RM_OPAQUE, 1, 1, 0, 0, 1>;
            PixelFlush_tspFns[0][1][1][0][1][0] = &PixelFlush_tsp<RM_OPAQUE, 1, 1, 0, 1, 0>;
            PixelFlush_tspFns[0][1][1][0][1][1] = &PixelFlush_tsp<RM_OPAQUE, 1, 1, 0, 1, 1>;
            PixelFlush_tspFns[0][1][1][0][2][0] = &PixelFlush_tsp<RM_OPAQUE, 1, 1, 0, 2, 0>;
            PixelFlush_tspFns[0][1][1][0][2][1] = &PixelFlush_tsp<RM_OPAQUE, 1, 1, 0, 2, 1>;
            PixelFlush_tspFns[0][1][1][0][3][0] = &PixelFlush_tsp<RM_OPAQUE, 1, 1, 0, 3, 0>;
            PixelFlush_tspFns[0][1][1][0][3][1] = &PixelFlush_tsp<RM_OPAQUE, 1, 1, 0, 3, 1>;
            PixelFlush_tspFns[0][1][1][1][0][0] = &PixelFlush_tsp<RM_OPAQUE, 1, 1, 1, 0, 0>;
            PixelFlush_tspFns[0][1][1][1][0][1] = &PixelFlush_tsp<RM_OPAQUE, 1, 1, 1, 0, 1>;
            PixelFlush_tspFns[0][1][1][1][1][0] = &PixelFlush_tsp<RM_OPAQUE, 1, 1, 1, 1, 0>;
            PixelFlush_tspFns[0][1][1][1][1][1] = &PixelFlush_tsp<RM_OPAQUE, 1, 1, 1, 1, 1>;
            PixelFlush_tspFns[0][1][1][1][2][0] = &PixelFlush_tsp<RM_OPAQUE, 1, 1, 1, 2, 0>;
            PixelFlush_tspFns[0][1][1][1][2][1] = &PixelFlush_tsp<RM_OPAQUE, 1, 1, 1, 2, 1>;
            PixelFlush_tspFns[0][1][1][1][3][0] = &PixelFlush_tsp<RM_OPAQUE, 1, 1, 1, 3, 0>;
            PixelFlush_tspFns[0][1][1][1][3][1] = &PixelFlush_tsp<RM_OPAQUE, 1, 1, 1, 3, 1>;
            PixelFlush_tspFns[0][1][0][0][0][0] = &PixelFlush_tsp<RM_OPAQUE, 1, 0, 0, 0, 0>;
            PixelFlush_tspFns[0][1][0][0][0][1] = &PixelFlush_tsp<RM_OPAQUE, 1, 0, 0, 0, 1>;
            PixelFlush_tspFns[0][1][0][0][1][0] = &PixelFlush_tsp<RM_OPAQUE, 1, 0, 0, 1, 0>;
            PixelFlush_tspFns[0][1][0][0][1][1] = &PixelFlush_tsp<RM_OPAQUE, 1, 0, 0, 1, 1>;
            PixelFlush_tspFns[0][1][0][0][2][0] = &PixelFlush_tsp<RM_OPAQUE, 1, 0, 0, 2, 0>;
            PixelFlush_tspFns[0][1][0][0][2][1] = &PixelFlush_tsp<RM_OPAQUE, 1, 0, 0, 2, 1>;
            PixelFlush_tspFns[0][1][0][0][3][0] = &PixelFlush_tsp<RM_OPAQUE, 1, 0, 0, 3, 0>;
            PixelFlush_tspFns[0][1][0][0][3][1] = &PixelFlush_tsp<RM_OPAQUE, 1, 0, 0, 3, 1>;
            PixelFlush_tspFns[0][1][0][1][0][0] = &PixelFlush_tsp<RM_OPAQUE, 1, 0, 1, 0, 0>;
            PixelFlush_tspFns[0][1][0][1][0][1] = &PixelFlush_tsp<RM_OPAQUE, 1, 0, 1, 0, 1>;
            PixelFlush_tspFns[0][1][0][1][1][0] = &PixelFlush_tsp<RM_OPAQUE, 1, 0, 1, 1, 0>;
            PixelFlush_tspFns[0][1][0][1][1][1] = &PixelFlush_tsp<RM_OPAQUE, 1, 0, 1, 1, 1>;
            PixelFlush_tspFns[0][1][0][1][2][0] = &PixelFlush_tsp<RM_OPAQUE, 1, 0, 1, 2, 0>;
            PixelFlush_tspFns[0][1][0][1][2][1] = &PixelFlush_tsp<RM_OPAQUE, 1, 0, 1, 2, 1>;
            PixelFlush_tspFns[0][1][0][1][3][0] = &PixelFlush_tsp<RM_OPAQUE, 1, 0, 1, 3, 0>;
            PixelFlush_tspFns[0][1][0][1][3][1] = &PixelFlush_tsp<RM_OPAQUE, 1, 0, 1, 3, 1>;
            
            PixelFlush_tspFns[1][0][1][0][0][0] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 0, 1, 0, 0, 0>;
            PixelFlush_tspFns[1][0][1][0][0][1] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 0, 1, 0, 0, 1>;
            PixelFlush_tspFns[1][0][1][0][1][0] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 0, 1, 0, 1, 0>;
            PixelFlush_tspFns[1][0][1][0][1][1] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 0, 1, 0, 1, 1>;
            PixelFlush_tspFns[1][0][1][0][2][0] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 0, 1, 0, 2, 0>;
            PixelFlush_tspFns[1][0][1][0][2][1] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 0, 1, 0, 2, 1>;
            PixelFlush_tspFns[1][0][1][0][3][0] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 0, 1, 0, 3, 0>;
            PixelFlush_tspFns[1][0][1][0][3][1] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 0, 1, 0, 3, 1>;
            PixelFlush_tspFns[1][0][1][1][0][0] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 0, 1, 1, 0, 0>;
            PixelFlush_tspFns[1][0][1][1][0][1] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 0, 1, 1, 0, 1>;
            PixelFlush_tspFns[1][0][1][1][1][0] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 0, 1, 1, 1, 0>;
            PixelFlush_tspFns[1][0][1][1][1][1] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 0, 1, 1, 1, 1>;
            PixelFlush_tspFns[1][0][1][1][2][0] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 0, 1, 1, 2, 0>;
            PixelFlush_tspFns[1][0][1][1][2][1] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 0, 1, 1, 2, 1>;
            PixelFlush_tspFns[1][0][1][1][3][0] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 0, 1, 1, 3, 0>;
            PixelFlush_tspFns[1][0][1][1][3][1] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 0, 1, 1, 3, 1>;
            PixelFlush_tspFns[1][0][0][0][0][0] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 0, 0, 0, 0, 0>;
            PixelFlush_tspFns[1][0][0][0][0][1] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 0, 0, 0, 0, 1>;
            PixelFlush_tspFns[1][0][0][0][1][0] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 0, 0, 0, 1, 0>;
            PixelFlush_tspFns[1][0][0][0][1][1] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 0, 0, 0, 1, 1>;
            PixelFlush_tspFns[1][0][0][0][2][0] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 0, 0, 0, 2, 0>;
            PixelFlush_tspFns[1][0][0][0][2][1] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 0, 0, 0, 2, 1>;
            PixelFlush_tspFns[1][0][0][0][3][0] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 0, 0, 0, 3, 0>;
            PixelFlush_tspFns[1][0][0][0][3][1] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 0, 0, 0, 3, 1>;
            PixelFlush_tspFns[1][0][0][1][0][0] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 0, 0, 1, 0, 0>;
            PixelFlush_tspFns[1][0][0][1][0][1] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 0, 0, 1, 0, 1>;
            PixelFlush_tspFns[1][0][0][1][1][0] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 0, 0, 1, 1, 0>;
            PixelFlush_tspFns[1][0][0][1][1][1] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 0, 0, 1, 1, 1>;
            PixelFlush_tspFns[1][0][0][1][2][0] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 0, 0, 1, 2, 0>;
            PixelFlush_tspFns[1][0][0][1][2][1] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 0, 0, 1, 2, 1>;
            PixelFlush_tspFns[1][0][0][1][3][0] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 0, 0, 1, 3, 0>;
            PixelFlush_tspFns[1][0][0][1][3][1] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 0, 0, 1, 3, 1>;
            PixelFlush_tspFns[1][1][1][0][0][0] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 1, 1, 0, 0, 0>;
            PixelFlush_tspFns[1][1][1][0][0][1] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 1, 1, 0, 0, 1>;
            PixelFlush_tspFns[1][1][1][0][1][0] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 1, 1, 0, 1, 0>;
            PixelFlush_tspFns[1][1][1][0][1][1] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 1, 1, 0, 1, 1>;
            PixelFlush_tspFns[1][1][1][0][2][0] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 1, 1, 0, 2, 0>;
            PixelFlush_tspFns[1][1][1][0][2][1] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 1, 1, 0, 2, 1>;
            PixelFlush_tspFns[1][1][1][0][3][0] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 1, 1, 0, 3, 0>;
            PixelFlush_tspFns[1][1][1][0][3][1] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 1, 1, 0, 3, 1>;
            PixelFlush_tspFns[1][1][1][1][0][0] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 1, 1, 1, 0, 0>;
            PixelFlush_tspFns[1][1][1][1][0][1] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 1, 1, 1, 0, 1>;
            PixelFlush_tspFns[1][1][1][1][1][0] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 1, 1, 1, 1, 0>;
            PixelFlush_tspFns[1][1][1][1][1][1] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 1, 1, 1, 1, 1>;
            PixelFlush_tspFns[1][1][1][1][2][0] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 1, 1, 1, 2, 0>;
            PixelFlush_tspFns[1][1][1][1][2][1] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 1, 1, 1, 2, 1>;
            PixelFlush_tspFns[1][1][1][1][3][0] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 1, 1, 1, 3, 0>;
            PixelFlush_tspFns[1][1][1][1][3][1] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 1, 1, 1, 3, 1>;
            PixelFlush_tspFns[1][1][0][0][0][0] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 1, 0, 0, 0, 0>;
            PixelFlush_tspFns[1][1][0][0][0][1] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 1, 0, 0, 0, 1>;
            PixelFlush_tspFns[1][1][0][0][1][0] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 1, 0, 0, 1, 0>;
            PixelFlush_tspFns[1][1][0][0][1][1] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 1, 0, 0, 1, 1>;
            PixelFlush_tspFns[1][1][0][0][2][0] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 1, 0, 0, 2, 0>;
            PixelFlush_tspFns[1][1][0][0][2][1] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 1, 0, 0, 2, 1>;
            PixelFlush_tspFns[1][1][0][0][3][0] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 1, 0, 0, 3, 0>;
            PixelFlush_tspFns[1][1][0][0][3][1] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 1, 0, 0, 3, 1>;
            PixelFlush_tspFns[1][1][0][1][0][0] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 1, 0, 1, 0, 0>;
            PixelFlush_tspFns[1][1][0][1][0][1] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 1, 0, 1, 0, 1>;
            PixelFlush_tspFns[1][1][0][1][1][0] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 1, 0, 1, 1, 0>;
            PixelFlush_tspFns[1][1][0][1][1][1] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 1, 0, 1, 1, 1>;
            PixelFlush_tspFns[1][1][0][1][2][0] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 1, 0, 1, 2, 0>;
            PixelFlush_tspFns[1][1][0][1][2][1] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 1, 0, 1, 2, 1>;
            PixelFlush_tspFns[1][1][0][1][3][0] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 1, 0, 1, 3, 0>;
            PixelFlush_tspFns[1][1][0][1][3][1] = &PixelFlush_tsp<RM_PUNCHTHROUGH, 1, 0, 1, 3, 1>;

            PixelFlush_tspFns[2][0][1][0][0][0] = &PixelFlush_tsp<RM_TRANSLUCENT, 0, 1, 0, 0, 0>;
            PixelFlush_tspFns[2][0][1][0][0][1] = &PixelFlush_tsp<RM_TRANSLUCENT, 0, 1, 0, 0, 1>;
            PixelFlush_tspFns[2][0][1][0][1][0] = &PixelFlush_tsp<RM_TRANSLUCENT, 0, 1, 0, 1, 0>;
            PixelFlush_tspFns[2][0][1][0][1][1] = &PixelFlush_tsp<RM_TRANSLUCENT, 0, 1, 0, 1, 1>;
            PixelFlush_tspFns[2][0][1][0][2][0] = &PixelFlush_tsp<RM_TRANSLUCENT, 0, 1, 0, 2, 0>;
            PixelFlush_tspFns[2][0][1][0][2][1] = &PixelFlush_tsp<RM_TRANSLUCENT, 0, 1, 0, 2, 1>;
            PixelFlush_tspFns[2][0][1][0][3][0] = &PixelFlush_tsp<RM_TRANSLUCENT, 0, 1, 0, 3, 0>;
            PixelFlush_tspFns[2][0][1][0][3][1] = &PixelFlush_tsp<RM_TRANSLUCENT, 0, 1, 0, 3, 1>;
            PixelFlush_tspFns[2][0][1][1][0][0] = &PixelFlush_tsp<RM_TRANSLUCENT, 0, 1, 1, 0, 0>;
            PixelFlush_tspFns[2][0][1][1][0][1] = &PixelFlush_tsp<RM_TRANSLUCENT, 0, 1, 1, 0, 1>;
            PixelFlush_tspFns[2][0][1][1][1][0] = &PixelFlush_tsp<RM_TRANSLUCENT, 0, 1, 1, 1, 0>;
            PixelFlush_tspFns[2][0][1][1][1][1] = &PixelFlush_tsp<RM_TRANSLUCENT, 0, 1, 1, 1, 1>;
            PixelFlush_tspFns[2][0][1][1][2][0] = &PixelFlush_tsp<RM_TRANSLUCENT, 0, 1, 1, 2, 0>;
            PixelFlush_tspFns[2][0][1][1][2][1] = &PixelFlush_tsp<RM_TRANSLUCENT, 0, 1, 1, 2, 1>;
            PixelFlush_tspFns[2][0][1][1][3][0] = &PixelFlush_tsp<RM_TRANSLUCENT, 0, 1, 1, 3, 0>;
            PixelFlush_tspFns[2][0][1][1][3][1] = &PixelFlush_tsp<RM_TRANSLUCENT, 0, 1, 1, 3, 1>;
            PixelFlush_tspFns[2][0][0][0][0][0] = &PixelFlush_tsp<RM_TRANSLUCENT, 0, 0, 0, 0, 0>;
            PixelFlush_tspFns[2][0][0][0][0][1] = &PixelFlush_tsp<RM_TRANSLUCENT, 0, 0, 0, 0, 1>;
            PixelFlush_tspFns[2][0][0][0][1][0] = &PixelFlush_tsp<RM_TRANSLUCENT, 0, 0, 0, 1, 0>;
            PixelFlush_tspFns[2][0][0][0][1][1] = &PixelFlush_tsp<RM_TRANSLUCENT, 0, 0, 0, 1, 1>;
            PixelFlush_tspFns[2][0][0][0][2][0] = &PixelFlush_tsp<RM_TRANSLUCENT, 0, 0, 0, 2, 0>;
            PixelFlush_tspFns[2][0][0][0][2][1] = &PixelFlush_tsp<RM_TRANSLUCENT, 0, 0, 0, 2, 1>;
            PixelFlush_tspFns[2][0][0][0][3][0] = &PixelFlush_tsp<RM_TRANSLUCENT, 0, 0, 0, 3, 0>;
            PixelFlush_tspFns[2][0][0][0][3][1] = &PixelFlush_tsp<RM_TRANSLUCENT, 0, 0, 0, 3, 1>;
            PixelFlush_tspFns[2][0][0][1][0][0] = &PixelFlush_tsp<RM_TRANSLUCENT, 0, 0, 1, 0, 0>;
            PixelFlush_tspFns[2][0][0][1][0][1] = &PixelFlush_tsp<RM_TRANSLUCENT, 0, 0, 1, 0, 1>;
            PixelFlush_tspFns[2][0][0][1][1][0] = &PixelFlush_tsp<RM_TRANSLUCENT, 0, 0, 1, 1, 0>;
            PixelFlush_tspFns[2][0][0][1][1][1] = &PixelFlush_tsp<RM_TRANSLUCENT, 0, 0, 1, 1, 1>;
            PixelFlush_tspFns[2][0][0][1][2][0] = &PixelFlush_tsp<RM_TRANSLUCENT, 0, 0, 1, 2, 0>;
            PixelFlush_tspFns[2][0][0][1][2][1] = &PixelFlush_tsp<RM_TRANSLUCENT, 0, 0, 1, 2, 1>;
            PixelFlush_tspFns[2][0][0][1][3][0] = &PixelFlush_tsp<RM_TRANSLUCENT, 0, 0, 1, 3, 0>;
            PixelFlush_tspFns[2][0][0][1][3][1] = &PixelFlush_tsp<RM_TRANSLUCENT, 0, 0, 1, 3, 1>;
            PixelFlush_tspFns[2][1][1][0][0][0] = &PixelFlush_tsp<RM_TRANSLUCENT, 1, 1, 0, 0, 0>;
            PixelFlush_tspFns[2][1][1][0][0][1] = &PixelFlush_tsp<RM_TRANSLUCENT, 1, 1, 0, 0, 1>;
            PixelFlush_tspFns[2][1][1][0][1][0] = &PixelFlush_tsp<RM_TRANSLUCENT, 1, 1, 0, 1, 0>;
            PixelFlush_tspFns[2][1][1][0][1][1] = &PixelFlush_tsp<RM_TRANSLUCENT, 1, 1, 0, 1, 1>;
            PixelFlush_tspFns[2][1][1][0][2][0] = &PixelFlush_tsp<RM_TRANSLUCENT, 1, 1, 0, 2, 0>;
            PixelFlush_tspFns[2][1][1][0][2][1] = &PixelFlush_tsp<RM_TRANSLUCENT, 1, 1, 0, 2, 1>;
            PixelFlush_tspFns[2][1][1][0][3][0] = &PixelFlush_tsp<RM_TRANSLUCENT, 1, 1, 0, 3, 0>;
            PixelFlush_tspFns[2][1][1][0][3][1] = &PixelFlush_tsp<RM_TRANSLUCENT, 1, 1, 0, 3, 1>;
            PixelFlush_tspFns[2][1][1][1][0][0] = &PixelFlush_tsp<RM_TRANSLUCENT, 1, 1, 1, 0, 0>;
            PixelFlush_tspFns[2][1][1][1][0][1] = &PixelFlush_tsp<RM_TRANSLUCENT, 1, 1, 1, 0, 1>;
            PixelFlush_tspFns[2][1][1][1][1][0] = &PixelFlush_tsp<RM_TRANSLUCENT, 1, 1, 1, 1, 0>;
            PixelFlush_tspFns[2][1][1][1][1][1] = &PixelFlush_tsp<RM_TRANSLUCENT, 1, 1, 1, 1, 1>;
            PixelFlush_tspFns[2][1][1][1][2][0] = &PixelFlush_tsp<RM_TRANSLUCENT, 1, 1, 1, 2, 0>;
            PixelFlush_tspFns[2][1][1][1][2][1] = &PixelFlush_tsp<RM_TRANSLUCENT, 1, 1, 1, 2, 1>;
            PixelFlush_tspFns[2][1][1][1][3][0] = &PixelFlush_tsp<RM_TRANSLUCENT, 1, 1, 1, 3, 0>;
            PixelFlush_tspFns[2][1][1][1][3][1] = &PixelFlush_tsp<RM_TRANSLUCENT, 1, 1, 1, 3, 1>;
            PixelFlush_tspFns[2][1][0][0][0][0] = &PixelFlush_tsp<RM_TRANSLUCENT, 1, 0, 0, 0, 0>;
            PixelFlush_tspFns[2][1][0][0][0][1] = &PixelFlush_tsp<RM_TRANSLUCENT, 1, 0, 0, 0, 1>;
            PixelFlush_tspFns[2][1][0][0][1][0] = &PixelFlush_tsp<RM_TRANSLUCENT, 1, 0, 0, 1, 0>;
            PixelFlush_tspFns[2][1][0][0][1][1] = &PixelFlush_tsp<RM_TRANSLUCENT, 1, 0, 0, 1, 1>;
            PixelFlush_tspFns[2][1][0][0][2][0] = &PixelFlush_tsp<RM_TRANSLUCENT, 1, 0, 0, 2, 0>;
            PixelFlush_tspFns[2][1][0][0][2][1] = &PixelFlush_tsp<RM_TRANSLUCENT, 1, 0, 0, 2, 1>;
            PixelFlush_tspFns[2][1][0][0][3][0] = &PixelFlush_tsp<RM_TRANSLUCENT, 1, 0, 0, 3, 0>;
            PixelFlush_tspFns[2][1][0][0][3][1] = &PixelFlush_tsp<RM_TRANSLUCENT, 1, 0, 0, 3, 1>;
            PixelFlush_tspFns[2][1][0][1][0][0] = &PixelFlush_tsp<RM_TRANSLUCENT, 1, 0, 1, 0, 0>;
            PixelFlush_tspFns[2][1][0][1][0][1] = &PixelFlush_tsp<RM_TRANSLUCENT, 1, 0, 1, 0, 1>;
            PixelFlush_tspFns[2][1][0][1][1][0] = &PixelFlush_tsp<RM_TRANSLUCENT, 1, 0, 1, 1, 0>;
            PixelFlush_tspFns[2][1][0][1][1][1] = &PixelFlush_tsp<RM_TRANSLUCENT, 1, 0, 1, 1, 1>;
            PixelFlush_tspFns[2][1][0][1][2][0] = &PixelFlush_tsp<RM_TRANSLUCENT, 1, 0, 1, 2, 0>;
            PixelFlush_tspFns[2][1][0][1][2][1] = &PixelFlush_tsp<RM_TRANSLUCENT, 1, 0, 1, 2, 1>;
            PixelFlush_tspFns[2][1][0][1][3][0] = &PixelFlush_tsp<RM_TRANSLUCENT, 1, 0, 1, 3, 0>;
            PixelFlush_tspFns[2][1][0][1][3][1] = &PixelFlush_tsp<RM_TRANSLUCENT, 1, 0, 1, 3, 1>;
        }
#undef PixelFlush_tsp

        return true;
    }

    virtual void Resize(int w, int h) {

    }

    ~refrend() {

#if HOST_OS == OS_WINDOWS
        if (hBMP) {
            DeleteObject(SelectObject(hmem, holdBMP));
            DeleteDC(hmem);
        }
#endif
    }

    virtual void Present()
    {

        if (FB_R_SIZE.fb_x_size == 0 || FB_R_SIZE.fb_y_size == 0)
            return;

        int width = (FB_R_SIZE.fb_x_size + 1) << 1; // in 16-bit words
        int height = FB_R_SIZE.fb_y_size + 1;
        int modulus = (FB_R_SIZE.fb_modulus - 1) << 1;

        int bpp;
        switch (FB_R_CTRL.fb_depth)
        {
        case fbde_0555:
        case fbde_565:
            bpp = 2;
            break;
        case fbde_888:
            bpp = 3;
            width = (width * 2) / 3;     // in pixels
            modulus = (modulus * 2) / 3; // in pixels
            break;
        case fbde_C888:
            bpp = 4;
            width /= 2;   // in pixels
            modulus /= 2; // in pixels
            break;
        default:
            die("Invalid framebuffer format\n");
            bpp = 4;
            break;
        }
        u32 addr = SPG_CONTROL.interlace && !SPG_STATUS.fieldnum ? FB_R_SOF2 : FB_R_SOF1;

        PixelBuffer<u32> pb;
        pb.init(width, height);
        u8 *dst = (u8 *)pb.data();

        switch (FB_R_CTRL.fb_depth)
        {
        case fbde_0555: // 555 RGB
            for (int y = 0; y < height; y++)
            {
                for (int i = 0; i < width; i++)
                {
                    u16 src = pvr_read_area1_16(sh4_cpu->vram.data, addr);
                    *dst++ = (((src >> 10) & 0x1F) << 3) + FB_R_CTRL.fb_concat;
                    *dst++ = (((src >> 5) & 0x1F) << 3) + FB_R_CTRL.fb_concat;
                    *dst++ = (((src >> 0) & 0x1F) << 3) + FB_R_CTRL.fb_concat;
                    *dst++ = 0xFF;
                    addr += bpp;
                }
                addr += modulus * bpp;
            }
            break;

        case fbde_565: // 565 RGB
            for (int y = 0; y < height; y++)
            {
                for (int i = 0; i < width; i++)
                {
                    u16 src = pvr_read_area1_16(sh4_cpu->vram.data, addr);
                    *dst++ = (((src >> 11) & 0x1F) << 3) + FB_R_CTRL.fb_concat;
                    *dst++ = (((src >> 5) & 0x3F) << 2) + (FB_R_CTRL.fb_concat >> 1);
                    *dst++ = (((src >> 0) & 0x1F) << 3) + FB_R_CTRL.fb_concat;
                    *dst++ = 0xFF;
                    addr += bpp;
                }
                addr += modulus * bpp;
            }
            break;
        case fbde_888: // 888 RGB
            for (int y = 0; y < height; y++)
            {
                for (int i = 0; i < width; i++)
                {
                    if (addr & 1)
                    {
                        u32 src = pvr_read_area1_32(sh4_cpu->vram.data, addr - 1);
                        *dst++ = src >> 16;
                        *dst++ = src >> 8;
                        *dst++ = src;
                    }
                    else
                    {
                        u32 src = pvr_read_area1_32(sh4_cpu->vram.data, addr);
                        *dst++ = src >> 24;
                        *dst++ = src >> 16;
                        *dst++ = src >> 8;
                    }
                    *dst++ = 0xFF;
                    addr += bpp;
                }
                addr += modulus * bpp;
            }
            break;
        case fbde_C888: // 0888 RGB
            for (int y = 0; y < height; y++)
            {
                for (int i = 0; i < width; i++)
                {
                    u32 src = pvr_read_area1_32(sh4_cpu->vram.data, addr);
                    *dst++ = src >> 16;
                    *dst++ = src >> 8;
                    *dst++ = src;
                    *dst++ = 0xFF;
                    addr += bpp;
                }
                addr += modulus * bpp;
            }
            break;
        }
        u32 *psrc = pb.data();

#if HOST_OS == OS_WINDOWS
        SetDIBits(hmem, hBMP, 0, 480, psrc, (BITMAPINFO*)& bi, DIB_RGB_COLORS);

        RECT clientRect;

        GetClientRect(hWnd, &clientRect);

        HDC hdc = GetDC(hWnd);
        int w = clientRect.right - clientRect.left;
        int h = clientRect.bottom - clientRect.top;
        int x = (w - 640) / 2;
        int y = (h - 480) / 2;

        StretchBlt(hdc, x, y+480, 640, -480, hmem, 0, 0, 640, 480, SRCCOPY);
        ReleaseDC(hWnd, hdc);
#elif defined(SUPPORT_X11)
        extern Window x11_win;
        extern Display* x11_disp;
        extern Visual* x11_vis;

        extern int x11_width;
        extern int x11_height;

        XImage* ximage = XCreateImage(x11_disp, x11_vis, 24, ZPixmap, 0, (char*)psrc, width, height, 32, width * 4);

        GC gc = XCreateGC(x11_disp, x11_win, 0, 0);
        XPutImage(x11_disp, x11_win, gc, ximage, 0, 0, (x11_width - width) / 2, (x11_height - height) / 2, width, height);
        XFree(ximage);
        XFreeGC(x11_disp, gc);
#else
        // TODO softrend without X11 (SDL f.e.)
    die("Softrend doesn't know how to update the screen");
#endif
    }

    void operator delete(void* p) {
        _mm_free(p);
    }
};

Renderer* rend_refrend(u8* vram) {
    return new(_mm_malloc(sizeof(refrend), 32)) ::refrend(vram);
}

static auto refrend = RegisterRendererBackend(rendererbackend_t{ "refsw", "RefSW", 0, rend_refrend });
