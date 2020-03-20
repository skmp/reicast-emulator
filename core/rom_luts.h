/*
 * Notes
 * Games that need certain settings activated in order to function properly:
 *
 * update mode type -
 * * Street Fighter III Double Impact (would run at choppy framerate otherwise)
 */

struct game_type
{
   char product_number[11];
   int updatemode_type;     /* -1, make no decision, 0 = update mode normal, 1 = update mode fullspeed */
   int alpha_sort_mode;     /* -1, make no decision */
   int translucentPolygonDepthMask; /* -1, make no decision */
   int rendertotexturebuffer;       /* -1, make no decision */
   int disable_div;                 /* -1, make no decision */
   float extra_depth_scale;			/* 1, default */
   int disable_vmem32;				/* 0 = normal, 1 = disable */
   int no_rgb_component;			/* 0 = normal, 1 = disable */
};

struct game_type_naomi
{
   char product_number[128];
   int updatemode_type;     /* -1, make no decision, 0 = update mode normal, 1 = update mode fullspeed */
   int alpha_sort_mode;     /* -1, make no decision */
   int translucentPolygonDepthMask; /* -1, make no decision */
   int rendertotexturebuffer;       /* -1, make no decision */
   int disable_div;                 /* -1, make no decision */
   int jamma_setup;                 /* -1, make no decision,
									   0 = normal 2P setup,
									   1 = 4 players setup
									   2 = 2 I/O boards w/ rotary encoder (or Track ball for AtomisWave)
									   3 = 12bits/player, 16 drivers out for Sega Marine Fishing
									   4 = 2 I/O boards, 4 players for Ring Out 4x4
									   5 = 3 analog axes (AtomisWave)
									   6 = 2 light guns (AtomisWave)
									   */
   float extra_depth_scale;         /* 1, default */
   InputDescriptors *game_inputs;
};

static struct game_type lut_games[] = 
{
   /* Update mode fullspeed */
   { "T1210N    ",  1, -1, -1, -1,  -1, 1, 0, 0 },           /* Street Fighter III Double Impact */

   /* Alpha sort mode */
   { "MK-5100050", -1,  1, -1, -1,  -1, 1, 0, 0 },           /* Sonic Adventure */

   /* Translucent Polygon Depth Mask */
   { "RDC-0057  ", -1, -1,  1, -1,  -1, 1, 0, 0 },           /* Cosmic Smash */
   { "HDR-0176  ", -1, -1,  1, -1,  -1, 1, 0, 0 },           /* Cosmic Smash */

   /* Render to texture buffer */
   { "T40205N   ", -1, -1, -1,  1,  -1, 1, 0, 0 },           /* Tony Hawk's Pro Skater 1 (USA) */
   { "T40204D   ", -1, -1, -1,  1,  -1, 1, 0, 0 },           /* Tony Hawk's Skateboarding (EU) */
   { "T13006N   ", -1, -1, -1,  1,  -1, 1, 0, 0 },           /* Tony Hawk's Pro Skater 2 */
   { "T13008D   ", -1, -1, -1,  1,  -1, 1, 0, 0 },           /* Tony Hawk's Pro Skater 2 */
   { "T23002N   ", -1, -1, -1,  1,  -1, 1, 0, 0 },           /* Star Wars Episode I: Jedi Power Battle (USA) */
   { "MK-51052  ", -1, -1, -1,  1,  -1, 1, 0, 0 },           /* Skies of Arcadia */
   { "HDR-0076  ", -1, -1, -1,  1,  -1, 1, 0, 0 },           /* Eternal Arcadia */
   { "MK-51007  ", -1, -1, -1,  1,  -1, 1, 0, 0 },           /* Flag to Flag */
   { "HDR-0013  ", -1, -1, -1,  1,  -1, 1, 0, 0 },           /* Super Speed Racing (JP) */
   { "6108099   ", -1, -1, -1,  1,  -1, 1, 0, 0 },           /* Yu Suzuki Game Works Vol. 1 */
   { "T2106M    ", -1, -1, -1,  1,  -1, 1, 0, 0 },           /* L.O.L */
   { "T18702M   ", -1, -1, -1,  1,  -1, 1, 0, 0 },           /* Miss Moonlight */
   { "T40401N   ", -1, -1, -1,  1,  -1, 1, 0, 0 },           /* Tom Clancy's Rainbow Six (US) */
   { "T-45001D05", -1, -1, -1,  1,  -1, 1, 0, 0 },           /* Tom Clancy's Rainbow Six incl. Eagle Watch Missions (EU) */

