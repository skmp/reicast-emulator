/*
	This file is part of libswirl

    Implemenets Pixel operations (PixelPipeline) for RefSW

    Naive easy-to-read implementation
*/

#include "license/bsd"

#include "refsw.h"

#include <cmath>
#include <cfloat>

#include <mmintrin.h>
#include <xmmintrin.h>
#include <emmintrin.h>
#include <smmintrin.h>

union mem128i {
    uint8_t m128i_u8[16];
    int8_t m128i_i8[16];
    int16_t m128i_i16[8];
    int32_t m128i_i32[4];
    uint32_t m128i_u32[4];
};

struct RefPixelPipeline : PixelPipeline {

    //RenderMode, DepthInst
    IspFn PixelFlush_ispFns[RM_COUNT][8];

    TspFn PixelFlush_tspFns[2][2][2][2][4];

    TextureFetchFn PixelFlush_textureFns[2][2][2][2][2][4];
    ColorCombinerFn PixelFlush_combinerFns[2][2][4];
    BlendingUnitFn PixelFlush_alphaFns[2][2][2][8][8];

    RefPixelPipeline() {
        #include "refsw_tsp_init.inl"
    }

    virtual IspFn GetIsp(RenderMode render_mode, ISP_TSP isp) {
        return PixelFlush_ispFns[render_mode][isp.DepthMode];
    }

    virtual TspFn GetTsp(ISP_TSP isp, TSP tsp) {
        return PixelFlush_tspFns[tsp.UseAlpha][isp.Texture][isp.Offset][tsp.ColorClamp][tsp.FogCtrl];
    }

    virtual TextureFetchFn GetTextureFetch(TSP tsp) {
        return PixelFlush_textureFns[tsp.IgnoreTexA][tsp.ClampU][tsp.ClampV][tsp.FlipU][tsp.FlipV][tsp.FilterMode];
    }

    virtual ColorCombinerFn GetColorCombiner(ISP_TSP isp, TSP tsp) {
        return PixelFlush_combinerFns[isp.Texture][isp.Offset][tsp.ShadInstr];
    }
    
    virtual BlendingUnitFn GetBlendingUnit(RenderMode render_mode, TSP tsp) {
        return PixelFlush_alphaFns[render_mode == RM_PUNCHTHROUGH ? 1 : 0][tsp.SrcSelect][tsp.DstSelect][tsp.SrcInstr][tsp.DstInstr];
    }

    /*
    
        textel = TextureFetch<pp_Texture, pp_ingoreAlpha, volume, filtering>();
        baseCol = BaseColor<pp_UseAlpha, cheap shadows>();
        offsCol = OffsetColor<pp_Texture, pp_Offset, cheap shadows>()

        [trilinear, color] = ColorCombiner<pp_Texture, pp_Offset, pp_ShadInstr, bump map> (textel, baseCol, offsCol.rgb);

        color.bgra = PostProcess<clamp, trilinear, fog_mode>(color, trilinear, offsCol.a);

        BlendUnit<src_buf, dst_buf, src_sel, dst_sel>(color);
    */

    // Clamp and flip a texture coordinate
    template<bool pp_Clamp, bool pp_Flip>
    static int ClampFlip(int coord, int size) {
        if (pp_Clamp) { // clamp
            if (coord < 0) {
                coord = 0;
            } else if (coord >= size) {
                coord = size-1;
            }
        } else if (pp_Flip) { // flip
            coord &= size*2-1;
            if (coord & size) {
                coord ^= size*2-1;
            }
        } else { //wrap
            coord &= size-1;
        }

        return coord;
    }

    // Fetch pixels from UVs, interpolate
    template<bool pp_IgnoreTexA,  bool pp_ClampU, bool pp_ClampV, bool pp_FlipU, bool pp_FlipV, u32 pp_FilterMode>
    static Color TextureFetch(const text_info *texture, float u, float v) {

        int ui = u * 256;
        int vi = v * 256;
        auto offset = ClampFlip<pp_ClampU, pp_FlipU>(ui >> 8, texture->width) + ClampFlip<pp_ClampV, pp_FlipV>(vi >> 8, texture->height) * texture->width;

        mem128i px = ((mem128i *)texture->pdata)[offset];

        Color textel = {0xAF674839};

        if (pp_FilterMode == 0) {
            // Point sampling
            for (int i = 0; i < 4; i++)
            {
                textel.bgra[i] = px.m128i_u8[0 + i];
            }
        } else if (pp_FilterMode == 1) {
            // Bilinear filtering
            int ublend = ui & 255;
            int vblend = vi & 255;
            int nublend = 255 - ublend;
            int nvblend = 255 - vblend;


            for (int i = 0; i < 4; i++)
            {
                textel.bgra[i] =
                    (px.m128i_u8[0 + i] * ublend * vblend) / 65536 +
                    (px.m128i_u8[4 + i] * nublend * vblend) / 65536 +
                    (px.m128i_u8[8 + i] * ublend * nvblend) / 65536 +
                    (px.m128i_u8[12 + i] * nublend * nvblend) / 65536;
            };
        } else {
            // trilinear filtering A and B
            die("pp_FilterMode is trilinear");
        }
        

        if (pp_IgnoreTexA)
        {
            textel.a = 255;
        }

        return textel;
    }

