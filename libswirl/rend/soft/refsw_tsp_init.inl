/*
	This file is part of libswirl
    #include "license/bsd"
*/

{
    #define ISP_FNS(rm) \
        PixelFlush_ispFns[rm][0] = &PixelFlush_isp<rm, 0>; \
        PixelFlush_ispFns[rm][1] = &PixelFlush_isp<rm, 1>; \
        PixelFlush_ispFns[rm][2] = &PixelFlush_isp<rm, 2>; \
        PixelFlush_ispFns[rm][3] = &PixelFlush_isp<rm, 3>; \
        PixelFlush_ispFns[rm][4] = &PixelFlush_isp<rm, 4>; \
        PixelFlush_ispFns[rm][5] = &PixelFlush_isp<rm, 5>; \
        PixelFlush_ispFns[rm][6] = &PixelFlush_isp<rm, 6>; \
        PixelFlush_ispFns[rm][7] = &PixelFlush_isp<rm, 7>;

    ISP_FNS(RM_OPAQUE)
    ISP_FNS(RM_PUNCHTHROUGH)
    ISP_FNS(RM_TRANSLUCENT)
    ISP_FNS(RM_MODIFIER)

#define PIXEL_FNS(cf, fc) \
    PixelFlush_tspFns[0][0][0][cf][fc] = &PixelFlush_tsp<0, 0, 0, cf, fc>; \
    PixelFlush_tspFns[0][0][1][cf][fc] = &PixelFlush_tsp<0, 0, 1, cf, fc>; \
    PixelFlush_tspFns[0][1][0][cf][fc] = &PixelFlush_tsp<0, 1, 0, cf, fc>; \
    PixelFlush_tspFns[0][1][1][cf][fc] = &PixelFlush_tsp<0, 1, 1, cf, fc>; \
    PixelFlush_tspFns[1][0][0][cf][fc] = &PixelFlush_tsp<1, 0, 0, cf, fc>; \
    PixelFlush_tspFns[1][0][1][cf][fc] = &PixelFlush_tsp<1, 0, 1, cf, fc>; \
    PixelFlush_tspFns[1][1][0][cf][fc] = &PixelFlush_tsp<1, 1, 0, cf, fc>; \
    PixelFlush_tspFns[1][1][1][cf][fc] = &PixelFlush_tsp<1, 1, 1, cf, fc>;

    PIXEL_FNS(0, 0)
    PIXEL_FNS(0, 1)
    PIXEL_FNS(0, 2)
    PIXEL_FNS(0, 3)

    PIXEL_FNS(1, 0)
    PIXEL_FNS(1, 1)
    PIXEL_FNS(1, 2)
    PIXEL_FNS(1, 3)

#define TEXTURE_FNS(fu, fv, fm) \
    PixelFlush_textureFns[0][0][0][fu][fv][fm] = &TextureFetch<0, 0, 0, fu, fv, fm>; \
    PixelFlush_textureFns[0][0][1][fu][fv][fm] = &TextureFetch<0, 0, 1, fu, fv, fm>; \
    PixelFlush_textureFns[0][1][0][fu][fv][fm] = &TextureFetch<0, 1, 0, fu, fv, fm>; \
    PixelFlush_textureFns[0][1][1][fu][fv][fm] = &TextureFetch<0, 1, 1, fu, fv, fm>; \
    PixelFlush_textureFns[1][0][0][fu][fv][fm] = &TextureFetch<1, 0, 0, fu, fv, fm>; \
    PixelFlush_textureFns[1][0][1][fu][fv][fm] = &TextureFetch<1, 0, 1, fu, fv, fm>; \
    PixelFlush_textureFns[1][1][0][fu][fv][fm] = &TextureFetch<1, 1, 0, fu, fv, fm>; \
    PixelFlush_textureFns[1][1][1][fu][fv][fm] = &TextureFetch<1, 1, 1, fu, fv, fm>;

#define TEXTURE_FNS_fufv(fm) \
    TEXTURE_FNS(0, 0, fm) \
    TEXTURE_FNS(0, 1, fm) \
    TEXTURE_FNS(1, 0, fm) \
    TEXTURE_FNS(1, 1, fm)

    TEXTURE_FNS_fufv(0)
    TEXTURE_FNS_fufv(1)
    TEXTURE_FNS_fufv(2)
    TEXTURE_FNS_fufv(3)

#define COMBINER_FNS(c) \
    PixelFlush_combinerFns[0][0][c] = &ColorCombiner<0, 0, c>; \
    PixelFlush_combinerFns[0][1][c] = &ColorCombiner<0, 1, c>; \
    PixelFlush_combinerFns[1][0][c] = &ColorCombiner<1, 0, c>; \
    PixelFlush_combinerFns[1][1][c] = &ColorCombiner<1, 1, c>;

#define COMBINER_FNS_C() \
    COMBINER_FNS(0) \
    COMBINER_FNS(1) \
    COMBINER_FNS(2) \
    COMBINER_FNS(3)
    
    COMBINER_FNS_C()


    #define ALPHA_FNS(s,d) \
        PixelFlush_alphaFns[0][0][0][s][d] = &BlendingUnit<0, 0, 0, s, d>; \
        PixelFlush_alphaFns[0][0][1][s][d] = &BlendingUnit<0, 0, 1, s, d>; \
        PixelFlush_alphaFns[0][1][0][s][d] = &BlendingUnit<0, 1, 0, s, d>; \
        PixelFlush_alphaFns[0][1][1][s][d] = &BlendingUnit<0, 1, 1, s, d>; \
        PixelFlush_alphaFns[1][0][0][s][d] = &BlendingUnit<1, 0, 0, s, d>; \
        PixelFlush_alphaFns[1][0][1][s][d] = &BlendingUnit<1, 0, 1, s, d>; \
        PixelFlush_alphaFns[1][1][0][s][d] = &BlendingUnit<1, 1, 0, s, d>; \
        PixelFlush_alphaFns[1][1][1][s][d] = &BlendingUnit<1, 1, 1, s, d>;

    #define ALPHA_FNS_s(d) \
        ALPHA_FNS(0,d)\
        ALPHA_FNS(1,d)\
        ALPHA_FNS(2,d)\
        ALPHA_FNS(3,d)\
        ALPHA_FNS(4,d)\
        ALPHA_FNS(5,d)\
        ALPHA_FNS(6,d)\
        ALPHA_FNS(7,d)

    #define ALPHA_FNS_sd() \
        ALPHA_FNS_s(0)\
        ALPHA_FNS_s(1)\
        ALPHA_FNS_s(2)\
        ALPHA_FNS_s(3)\
        ALPHA_FNS_s(4)\
        ALPHA_FNS_s(5)\
        ALPHA_FNS_s(6)\
        ALPHA_FNS_s(7)


    ALPHA_FNS_sd()
}