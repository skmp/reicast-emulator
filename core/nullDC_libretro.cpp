// nullDC.cpp : Makes magic cookies
//

//initialse Emu
#include "types.h"
#include "oslib/oslib.h"
#include "oslib/audiostream.h"
#include "hw/mem/_vmem.h"
#include "stdclass.h"
#include "cfg/cfg.h"

#include "types.h"
#include "hw/maple/maple_cfg.h"
#include "hw/sh4/sh4_mem.h"

#include "hw/naomi/naomi_cart.h"

settings_t settings;

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
   strcpy(szFileName, "/home/squarepusher/roms/dc/scalibur.cdi");

	return 1; 
}


s32 plugins_Init(void)
{
   if (s32 rv = libPvr_Init())
      return rv;

   if (s32 rv = libGDR_Init())
      return rv;
#if DC_PLATFORM == DC_PLATFORM_NAOMI
   if (!naomi_cart_SelectFile())
      return rv_serror;
#endif

   if (s32 rv = libAICA_Init())
      return rv;

   if (s32 rv = libARM_Init())
      return rv;

   //if (s32 rv = libExtDevice_Init())
   //	return rv;



   return rv_ok;
}

void plugins_Term(void)
{
   //term all plugins
   //libExtDevice_Term();
   libARM_Term();
   libAICA_Term();
   libGDR_Term();
   libPvr_Term();
}

void plugins_Reset(bool Manual)
{
	libPvr_Reset(Manual);
	libGDR_Reset(Manual);
	libAICA_Reset(Manual);
	libARM_Reset(Manual);
	//libExtDevice_Reset(Manual);
}

int dc_init(int argc,wchar* argv[])
{
	setbuf(stdin,0);
	setbuf(stdout,0);
	setbuf(stderr,0);
	if (!_vmem_reserve())
	{
		printf("Failed to alloc mem\n");
		return -1;
	}

   LoadSettings();
	os_CreateWindow();

	int rv= 0;

#define DATA_PATH "/home/squarepusher/roms/dc/data"
    
	if (settings.bios.UseReios || !LoadRomFiles(get_readonly_data_path(DATA_PATH)))
	{
		if (!LoadHle(get_readonly_data_path(DATA_PATH)))
			return -3;
      printf("Did not load bios, using reios\n");
	}

#if FEAT_SHREC != DYNAREC_NONE
	if(settings.dynarec.Enable)
	{
		Get_Sh4Recompiler(&sh4_cpu);
		printf("Using Recompiler\n");
	}
	else
#endif
	{
		Get_Sh4Interpreter(&sh4_cpu);
		printf("Using Interpreter\n");
	}
	
  InitAudio();

	sh4_cpu.Init();
	mem_Init();

	plugins_Init();
	
	mem_map_default();

	mcfg_CreateDevices();

	plugins_Reset(false);
	mem_Reset(false);
	

	sh4_cpu.Reset(false);
	
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

	SaveRomFiles(get_writable_data_path("/data/"));
}

void LoadSettings(void)
{
	settings.dynarec.Enable			= 1;
	settings.dynarec.idleskip		= 1;
	settings.dynarec.unstable_opt	= 0; 
	//disable_nvmem can't be loaded, because nvmem init is before cfg load
	settings.dreamcast.cable		= 3;
	settings.dreamcast.RTC			= GetRTC_now();
	settings.dreamcast.region		= 3;
	settings.dreamcast.broadcast	= 4;
	settings.aica.LimitFPS			= 1;
	settings.aica.NoBatch			= 0;
   settings.aica.NoSound			= 0;
   settings.rend.UseMipmaps		= 1;
   settings.rend.WideScreen		= 0; 
	settings.pvr.subdivide_transp	= 0;
	settings.pvr.ta_skip			   = 0;
	settings.pvr.rend				   = 0;

	settings.pvr.MaxThreads			      = 3;
	settings.pvr.SynchronousRendering	= 0;

	settings.debug.SerialConsole        = 0;

	settings.reios.ElfFile              = "";

	settings.validate.OpenGlChecks      = 0;

	settings.bios.UseReios              = 0;

	settings.aica.BufferSize=1024;

/*
	//make sure values are valid
	settings.dreamcast.cable	= min(max(settings.dreamcast.cable,    0),3);
	settings.dreamcast.region	= min(max(settings.dreamcast.region,   0),3);
	settings.dreamcast.broadcast= min(max(settings.dreamcast.broadcast,0),4);
*/
}

void SaveSettings(void)
{
}