   /* Disable DIV matching */
   { "T15112N   ", -1, -1, -1,  -1,  1, 1, 0, 0 },           /* Demolition Racer */
	{ "T17714D50 ", -1, -1, -1,  -1,  1, 1, 0, 0 },           /* Donald Duck - Goin' Quackers / Quack Attack */
   { "T-8113N   ", -1, -1, -1,  -1,  1, 1, 0, 0 },           /* Ducati World - Racing Challenge (NTSC) */
   { "T-8121D-50", -1, -1, -1,  -1,  1, 1, 0, 0 },           /* Ducati World (PAL) */
   { "T40509D 50", -1, -1, -1,  -1,  1, 1, 0, 0 },           /* Aqua GT */
   { "17707N    ", -1, -1, -1,  -1,  1, 1, 0, 0 },           /* Rayman 2 (NTSC) */
   { "17707D    ", -1, -1, -1,  -1,  1, 1, 0, 0 },           /* Rayman 2 (PAL) */
   { "T20116M   ", -1, -1, -1,  -1,  1, 1, 0, 0 },           /* Elysion */
   { "T9507N    ", -1, -1, -1,  -1,  1, 1, 0, 0 },           /* Silent Scope (NTSC) */
   { "T9505D    ", -1, -1, -1,  -1,  1, 1, 0, 0 },           /* Silent Scope (PAL) */
   { "T1201N    ", -1, -1, -1,  -1,  1, 1, 0, 0 },           /* Power Stone (US) */
   { "T1201M    ", -1, -1, -1,  -1,  1, 1, 0, 0 },           /* Power Stone (JP) */
   { "T36801D 50", -1, -1, -1,  -1,  1, 1, 0, 0 },           /* Power Stone (PAL) */
   { "MK-51012  ", -1, -1, -1,  -1,  1, 1, 0, 0 },           /* Metropolis Street Racer (NTSC) */
   { "MK-5102250", -1, -1, -1,  -1,  1, 1, 0, 0 },           /* Metropolis Street Racer (PAL) */

   /* Extra depth scaling */
   { "MK-51182  ", -1, -1, -1,  -1,  1, 10000.f, 0, 0 },     /* NHL 2K2 */
   { "T-8109N   ", -1, -1, -1,  -1,  1, 100.f, 0, 0 },       /* Re-Volt (US) */
   { "T8107D  50", -1, -1, -1,  -1,  1, 100.f, 0, 0 },       /* Re-Volt (EU) */

   /* Disable VMEM32 (WinCE) */
   { "T14303M   ", -1, -1, -1,  -1, -1, 1,  1, 0 },           /* Super Producers */
   { "T45401D 50", -1, -1, -1,  -1, -1, 1,  1, 0 },           /* Giant Killers */
   { "T42101N 00", -1, -1, -1,  -1, -1, 1,  1, 0 },           /* Wild Metal (US) */
   { "T40501D-50", -1, -1, -1,  -1, -1, 1,  1, 0 },           /* Wild Metal (EU) */
   { "T1205N    ", -1, -1, -1,  -1, -1, 1,  1, 0 },           /* Resident Evil 2 (US) */
   { "T7004D  50", -1, -1, -1,  -1, -1, 1,  1, 0 },           /* Resident Evil 2 (EU) */
   { "T14304M   ", -1, -1, -1,  -1, -1, 1,  1, 0 },           /* Rune Jade */
   { "T5202M    ", -1, -1, -1,  -1, -1, 1,  1, 0 },           /* Marionette Company */
   { "T5203M    ", -1, -1, -1,  -1, -1, 1,  1, 0 },           /* Marionette Company 2 */
   { "T11010N   ", -1, -1, -1,  -1, -1, 1,  1, 0 },           /* Maximum Pool (online support) */
   { "T40209N   ", -1, -1, -1,  -1, -1, 1,  1, 0 },           /* StarLancer (US) (online support)  */
   { "T17723D 05", -1, -1, -1,  -1, -1, 1,  1, 0 },           /* StarLancer (EU) (online support)  */

