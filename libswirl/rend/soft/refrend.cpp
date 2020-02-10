/*
	This file is part of libswirl
*/
#include "license/bsd"


#include <omp.h>
#include "hw/pvr/Renderer_if.h"
#include "hw/pvr/pvr_mem.h"
#include "oslib/oslib.h"

/*
    SSE/MMX based softrend

    Initial code by skmp and gigaherz

    This is a rather weird very basic pvr softrend.
    Renders	in some kind of tile format (that I forget now),
    and does depth and color, but no alpha, texture, or pixel
    processing. All of the pipeline is based on quads.
*/

#include <mmintrin.h>
#include <xmmintrin.h>
#include <emmintrin.h>
#include <smmintrin.h>

#include <cmath>

#include "rend/gles/gles.h"
#include "gui/gui.h"

extern u32 decoded_colors[3][65536];

#define MAX_RENDER_WIDTH 640
#define MAX_RENDER_HEIGHT 480
#define MAX_RENDER_PIXELS (MAX_RENDER_WIDTH * MAX_RENDER_HEIGHT)

#define STRIDE_PIXEL_OFFSET MAX_RENDER_WIDTH
#define Z_BUFFER_PIXEL_OFFSET MAX_RENDER_PIXELS

static DECL_ALIGN(32) u32 render_buffer[MAX_RENDER_PIXELS * 2]; //Color + depth

#if HOST_OS != OS_WINDOWS

 union RegionArrayEntryControl {
        struct {
            u32 res0:2;
            u32 tilex:6;
            u32 tiley:6;
            u32 res1:14;
            u32 no_writeout:1;
            u32 pre_sort:1;
            u32 z_keep:1;
            u32 last_region:1;
        };
        u32 full;
    };

    typedef u32 pvr32addr_t;

    union ListPointer {
      struct
      {
          u32 pad0:2;
          u32 ptr_in_words: 22;
          u32 pad1:7;
          u32 empty:1;
      };
      u32 full;
    };
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
    };

    union ObjectListEntry {
        struct {
            u32 pad0:31;
            u32 is_not_triangle_strip:1;
        };

        struct {
            u32 pad1:29;
            u32 type: 3;
        };

        struct {
            u32 param_offs_in_words: 21;
            u32 skip: 3;
            u32 shadow: 1;
            u32 mask: 6;
        } tstrip;

        struct {
            u32 param_offs_in_words: 21;
            u32 skip: 3;
            u32 shadow: 1;
            u32 prims: 4;
        } tarray;

        struct {
            u32 param_offs_in_words: 21;
            u32 skip: 3;
            u32 shadow: 1;
            u32 prims: 4;
        } qarray;

        struct {
            u32 pad3: 2;
            u32 next_block_ptr_in_words: 22;
            u32 pad4: 4;
            u32 end_of_list: 1;
        } link;

        u32 full;
    };

struct RECT {
    int left, top, right, bottom;
};

#include     <X11/Xlib.h>
#endif

union mem128i {
    int8_t m128i_u8[16];
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

struct IPs3
{
    PlaneStepper3 Z;
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

        Z.Setup(v1, v2, v3, v1.z, v2.z, v3.z);
        U.Setup(v1, v2, v3, v1.u * w * v1.z, v2.u * w * v2.z, v3.u * w * v3.z);
        V.Setup(v1, v2, v3, v1.v * h * v1.z, v2.v * h * v2.z, v3.v * h * v3.z);
        
        for (int i = 0; i < 4; i++)
            Col[i].Setup(v1, v2, v3, v1.col[i], v2.col[i], v3.col[i]);

        for (int i = 0; i < 4; i++)
            Ofs[i].Setup(v1, v2, v3, v1.spc[i], v2.spc[i], v3.spc[i]);
    }
};


#define TPL_DECL_pixel template<int alpha_mode, bool pp_UseAlpha, bool pp_Texture, bool pp_IgnoreTexA, int pp_ShadInstr, bool pp_Offset >
#define TPL_DECL_triangle template<int alpha_mode, bool pp_UseAlpha, bool pp_Texture, bool pp_IgnoreTexA, int pp_ShadInstr, bool pp_Offset >

#define TPL_PRMS_pixel <alpha_mode, pp_UseAlpha, pp_Texture, pp_IgnoreTexA, pp_ShadInstr, pp_Offset >
#define TPL_PRMS_triangle <alpha_mode, pp_UseAlpha, pp_Texture, pp_IgnoreTexA, pp_ShadInstr, pp_Offset >

struct refrend;

//<alpha_blend, pp_UseAlpha, pp_Texture, pp_IgnoreTexA, pp_ShadInstr, pp_Offset >
typedef void(refrend::*RendtriangleFn)(DrawParameters* params, int vertex_offset, const Vertex& v1, const Vertex& v2, const Vertex& v3, u32* colorBuffer, RECT* area);
static RendtriangleFn RendtriangleFns[3][2][2][2][4][2];

