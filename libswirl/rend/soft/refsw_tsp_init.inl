{
    PixelFlush_tspFns[0][0][0] = &PixelFlush_tsp<0, 0, 0>;
    PixelFlush_tspFns[0][0][1] = &PixelFlush_tsp<0, 0, 1>;
    PixelFlush_tspFns[0][1][0] = &PixelFlush_tsp<0, 1, 0>;
    PixelFlush_tspFns[0][1][1] = &PixelFlush_tsp<0, 1, 1>;
    PixelFlush_tspFns[1][0][0] = &PixelFlush_tsp<1, 0, 0>;
    PixelFlush_tspFns[1][0][1] = &PixelFlush_tsp<1, 0, 1>;
    PixelFlush_tspFns[1][1][0] = &PixelFlush_tsp<1, 1, 0>;
    PixelFlush_tspFns[1][1][1] = &PixelFlush_tsp<1, 1, 1>;


    PixelFlush_textureFns[0] = &TextureFetch<0>;
    PixelFlush_textureFns[1] = &TextureFetch<1>;

    PixelFlush_combinerFns[0][0][0] = &ColorCombiner<0, 0, 0>;
    PixelFlush_combinerFns[0][1][0] = &ColorCombiner<0, 1, 0>;
    PixelFlush_combinerFns[1][0][0] = &ColorCombiner<1, 0, 0>;
    PixelFlush_combinerFns[1][1][0] = &ColorCombiner<1, 1, 0>;

    PixelFlush_combinerFns[0][0][1] = &ColorCombiner<0, 0, 1>;
    PixelFlush_combinerFns[0][1][1] = &ColorCombiner<0, 1, 1>;
    PixelFlush_combinerFns[1][0][1] = &ColorCombiner<1, 0, 1>;
    PixelFlush_combinerFns[1][1][1] = &ColorCombiner<1, 1, 1>;

    PixelFlush_combinerFns[0][0][2] = &ColorCombiner<0, 0, 2>;
    PixelFlush_combinerFns[0][1][2] = &ColorCombiner<0, 1, 2>;
    PixelFlush_combinerFns[1][0][2] = &ColorCombiner<1, 0, 2>;
    PixelFlush_combinerFns[1][1][2] = &ColorCombiner<1, 1, 2>;

    PixelFlush_combinerFns[0][0][3] = &ColorCombiner<0, 0, 3>;
    PixelFlush_combinerFns[0][1][3] = &ColorCombiner<0, 1, 3>;
    PixelFlush_combinerFns[1][0][3] = &ColorCombiner<1, 0, 3>;
    PixelFlush_combinerFns[1][1][3] = &ColorCombiner<1, 1, 3>;

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