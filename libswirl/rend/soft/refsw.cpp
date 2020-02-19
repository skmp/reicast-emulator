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

struct refsw : RefRendInterface
{
    DECL_ALIGN(32) u32 render_buffer[MAX_RENDER_PIXELS * 6]; //param pointers + depth1 + depth2 + stencil + acum 1 + acum 2
    
    struct FpuEntry;

    union Color {
        u8 rgba[4];
        u32 raw;
        struct {
            u8 r;
            u8 g;
            u8 b;
            u8 a;
        };
    };
    // lookup tables
    typedef void(*PixelFlush_ispFn)(refsw* backend, float x, float y, float invW, u8 *pb, parameter_tag_t tag);
    typedef bool(*PixelFlush_tspFn)(const FpuEntry *entry, float x, float y, u8 *pb);
    typedef Color(*PixelFlush_textureFn)(const text_info *texture, float u, float v);
    typedef Color(*PixelFlush_combinerFn)(Color base, Color textel, Color offset);
    typedef bool(*PixelFlush_alphaFn)(Color* cb, Color col);

    // Used for deferred TSP processing lookups
    struct FpuEntry
    {
        IPs3 ips;
        DrawParameters params;
        text_info texture;
        PixelFlush_tspFn tsp;
        PixelFlush_textureFn textureFetch;
        PixelFlush_combinerFn colorCombiner;
        PixelFlush_alphaFn blendingUnit;
    };

    //RenderMode, DepthInst
    PixelFlush_ispFn PixelFlush_ispFns[RM_COUNT][8];

    #define TPL_DECL_pixel template<bool pp_UseAlpha, bool pp_Texture, bool pp_Offset >
    PixelFlush_tspFn PixelFlush_tspFns[2][2][2];

    PixelFlush_textureFn PixelFlush_textureFns[2];
    PixelFlush_combinerFn PixelFlush_combinerFns[2][2][4];
    PixelFlush_alphaFn PixelFlush_alphaFns[2][2][2][8][8];

    u8* vram;
    refsw(u8* vram) : vram(vram) {
        #define PixelFlush_isp refsw::PixelFlush_isp_impl
        #define PixelFlush_tsp refsw::PixelFlush_tsp_impl
        #include "refsw_tsp_init.inl"
        #undef PixelFlush_tsp
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

        auto pb = reinterpret_cast<parameter_tag_t*>(render_buffer + PARAM_BUFFER_PIXEL_OFFSET);

        for (int y = 0; y < 32; y++) {
            for (int x = 0; x < 32; x++) {
                PixelFlush_tsp(this, tileX + x, tileY + y, (u8*)&render_buffer[x + y * MAX_RENDER_WIDTH], *pb++);
            }
        }
    }
    

    /*
    
        textel = TextureFetch<pp_Texture, pp_ingoreAlpha, volume, filtering>();
        baseCol = BaseColor<pp_UseAlpha, cheap shadows>();
        offsCol = OffsetColor<pp_Texture, pp_Offset, cheap shadows>()

        [trilinear, color] = ColorCombiner<pp_Texture, pp_Offset, pp_ShadInstr, bump map> (textel, baseCol, offsCol.rgb);

        color.rgba = PostProcess<clamp, trilinear, fog_mode>(color, trilinear, offsCol.a);

        BlendUnit<src_buf, dst_buf, src_sel, dst_sel>(color);
    */


    // can be
    // repeat, filtering, two volumes
    template<bool pp_IgnoreTexA>
    static Color TextureFetch(const text_info *texture, float u, float v) {

        int ui = u * 256;
        int vi = v * 256;
        mem128i px = ((mem128i *)texture->pdata)[((ui >> 8) % texture->width + (vi >> 8) % texture->height * texture->width)];

        int ublend = ui & 255;
        int vblend = vi & 255;
        int nublend = 255 - ublend;
        int nvblend = 255 - vblend;

        Color textel;

        for (int i = 0; i < 4; i++)
        {
            textel.rgba[i] =
                (px.m128i_u8[0 + i] * ublend * vblend) / 65536 +
                (px.m128i_u8[4 + i] * nublend * vblend) / 65536 +
                (px.m128i_u8[8 + i] * ublend * nvblend) / 65536 +
                (px.m128i_u8[12 + i] * nublend * nvblend) / 65536;
        };

        if (pp_IgnoreTexA)
        {
            textel.a = 255;
        }

        return textel;
    }