TPL_DECL_pixel
static void PixelFlush(text_info* texture, float x, float y, u8* cb, IPs3& ip)
{
    float invW = ip.Z.Ip(x, y);
    float u = ip.U.Ip(x, y);
    float v = ip.V.Ip(x, y);

    u = u / invW;
    v = v / invW;

    float* zb = (float*)&cb[Z_BUFFER_PIXEL_OFFSET * 4];

    // Z test
    if (invW < * zb)
        return;

    u8 rv[4];
    {
        rv[0] = ip.Col[2].Ip(x, y);
        rv[1] = ip.Col[1].Ip(x, y);
        rv[2] = ip.Col[0].Ip(x, y);
        rv[3] = ip.Col[3].Ip(x, y);

        if (!pp_UseAlpha) {
            rv[3] = 255;
        }

        if (pp_Texture) {

            int ui = u * 256;
            int vi = v * 256;
            mem128i px = ((mem128i*)texture->pdata)[((ui >> 8) % texture->width + (vi >> 8) % texture->height * texture->width)];

            int ublend = ui & 255;
            int vblend =  vi & 255;
            int nublend = 255 - ublend;
            int nvblend = 255 - vblend;

            u8 textel[4];

            for (int i = 0 ; i < 4; i++)
            {
                textel[i] =
                    (px.m128i_u8[0 + i] * ublend * vblend) / 65536 +
                    (px.m128i_u8[4 + i] * nublend * vblend) / 65536 +
                    (px.m128i_u8[8 + i] * ublend * nvblend) / 65536 +
                    (px.m128i_u8[12 + i] * nublend * nvblend) / 65536;
            };

            if (pp_IgnoreTexA) {
                textel[3] = 255;
            }

            if (pp_ShadInstr == 0) {
                //color.rgb = texcol.rgb;
                //color.a = texcol.a;

                memcpy(rv, textel, sizeof(rv));
            }
            else if (pp_ShadInstr == 1) {
                //color.rgb *= texcol.rgb;
                //color.a = texcol.a;
                for (int i = 0; i < 3; i++)
                {
                    rv[i] = textel[i] * rv[i] / 256;
                }

                rv[3] = textel[3];
            }
            else if (pp_ShadInstr == 2) {
                //color.rgb=mix(color.rgb,texcol.rgb,texcol.a);
                u8 tb = textel[3];
                u8 cb = 255 - tb;

                for (int i = 0; i < 3; i++)
                {
                    rv[i] = (textel[i] * tb + rv[i] * cb) / 256;
                }

                //rv[3] is not affected
            }
            else if (pp_ShadInstr == 3) {
                //color*=texcol
                for (int i = 0; i < 4; i++)
                {
                    rv[i] = textel[i] * rv[i] / 256;
                }
            }

            if (pp_Offset) {
                //add offset

                rv[0] += ip.Ofs[2].Ip(x, y);
                rv[1] += ip.Ofs[1].Ip(x, y);
                rv[2] += ip.Ofs[0].Ip(x, y);
                rv[3] += ip.Ofs[3].Ip(x, y);
            }
        }
    }

    if (alpha_mode == 1) {
        if (rv[3] < PT_ALPHA_REF) {
            *zb = invW;
            memcpy(cb, rv, 4);
        }
    }
    else if (alpha_mode == 2) {
        u8* fb = (u8*)cb;
        u8 src_blend = rv[3];
        u8 dst_blend = 255 - rv[3];
        for (int j = 0; j < 3; j++) {
            rv[j] = (rv[j] * src_blend) / 256 + (fb[j] * dst_blend) / 256;
        }

        memcpy(cb, rv, 4);
    }
    else
    {
        *zb = invW;
        memcpy(cb, rv, 4);
    }
}


#if HOST_OS == OS_WINDOWS
static BITMAPINFOHEADER bi = { sizeof(BITMAPINFOHEADER), 0, 0, 1, 32, BI_RGB };
#endif

bool gles_init();

struct refrend : Renderer
{
    u8* vram;

    refrend(u8* vram) : vram(vram) { }