	// Disable RGB Component
	{ "T40602N   ", -1, -1, -1,  -1, -1, 1,  0, 1 },           /* Centipede */
	{ "T9710N    ", -1, -1, -1,  -1, -1, 1,  0, 1 },           /* Gauntlet Legends (US) */
	{ "MK-51152  ", -1, -1, -1,  -1, -1, 1,  0, 1 },           /* World Series Baseball 2K2 */
	{ "T-9701N   ", -1, -1, -1,  -1, -1, 1,  0, 1 },           /* Mortal Kombat Gold (US) */
	{ "T1203N    ", -1, -1, -1,  -1, -1, 1,  0, 1 },           /* Street Fighter Alpha 3 (US) */
	{ "T1203M    ", -1, -1, -1,  -1, -1, 1,  0, 1 },           /* Street Fighter Zero 3 (JP) */
	{ "T13002N   ", -1, -1, -1,  -1, -1, 1,  0, 1 },           /* Vigilante 8 (US) */
	{ "T13003N   ", -1, -1, -1,  -1, -1, 1,  0, 1 },           /* Toy Story 2 (US) */
	{ "T1209N    ", -1, -1, -1,  -1, -1, 1,  0, 1 },           /* Gigawing (US) */
	{ "T1208M    ", -1, -1, -1,  -1, -1, 1,  0, 1 },           /* Gigawing (JP) */
	{ "T1235M    ", -1, -1, -1,  -1, -1, 1,  0, 1 },           /* Vampire Chronicle for Matching Service */

};

extern InputDescriptors gunsur2_inputs;
extern InputDescriptors ftspeed_inputs;
extern InputDescriptors maxspeed_inputs;
extern InputDescriptors monkeyba_inputs;
extern InputDescriptors cvs2_inputs;
extern InputDescriptors deathcox_inputs;

static struct game_type_naomi lut_games_naomi[] = 
{
   /* Div matching disabled */
   { "ALIEN FRONT"                       , -1, -1, -1, -1,  1, -1,  1 },                /* Alien Front */
   { "METAL SLUG 6"                      , -1, -1, -1, -1,  1, -1,  1 },                /* Metal Slug 6 */
   { "WAVE RUNNER GP"                    , -1, -1, -1, -1,  1, -1,  1 },                /* WaveRunner GP */
   { "STREET FIGHTER ZERO3 UPPER"        , -1, -1, -1, -1,  1, -1,  1 },                /* Street Fighter Zero 3 Upper */

   /* Extra Depth Scaling */
   { "SAMURAI SPIRITS 6"                 , -1, -1, -1, -1,  -1, -1, 1e26 },             /* Samurai Shodown VI */

   /* Translucent Polygon Depth Mask */
   { "COSMIC SMASH IN JAPAN"             , -1, -1,  1, -1,  -1, -1, 1 },                /* Cosmic Smash */