    template<bool pp_Texture, bool pp_Offset, u32 pp_ShadInstr>
    static Color ColorCombiner(Color base, Color textel, Color offset) {

        Color rv = base;
        if (pp_Texture)
        {
            if (pp_ShadInstr == 0)
            {
                //color.rgb = texcol.rgb;
                //color.a = texcol.a;

                rv = textel;
            }
            else if (pp_ShadInstr == 1)
            {
                //color.rgb *= texcol.rgb;
                //color.a = texcol.a;
                for (int i = 0; i < 3; i++)
                {
                    rv.rgba[i] = textel.rgba[i] * base.rgba[i] / 256;
                }

                rv.a = textel.a;
            }
            else if (pp_ShadInstr == 2)
            {
                //color.rgb=mix(color.rgb,texcol.rgb,texcol.a);
                u8 tb = textel.a;
                u8 cb = 255 - tb;

                for (int i = 0; i < 3; i++)
                {
                    rv.rgba[i] = (textel.rgba[i] * tb + base.rgba[i] * cb) / 256;
                }

                rv.a = base.a;
            }
            else if (pp_ShadInstr == 3)
            {
                //color*=texcol
                for (int i = 0; i < 4; i++)
                {
                    rv.rgba[i] = textel.rgba[i] * base.rgba[i] / 256;
                }
            }

            if (pp_Offset) {
                // mix only color, saturate
                for (int i = 0; i < 3; i++)
                {
                    rv.rgba[i] = min(rv.rgba[i] + offset.rgba[i], 255);
                }
            }
        }

        return rv;
    }

    template<bool pp_UseAlpha, bool pp_CheapShadows>
    static Color InterpolateBase(const PlaneStepper3* Col, float x, float y, float invW, u32 stencil) {
        Color rv;
        u32 mult = 256;

        if (pp_CheapShadows) {
            if (stencil & 1) {
                mult = FPU_SHAD_SCALE.scale_factor;
            }
        }

        rv.rgba[0] = Col[0].Ip(x, y) * mult / 256;
        rv.rgba[1] = Col[1].Ip(x, y) * mult / 256;
        rv.rgba[2] = Col[2].Ip(x, y) * mult / 256;
        rv.rgba[3] = Col[3].Ip(x, y) * mult / 256;    

        if (!pp_UseAlpha)
        {
            rv.a = 255;
        }

        return rv;
    }

    template<bool pp_CheapShadows>
    static Color InterpolateOffs(const PlaneStepper3* Ofs, float x, float y, float invW, u32 stencil) {
        Color rv;
        u32 mult = 256;

        if (pp_CheapShadows) {
            if (stencil & 1) {
                mult = FPU_SHAD_SCALE.scale_factor;
            }
        }

        rv.rgba[0] = Ofs[0].Ip(x, y) * mult / 256;
        rv.rgba[1] = Ofs[1].Ip(x, y) * mult / 256;
        rv.rgba[2] = Ofs[2].Ip(x, y) * mult / 256;
        rv.rgba[3] = Ofs[3].Ip(x, y);

        return rv;
    }

/*

*/
    template<u32 pp_AlphaInst, bool srcOther>
    static Color BlendCoefs(Color src, Color dst) {
        Color rv;

        switch(pp_AlphaInst>>1) {
            // zero
            case 0: rv.raw = 0; break;
            // other color
            case 1: rv = srcOther? src : dst; break;
            // src alpha
            case 2: for (int i = 0; i < 4; i++) rv.rgba[i] = src.a; break;
            // dst alpha
            case 3: for (int i = 0; i < 4; i++) rv.rgba[i] = dst.a; break;
        }

        if (pp_AlphaInst & 1) {
            for (int i = 0; i < 4; i++)
                rv.rgba[i] = 255 - rv.rgba[i];
        }

        return rv;
    }

