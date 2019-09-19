// nullDC.cpp : Makes magic cookies
//

//initialse Emu
#include "types.h"
#include "hw/mem/_vmem.h"
#include "hw/mem/vmem32.h"
#include "stdclass.h"

#include "types.h"
#include "hw/flashrom/flashrom.h"
#include "hw/maple/maple_cfg.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/arm7/arm7.h"

#include "hw/naomi/naomi_cart.h"

#include "reios/reios.h"
#include <libretro.h>

extern RomChip sys_rom;
extern SRamChip sys_nvmem_sram;
extern DCFlashChip sys_nvmem_flash;
unsigned FLASH_SIZE;
unsigned BBSRAM_SIZE;
unsigned BIOS_SIZE;
unsigned RAM_SIZE;
unsigned ARAM_SIZE;
unsigned VRAM_SIZE;
unsigned RAM_MASK;
unsigned ARAM_MASK;
unsigned VRAM_MASK;

settings_t settings;

extern char game_dir[1024];
extern char *game_data;
extern bool boot_to_bios;
extern bool reset_requested;

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
   if (settings.System != DC_PLATFORM_DREAMCAST)
   {
      if (!naomi_cart_SelectFile(s, len))
         return -1;
   }

   if (s32 rv = libAICA_Init())
      return rv;

   init_mem();
   arm_Init();

   //if (s32 rv = libExtDevice_Init())
   //	return rv;



   return 0;
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
		INFO_LOG(DYNAREC, "Using Recompiler");
	}
	else
#endif
	{
		Get_Sh4Interpreter(&sh4_cpu);
		INFO_LOG(INTERPRETER, "Using Interpreter");
	}
   sh4_cpu.Reset(false);
}

static void LoadSpecialSettings(void)
{
   unsigned i;

   char prod_id[sizeof(ip_meta.product_number) + 1] = {0};
   memcpy(prod_id, ip_meta.product_number, sizeof(ip_meta.product_number));

   NOTICE_LOG(BOOT, "[LUT]: Product number: %s.", prod_id);
	if (ip_meta.isWindowsCE() || settings.dreamcast.ForceWinCE
			|| !strncmp("T26702N", prod_id, 7)) // PBA Tour Bowling 2001
	{
		NOTICE_LOG(BOOT, "Enabling Full MMU and Extra depth scaling for Windows CE game");
		settings.rend.ExtraDepthScale = 0.1;
		settings.dreamcast.FullMMU = true;
		settings.aica.NoBatch = 1;
	}
   for (i = 0; i < sizeof(lut_games)/sizeof(lut_games[0]); i++)
   {
      if (!strncmp(lut_games[i].product_number, prod_id, sizeof(prod_id) - 1))
      {
      	INFO_LOG(BOOT, "[LUT]: Found game in LUT database..");

         if (lut_games[i].dynarec_type != -1)
         {
         	NOTICE_LOG(BOOT, "[Hack]: Applying dynarec type hack.");
            settings.dynarec.Type = lut_games[i].dynarec_type;
            LoadSpecialSettingsCPU();
         }

         if (lut_games[i].alpha_sort_mode != -1)
         {
         	NOTICE_LOG(BOOT, "[Hack]: Applying alpha sort hack.");
            settings.pvr.Emulation.AlphaSortMode = lut_games[i].alpha_sort_mode;
         }

         if (lut_games[i].updatemode_type != -1)
         {
         	NOTICE_LOG(BOOT, "[Hack]: Applying update mode type hack.");
            settings.UpdateModeForced = 1;
         }

         if (lut_games[i].translucentPolygonDepthMask != -1)
         {
         	NOTICE_LOG(BOOT, "[Hack]: Applying translucent polygon depth mask hack.");
            settings.rend.TranslucentPolygonDepthMask = lut_games[i].translucentPolygonDepthMask;
         }

         if (lut_games[i].rendertotexturebuffer != -1)
         {
         	NOTICE_LOG(BOOT, "[Hack]: Applying rendertotexture hack.");
            settings.rend.RenderToTextureBuffer = lut_games[i].rendertotexturebuffer;
         }

         if (lut_games[i].disable_div != -1 &&
               settings.dynarec.AutoDivMatching)
         {
         	NOTICE_LOG(BOOT, "[Hack]: Applying Disable DIV hack.");
            settings.dynarec.DisableDivMatching = lut_games[i].disable_div;
         }
         if (lut_games[i].extra_depth_scale != 1 && settings.rend.AutoExtraDepthScale)
         {
         	NOTICE_LOG(BOOT, "[Hack]: Applying auto extra depth scale.");
         	settings.rend.ExtraDepthScale = lut_games[i].extra_depth_scale;
         }
         if (lut_games[i].disable_vmem32 == 1)
         {
         	NOTICE_LOG(BOOT, "[Hack]: Disabling WinCE virtual memory.");
            settings.dynarec.disable_vmem32 = true;
         }

         break;
      }
   }
	std::string areas(ip_meta.area_symbols, sizeof(ip_meta.area_symbols));
	bool region_usa = areas.find('U') != std::string::npos;
	bool region_eu = areas.find('E') != std::string::npos;
	bool region_japan = areas.find('J') != std::string::npos;
	if (region_usa || region_eu || region_japan)
	{
		switch (settings.dreamcast.region)
		{
		case 0: // Japan
			if (!region_japan)
			{
				NOTICE_LOG(BOOT, "Japan region not supported. Using %s instead", region_usa ? "USA" : "Europe");
				settings.dreamcast.region = region_usa ? 1 : 2;
			}
			break;
		case 1: // USA
			if (!region_usa)
			{
				NOTICE_LOG(BOOT, "USA region not supported. Using %s instead", region_eu ? "Europe" : "Japan");
				settings.dreamcast.region = region_eu ? 2 : 0;
			}
			break;
		case 2: // Europe
			if (!region_eu)
			{
				NOTICE_LOG(BOOT, "Europe region not supported. Using %s instead", region_usa ? "USA" : "Japan");
				settings.dreamcast.region = region_usa ? 1 : 0;
			}
			break;
		case 3: // Default
			if (region_usa)
				settings.dreamcast.region = 1;
			else if (region_eu)
				settings.dreamcast.region = 2;
			else
				settings.dreamcast.region = 0;
				break;
		}
	}
	else
		WARN_LOG(BOOT, "No region specified in IP.BIN");
	if (settings.dreamcast.cable <= 1 && !ip_meta.supportsVGA())
	{
		NOTICE_LOG(BOOT, "Game doesn't support VGA. Using TV Composite instead");
		settings.dreamcast.cable = 3;
	}
}

