/*
 * Notes
 * Games that need certain settings activated in order to function properly:
 *
 * update mode type -
 * * Street Fighter III Double Impact (would run at choppy framerate otherwise)
 *
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
   int eg_hack;                     /* -1, make no decision */
   int disable_div;                 /* -1, make no decision */
};

struct game_type_naomi
{
   char product_number[128];
   int updatemode_type;     /* -1, make no decision, 0 = update mode normal, 1 = update mode fullspeed */
   int dynarec_type;        /* -1, make no decision, 0 = dynamic recompiler, 1 = generic_recompiler */
   int aica_interrupt_hack; /* -1, make no decision, 0 = normal, 1 = enable hack */
   int alpha_sort_mode;     /* -1, make no decision */
   int translucentPolygonDepthMask; /* -1, make no decision */
   int rendertotexturebuffer;       /* -1, make no decision */
   int eg_hack;                     /* -1, make no decision */
   int disable_div;                 /* -1, make no decision */
};

static struct game_type lut_games[] = 
{
   /* Update mode fullspeed */
   { "T1210N    ",  1, -1, -1, -1, -1, -1,  -1, -1  },                /* Street Fighter III Double Impact */

   /* Fallback to generic recompiler */

   /* EG Hack */
   { "T46703M   ", -1, -1, -1, -1, -1, -1,   1, -1  },                /* Border Down (Japan) */
   { "MK-51065  ", -1, -1, -1, -1, -1, -1,   1, -1  },                /* Bomberman Online (USA) */
   { "T47801M   ", -1, -1, -1, -1, -1, -1,   1, -1  },                /* Chaos Field (Japan) */
   { "T23202M   ", -1, -1, -1, -1, -1, -1,   1, -1  },                /* Death Crimson OX (Japan) */
   { "T2401N    ", -1, -1, -1, -1, -1, -1,   1, -1  },                /* Death Crimson OX (USA) */
   { "T44306N   ", -1, -1, -1, -1, -1, -1,   1, -1  },                /* Fatal Fury: Mark of the Wolves (USA) */
   { "T3108M    ", -1, -1, -1, -1, -1, -1,   1, -1  },                /* Garou: Mark of the Wolves (Japan) */
   { "HDR-0078  ", -1, -1, -1, -1, -1, -1,   1, -1  },                /* Jet Set Radio (Japan) */
   { "MK-51058  ", -1, -1, -1, -1, -1, -1,   1, -1  },                /* Jet Grind Radio (USA) */
   { "MK-5105850", -1, -1, -1, -1, -1, -1,   1, -1  },                /* Jet Set Radio (Europe) */
   { "HDR-0079  ", -1, -1, -1, -1, -1, -1,   1, -1  },                /* Napple Tale (Japan) */
   { "MK-5110050", -1, -1, -1, -1, -1, -1,   1, -1  },                /* Phantasy Star Online (Europe) */
   { "HDR-0129  ", -1, -1, -1, -1, -1, -1,   1, -1  },                /* Phantasy Star Online (Japan) */
   { "MK-51100  ", -1, -1, -1, -1, -1, -1,   1, -1  },                /* Phantasy Star Online (USA) */
   { "MK-5119350", -1, -1, -1, -1, -1, -1,   1, -1  },                /* Phantasy Star Online Ver. 2 (Europe) */
   { "HDR-0163  ", -1, -1, -1, -1, -1, -1,   1, -1  },                /* Phantasy Star Online Ver. 2 (Japan) */
   { "MK-51193  ", -1, -1, -1, -1, -1, -1,   1, -1  },                /* Phantasy Star Online Ver. 2 (USA) */
   { "T9907M    ", -1, -1, -1, -1, -1, -1,   1, -1  },                /* Psyvariar 2 (Japan) */
   { "HDR-0216  ", -1, -1, -1, -1, -1, -1,   1, -1  },                /* Puyo Puyo Fever (Japan) */
   { "T47802M   ", -1, -1, -1, -1, -1, -1,   1, -1  },                /* Radirgy (Japan) */
   { "HDR-0151  ", -1, -1, -1, -1, -1, -1,   1, -1  },                /* Segagaga (Japan) */
   { "HDR-0125  ", -1, -1, -1, -1, -1, -1,   1, -1  },                /* Sonic Shuffle (Japan) */
   { "MK-5106050", -1, -1, -1, -1, -1, -1,   1, -1  },                /* Sonic Shuffle (Europe) */
   { "MK-51060  ", -1, -1, -1, -1, -1, -1,   1, -1  },                /* Sonic Shuffle (USA) */
   { "T29102M   ", -1, -1, -1, -1, -1, -1,   1, -1  },                /* Trigger Heart Exelica (Japan) */
   { "T45101M   ", -1, -1, -1, -1, -1, -1,   1, -1  },                /* WWF Royal Rumble (Japan) */
   { "T10003D 50", -1, -1, -1, -1, -1, -1,   1, -1  },                /* WWF Royal Rumble (Europe) */
   { "T10005N   ", -1, -1, -1, -1, -1, -1,   1, -1  },                /* WWF Royal Rumble (USA) */

   /* AICA Interrupt Hack */
   { "T1203N    ", -1, -1,  1, -1, -1, -1,  -1, -1  },                /* Street Fighter Alpha 3 */

   /* Alpha sort mode */
   { "MK-5100050", -1, -1, -1,  1, -1, -1,  -1, -1  },                /* Sonic Adventure */

   /* Translucent Polygon Depth Mask */
   { "RDC-0057",   -1, -1, -1, -1,  1, -1,  -1, -1  },                /* Cosmic Smash */
   { "HDR-0176  ", -1, -1, -1, -1,  1, -1,  -1, -1  },                /* Cosmic Smash */

   /* Render to texture buffer */
   { "T40205N   ", -1, -1, -1, -1, -1,  1,  -1, -1  },                /* Tony Hawk's Pro Skater 1 (USA) */
   { "T13006N   ", -1, -1, -1, -1, -1,  1,  -1, -1  },                /* Tony Hawk's Pro Skater 2 (USA) */
   { "T13008D",    -1, -1, -1, -1, -1,  1,  -1, -1  },                /* Tony Hawk's Pro Skater 2 (USA) */
   { "T23002N   ", -1, -1, -1, -1, -1,  1,  -1, -1  },                /* Star Wars Episode I: Jedi Power Battle (USA) */

   /* Disable DIV matching */
   { "T30701D   ", -1, -1, -1, -1, -1,  -1,  -1, 1  },                /* Pro Pinball Trilogy */
};

static struct game_type_naomi lut_games_naomi[] = 
{
   /* Update mode fullspeed */
   /* AICA Interrupt Hack */
   { "Street Fighter Zero 3 Upper", -1, -1,  1, -1, -1, -1,  -1, -1  },                /* Street Fighter Alpha 3 */
};