    // Combine Base, Textel and Offset colors
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
                    rv.bgra[i] = textel.bgra[i] * base.bgra[i] / 256;
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
                    rv.bgra[i] = (textel.bgra[i] * tb + base.bgra[i] * cb) / 256;
                }

                rv.a = base.a;
            }
            else if (pp_ShadInstr == 3)
            {
                //color*=texcol
                for (int i = 0; i < 4; i++)
                {
                    rv.bgra[i] = textel.bgra[i] * base.bgra[i] / 256;
                }
            }

            if (pp_Offset) {
                // mix only color, saturate
                for (int i = 0; i < 3; i++)
                {
                    rv.bgra[i] = min(rv.bgra[i] + offset.bgra[i], 255);
                }
            }
        }

        return rv;
    }

    // Interpolate the base color, also cheap shadows modifier
    template<bool pp_UseAlpha, bool pp_CheapShadows>
    INLINE static Color InterpolateBase(const PlaneStepper3* Col, float x, float y, float W, u32 stencil) {
        Color rv;
        u32 mult = 256;

        if (pp_CheapShadows) {
            if (stencil & 1) {
                mult = FPU_SHAD_SCALE.scale_factor;
            }
        }

        rv.bgra[0] = Col[2].Ip(x, y, W) * mult / 256;   // FIXME: why is input in RGBA instead of BGRA here?
        rv.bgra[1] = Col[1].Ip(x, y, W) * mult / 256;
        rv.bgra[2] = Col[0].Ip(x, y, W) * mult / 256;
        rv.bgra[3] = Col[3].Ip(x, y, W) * mult / 256;    

        if (!pp_UseAlpha)
        {
            rv.a = 255;
        }

        return rv;
    }

    // Interpolate the offset color, also cheap shadows modifier
    template<bool pp_CheapShadows>
    INLINE static Color InterpolateOffs(const PlaneStepper3* Ofs, float x, float y, float W, u32 stencil) {
        Color rv;
        u32 mult = 256;

        if (pp_CheapShadows) {
            if (stencil & 1) {
                mult = FPU_SHAD_SCALE.scale_factor;
            }
        }

        rv.bgra[0] = Ofs[2].Ip(x, y, W) * mult / 256;   // FIXME: why is input in RGBA instead of BGRA here?
        rv.bgra[1] = Ofs[1].Ip(x, y, W) * mult / 256;
        rv.bgra[2] = Ofs[0].Ip(x, y, W) * mult / 256;
        rv.bgra[3] = Ofs[3].Ip(x, y, W);

        return rv;
    }

    // select/calculate blend coefficient for the blend unit
    template<u32 pp_AlphaInst, bool srcOther>
    INLINE static Color BlendCoefs(Color src, Color dst) {
        Color rv;

        switch(pp_AlphaInst>>1) {
            // zero
            case 0: rv.raw = 0; break;
            // other color
            case 1: rv = srcOther? src : dst; break;
            // src alpha
            case 2: for (int i = 0; i < 4; i++) rv.bgra[i] = src.a; break;
            // dst alpha
            case 3: for (int i = 0; i < 4; i++) rv.bgra[i] = dst.a; break;
        }

        if (pp_AlphaInst & 1) {
            for (int i = 0; i < 4; i++)
                rv.bgra[i] = 255 - rv.bgra[i];
        }

        return rv;
    }

    // Blending Unit implementation. Alpha blend, accum buffers and such
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
            rv.bgra[j] = min((src.bgra[j] * src_blend.bgra[j] + dst.bgra[j] * dst_blend.bgra[j]) >> 8, 255);
        }

        if (!pp_AlphaTest || src.a >= PT_ALPHA_REF)
        {
            cb[pp_DstSel ? MAX_RENDER_PIXELS : 0] = rv;
            return true;
        }
        else
        {
            return false;
        }
    }

    // Color Clamp and Fog a pixel
    template<bool pp_Offset, bool pp_ColorClamp, u32 pp_FogCtrl>
    INLINE static Color FogUnit(Color col, float invW, u8 offs_a) {

        if (pp_ColorClamp) {
            Color clamp_max = { FOG_CLAMP_MAX };
            Color clamp_min = { FOG_CLAMP_MIN };

            for (int i = 0; i < 4; i++)
            {
                col.bgra[i] = min(col.bgra[i], clamp_max.bgra[i]);
                col.bgra[i] = max(col.bgra[i], clamp_min.bgra[i]);
            }
        }

        switch(pp_FogCtrl) {
            // Look up mode 1
            case 0b00:
            // look up mode 2
            case 0b11:
                {
                    u8* fog_density=(u8*)&FOG_DENSITY;
                    float fog_den_mant=fog_density[1]/128.0f;  //bit 7 -> x. bit, so [6:0] -> fraction -> /128
                    s32 fog_den_exp=(s8)fog_density[0];
                    
                    float fog_den = fog_den_mant*powf(2.0f,fog_den_exp);

                    f32 fogW = fog_den * invW;

                    fogW = max(fogW, 1.0f);
                    fogW = min(fogW, 255.999985f);

                    union f32_fields {
                        f32 full;
                        struct {
                            u32 m: 23;
                            u32 e: 8;
                            u32 s: 1;
                        };
                    };

                    f32_fields fog_fields = { fogW };

                    u32 index = (((fog_fields.e +1) & 7) << 4) |  ((fog_fields.m>>19) & 15);

                    u8 blendFactor = (fog_fields.m>>11) & 255;
                    u8 blend_inv = 255^blendFactor;

                    auto fog_entry = (u8*)&FOG_TABLE[index];

                    u8 fog_alpha = (fog_entry[0] * blendFactor + fog_entry[1] * blend_inv) >> 8;
                    
                    u8 fog_inv = 255^fog_alpha;

                    Color col_ram = { FOG_COL_RAM };

                    if (pp_FogCtrl == 0b00) {
                        for (int i = 0; i < 3; i++) {
                            col.bgra[i] = (col.bgra[i] * fog_inv + col_ram.bgra[i] * fog_alpha)>>8;
                        }
                    } else {
                        for (int i = 0; i < 3; i++) {
                            col.bgra[i] = col_ram.bgra[i];
                        }
                        col.a = col_ram.a;
                    }
                }
                break;

            // Per Vertex
            case 0b01:
                if (pp_Offset) {
                    Color col_vert = { FOG_COL_VERT };
                    u8 alpha = offs_a;
                    u8 inv = 255^alpha;
                  
                    for (int i = 0; i < 3; i++)
                    {
                        col.bgra[i] = (col.bgra[i] * inv + col_vert.bgra[i] * alpha)>>8;
                    }
                }
                break;

                
            // No Fog
            case 0b10:
                break;
        }

        return col;
    }

    // Implement the full texture/shade pipeline for a pixel
    template<bool pp_UseAlpha, bool pp_Texture, bool pp_Offset, bool pp_ColorClamp, u32 pp_FogCtrl>
    static bool PixelFlush_tsp(const FpuEntry *entry, float x, float y, float W, u8 *rb)
    {
        auto zb = (float *)&rb[DEPTH1_BUFFER_PIXEL_OFFSET * 4];
        auto stencil = (u32 *)&rb[STENCIL_BUFFER_PIXEL_OFFSET * 4];
        auto cb = (Color*)&rb[ACCUM1_BUFFER_PIXEL_OFFSET * 4];

        Color base = { 0 }, textel = { 0 }, offs = { 0 };

        base = InterpolateBase<pp_UseAlpha, true>(entry->ips.Col, x, y, W, *stencil);
        
        if (pp_Texture) {
            float u = entry->ips.U.Ip(x, y, W);
            float v = entry->ips.V.Ip(x, y, W);

            textel = entry->textureFetch(&entry->texture, u, v);
            if (pp_Offset) {
                offs = InterpolateOffs<true>(entry->ips.Ofs, x, y, W, *stencil);
            }
        }

        Color col = entry->colorCombiner(base, textel, offs);
        
        col = FogUnit<pp_Offset, pp_ColorClamp, pp_FogCtrl>(col, 1/W, offs.a);

        return entry->blendingUnit(cb, col);
    }
    // Lookup/create cached TSP parameters, and call PixelFlush_tsp
    static bool AlphaTest_tsp(refsw* backend, float x, float y, u8 *rb, float invW, parameter_tag_t tag)
    {
        auto entry = &backend->fpu_entires[tag-1];
        
        return entry->tsp(entry, x, y, 1/invW, rb);
    }

    // Depth processing for a pixel -- render_mode 0: OPAQ, 1: PT, 2: TRANS
    template <RenderMode render_mode, u32 depth_mode>
    static void PixelFlush_isp(refsw* backend, float x, float y, float invW, u8 *pb, parameter_tag_t tag)
    {
        auto zb = (float*)&pb [DEPTH1_BUFFER_PIXEL_OFFSET * 4];
        auto zb2 = (float*)&pb[DEPTH2_BUFFER_PIXEL_OFFSET * 4];
        auto stencil = (u32*)&pb[STENCIL_BUFFER_PIXEL_OFFSET * 4];

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

                if (AlphaTest_tsp(backend, x, y, pb, invW, tag))
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
    
};

PixelPipeline* Create_RefPixelPipeline() {
    return new RefPixelPipeline();
}