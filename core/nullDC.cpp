// nullDC.cpp : Makes magic cookies
//

//initialse Emu
#include "types.h"
#include "hw/mem/_vmem.h"
#include "stdclass.h"

#include "types.h"
#include "hw/maple/maple_cfg.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/arm7/arm7.h"

#include "hw/naomi/naomi_cart.h"

#include "reios/reios.h"
#include <libretro.h>

unsigned ARAM_SIZE;
unsigned VRAM_SIZE;
unsigned ARAM_MASK;
unsigned VRAM_MASK;

extern retro_log_printf_t         log_cb;
settings_t settings;

extern char game_dir[1024];
extern char *game_data;
extern bool boot_to_bios;

extern void init_mem();
extern void arm_Init();
extern void term_mem();

/*
	libndc

	//initialise (and parse the command line)
	ndc_init(argc,argv);

	...
	//run a dreamcast slice
	//either a frame, or up to 25 ms of emulation
	//returns 1 if the frame is ready (fb needs to be flipped -- i'm looking at you android)
	ndc_step();

	...
	//terminate (and free everything)
	ndc_term()
*/

int GetFile(char *szFileName, char *szParse=0,u32 flags=0) 
{
   if (!boot_to_bios)
   {
      strcpy(szFileName, game_data);
      strcpy(settings.imgread.DefaultImage, szFileName);
   }

	return 1; 
}


s32 plugins_Init(char *s, size_t len)
{
   if (s32 rv = libPvr_Init())
      return rv;

   if (s32 rv = libGDR_Init())
      return rv;
#if DC_PLATFORM == DC_PLATFORM_NAOMI
   if (!naomi_cart_SelectFile(s, len))
      return rv_serror;
#endif

   if (s32 rv = libAICA_Init())
      return rv;

   init_mem();
   arm_Init();

   //if (s32 rv = libExtDevice_Init())
   //	return rv;



   return rv_ok;
}

void plugins_Term(void)
{
   //term all plugins
   //libExtDevice_Term();
   
	term_mem();
	//arm7_Term ?
   libAICA_Term();

   libGDR_Term();
   libPvr_Term();
}

void plugins_Reset(bool Manual)
{
	libPvr_Reset(Manual);
	libGDR_Reset(Manual);
	libAICA_Reset(Manual);

	arm_Reset();
	arm_SetEnabled(false);

	//libExtDevice_Reset(Manual);
}

#include "rom_luts.h"

static void LoadSpecialSettingsCPU(void)
{
#if FEAT_SHREC != DYNAREC_NONE
	if(settings.dynarec.Enable)
	{
		Get_Sh4Recompiler(&sh4_cpu);
		log_cb(RETRO_LOG_INFO, "Using Recompiler\n");
	}
	else
#endif
	{
		Get_Sh4Interpreter(&sh4_cpu);
		log_cb(RETRO_LOG_INFO, "Using Interpreter\n");
	}
   sh4_cpu.Reset(false);
}

extern bool update_zmax;

