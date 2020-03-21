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
    struct RECT {
        int left, top, right, bottom;
    };

    #if HOST_OS != OS_DARWIN
        #include     <X11/Xlib.h>
    #endif
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


struct PlaneStepper2
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

struct IPs2
{
    PlaneStepper2 Z;
    PlaneStepper2 U;
    PlaneStepper2 V;
    PlaneStepper2 Col[4];
    PlaneStepper2 Ofs[4];

    void Setup(PolyParam* pp, text_info* texture, const Vertex& v1, const Vertex& v2, const Vertex& v3)
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

struct slowsoftrend;

//<alpha_blend, pp_UseAlpha, pp_Texture, pp_IgnoreTexA, pp_ShadInstr, pp_Offset >
typedef void(slowsoftrend::*RendtriangleFn)(PolyParam* pp, int vertex_offset, const Vertex& v1, const Vertex& v2, const Vertex& v3, u32* colorBuffer, RECT* area);
static RendtriangleFn RendtriangleFns[3][2][2][2][4][2];

TPL_DECL_pixel
static void PixelFlush(PolyParam* pp, text_info* texture, float x, float y, u8* cb, IPs2& ip)
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

struct slowsoftrend : Renderer
{
    u8* vram;

    slowsoftrend(u8* vram) : vram(vram) { }

    //u32 nok,fok;
    TPL_DECL_triangle
    void Rendtriangle(PolyParam* pp, int vertex_offset, const Vertex& v1, const Vertex& v2, const Vertex& v3, u32* colorBuffer, RECT* area)
    {
        text_info texture = { 0 };

        if (pp_Texture) {

#pragma omp critical (texture_lookup)
            {
                texture = raw_GetTexture(vram, pp->tsp, pp->tcw);
            }

        }

        const int stride_bytes = STRIDE_PIXEL_OFFSET * 4;
        //Plane equation


        const float Y1 = v1.y;
        const float Y2 = v2.y;
        const float Y3 = v3.y;

        const float X1 = v1.x;
        const float X2 = v2.x;
        const float X3 = v3.x;

        int sgn = 1;

        // Deltas
        {
            //area: (X1-X3)*(Y2-Y3)-(Y1-Y3)*(X2-X3)
            float area = ((X1 - X3) * (Y2 - Y3) - (Y1 - Y3) * (X2 - X3));

            if (area > 0)
                sgn = -1;

            if (pp->isp.CullMode != 0) {
                float abs_area = fabsf(area);

                if (abs_area < FPU_CULL_VAL)
                    return;

                if (pp->isp.CullMode >= 2) {
                    u32 mode = vertex_offset ^ (pp->isp.CullMode & 1);

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

        DECL_ALIGN(64) IPs2 ip;

        ip.Setup(pp, &texture, v1, v2, v3);


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
                    PixelFlush TPL_PRMS_pixel(pp, &texture, x_ps, y_ps, cb_x, ip);
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
        //disable RTTs for now ..
        if (ctx->rend.isRTT)
            return false;

        ctx->rend_inuse.Lock();

        if (!ta_parse_vdrc(this, vram, ctx))
            return false;

        return true;
    }



    template <int alpha_mode>
    void RenderParamList(List<PolyParam>* param_list, RECT* area) {

        Vertex* verts = pvrrc.verts.head();
        u32* idx = pvrrc.idx.head();

        PolyParam* params = param_list->head();
        int param_count = param_list->used();

        for (int i = 0; i < param_count; i++)
        {
            int vertex_count = params[i].count - 2;

            u32* poly_idx = &idx[params[i].first];

            for (int v = 0; v < vertex_count; v++) {
                ////<alpha_blend, pp_UseAlpha, pp_Texture, pp_IgnoreTexA, pp_ShadInstr, pp_Offset >
                RendtriangleFn fn = RendtriangleFns[alpha_mode][params[i].tsp.UseAlpha][params[i].pcw.Texture][params[i].tsp.IgnoreTexA][params[i].tsp.ShadInstr][params[i].pcw.Offset];

                (this->*fn)(&params[i], v, verts[poly_idx[v]], verts[poly_idx[v + 1]], verts[poly_idx[v + 2]], render_buffer, area);
            }
        }
    }
    virtual bool RenderFramebuffer() {
        return true;
    }
    virtual bool RenderPVR() {
        bool is_rtt = pvrrc.isRTT;

        memset(render_buffer, 0, sizeof(render_buffer));

        if (pvrrc.verts.used() < 3)
            return false;

        if (pvrrc.render_passes.head()[0].autosort)
            SortPParams(0, pvrrc.global_param_tr.used());

        int tcount = omp_get_num_procs() - 1;
        if (tcount == 0) tcount = 1;
        //if (tcount > settings.pvr.MaxThreads) tcount = settings.pvr.MaxThreads;
#pragma omp parallel num_threads(tcount)
        {
            int thd = omp_get_thread_num();
            int y_offs = 480 % omp_get_num_threads();
            int y_thd = 480 / omp_get_num_threads();
            int y_start = (!!thd) * y_offs + y_thd * thd;
            int y_end = y_offs + y_thd * (thd + 1);

            RECT area = { 0, y_start, 640, y_end };
            RenderParamList<0>(&pvrrc.global_param_op, &area);
            RenderParamList<1>(&pvrrc.global_param_pt, &area);
            RenderParamList<2>(&pvrrc.global_param_tr, &area);
        }

        if (!is_rtt) {
            Present();
        }

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

#define Rendtriangle slowsoftrend::Rendtriangle
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

    ~slowsoftrend() {

#if HOST_OS == OS_WINDOWS
        if (hBMP) {
            DeleteObject(SelectObject(hmem, holdBMP));
            DeleteDC(hmem);
        }
#endif
    }

    virtual void Present() {

#if HOST_OS == OS_WINDOWS
        u32* psrc = (u32*)render_buffer;
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

        u32* psrc = (u32*)render_buffer;
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

#if FEAT_TA == TA_HLE
Renderer* rend_slowsoftrend(u8* vram) {
    return new(_mm_malloc(sizeof(slowsoftrend), 32)) ::slowsoftrend(vram);
}

static auto slowsoftrend = RegisterRendererBackend(rendererbackend_t{ "slow", "Slow Software Renderer", 0, rend_slowsoftrend });
#endif