    //u32 nok,fok;
    TPL_DECL_triangle
    void Rendtriangle(DrawParameters* params, int vertex_offset, const Vertex& v1, const Vertex& v2, const Vertex& v3, u32* colorBuffer, RECT* area)
    {
        text_info texture = { 0 };

        if (pp_Texture) {
            texture = raw_GetTexture(vram, params->tsp, params->tcw);
        }

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

        // Deltas
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

        const float DX12 = sgn * (X1 - X2);
        const float DX23 = sgn * (X2 - X3);
        const float DX31 = sgn * (X3 - X1);

        const float DY12 = sgn * (Y1 - Y2);
        const float DY23 = sgn * (Y2 - Y3);
        const float DY31 = sgn * (Y3 - Y1);

        // Bounding rectangle
        int minx = iround(mmin(X1, X2, X3, area->left));
        int miny = iround(mmin(Y1, Y2, Y3, area->top));

        int spanx = iround(mmax(X1, X2, X3, area->right - 1)) - minx + 1;
        int spany = iround(mmax(Y1, Y2, Y3, area->bottom - 1)) - miny + 1;

        //Inside scissor area?
        if (spanx < 0 || spany < 0)
            return;


        // Half-edge constants
        float C1 = DY12 * X1 - DX12 * Y1;
        float C2 = DY23 * X2 - DX23 * Y2;
        float C3 = DY31 * X3 - DX31 * Y3;


        float hs12 = C1 + DX12 * miny - DY12 * minx;
        float hs23 = C2 + DX23 * miny - DY23 * minx;
        float hs31 = C3 + DX31 * miny - DY31 * minx;


        u8* cb_y = (u8*)colorBuffer;
        cb_y += miny * stride_bytes + minx * 4;

        DECL_ALIGN(64) IPs3 ip;

        ip.Setup(&texture, v1, v2, v3);


        float y_ps = miny;
        float minx_ps = minx;

        // Loop through blocks
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

                // Corners of block
                bool inTriangle = EvalHalfSpaceAll(Xhs12, Xhs23, Xhs31);

                // Skip block when outside an edge
                if (inTriangle)
                {
                    PixelFlush TPL_PRMS_pixel(&texture, x_ps, y_ps, cb_x, ip);
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

    void SetFBScale(float x, float y)
    {
        fb_scale_x = x;
        fb_scale_y = y;
    }

    virtual bool Process(TA_context* ctx) {

        // no-op for now
        return true;
    }


    virtual bool RenderFramebuffer() {
        Present();
        return false;
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
    //used for bg poly
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

    void decode_pvr_vetrices(DrawParameters* params, pvr32addr_t base, u32 skip, u32 shadow, Vertex* vtx, int count)
    {
        bool PSVM=FPU_SHAD_SCALE.intensity_shadow!=0;

        if (!PSVM) {
            shadow = 0; // no double volume stuff
        }

        params->isp.full=vri(vram, base);
        params->tsp.full=vri(vram, base+4);
        params->tcw.full=vri(vram, base+8);

        base += 12;

        for (int i = 0; i < count; i++) {
            decode_pvr_vertex(params,base, &vtx[i]);
            base += (3 + skip * (shadow+1)) * 4;
        }
    }

    

    template<int alpha_mode>
    void DrawTriangle(DrawParameters* params, int vertex_offset, Vertex* vtx, RECT* area)
    {
        RendtriangleFn fn = RendtriangleFns[alpha_mode][params->tsp.UseAlpha][params->isp.Texture][params->tsp.IgnoreTexA][params->tsp.ShadInstr][params->isp.Offset];

        (this->*fn)(params, vertex_offset, vtx[0], vtx[1], vtx[2], render_buffer, area);
    }

    template<int alpha_mode>
    void RenderTriangleStrip(ObjectListEntry obj, RECT* rect)
    {
        Vertex vtx[8];
        DrawParameters params;

        u32 param_base = PARAM_BASE & 0xF00000;

        decode_pvr_vetrices(&params, param_base + obj.tstrip.param_offs_in_words * 4, obj.tstrip.skip, obj.tstrip.shadow, vtx, 8);
        
        if (obj.tstrip.mask & (1<<5)) {
            DrawTriangle<alpha_mode>(&params, 0, &vtx[0], rect);
        }

        if (obj.tstrip.mask & (1<<4)) {
            DrawTriangle<alpha_mode>(&params, 1, &vtx[1], rect);
        }

        if (obj.tstrip.mask & (1<<3)) {
            DrawTriangle<alpha_mode>(&params, 0, &vtx[2], rect);
        }

        if (obj.tstrip.mask & (1<<2)) {
            DrawTriangle<alpha_mode>(&params, 1, &vtx[3], rect);
        }

        if (obj.tstrip.mask & (1<<1)) {
            DrawTriangle<alpha_mode>(&params, 0, &vtx[4], rect);
        }

        if (obj.tstrip.mask & (1<<0)) {
            DrawTriangle<alpha_mode>(&params, 1, &vtx[5], rect);
        }
    }


    template<int alpha_mode>
    void RenderTriangleArray(ObjectListEntry obj, RECT* rect)
    {
        auto triangles = obj.tarray.prims + 1;
        auto vertices = triangles * 3;

        DrawParameters params;
        Vertex vtx[vertices];

        u32 param_base = PARAM_BASE & 0xF00000;

        decode_pvr_vetrices(&params, param_base + obj.tarray.param_offs_in_words * 4, obj.tarray.skip, obj.tarray.shadow, vtx, vertices);
        
        for (int i = 0; i < triangles; i++) {
            DrawTriangle<alpha_mode>(&params, 0, &vtx[i*3], rect);
        }
    }

    template<int alpha_mode>
    void DrawQuad(DrawParameters* params, Vertex* vtx, RECT* area)
    {
        //TODO: implement this
    }

    template<int alpha_mode>
    void RenderQuadArray(ObjectListEntry obj, RECT* rect)
    {
        //TODO: implement this
    }

    template<int alpha_mode>
    void RenderObjectList(pvr32addr_t base, RECT* rect)
    {
        ObjectListEntry obj;

        for (;;) {
            obj.full = vri(vram, base);
            base += 4;

            if (!obj.is_not_triangle_strip) {
                RenderTriangleStrip<alpha_mode>(obj, rect);
            } else {
                switch(obj.type) {
                    case 0b111: // link
                        if (obj.link.end_of_list)
                            return;

                            base = obj.link.next_block_ptr_in_words * 4;
                        break;

                    case 0b100: // triangle array
                        RenderTriangleArray<alpha_mode>(obj, rect);
                        break;
                    
                    case 0b101: // quad array
                        RenderQuadArray<alpha_mode>(obj, rect);
                        break;

                    default:
                        printf("RenderObjectList: Not handled object type: %d\n", obj.type);
                }
            }
        }
    }

    virtual bool RenderPVR() {

        u32 base = REGION_BASE;

        RegionArrayEntry entry;
        int tilenum = 1;

        //printf("New Frame\n");
        do {
            base += ReadRegionArrayEntry(base, &entry);
/*          
            printf("Tile: %d at %dx%d\n", tilenum++, entry.control.tilex*32, entry.control.tiley*32);
            printf("CTRL: %08X\n", entry.control.full);
            printf("OPAQ: %08X\n", entry.opaque.full);
            printf("OPAQMOD: %08X\n", entry.opaque_mod.full);
            printf("TRANS: %08X\n", entry.trans.full);
            printf("TRANSMOD: %08X\n", entry.trans_mod.full);
            printf("PT: %08X\n", entry.puncht.full);
*/


#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MIN(x,y) ((x) < (y) ? (x) : (y))

            RECT rect;
            rect.top = MAX(entry.control.tiley * 32, FB_Y_CLIP.min);
            rect.left = MAX(entry.control.tilex * 32, FB_X_CLIP.min);
            
            rect.bottom = MIN(rect.top + 32, FB_Y_CLIP.max);
            rect.right = MIN(rect.left + 32, FB_X_CLIP.max);

            if (!entry.control.z_keep) {
                //render bg quad
            }
            
            if (!entry.opaque.empty) {
                RenderObjectList<0>(entry.opaque.ptr_in_words * 4, &rect);
            }

            if (!entry.puncht.empty) {
                RenderObjectList<1>(entry.puncht.ptr_in_words * 4, &rect);
            }

            if (!entry.trans.empty) {
                RenderObjectList<2>(entry.trans.ptr_in_words * 4, &rect);
            }

            if (!entry.control.no_writeout) {
                // copy to vram
            }

        } while (!entry.control.last_region);

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

#define Rendtriangle refrend::Rendtriangle
        {
            RendtriangleFns[0][0][1][0][0][0] = &Rendtriangle<0, 0, 1, 0, 0, 0>;
            RendtriangleFns[0][0][1][0][0][1] = &Rendtriangle<0, 0, 1, 0, 0, 1>;
            RendtriangleFns[0][0][1][0][1][0] = &Rendtriangle<0, 0, 1, 0, 1, 0>;
            RendtriangleFns[0][0][1][0][1][1] = &Rendtriangle<0, 0, 1, 0, 1, 1>;
            RendtriangleFns[0][0][1][0][2][0] = &Rendtriangle<0, 0, 1, 0, 2, 0>;
            RendtriangleFns[0][0][1][0][2][1] = &Rendtriangle<0, 0, 1, 0, 2, 1>;
            RendtriangleFns[0][0][1][0][3][0] = &Rendtriangle<0, 0, 1, 0, 3, 0>;
            RendtriangleFns[0][0][1][0][3][1] = &Rendtriangle<0, 0, 1, 0, 3, 1>;
            RendtriangleFns[0][0][1][1][0][0] = &Rendtriangle<0, 0, 1, 1, 0, 0>;
            RendtriangleFns[0][0][1][1][0][1] = &Rendtriangle<0, 0, 1, 1, 0, 1>;
            RendtriangleFns[0][0][1][1][1][0] = &Rendtriangle<0, 0, 1, 1, 1, 0>;
            RendtriangleFns[0][0][1][1][1][1] = &Rendtriangle<0, 0, 1, 1, 1, 1>;
            RendtriangleFns[0][0][1][1][2][0] = &Rendtriangle<0, 0, 1, 1, 2, 0>;
            RendtriangleFns[0][0][1][1][2][1] = &Rendtriangle<0, 0, 1, 1, 2, 1>;
            RendtriangleFns[0][0][1][1][3][0] = &Rendtriangle<0, 0, 1, 1, 3, 0>;
            RendtriangleFns[0][0][1][1][3][1] = &Rendtriangle<0, 0, 1, 1, 3, 1>;
            RendtriangleFns[0][0][0][0][0][0] = &Rendtriangle<0, 0, 0, 0, 0, 0>;
            RendtriangleFns[0][0][0][0][0][1] = &Rendtriangle<0, 0, 0, 0, 0, 1>;
            RendtriangleFns[0][0][0][0][1][0] = &Rendtriangle<0, 0, 0, 0, 1, 0>;
            RendtriangleFns[0][0][0][0][1][1] = &Rendtriangle<0, 0, 0, 0, 1, 1>;
            RendtriangleFns[0][0][0][0][2][0] = &Rendtriangle<0, 0, 0, 0, 2, 0>;
            RendtriangleFns[0][0][0][0][2][1] = &Rendtriangle<0, 0, 0, 0, 2, 1>;
            RendtriangleFns[0][0][0][0][3][0] = &Rendtriangle<0, 0, 0, 0, 3, 0>;
            RendtriangleFns[0][0][0][0][3][1] = &Rendtriangle<0, 0, 0, 0, 3, 1>;
            RendtriangleFns[0][0][0][1][0][0] = &Rendtriangle<0, 0, 0, 1, 0, 0>;
            RendtriangleFns[0][0][0][1][0][1] = &Rendtriangle<0, 0, 0, 1, 0, 1>;
            RendtriangleFns[0][0][0][1][1][0] = &Rendtriangle<0, 0, 0, 1, 1, 0>;
            RendtriangleFns[0][0][0][1][1][1] = &Rendtriangle<0, 0, 0, 1, 1, 1>;
            RendtriangleFns[0][0][0][1][2][0] = &Rendtriangle<0, 0, 0, 1, 2, 0>;
            RendtriangleFns[0][0][0][1][2][1] = &Rendtriangle<0, 0, 0, 1, 2, 1>;
            RendtriangleFns[0][0][0][1][3][0] = &Rendtriangle<0, 0, 0, 1, 3, 0>;
            RendtriangleFns[0][0][0][1][3][1] = &Rendtriangle<0, 0, 0, 1, 3, 1>;
            RendtriangleFns[0][1][1][0][0][0] = &Rendtriangle<0, 1, 1, 0, 0, 0>;
            RendtriangleFns[0][1][1][0][0][1] = &Rendtriangle<0, 1, 1, 0, 0, 1>;
            RendtriangleFns[0][1][1][0][1][0] = &Rendtriangle<0, 1, 1, 0, 1, 0>;
            RendtriangleFns[0][1][1][0][1][1] = &Rendtriangle<0, 1, 1, 0, 1, 1>;
            RendtriangleFns[0][1][1][0][2][0] = &Rendtriangle<0, 1, 1, 0, 2, 0>;
            RendtriangleFns[0][1][1][0][2][1] = &Rendtriangle<0, 1, 1, 0, 2, 1>;
            RendtriangleFns[0][1][1][0][3][0] = &Rendtriangle<0, 1, 1, 0, 3, 0>;
            RendtriangleFns[0][1][1][0][3][1] = &Rendtriangle<0, 1, 1, 0, 3, 1>;
            RendtriangleFns[0][1][1][1][0][0] = &Rendtriangle<0, 1, 1, 1, 0, 0>;
            RendtriangleFns[0][1][1][1][0][1] = &Rendtriangle<0, 1, 1, 1, 0, 1>;
            RendtriangleFns[0][1][1][1][1][0] = &Rendtriangle<0, 1, 1, 1, 1, 0>;
            RendtriangleFns[0][1][1][1][1][1] = &Rendtriangle<0, 1, 1, 1, 1, 1>;
            RendtriangleFns[0][1][1][1][2][0] = &Rendtriangle<0, 1, 1, 1, 2, 0>;
            RendtriangleFns[0][1][1][1][2][1] = &Rendtriangle<0, 1, 1, 1, 2, 1>;
            RendtriangleFns[0][1][1][1][3][0] = &Rendtriangle<0, 1, 1, 1, 3, 0>;
            RendtriangleFns[0][1][1][1][3][1] = &Rendtriangle<0, 1, 1, 1, 3, 1>;
            RendtriangleFns[0][1][0][0][0][0] = &Rendtriangle<0, 1, 0, 0, 0, 0>;
            RendtriangleFns[0][1][0][0][0][1] = &Rendtriangle<0, 1, 0, 0, 0, 1>;
            RendtriangleFns[0][1][0][0][1][0] = &Rendtriangle<0, 1, 0, 0, 1, 0>;
            RendtriangleFns[0][1][0][0][1][1] = &Rendtriangle<0, 1, 0, 0, 1, 1>;
            RendtriangleFns[0][1][0][0][2][0] = &Rendtriangle<0, 1, 0, 0, 2, 0>;
            RendtriangleFns[0][1][0][0][2][1] = &Rendtriangle<0, 1, 0, 0, 2, 1>;
            RendtriangleFns[0][1][0][0][3][0] = &Rendtriangle<0, 1, 0, 0, 3, 0>;
            RendtriangleFns[0][1][0][0][3][1] = &Rendtriangle<0, 1, 0, 0, 3, 1>;
            RendtriangleFns[0][1][0][1][0][0] = &Rendtriangle<0, 1, 0, 1, 0, 0>;
            RendtriangleFns[0][1][0][1][0][1] = &Rendtriangle<0, 1, 0, 1, 0, 1>;
            RendtriangleFns[0][1][0][1][1][0] = &Rendtriangle<0, 1, 0, 1, 1, 0>;
            RendtriangleFns[0][1][0][1][1][1] = &Rendtriangle<0, 1, 0, 1, 1, 1>;
            RendtriangleFns[0][1][0][1][2][0] = &Rendtriangle<0, 1, 0, 1, 2, 0>;
            RendtriangleFns[0][1][0][1][2][1] = &Rendtriangle<0, 1, 0, 1, 2, 1>;
            RendtriangleFns[0][1][0][1][3][0] = &Rendtriangle<0, 1, 0, 1, 3, 0>;
            RendtriangleFns[0][1][0][1][3][1] = &Rendtriangle<0, 1, 0, 1, 3, 1>;
            RendtriangleFns[1][0][1][0][0][0] = &Rendtriangle<1, 0, 1, 0, 0, 0>;
            RendtriangleFns[1][0][1][0][0][1] = &Rendtriangle<1, 0, 1, 0, 0, 1>;
            RendtriangleFns[1][0][1][0][1][0] = &Rendtriangle<1, 0, 1, 0, 1, 0>;
            RendtriangleFns[1][0][1][0][1][1] = &Rendtriangle<1, 0, 1, 0, 1, 1>;
            RendtriangleFns[1][0][1][0][2][0] = &Rendtriangle<1, 0, 1, 0, 2, 0>;
            RendtriangleFns[1][0][1][0][2][1] = &Rendtriangle<1, 0, 1, 0, 2, 1>;
            RendtriangleFns[1][0][1][0][3][0] = &Rendtriangle<1, 0, 1, 0, 3, 0>;
            RendtriangleFns[1][0][1][0][3][1] = &Rendtriangle<1, 0, 1, 0, 3, 1>;
            RendtriangleFns[1][0][1][1][0][0] = &Rendtriangle<1, 0, 1, 1, 0, 0>;
            RendtriangleFns[1][0][1][1][0][1] = &Rendtriangle<1, 0, 1, 1, 0, 1>;
            RendtriangleFns[1][0][1][1][1][0] = &Rendtriangle<1, 0, 1, 1, 1, 0>;
            RendtriangleFns[1][0][1][1][1][1] = &Rendtriangle<1, 0, 1, 1, 1, 1>;
            RendtriangleFns[1][0][1][1][2][0] = &Rendtriangle<1, 0, 1, 1, 2, 0>;
            RendtriangleFns[1][0][1][1][2][1] = &Rendtriangle<1, 0, 1, 1, 2, 1>;
            RendtriangleFns[1][0][1][1][3][0] = &Rendtriangle<1, 0, 1, 1, 3, 0>;
            RendtriangleFns[1][0][1][1][3][1] = &Rendtriangle<1, 0, 1, 1, 3, 1>;
            RendtriangleFns[1][0][0][0][0][0] = &Rendtriangle<1, 0, 0, 0, 0, 0>;
            RendtriangleFns[1][0][0][0][0][1] = &Rendtriangle<1, 0, 0, 0, 0, 1>;
            RendtriangleFns[1][0][0][0][1][0] = &Rendtriangle<1, 0, 0, 0, 1, 0>;
            RendtriangleFns[1][0][0][0][1][1] = &Rendtriangle<1, 0, 0, 0, 1, 1>;
            RendtriangleFns[1][0][0][0][2][0] = &Rendtriangle<1, 0, 0, 0, 2, 0>;
            RendtriangleFns[1][0][0][0][2][1] = &Rendtriangle<1, 0, 0, 0, 2, 1>;
            RendtriangleFns[1][0][0][0][3][0] = &Rendtriangle<1, 0, 0, 0, 3, 0>;
            RendtriangleFns[1][0][0][0][3][1] = &Rendtriangle<1, 0, 0, 0, 3, 1>;
            RendtriangleFns[1][0][0][1][0][0] = &Rendtriangle<1, 0, 0, 1, 0, 0>;
            RendtriangleFns[1][0][0][1][0][1] = &Rendtriangle<1, 0, 0, 1, 0, 1>;
            RendtriangleFns[1][0][0][1][1][0] = &Rendtriangle<1, 0, 0, 1, 1, 0>;
            RendtriangleFns[1][0][0][1][1][1] = &Rendtriangle<1, 0, 0, 1, 1, 1>;
            RendtriangleFns[1][0][0][1][2][0] = &Rendtriangle<1, 0, 0, 1, 2, 0>;
            RendtriangleFns[1][0][0][1][2][1] = &Rendtriangle<1, 0, 0, 1, 2, 1>;
            RendtriangleFns[1][0][0][1][3][0] = &Rendtriangle<1, 0, 0, 1, 3, 0>;
            RendtriangleFns[1][0][0][1][3][1] = &Rendtriangle<1, 0, 0, 1, 3, 1>;
            RendtriangleFns[1][1][1][0][0][0] = &Rendtriangle<1, 1, 1, 0, 0, 0>;
            RendtriangleFns[1][1][1][0][0][1] = &Rendtriangle<1, 1, 1, 0, 0, 1>;
            RendtriangleFns[1][1][1][0][1][0] = &Rendtriangle<1, 1, 1, 0, 1, 0>;
            RendtriangleFns[1][1][1][0][1][1] = &Rendtriangle<1, 1, 1, 0, 1, 1>;
            RendtriangleFns[1][1][1][0][2][0] = &Rendtriangle<1, 1, 1, 0, 2, 0>;
            RendtriangleFns[1][1][1][0][2][1] = &Rendtriangle<1, 1, 1, 0, 2, 1>;
            RendtriangleFns[1][1][1][0][3][0] = &Rendtriangle<1, 1, 1, 0, 3, 0>;
            RendtriangleFns[1][1][1][0][3][1] = &Rendtriangle<1, 1, 1, 0, 3, 1>;
            RendtriangleFns[1][1][1][1][0][0] = &Rendtriangle<1, 1, 1, 1, 0, 0>;
            RendtriangleFns[1][1][1][1][0][1] = &Rendtriangle<1, 1, 1, 1, 0, 1>;
            RendtriangleFns[1][1][1][1][1][0] = &Rendtriangle<1, 1, 1, 1, 1, 0>;
            RendtriangleFns[1][1][1][1][1][1] = &Rendtriangle<1, 1, 1, 1, 1, 1>;
            RendtriangleFns[1][1][1][1][2][0] = &Rendtriangle<1, 1, 1, 1, 2, 0>;
            RendtriangleFns[1][1][1][1][2][1] = &Rendtriangle<1, 1, 1, 1, 2, 1>;
            RendtriangleFns[1][1][1][1][3][0] = &Rendtriangle<1, 1, 1, 1, 3, 0>;
            RendtriangleFns[1][1][1][1][3][1] = &Rendtriangle<1, 1, 1, 1, 3, 1>;
            RendtriangleFns[1][1][0][0][0][0] = &Rendtriangle<1, 1, 0, 0, 0, 0>;
            RendtriangleFns[1][1][0][0][0][1] = &Rendtriangle<1, 1, 0, 0, 0, 1>;
            RendtriangleFns[1][1][0][0][1][0] = &Rendtriangle<1, 1, 0, 0, 1, 0>;
            RendtriangleFns[1][1][0][0][1][1] = &Rendtriangle<1, 1, 0, 0, 1, 1>;
            RendtriangleFns[1][1][0][0][2][0] = &Rendtriangle<1, 1, 0, 0, 2, 0>;
            RendtriangleFns[1][1][0][0][2][1] = &Rendtriangle<1, 1, 0, 0, 2, 1>;
            RendtriangleFns[1][1][0][0][3][0] = &Rendtriangle<1, 1, 0, 0, 3, 0>;
            RendtriangleFns[1][1][0][0][3][1] = &Rendtriangle<1, 1, 0, 0, 3, 1>;
            RendtriangleFns[1][1][0][1][0][0] = &Rendtriangle<1, 1, 0, 1, 0, 0>;
            RendtriangleFns[1][1][0][1][0][1] = &Rendtriangle<1, 1, 0, 1, 0, 1>;
            RendtriangleFns[1][1][0][1][1][0] = &Rendtriangle<1, 1, 0, 1, 1, 0>;
            RendtriangleFns[1][1][0][1][1][1] = &Rendtriangle<1, 1, 0, 1, 1, 1>;
            RendtriangleFns[1][1][0][1][2][0] = &Rendtriangle<1, 1, 0, 1, 2, 0>;
            RendtriangleFns[1][1][0][1][2][1] = &Rendtriangle<1, 1, 0, 1, 2, 1>;
            RendtriangleFns[1][1][0][1][3][0] = &Rendtriangle<1, 1, 0, 1, 3, 0>;
            RendtriangleFns[1][1][0][1][3][1] = &Rendtriangle<1, 1, 0, 1, 3, 1>;


            RendtriangleFns[2][0][1][0][0][0] = &Rendtriangle<2, 0, 1, 0, 0, 0>;
            RendtriangleFns[2][0][1][0][0][1] = &Rendtriangle<2, 0, 1, 0, 0, 1>;
            RendtriangleFns[2][0][1][0][1][0] = &Rendtriangle<2, 0, 1, 0, 1, 0>;
            RendtriangleFns[2][0][1][0][1][1] = &Rendtriangle<2, 0, 1, 0, 1, 1>;
            RendtriangleFns[2][0][1][0][2][0] = &Rendtriangle<2, 0, 1, 0, 2, 0>;
            RendtriangleFns[2][0][1][0][2][1] = &Rendtriangle<2, 0, 1, 0, 2, 1>;
            RendtriangleFns[2][0][1][0][3][0] = &Rendtriangle<2, 0, 1, 0, 3, 0>;
            RendtriangleFns[2][0][1][0][3][1] = &Rendtriangle<2, 0, 1, 0, 3, 1>;
            RendtriangleFns[2][0][1][1][0][0] = &Rendtriangle<2, 0, 1, 1, 0, 0>;
            RendtriangleFns[2][0][1][1][0][1] = &Rendtriangle<2, 0, 1, 1, 0, 1>;
            RendtriangleFns[2][0][1][1][1][0] = &Rendtriangle<2, 0, 1, 1, 1, 0>;
            RendtriangleFns[2][0][1][1][1][1] = &Rendtriangle<2, 0, 1, 1, 1, 1>;
            RendtriangleFns[2][0][1][1][2][0] = &Rendtriangle<2, 0, 1, 1, 2, 0>;
            RendtriangleFns[2][0][1][1][2][1] = &Rendtriangle<2, 0, 1, 1, 2, 1>;
            RendtriangleFns[2][0][1][1][3][0] = &Rendtriangle<2, 0, 1, 1, 3, 0>;
            RendtriangleFns[2][0][1][1][3][1] = &Rendtriangle<2, 0, 1, 1, 3, 1>;
            RendtriangleFns[2][0][0][0][0][0] = &Rendtriangle<2, 0, 0, 0, 0, 0>;
            RendtriangleFns[2][0][0][0][0][1] = &Rendtriangle<2, 0, 0, 0, 0, 1>;
            RendtriangleFns[2][0][0][0][1][0] = &Rendtriangle<2, 0, 0, 0, 1, 0>;
            RendtriangleFns[2][0][0][0][1][1] = &Rendtriangle<2, 0, 0, 0, 1, 1>;
            RendtriangleFns[2][0][0][0][2][0] = &Rendtriangle<2, 0, 0, 0, 2, 0>;
            RendtriangleFns[2][0][0][0][2][1] = &Rendtriangle<2, 0, 0, 0, 2, 1>;
            RendtriangleFns[2][0][0][0][3][0] = &Rendtriangle<2, 0, 0, 0, 3, 0>;
            RendtriangleFns[2][0][0][0][3][1] = &Rendtriangle<2, 0, 0, 0, 3, 1>;
            RendtriangleFns[2][0][0][1][0][0] = &Rendtriangle<2, 0, 0, 1, 0, 0>;
            RendtriangleFns[2][0][0][1][0][1] = &Rendtriangle<2, 0, 0, 1, 0, 1>;
            RendtriangleFns[2][0][0][1][1][0] = &Rendtriangle<2, 0, 0, 1, 1, 0>;
            RendtriangleFns[2][0][0][1][1][1] = &Rendtriangle<2, 0, 0, 1, 1, 1>;
            RendtriangleFns[2][0][0][1][2][0] = &Rendtriangle<2, 0, 0, 1, 2, 0>;
            RendtriangleFns[2][0][0][1][2][1] = &Rendtriangle<2, 0, 0, 1, 2, 1>;
            RendtriangleFns[2][0][0][1][3][0] = &Rendtriangle<2, 0, 0, 1, 3, 0>;
            RendtriangleFns[2][0][0][1][3][1] = &Rendtriangle<2, 0, 0, 1, 3, 1>;
            RendtriangleFns[2][1][1][0][0][0] = &Rendtriangle<2, 1, 1, 0, 0, 0>;
            RendtriangleFns[2][1][1][0][0][1] = &Rendtriangle<2, 1, 1, 0, 0, 1>;
            RendtriangleFns[2][1][1][0][1][0] = &Rendtriangle<2, 1, 1, 0, 1, 0>;
            RendtriangleFns[2][1][1][0][1][1] = &Rendtriangle<2, 1, 1, 0, 1, 1>;
            RendtriangleFns[2][1][1][0][2][0] = &Rendtriangle<2, 1, 1, 0, 2, 0>;
            RendtriangleFns[2][1][1][0][2][1] = &Rendtriangle<2, 1, 1, 0, 2, 1>;
            RendtriangleFns[2][1][1][0][3][0] = &Rendtriangle<2, 1, 1, 0, 3, 0>;
            RendtriangleFns[2][1][1][0][3][1] = &Rendtriangle<2, 1, 1, 0, 3, 1>;
            RendtriangleFns[2][1][1][1][0][0] = &Rendtriangle<2, 1, 1, 1, 0, 0>;
            RendtriangleFns[2][1][1][1][0][1] = &Rendtriangle<2, 1, 1, 1, 0, 1>;
            RendtriangleFns[2][1][1][1][1][0] = &Rendtriangle<2, 1, 1, 1, 1, 0>;
            RendtriangleFns[2][1][1][1][1][1] = &Rendtriangle<2, 1, 1, 1, 1, 1>;
            RendtriangleFns[2][1][1][1][2][0] = &Rendtriangle<2, 1, 1, 1, 2, 0>;
            RendtriangleFns[2][1][1][1][2][1] = &Rendtriangle<2, 1, 1, 1, 2, 1>;
            RendtriangleFns[2][1][1][1][3][0] = &Rendtriangle<2, 1, 1, 1, 3, 0>;
            RendtriangleFns[2][1][1][1][3][1] = &Rendtriangle<2, 1, 1, 1, 3, 1>;
            RendtriangleFns[2][1][0][0][0][0] = &Rendtriangle<2, 1, 0, 0, 0, 0>;
            RendtriangleFns[2][1][0][0][0][1] = &Rendtriangle<2, 1, 0, 0, 0, 1>;
            RendtriangleFns[2][1][0][0][1][0] = &Rendtriangle<2, 1, 0, 0, 1, 0>;
            RendtriangleFns[2][1][0][0][1][1] = &Rendtriangle<2, 1, 0, 0, 1, 1>;
            RendtriangleFns[2][1][0][0][2][0] = &Rendtriangle<2, 1, 0, 0, 2, 0>;
            RendtriangleFns[2][1][0][0][2][1] = &Rendtriangle<2, 1, 0, 0, 2, 1>;
            RendtriangleFns[2][1][0][0][3][0] = &Rendtriangle<2, 1, 0, 0, 3, 0>;
            RendtriangleFns[2][1][0][0][3][1] = &Rendtriangle<2, 1, 0, 0, 3, 1>;
            RendtriangleFns[2][1][0][1][0][0] = &Rendtriangle<2, 1, 0, 1, 0, 0>;
            RendtriangleFns[2][1][0][1][0][1] = &Rendtriangle<2, 1, 0, 1, 0, 1>;
            RendtriangleFns[2][1][0][1][1][0] = &Rendtriangle<2, 1, 0, 1, 1, 0>;
            RendtriangleFns[2][1][0][1][1][1] = &Rendtriangle<2, 1, 0, 1, 1, 1>;
            RendtriangleFns[2][1][0][1][2][0] = &Rendtriangle<2, 1, 0, 1, 2, 0>;
            RendtriangleFns[2][1][0][1][2][1] = &Rendtriangle<2, 1, 0, 1, 2, 1>;
            RendtriangleFns[2][1][0][1][3][0] = &Rendtriangle<2, 1, 0, 1, 3, 0>;
            RendtriangleFns[2][1][0][1][3][1] = &Rendtriangle<2, 1, 0, 1, 3, 1>;
        }
#undef Rendtriangle

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

    virtual void Present() {

        u32* psrc = (u32*)render_buffer;
        

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

        int width = 640;
        int height = 480;

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
