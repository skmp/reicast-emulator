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
   JVS jamma_setup;
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

   /* Extra depth scaling */
   { "MK-51182  ", -1, -1, -1,  -1, -1, 10000.f, 0, 0 },     /* NHL 2K2 */
   { "T-8109N   ", -1, -1, -1,  -1, -1, 100.f, 0, 0 },       /* Re-Volt (US) */
   { "T8107D  50", -1, -1, -1,  -1, -1, 100.f, 0, 0 },       /* Re-Volt (EU) */

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
   /* Extra Depth Scaling */
   { "SAMURAI SPIRITS 6"                 , -1, -1, -1, -1,  -1, JVS::Default, 1e26 },             /* Samurai Shodown VI */

   /* Translucent Polygon Depth Mask */
   { "COSMIC SMASH IN JAPAN"             , -1, -1,  1, -1,  -1, JVS::Default, 1 },                /* Cosmic Smash */

   /* Alternate Jamma I/O Setup */
   { "POWER STONE 2 JAPAN"               , -1, -1, -1, -1,  -1, JVS::FourPlayers, 1 },            /* Power Stone 2 (4 players, also needs to be set in service menu) */
   { "GUILTY GEAR isuka"                 , -1, -1, -1, -1,  -1, JVS::FourPlayers, 1 },            /* Guilty Gear (AW) */
   { "SHOOTOUT POOL"                     , -1, -1, -1, -1,  -1, JVS::RotaryEncoders, 1 },         /* Shootout Pool: rotary encoders */
   { "SHOOTOUT POOL MEDAL"               , -1, -1, -1, -1,  -1, JVS::RotaryEncoders, 1 },         /* Shootout Pool The Medal: rotary encoders */
   { "DYNAMIC GOLF"                      , -1, -1, -1, -1,  -1, JVS::RotaryEncoders, 1 },         /* Virtua Golf/Dynamic Golf: rotary encoders */
   { "CRACKIN'DJ  ver JAPAN"             , -1, -1, -1, -1,  -1, JVS::RotaryEncoders, 1 },         /* Crackin'DJ: rotary encoders */
   { "CRACKIN'DJ PART2  ver JAPAN"       , -1, -1, -1, -1,  -1, JVS::RotaryEncoders, 1 },         /* Crackin'DJ 2: rotary encoders */
   { "OUTTRIGGER     JAPAN"              , -1, -1, -1, -1,  -1, JVS::OutTrigger, 1 },             /* Outtrigger: rotary encoders */
   { "SEGA MARINE FISHING JAPAN"         , -1, -1, -1, -1,  -1, JVS::SegaMarineFishing, 1 },      /* Sega Marine Fishing */
   { "RINGOUT 4X4 JAPAN"                 , -1, -1, -1, -1,  -1, JVS::DualIOBoards4P, 1 },         /* Ring Out 4x4 (4 players, also needs to be set in service menu) */
   { "Sports Shooting USA"               , -1, -1, -1, -1,  -1, JVS::LightGun, 1 },               /* Sports Shooting USA (light guns) */
   { "SEGA CLAY CHALLENGE"               , -1, -1, -1, -1,  -1, JVS::LightGun, 1 },               /* Sega Clay Challenge (light guns) */
   { "RANGER MISSION"                    , -1, -1, -1, -1,  -1, JVS::LightGun, 1 },               /* Ranger Mission (light guns) */
   { "EXTREME HUNTING"                   , -1, -1, -1, -1,  -1, JVS::LightGun, 1 },               /* Extreme Hunting (light guns) */
   { "FASTER THAN SPEED"                 , -1, -1, -1, -1,  -1, JVS::Analog, 1, &ftspeed_inputs },/* Faster Than Speed (analog axes) */
   { "MAXIMUM SPEED"                     , -1, -1, -1, -1,  -1, JVS::Analog, 1, &maxspeed_inputs },/* Maximum Speed (analog axes) */
   { "BLOPON"                            , -1, -1, -1, -1,  -1, JVS::Analog, 1 },                 /* Block Pong (analog axes) */
   { "BASS FISHING SIMULATOR VER.A"      , -1, -1, -1, -1,  -1, JVS::RotaryEncoders, 1 },         /* Sega Bass Fishing Challenge (Track-ball) */
   { "DRIVE"                             , -1, -1, -1, -1,  -1, JVS::RotaryEncoders, 1 },         /* WaiWai Drive */
   { "KICK '4' CASH"                     , -1, -1, -1, -1,  -1, JVS::RotaryEncoders, 1 },         /* Kick '4' Cash */
   { "NINJA ASSAULT"                     , -1, -1, -1, -1,  -1, JVS::LightGun, 1 },               /* Ninja Assault */
   { "MAZAN"                             , -1, -1, -1, -1,  -1, JVS::Mazan, 1 },                  /* Mazan */
   { "WORLD KICKS"                       , -1, -1, -1, -1,  -1, JVS::WorldKicks, 1 },             /* World Kicks */
   { "WORLD KICKS PCB"                   , -1, -1, -1, -1,  -1, JVS::WorldKicksPCB, 1 },          /* World Kicks 2 kit a.k.a. World Kicks PCB */
   { "THE MAZE OF THE KINGS"             , -1, -1, -1, -1,  -1, JVS::LightGunAsAnalog, 1 },
   { " CONFIDENTIAL MISSION ---------"   , -1, -1, -1, -1,  -1, JVS::LightGunAsAnalog, 1 },
   { "hotd2"                             , -1, -1, -1, -1,  -1, JVS::LightGunAsAnalog, 1 },		  /* The House of The Dead 2 */
   { "hotd2o"                            , -1, -1, -1, -1,  -1, JVS::LightGunAsAnalog, 1 },		  /* The House of The Dead 2 */
   { "hotd2e"                            , -1, -1, -1, -1,  -1, JVS::LightGunAsAnalog, 1 },		  /* The House of The Dead 2 (export) */
   { "hotd2p"                            , -1, -1, -1, -1,  -1, JVS::LightGunAsAnalog, 1 },		  /* The House of The Dead 2 (prototype) */
   { "INU NO OSANPO"                     , -1, -1, -1, -1,  -1, JVS::DogWalking, 1 },   		  	  /* Dog Walking */
   { " TOUCH DE UNOH -------------"      , -1, -1, -1, -1,  -1, JVS::TouchDeUno, 1 },				  /* Touch de Uno! */

   /* Input descriptors */
   { " BIOHAZARD  GUN SURVIVOR2"         , -1, -1, -1, -1,  -1, JVS::GunSurvivor, 1, &gunsur2_inputs }, /* Gun Survivor 2 Biohazard Code: Veronica */
   { "MONKEY BALL JAPAN VERSION"         , -1, -1, -1, -1,  -1, JVS::Default, 1, &monkeyba_inputs },/* Monkey Ball */
   { "CAPCOM VS SNK 2  JAPAN"            , -1, -1, -1, -1,  -1, JVS::Default, 1, &cvs2_inputs },  /* Capcom Vs. SNK 2 Mark Of The Millennium 2001 */
   { "DEATH CRIMSON OX"                  , -1, -1, -1, -1,  -1, JVS::LightGunAsAnalog, 1, &deathcox_inputs }, /* Death Crimson OX */
};