   /* Alternate Jamma I/O Setup */
   { "POWER STONE 2 JAPAN"               , -1, -1, -1, -1,  -1,  1, 1 },                /* Power Stone 2 (4 players, also needs to be set in service menu) */
   { "SHOOTOUT POOL"                     , -1, -1, -1, -1,  -1,  2, 1 },                /* Shootout Pool: rotary encoders */
   { "SHOOTOUT POOL MEDAL"               , -1, -1, -1, -1,  -1,  2, 1 },                /* Shootout Pool The Medal: rotary encoders */
   { "DYNAMIC GOLF"                      , -1, -1, -1, -1,  -1,  2, 1 },                /* Virtua Golf/Dynamic Golf: rotary encoders */
   { "CRACKIN'DJ  ver JAPAN"             , -1, -1, -1, -1,  -1,  2, 1 },                /* Crackin'DJ: rotary encoders */
   { "CRACKIN'DJ PART2  ver JAPAN"       , -1, -1, -1, -1,  -1,  2, 1 },                /* Crackin'DJ 2: rotary encoders */
   { "OUTTRIGGER     JAPAN"              , -1, -1, -1, -1,  -1,  2, 1 },                /* Outtrigger: rotary encoders */
   { "SEGA MARINE FISHING JAPAN"         , -1, -1, -1, -1,  -1,  3, 1 },                /* Sega Marine Fishing */
   { "RINGOUT 4X4 JAPAN"                 , -1, -1, -1, -1,  -1,  4, 1 },                /* Ring Out 4x4 (4 players, also needs to be set in service menu) */
   { "Sports Shooting USA"               , -1, -1, -1, -1,  -1,  6, 1 },                /* Sports Shooting USA (light guns) */
   { "SEGA CLAY CHALLENGE"               , -1, -1, -1, -1,  -1,  6, 1 },                /* Sega Clay Challenge (light guns) */
   { "RANGER MISSION"                    , -1, -1, -1, -1,  -1,  6, 1 },                /* Ranger Mission (light guns) */
   { "EXTREME HUNTING"                   , -1, -1, -1, -1,  -1,  6, 1 },                /* Extreme Hunting (light guns) */
   { "FASTER THAN SPEED"                 , -1, -1, -1, -1,  -1,  5, 1, &ftspeed_inputs },/* Faster Than Speed (analog axes) */
   { "MAXIMUM SPEED"                     , -1, -1, -1, -1,  -1,  5, 1, &maxspeed_inputs },/* Maximum Speed (analog axes) */
   { "BLOPON"                            , -1, -1, -1, -1,  -1,  5, 1 },                /* Block Pong (analog axes) */
   { "BASS FISHING SIMULATOR VER.A"      , -1, -1, -1, -1,  -1,  2, 1 },                /* Sega Bass Fishing Challenge (Track-ball) */
   { "DRIVE"                             , -1, -1, -1, -1,  -1,  2, 1 },                /* WaiWai Drive */
   { "KICK '4' CASH"                     , -1, -1, -1, -1,  -1,  2, 1 },                /* Kick '4' Cash */
   { "NINJA ASSAULT"                     , -1, -1, -1, -1,  -1,  7, 1 },                /* Ninja Assault */
   { "MAZAN"                             , -1, -1, -1, -1,  -1,  8, 1 },                /* Mazan */
   { "WORLD KICKS"                       , -1, -1, -1, -1,  -1, 10, 1 },                /* World Kicks */
   { "WORLD KICKS PCB"                   , -1,  -1, -1, -1,  -1, 11, 1 },                /* World Kicks 2 kit a.k.a. World Kicks PCB */

   /* Input descriptors */
   { " BIOHAZARD  GUN SURVIVOR2"         , -1, -1, -1, -1,  -1, -1, 1, &gunsur2_inputs },  /* Gun Survivor 2 Biohazard Code: Veronica */
   { "MONKEY BALL JAPAN VERSION"         , -1, -1, -1, -1,  -1, -1, 1, &monkeyba_inputs }, /* Monkey Ball */
   { "CAPCOM VS SNK 2  JAPAN"            , -1, -1, -1, -1,  -1, -1, 1, &cvs2_inputs },     /* Capcom Vs. SNK 2 Mark Of The Millennium 2001 */
   { "DEATH CRIMSON OX"                  , -1, -1, -1, -1,  -1, -1, 1, &deathcox_inputs }, /* Death Crimson OX */
};