static void LoadSpecialSettings(void)
{
   unsigned i;

   log_cb(RETRO_LOG_INFO, "[LUT]: Product number: %s.\n", reios_product_number);
   for (i = 0; i < sizeof(lut_games)/sizeof(lut_games[0]); i++)
   {
      if (strstr(lut_games[i].product_number, reios_product_number))
      {
         log_cb(RETRO_LOG_INFO, "[LUT]: Found game in LUT database..\n");

         if (lut_games[i].aica_interrupt_hack != -1)
            settings.aica.InterruptHack   = lut_games[i].aica_interrupt_hack;

         if (lut_games[i].dynarec_type != -1)
         {
            log_cb(RETRO_LOG_INFO, "[Hack]: Applying dynarec type hack.\n");
            settings.dynarec.Type = lut_games[i].dynarec_type;
            LoadSpecialSettingsCPU();
         }

         if (lut_games[i].alpha_sort_mode != -1)
         {
            log_cb(RETRO_LOG_INFO, "[Hack]: Applying alpha sort hack.\n");
            settings.pvr.Emulation.AlphaSortMode = lut_games[i].alpha_sort_mode;
         }

         if (lut_games[i].updatemode_type != -1)
         {
            log_cb(RETRO_LOG_INFO, "[Hack]: Applying update mode type hack.\n");
            settings.UpdateModeForced = 1;
         }

         if (lut_games[i].translucentPolygonDepthMask != -1)
         {
            log_cb(RETRO_LOG_INFO, "[Hack]: Applying translucent polygon depth mask hack.\n");
            settings.rend.TranslucentPolygonDepthMask = lut_games[i].translucentPolygonDepthMask;
         }

         if (lut_games[i].rendertotexturebuffer != -1)
         {
            log_cb(RETRO_LOG_INFO, "[Hack]: Applying rendertotexture hack.\n");
            settings.rend.RenderToTextureBuffer = lut_games[i].rendertotexturebuffer;
         }

         if (lut_games[i].eg_hack != -1)
         {
            log_cb(RETRO_LOG_INFO, "[Hack]: Applying EG hack.\n");
            settings.aica.EGHack = lut_games[i].eg_hack;
         }

         break;
      }
   }
}

static void LoadSpecialSettingsNaomi(const char *name)
{
   unsigned i;

   log_cb(RETRO_LOG_INFO, "[LUT]: Naomi ROM name is: %s.\n", name);
   for (i = 0; i < sizeof(lut_games_naomi)/sizeof(lut_games_naomi[0]); i++)
   {
      if (strstr(lut_games_naomi[i].product_number, reios_product_number))
      {
         log_cb(RETRO_LOG_INFO, "[LUT]: Found game in LUT database..\n");

         if (lut_games_naomi[i].aica_interrupt_hack != -1)
            settings.aica.InterruptHack   = lut_games_naomi[i].aica_interrupt_hack;

         if (lut_games_naomi[i].dynarec_type != -1)
         {
            log_cb(RETRO_LOG_INFO, "[Hack]: Applying dynarec type hack.\n");
            settings.dynarec.Type = lut_games_naomi[i].dynarec_type;
            LoadSpecialSettingsCPU();
         }

         if (lut_games_naomi[i].alpha_sort_mode != -1)
         {
            log_cb(RETRO_LOG_INFO, "[Hack]: Applying alpha sort hack.\n");
            settings.pvr.Emulation.AlphaSortMode = lut_games_naomi[i].alpha_sort_mode;
         }

         if (lut_games_naomi[i].updatemode_type != -1)
         {
            log_cb(RETRO_LOG_INFO, "[Hack]: Applying update mode type hack.\n");
            settings.UpdateModeForced = 1;
         }

         if (lut_games_naomi[i].translucentPolygonDepthMask != -1)
         {
            log_cb(RETRO_LOG_INFO, "[Hack]: Applying translucent polygon depth mask hack.\n");
            settings.rend.TranslucentPolygonDepthMask = lut_games_naomi[i].translucentPolygonDepthMask;
         }

         if (lut_games_naomi[i].rendertotexturebuffer != -1)
         {
            log_cb(RETRO_LOG_INFO, "[Hack]: Applying rendertotexture hack.\n");
            settings.rend.RenderToTextureBuffer = lut_games_naomi[i].rendertotexturebuffer;
         }

         if (lut_games_naomi[i].eg_hack != -1)
         {
            log_cb(RETRO_LOG_INFO, "[Hack]: Applying EG hack.\n");
            settings.aica.EGHack = lut_games_naomi[i].eg_hack;
         }

         break;
      }
   }
}

void dc_prepare_system(void)
{
   switch (settings.System)
   {
      case DC_PLATFORM_DREAMCAST:
         ARAM_SIZE         = (2*1024*1024);
         VRAM_SIZE         = (8*1024*1024);
         break;
      case DC_PLATFORM_DEV_UNIT:
         ARAM_SIZE         = (2*1024*1024);
         VRAM_SIZE         = (8*1024*1024);
         break;
      case DC_PLATFORM_NAOMI:
         ARAM_SIZE         = (8*1024*1024);
         VRAM_SIZE         = (16*1024*1024);
         break;
      case DC_PLATFORM_NAOMI2:
         ARAM_SIZE         = (8*1024*1024);
         VRAM_SIZE         = (16*1024*1024);
         break;
      case DC_PLATFORM_ATOMISWAVE:
         ARAM_SIZE         = (8*1024*1024);
         VRAM_SIZE         = (16*1024*1024);
         break;
   }

   ARAM_MASK        = (ARAM_SIZE-1);
   VRAM_MASK        = (VRAM_SIZE-1);
}