static void LoadSpecialSettingsNaomi(const char *name)
{
   unsigned i;

   NOTICE_LOG(BOOT, "[LUT]: Naomi ROM name is: %s.", name);
   for (i = 0; i < sizeof(lut_games_naomi)/sizeof(lut_games_naomi[0]); i++)
   {
      if (strstr(lut_games_naomi[i].product_number, name))
      {
      	INFO_LOG(BOOT, "[LUT]: Found game in LUT database..");

         if (lut_games_naomi[i].dynarec_type != -1)
         {
         	NOTICE_LOG(BOOT, "[Hack]: Applying dynarec type hack.");
            settings.dynarec.Type = lut_games_naomi[i].dynarec_type;
            LoadSpecialSettingsCPU();
         }

         if (lut_games_naomi[i].alpha_sort_mode != -1)
         {
         	NOTICE_LOG(BOOT, "[Hack]: Applying alpha sort hack.");
            settings.pvr.Emulation.AlphaSortMode = lut_games_naomi[i].alpha_sort_mode;
         }

         if (lut_games_naomi[i].updatemode_type != -1)
         {
         	NOTICE_LOG(BOOT, "[Hack]: Applying update mode type hack.");
            settings.UpdateModeForced = 1;
         }

         if (lut_games_naomi[i].translucentPolygonDepthMask != -1)
         {
         	NOTICE_LOG(BOOT, "[Hack]: Applying translucent polygon depth mask hack.");
            settings.rend.TranslucentPolygonDepthMask = lut_games_naomi[i].translucentPolygonDepthMask;
         }

         if (lut_games_naomi[i].rendertotexturebuffer != -1)
         {
         	NOTICE_LOG(BOOT, "[Hack]: Applying rendertotexture hack.");
            settings.rend.RenderToTextureBuffer = lut_games_naomi[i].rendertotexturebuffer;
         }

         if (lut_games_naomi[i].disable_div != -1 &&
               settings.dynarec.AutoDivMatching)
         {
         	NOTICE_LOG(BOOT, "[Hack]: Applying Disable DIV hack.");
            settings.dynarec.DisableDivMatching = lut_games_naomi[i].disable_div;
         }

         if (lut_games_naomi[i].jamma_setup != -1)
         {
         	NOTICE_LOG(BOOT, "[Hack]: Applying alternate Jamma I/O board setup.");
            settings.mapping.JammaSetup = lut_games_naomi[i].jamma_setup;
         }

         if (lut_games_naomi[i].extra_depth_scale != 1 && settings.rend.AutoExtraDepthScale)
         {
         	NOTICE_LOG(BOOT, "[Hack]: Applying auto extra depth scale.");
            settings.rend.ExtraDepthScale = lut_games_naomi[i].extra_depth_scale;
         }

         if (lut_games_naomi[i].game_inputs != NULL)
         {
         	NOTICE_LOG(BOOT, "Setting custom input descriptors\n");
         	naomi_game_inputs = lut_games_naomi[i].game_inputs;
         }

         break;
      }
   }
}

