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


    PixelFlush_tspFns[0][0][0] = &PixelFlush_tsp<0, 0, 0>;
    PixelFlush_tspFns[0][0][1] = &PixelFlush_tsp<0, 0, 1>;
    PixelFlush_tspFns[0][1][0] = &PixelFlush_tsp<0, 1, 0>;
    PixelFlush_tspFns[0][1][1] = &PixelFlush_tsp<0, 1, 1>;
    PixelFlush_tspFns[1][0][0] = &PixelFlush_tsp<1, 0, 0>;
    PixelFlush_tspFns[1][0][1] = &PixelFlush_tsp<1, 0, 1>;
    PixelFlush_tspFns[1][1][0] = &PixelFlush_tsp<1, 1, 0>;
    PixelFlush_tspFns[1][1][1] = &PixelFlush_tsp<1, 1, 1>;

#define TEXTURE_FNS(fu, fv) \
    PixelFlush_textureFns[0][0][0][fu][fv] = &TextureFetch<0, 0, 0, fu, fv>; \
    PixelFlush_textureFns[0][0][1][fu][fv] = &TextureFetch<0, 0, 1, fu, fv>; \
    PixelFlush_textureFns[0][1][0][fu][fv] = &TextureFetch<0, 1, 0, fu, fv>; \
    PixelFlush_textureFns[0][1][1][fu][fv] = &TextureFetch<0, 1, 1, fu, fv>; \
    PixelFlush_textureFns[1][0][0][fu][fv] = &TextureFetch<1, 0, 0, fu, fv>; \
    PixelFlush_textureFns[1][0][1][fu][fv] = &TextureFetch<1, 0, 1, fu, fv>; \
    PixelFlush_textureFns[1][1][0][fu][fv] = &TextureFetch<1, 1, 0, fu, fv>; \
    PixelFlush_textureFns[1][1][1][fu][fv] = &TextureFetch<1, 1, 1, fu, fv>;

#define TEXTURE_FNS_fufv() \
    TEXTURE_FNS(0, 0) \
    TEXTURE_FNS(0, 1) \
    TEXTURE_FNS(1, 0) \
    TEXTURE_FNS(1, 1)

    TEXTURE_FNS_fufv()

#define COMBINER_FNS(c, cl, fc) \
    PixelFlush_combinerFns[0][0][c][cl][fc] = &ColorCombiner<0, 0, c, cl, fc>; \
    PixelFlush_combinerFns[0][1][c][cl][fc] = &ColorCombiner<0, 1, c, cl, fc>; \
    PixelFlush_combinerFns[1][0][c][cl][fc] = &ColorCombiner<1, 0, c, cl, fc>; \
    PixelFlush_combinerFns[1][1][c][cl][fc] = &ColorCombiner<1, 1, c, cl, fc>;

#define COMBINER_FNS_C(cl, fc) \
    COMBINER_FNS(0, cl, fc) \
    COMBINER_FNS(1, cl, fc) \
    COMBINER_FNS(2, cl, fc) \
    COMBINER_FNS(3, cl, fc)
    
    COMBINER_FNS_C(0, 0)
    COMBINER_FNS_C(0, 1)
    COMBINER_FNS_C(0, 2)
    COMBINER_FNS_C(0, 3)

    COMBINER_FNS_C(1, 0)
    COMBINER_FNS_C(1, 1)
    COMBINER_FNS_C(1, 2)
    COMBINER_FNS_C(1, 3)


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