    template<bool pp_AlphaTest, u32 pp_SrcSel, u32 pp_DstSel, u32 pp_SrcInst, u32 pp_DstInst>
    static bool BlendingUnit(Color* cb, Color col)
    {
        Color rv;
        Color src = pp_SrcSel ? cb[MAX_RENDER_PIXELS] : col;
        Color dst = cb[pp_DstSel ? MAX_RENDER_PIXELS : 0];
        
        Color src_blend = BlendCoefs<pp_SrcInst, false>(src, dst);
        Color dst_blend = BlendCoefs<pp_DstInst, true>(src, dst);

        for (int j = 0; j < 4; j++)
        {
            rv.rgba[j] = min((src.rgba[j] * src_blend.rgba[j] + dst.rgba[j] * dst_blend.rgba[j]) >> 8, 255);
        }

        if (!pp_AlphaTest || src.a > PT_ALPHA_REF)
        {
            cb[pp_DstSel ? MAX_RENDER_PIXELS : 0] = rv;
            return true;
        }
        else
        {
            return false;
        }
    }
    // Texture and shade a pixel
    TPL_DECL_pixel 
    static bool PixelFlush_tsp_impl(const FpuEntry *entry, float x, float y, u8 *pb)
    {
        auto zb = (float *)&pb[DEPTH1_BUFFER_PIXEL_OFFSET * 4];
        auto stencil = (u32 *)&pb[STENCIL_BUFFER_PIXEL_OFFSET * 4];
        auto cb = (Color*)&pb[ACCUM1_BUFFER_PIXEL_OFFSET * 4];

        float invW = *zb;

        Color base, textel, offs;

        base = InterpolateBase<pp_UseAlpha, true>(entry->ips.Col, x, y, invW, *stencil);
        
        if (pp_Texture) {
            float u = entry->ips.U.Ip(x, y);
            float v = entry->ips.V.Ip(x, y);

            u = u / invW;
            v = v / invW;

            textel = entry->textureFetch(&entry->texture, u, v);
            if (pp_Offset) {
                offs = InterpolateOffs<true>(entry->ips.Ofs, x, y, invW, *stencil);
            }
        }

        Color col = entry->colorCombiner(base, textel, offs);
        
        return entry->blendingUnit(cb, col);
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

        entry.ips.Setup(&entry.texture, vtx[0], vtx[1], vtx[2]);

        entry.tsp = PixelFlush_tspFns[entry.params.tsp.UseAlpha][entry.params.isp.Texture][entry.params.isp.Offset];
        entry.textureFetch = PixelFlush_textureFns[entry.params.tsp.IgnoreTexA];
        entry.colorCombiner = PixelFlush_combinerFns[entry.params.isp.Texture][entry.params.isp.Offset][entry.params.tsp.ShadInstr];
        entry.blendingUnit = PixelFlush_alphaFns[render_mode == RM_PUNCHTHROUGH ? 1 : 0][entry.params.tsp.SrcSelect][entry.params.tsp.DstSelect][entry.params.tsp.SrcInstr][entry.params.tsp.DstInstr];

        fpu_entires.push_back(entry);

        return fpu_entires.size();
    }

    void ClearFpuEntries() {
        fpu_entires.clear();
    }

    // Lookup/create cached TSP parameters, and call PixelFlush_tsp
    static bool PixelFlush_tsp(refsw* backend, float x, float y, u8 *pb, parameter_tag_t tag)
    {
        if (tag == 0)
            return false;

        auto entry = &backend->fpu_entires[tag-1];
        
        return entry->tsp(entry, x, y, pb);
    }