void dc_prepare_system(void)
{
   BBSRAM_SIZE             = (32*1024);

   switch (settings.System)
   {
      case DC_PLATFORM_DREAMCAST:
         //DC : 16 mb ram, 8 mb vram, 2 mb aram, 2 mb bios, 128k flash
         FLASH_SIZE        = (128*1024);
         BIOS_SIZE         = (2*1024*1024);
         RAM_SIZE          = (16*1024*1024);
         ARAM_SIZE         = (2*1024*1024);
         VRAM_SIZE         = (8*1024*1024);
         sys_nvmem_flash.Allocate(FLASH_SIZE);
         sys_rom.Allocate(BIOS_SIZE);
         break;
      case DC_PLATFORM_DEV_UNIT:
         //Devkit : 32 mb ram, 8? mb vram, 2? mb aram, 2? mb bios, ? flash
         FLASH_SIZE        = (128*1024);
         BIOS_SIZE         = (2*1024*1024);
         RAM_SIZE          = (32*1024*1024);
         ARAM_SIZE         = (2*1024*1024);
         VRAM_SIZE         = (8*1024*1024);
         sys_nvmem_flash.Allocate(FLASH_SIZE);
         sys_rom.Allocate(BIOS_SIZE);
         break;
      case DC_PLATFORM_NAOMI:
         //Naomi : 32 mb ram, 16 mb vram, 8 mb aram, 2 mb bios, ? flash
         BIOS_SIZE         = (2*1024*1024);
         RAM_SIZE          = (32*1024*1024);
         ARAM_SIZE         = (8*1024*1024);
         VRAM_SIZE         = (16*1024*1024);
         sys_nvmem_sram.Allocate(BBSRAM_SIZE);
         sys_rom.Allocate(BIOS_SIZE);
         break;
      case DC_PLATFORM_NAOMI2:
         //Naomi2 : 32 mb ram, 16 mb vram, 8 mb aram, 2 mb bios, ? flash
         BIOS_SIZE         = (2*1024*1024);
         RAM_SIZE          = (32*1024*1024);
         ARAM_SIZE         = (8*1024*1024);
         VRAM_SIZE         = (16*1024*1024);
         sys_nvmem_sram.Allocate(BBSRAM_SIZE);
         sys_rom.Allocate(BIOS_SIZE);
         break;
      case DC_PLATFORM_ATOMISWAVE:
         //Atomiswave : 16 mb ram, 8 mb vram, 8 mb aram, 128kb bios+flash, 128k BBSRAM
         FLASH_SIZE        = 0;
         BIOS_SIZE         = (128*1024);
         RAM_SIZE          = (16*1024*1024);
         ARAM_SIZE         = (8*1024*1024);
         VRAM_SIZE         = (8*1024*1024);
         BBSRAM_SIZE       = (128*1024);
         sys_nvmem_flash.Allocate(BIOS_SIZE);
         sys_nvmem_flash.write_protect_size = BIOS_SIZE / 2;
         sys_nvmem_sram.Allocate(BBSRAM_SIZE);
         break;
   }

   RAM_MASK         = (RAM_SIZE-1);
   ARAM_MASK        = (ARAM_SIZE-1);
   VRAM_MASK        = (VRAM_SIZE-1);
}

