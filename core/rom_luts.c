/*
 * Notes
 * Games that need certain settings activated in order to function properly:
 *
 * update mode type -
 * * Street Fighter III Double Impact (would run at choppy framerate otherwise)
 *
 * dynarec_type -
 * * Marvel Vs Capcom 2 (would hang at beginning of second stage if not set to generic_recompiler)
 *
 * aica_interrupt_hack -
 * * Street Fighter Alpha 3 (would hang at startup screen otherwise
 */

struct game_type
{
   char product_number[11];
   int updatemode_type;     /* -1, make no decision, 0 = update mode normal, 1 = update mode fullspeed */
   int dynarec_type;        /* -1, make no decision, 0 = dynamic recompiler, 1 = generic_recompiler */
   int aica_interrupt_hack; /* -1, make no decision, 0 = normal, 1 = enable hack */
   int alpha_sort_mode;     /* -1, make no decision */
   int translucentPolygonDepthMask; /* -1, make no decision */
   int rendertotexturebuffer;       /* -1, make no decision */
};

static struct game_type lut_games[] = 
{
   { "T1210N    ",  1, -1, -1, -1, -1, -1  },                 /* Street Fighter III Double Impact */
   { "MK-51049  ", -1,  1, -1, -1, -1, -1  },                /* Marvel Vs Capcom 2 */
   { "T1203N    ", -1, -1,  1, -1, -1, -1  },                /* Street Fighter Alpha 3 */
   { "MK-5100050", -1, -1, -1, 1,  -1, -1  },                /* Sonic Adventure */
   { "MK-5105950", -1, -1, -1, 1,  -1, -1  },                /* Shenmue */
   { "RDC-0057",   -1, -1, -1, -1,  1, -1  },                /* Cosmic Smash */
   { "HDR-0176  ",   -1, -1, -1, -1,  1, -1  },                /* Cosmic Smash */
   { "T40205N   ",    -1, -1, -1, -1, -1,  1  },                /* Tony Hawk's Pro Skater 1 (USA) */
   { "T13006N   ",    -1, -1, -1, -1, -1,  1  },                /* Tony Hawk's Pro Skater 2 (USA) */
   { "T13008D",    -1, -1, -1, -1, -1,  1  },                /* Tony Hawk's Pro Skater 2 */
   { "T23002N   ", -1, -1, -1, -1, -1,  1  },                /* Star Wars Episode I: Jedi Power Battle (USA) */
};