    // Depth processing for a pixel -- render_mode 0: OPAQ, 1: PT, 2: TRANS
    template <RenderMode render_mode, u32 depth_mode>
    static void PixelFlush_isp_impl(refsw* backend, float x, float y, float invW, u8 *pb, parameter_tag_t tag)
    {
        auto zb = (float*)&pb [DEPTH1_BUFFER_PIXEL_OFFSET * 4];
        auto zb2 = (float*)&pb[DEPTH2_BUFFER_PIXEL_OFFSET * 4];
        auto stencil = (u32*)&pb[STENCIL_BUFFER_PIXEL_OFFSET * 4];

#if 0 // make sure we're writting to the right place
    verify((uintptr_t) pb >= (uintptr_t)&render_buffer[0]);

    verify((uintptr_t)pb < ((uintptr_t)&render_buffer[0] + sizeof(render_buffer)));


    verify((uintptr_t)zb >= (uintptr_t)&render_buffer[0]);

    verify((uintptr_t)zb < ((uintptr_t)&render_buffer[0] + sizeof(render_buffer)));
#endif

        auto mode = depth_mode;
        
        if (render_mode == RM_PUNCHTHROUGH)
            mode = 6; // TODO: FIXME
        else if (render_mode == RM_TRANSLUCENT)
            mode = 3; // TODO: FIXME
        else if (render_mode == RM_MODIFIER)
            mode = 6;
        
        switch(mode) {
            // never
            case 0: return; break;
            // less
            case 1: if (invW >= *zb) return; break;
            // equal
            case 2: if (invW != *zb) return; break;
            // less or equal
            case 3: if (invW > *zb) return; break;
            // greater
            case 4: if (invW <= *zb) return; break;
            // not equal
            case 5: if (invW == *zb) return; break;
            // greater or equal
            case 6: if (invW < *zb) return; break;
            // always
            case 7: break;
        }

        switch (render_mode)
        {
            // OPAQ
            case RM_OPAQUE:
            {
                // Z pre-pass only

                *zb = invW;
                *(parameter_tag_t *)pb = tag;
            }
            break;

            case RM_MODIFIER:
            {
                // Flip on Z pass

                *stencil ^= 0b10;
            }
            break;

            // PT
            case RM_PUNCHTHROUGH:
            {
                // Z + TSP syncronized for alpha test

                if (PixelFlush_tsp(backend, x, y, pb, tag))
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

                backend->PixelsDrawn++;

                *zb = invW;
                *(parameter_tag_t *)pb = tag;
            }
            break;
        }
    }
    
    // Used by layer peeling to determine end of processing
    int PixelsDrawn;

    // Rasterize a single triangle to ISP (or ISP+TSP for PT)
    void RasterizeTriangle(RenderMode render_mode, DrawParameters* params, parameter_tag_t tag, int vertex_offset, const Vertex& v1, const Vertex& v2, const Vertex& v3, RECT* area)
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
                    float invW = Z.Ip(x_ps, y_ps);
                    PixelFlush_ispFns[render_mode][params->isp.DepthMode](this, x_ps, y_ps, invW, cb_x, tag);
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

    vector<FpuEntry> fpu_entires;
    
    virtual u8* GetColorOutputBuffer() {
        return reinterpret_cast<u8*>(render_buffer + ACCUM1_BUFFER_PIXEL_OFFSET);
    }

    void operator delete(void* p) {
        _mm_free(p);
    }
};

#if FEAT_TA == TA_LLE
Renderer* rend_refsw(u8* vram) {
    return new refrend(vram, [=]() { 
        return (RefRendInterface*) new(_mm_malloc(sizeof(refsw), 32)) ::refsw(vram);
    });
}

static auto refrend = RegisterRendererBackend(rendererbackend_t{ "refsw", "RefSW", 0, rend_refsw });
#endif