void dc_reset()
{
	plugins_Reset(true);
	mem_Reset(true);

	sh4_cpu.Reset(true);
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
		ERROR_LOG(VMEM, "Failed to alloc mem");
		return -1;
	}
	reios_init();

	LoadSettings();

	int rv= 0;

   extern char game_dir_no_slash[1024];

   char new_system_dir[1024];

#ifdef _WIN32
   sprintf(new_system_dir, "%s\\", game_dir_no_slash);
#else
   sprintf(new_system_dir, "%s/", game_dir_no_slash);
#endif

   if (settings.System == DC_PLATFORM_DREAMCAST)
   {
      if (settings.bios.UseReios || !LoadRomFiles(new_system_dir))
      {
         if (!LoadHle(new_system_dir))
            return -3;
         WARN_LOG(COMMON, "Did not load bios, using reios");
      }
   }
   else
   {
   	LoadRomFiles(new_system_dir);
   }
   LoadSpecialSettingsCPU();

	sh4_cpu.Init();
	mem_Init();

#ifdef _WIN64
	extern void setup_seh();
	setup_seh();
#endif

	if (plugins_Init(name, sizeof(name)))
	   return -4;
	
	mem_map_default();

	mcfg_CreateDevices();

	dc_reset();

	switch (settings.System)
	{
		case DC_PLATFORM_DREAMCAST:
			reios_disk_id();
			LoadSpecialSettings();
			break;
		case DC_PLATFORM_ATOMISWAVE:
		case DC_PLATFORM_NAOMI:
			LoadSpecialSettingsNaomi(naomi_game_id);
			break;
	}
	FixUpFlash();

	return rv;
}

void dc_run(void)
{
   sh4_cpu.Run();
}

void dc_term(void)
{
	SaveRomFiles(get_writable_data_path(""));
	sh4_cpu.Term();
	naomi_cart_Close();
	plugins_Term();
	mem_Term();
	_vmem_release();
}

void dc_stop()
{
	sh4_cpu.Stop();
}


void dc_start()
{
	sh4_cpu.Start();
}

bool dc_is_running()
{
	return sh4_cpu.IsCpuRunning();
}

// Called on the emulator thread for soft reset
void dc_request_reset()
{
	reset_requested = true;
	dc_stop();
}

void LoadSettings(void)
{
   settings.dynarec.Enable			= 1;
	settings.dynarec.idleskip		= 1;
	settings.dynarec.unstable_opt	= 0; 
   //settings.dynarec.DisableDivMatching       = 0;
	//disable_nvmem can't be loaded, because nvmem init is before cfg load
	settings.dynarec.disable_vmem32 = false;
   settings.UpdateModeForced     = 0;
	settings.dreamcast.RTC			= GetRTC_now();
	settings.dreamcast.FullMMU		= false;
	settings.aica.LimitFPS			= 0;
   settings.aica.NoSound			= 0;
	settings.pvr.subdivide_transp	= 0;
	settings.pvr.ta_skip			   = 0;
   //settings.pvr.Emulation.AlphaSortMode= 0;
   settings.pvr.Emulation.zMin         = 0.f;
   settings.pvr.Emulation.zMax         = 1.0f;

	settings.pvr.MaxThreads			       = 3;
#ifndef __LIBRETRO__
   settings.pvr.Emulation.ModVol       = true;
   settings.rend.RenderToTextureBuffer  = false;
   settings.rend.RenderToTexture        = true;
   settings.rend.RenderToTextureUpscale = 1;
   settings.rend.MaxFilteredTextureSize = 256;
   settings.pvr.SynchronousRendering	 = 0;
#endif
   settings.rend.AutoExtraDepthScale    = true;
   settings.rend.ExtraDepthScale        = 1.f;

   settings.rend.Clipping               = true;


   settings.rend.ModifierVolumes        = true;
   settings.rend.TranslucentPolygonDepthMask = false;

	settings.debug.SerialConsole         = 0;

	settings.reios.ElfFile               = "";

	settings.validate.OpenGlChecks      = 0;
}

void SaveSettings(void)
{
}