int dc_init(int argc,wchar* argv[])
{
   char name[128];

   name[0] = '\0';

	setbuf(stdin,0);
	setbuf(stdout,0);
	setbuf(stderr,0);
   extern void common_libretro_setup(void);
   common_libretro_setup();

	if (!_vmem_reserve())
	{
		log_cb(RETRO_LOG_INFO, "Failed to alloc mem\n");
		return -1;
	}

   LoadSettings();
	os_CreateWindow();

	int rv= 0;

   extern char game_dir_no_slash[1024];

   char new_system_dir[1024];

#ifdef _WIN32
   sprintf(new_system_dir, "%s\\", game_dir_no_slash);
#else
   sprintf(new_system_dir, "%s/", game_dir_no_slash);
#endif

   if (
         settings.bios.UseReios || !LoadRomFiles(new_system_dir)
      )
	{
      if (!LoadHle(new_system_dir))
			return -3;
      log_cb(RETRO_LOG_WARN, "Did not load bios, using reios\n");
	}

   LoadSpecialSettingsCPU();

	sh4_cpu.Init();
	mem_Init();

	plugins_Init(name, sizeof(name));
	
	mem_map_default();

	mcfg_CreateDevices();

	plugins_Reset(false);
	mem_Reset(false);
	
	sh4_cpu.Reset(false);

   const char* bootfile = reios_locate_ip();
   if (!bootfile || !reios_locate_bootfile("1ST_READ.BIN"))
      log_cb(RETRO_LOG_ERROR, "Failed to locate bootfile.\n");

   LoadSpecialSettings();
#if DC_PLATFORM == DC_PLATFORM_NAOMI
   LoadSpecialSettingsNaomi(name);
#endif

	return rv;
}

void dc_run(void)
{
   sh4_cpu.Run();
}

void dc_term(void)
{
	sh4_cpu.Term();
	plugins_Term();
	_vmem_release();

	SaveRomFiles(get_writable_data_path(""));
}

void LoadSettings(void)
{
	settings.dynarec.Enable			= 1;
	settings.dynarec.idleskip		= 1;
	settings.dynarec.unstable_opt	= 0; 
	//disable_nvmem can't be loaded, because nvmem init is before cfg load
   settings.UpdateModeForced     = 0;
	settings.dreamcast.RTC			= GetRTC_now();
	settings.aica.LimitFPS			= 0;
	settings.aica.NoBatch			= 0;
   settings.aica.NoSound			= 0;
   settings.aica.EGHack          = 0;
	settings.pvr.subdivide_transp	= 0;
	settings.pvr.ta_skip			   = 0;
	settings.pvr.rend				   = 0;
   settings.QueueRender          = 0;
   settings.pvr.Emulation.AlphaSortMode= 0;
   settings.pvr.Emulation.zMin         = 0.f;
   settings.pvr.Emulation.zMax         = 1.0f;

#if !defined(NO_MMU)
   settings.MMUEnabled                  = true;
#endif
	settings.pvr.MaxThreads			       = 3;
#ifndef __LIBRETRO__
   settings.pvr.Emulation.ModVol       = true;
   settings.rend.RenderToTexture        = true;
   settings.rend.RenderToTextureBuffer  = false;
#endif
   settings.rend.TranslucentPolygonDepthMask = false;
	settings.pvr.SynchronousRendering	 = 0;

	settings.debug.SerialConsole         = 0;

	settings.reios.ElfFile               = "";

	settings.validate.OpenGlChecks      = 0;

	settings.bios.UseReios              = 0;
}

void SaveSettings(void)
{
}
