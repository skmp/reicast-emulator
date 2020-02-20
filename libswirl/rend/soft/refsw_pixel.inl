
struct RefPixelPipeline : PixelPipeline {

    //RenderMode, DepthInst
    IspFn PixelFlush_ispFns[RM_COUNT][8];

    #define TPL_DECL_pixel template<bool pp_UseAlpha, bool pp_Texture, bool pp_Offset >
    TspFn PixelFlush_tspFns[2][2][2];

    TextureFetchFn PixelFlush_textureFns[2][2][2][2][2];
    ColorCombinerFn PixelFlush_combinerFns[2][2][4];
    BlendingUnitFn PixelFlush_alphaFns[2][2][2][8][8];

    RefPixelPipeline() {
        #include "refsw_tsp_init.inl"
    }

    virtual IspFn GetIsp(RenderMode render_mode, ISP_TSP isp) {
        return PixelFlush_ispFns[render_mode][isp.DepthMode];
    }

    virtual TspFn GetTsp(ISP_TSP isp, TSP tsp) {
        return PixelFlush_tspFns[tsp.UseAlpha][isp.Texture][isp.Offset];
    }

    virtual TextureFetchFn GetTextureFetch(TSP tsp) {
        return PixelFlush_textureFns[tsp.IgnoreTexA][tsp.ClampU][tsp.ClampV][tsp.FlipU][tsp.FlipV];
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

        color.rgba = PostProcess<clamp, trilinear, fog_mode>(color, trilinear, offsCol.a);

        BlendUnit<src_buf, dst_buf, src_sel, dst_sel>(color);
    */

    template<bool pp_Clamp, bool pp_Flip>
    static int ClampFlip(int coord, int size) {
        if (pp_Clamp) {
            if (coord < 0) {
                coord = 0;
            } else if (coord >= size) {
                coord = size-1;
            }
        } else if (pp_Flip) {
            coord &= size*2-1;
            if (coord & size) {
                coord ^= size-1;
            }
        } else {
            coord &= size-1;
        }

        return coord;
    }

    // can be
    // repeat, filtering, two volumes
    template<bool pp_IgnoreTexA,  bool pp_ClampU, bool pp_ClampV, bool pp_FlipU, bool pp_FlipV>
    static Color TextureFetch(const text_info *texture, float u, float v) {

        int ui = u * 256;
        int vi = v * 256;
        mem128i px = ((mem128i *)texture->pdata)[ClampFlip<pp_ClampU, pp_FlipU>(ui >> 8, texture->width) + ClampFlip<pp_ClampV, pp_FlipV>(vi >> 8, texture->height) * texture->width];

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
    static Color InterpolateBase(const PlaneStepper3* Col, float x, float y, float W, u32 stencil) {
        Color rv;
        u32 mult = 256;

        if (pp_CheapShadows) {
            if (stencil & 1) {
                mult = FPU_SHAD_SCALE.scale_factor;
            }
        }

        rv.rgba[0] = Col[0].Ip(x, y, W) * mult / 256;
        rv.rgba[1] = Col[1].Ip(x, y, W) * mult / 256;
        rv.rgba[2] = Col[2].Ip(x, y, W) * mult / 256;
        rv.rgba[3] = Col[3].Ip(x, y, W) * mult / 256;    

        if (!pp_UseAlpha)
        {
            rv.a = 255;
        }

        return rv;
    }

    template<bool pp_CheapShadows>
    static Color InterpolateOffs(const PlaneStepper3* Ofs, float x, float y, float W, u32 stencil) {
        Color rv;
        u32 mult = 256;

        if (pp_CheapShadows) {
            if (stencil & 1) {
                mult = FPU_SHAD_SCALE.scale_factor;
            }
        }

        rv.rgba[0] = Ofs[0].Ip(x, y, W) * mult / 256;
        rv.rgba[1] = Ofs[1].Ip(x, y, W) * mult / 256;
        rv.rgba[2] = Ofs[2].Ip(x, y, W) * mult / 256;
        rv.rgba[3] = Ofs[3].Ip(x, y, W);

        return rv;
    }

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
    // Texture and shade a pixel
    TPL_DECL_pixel 
    static bool PixelFlush_tsp(const FpuEntry *entry, float x, float y, float W, u8 *rb)
    {
        auto zb = (float *)&rb[DEPTH1_BUFFER_PIXEL_OFFSET * 4];
        auto stencil = (u32 *)&rb[STENCIL_BUFFER_PIXEL_OFFSET * 4];
        auto cb = (Color*)&rb[ACCUM1_BUFFER_PIXEL_OFFSET * 4];

        Color base, textel, offs;

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