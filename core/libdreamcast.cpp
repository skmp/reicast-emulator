// nullDC.cpp : Makes magic cookies
//

//initialse Emu
#include "types.h"
#include "oslib.h"
#include "audiobackend/audiostream.h"
#include "mem/_vmem.h"
#include "stdclass.h"
#include "cfg/cfg.h"

#include "types.h"
#include "maple/maple_cfg.h"
#include "sh4/sh4_mem.h"

#include "webui/server.h"
#include "naomi/naomi_cart.h"
#include "reios/reios.h"

wchar_t* trim_ws(wchar_t* str);

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



s32 plugins_Init()
{

	if (s32 rv = libPvr_Init())
		return rv;

	if (s32 rv = libGDR_Init())
		return rv;
	#if DC_PLATFORM == DC_PLATFORM_NAOMI
	if (!naomi_cart_SelectFile(libPvr_GetRenderTarget()))
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

void plugins_Term()
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




#if !defined(USE_QT)

#if HOST_OS!=OS_WINDOWS && HOST_OS!=OS_UWP // && !USE_QT
int GetFile(wchar_t *szFileName, wchar_t *szParse = 0, u32 flags = 0) { return -1; }
#endif




#if !defined(TARGET_NO_WEBUI)

void* webui_th(void* p)
{
	webui_start();
	return 0;
}

cThread webui_thd(&webui_th,0);
#endif

#if defined(_ANDROID)
int reios_init_value;

void reios_init(int argc,wchar_t* argv[])
#else
int dc_init(int argc,wchar_t* argv[])
#endif
{
	setbuf(stdin,0);
	setbuf(stdout,0);
	setbuf(stderr,0);

	wprintf(L"---------------------------------\n\t_vmem_reserve()\n---------------------------------\n");

	if (!_vmem_reserve())
	{
		wprintf(L"Failed to alloc mem\n");
#if defined(_ANDROID)
		reios_init_value = -1;
		return;
#else
		return -1;
#endif
	}

#if !defined(TARGET_NO_WEBUI)
	webui_thd.Start();
#endif

	if(ParseCommandLine(argc,argv))
	{
#if defined(_ANDROID)
        reios_init_value = 69;
        return;
#else
        return 69;
#endif
	}

	wprintf(L"---------------------------------\n\t cfgOpen()\n---------------------------------\n");
	if(!cfgOpen())
	{
		msgboxf(L"Unable to open config file",MBX_ICONERROR);
#if defined(_ANDROID)
		reios_init_value = -4;
		return;
#else
		return -4;
#endif
	}
	wprintf(L"---------------------------------\n\t LoadSettings()\n---------------------------------\n");
	LoadSettings();
#ifndef _ANDROID
	os_CreateWindow();
#endif

	int rv = 0;

	wprintf(L"---------------------------------\n\t LoadRomFiles()\n---------------------------------\n");
#if HOST_OS != OS_DARWIN && !defined(PS4)
    // UWPTODO Point to external storage?
	#define DATA_PATH L"./data/"
#else
    #define DATA_PATH "/"
#endif

	wprintf(L"\n\t ---- FN DATA PATH : %s\n\n", DATA_PATH);

	if (settings.bios.UseReios || !LoadRomFiles(get_readonly_data_path(DATA_PATH)))
	{
		if (!LoadHle(get_readonly_data_path(DATA_PATH)))
		{
#if defined(_ANDROID)
			reios_init_value = -4;
			return;
#else
			return -3;
#endif
		}
		else
		{
			wprintf(L"Did not load bios, using reios\n");
		}
	}

	plugins_Init();

#if defined(_ANDROID)
}

int dc_init()
{
	int rv = 0;
	if (reios_init_value != 0)
		return reios_init_value;
#else

	LoadCustom();
//	LoadSettings();
#endif

#if FEAT_SHREC != DYNAREC_NONE
	if(settings.dynarec.Enable)
	{
		Get_Sh4Recompiler(&sh4_cpu);
		wprintf(L"Using Recompiler\n");
	}
	else
#endif
	{
		Get_Sh4Interpreter(&sh4_cpu);
		wprintf(L"Using Interpreter\n");
	}

    InitAudio();

	sh4_cpu.Init();
	mem_Init();

	mem_map_default();

#if DC_PLATFORM == DC_PLATFORM_DREAMCAST
	os_SetupInput();
#else
	mcfg_CreateNAOMIJamma();
#endif

	plugins_Reset(false);
	mem_Reset(false);

	sh4_cpu.Reset(false);
	
	return rv;
}

void dc_run()
{
	sh4_cpu.Run();
}

void dc_term()
{
	sh4_cpu.Term();
	plugins_Term();
	_vmem_release();

#ifndef _ANDROID
	SaveSettings();
#endif
	SaveRomFiles(get_writable_data_path(DATA_PATH));

    TermAudio();
}

#if defined(_ANDROID)
void dc_pause()
{
	SaveRomFiles(get_writable_data_path(DATA_PATH));
}
#endif

void dc_stop()
{
	sh4_cpu.Stop();
}







#endif // USE_QT





void LoadSettings()
{
#ifndef _ANDROID
	settings.dynarec.Enable		= cfgLoadInt(L"config", L"Dynarec.Enabled", 1) != 0;
	settings.dynarec.idleskip	= cfgLoadInt(L"config", L"Dynarec.idleskip", 1) != 0;
	settings.dynarec.unstable_opt	= cfgLoadInt(L"config", L"Dynarec.unstable-opt", 0);
	//disable_nvmem can't be loaded, because nvmem init is before cfg load
	settings.dreamcast.cable	= cfgLoadInt(L"config", L"Dreamcast.Cable", 3);
	settings.dreamcast.RTC		= cfgLoadInt(L"config", L"Dreamcast.RTC", GetRTC_now());
	settings.dreamcast.region	= cfgLoadInt(L"config", L"Dreamcast.Region", 3);
	settings.dreamcast.broadcast= cfgLoadInt(L"config", L"Dreamcast.Broadcast", 4);
	settings.aica.LimitFPS		= cfgLoadInt(L"config", L"aica.LimitFPS", 1);
	settings.aica.NoBatch		= cfgLoadInt(L"config", L"aica.NoBatch", 0);
	settings.aica.NoSound		= cfgLoadInt(L"config", L"aica.NoSound", 0);
	settings.rend.UseMipmaps	= cfgLoadInt(L"config", L"rend.UseMipmaps", 1);
	settings.rend.WideScreen	= cfgLoadInt(L"config", L"rend.WideScreen", 0);
	settings.rend.Clipping		= cfgLoadInt(L"config", L"rend.Clipping", 1);

	settings.pvr.subdivide_transp	= cfgLoadInt(L"config", L"pvr.Subdivide", 0);

	settings.pvr.ta_skip			= cfgLoadInt(L"config", L"ta.skip", 0);
	settings.pvr.rend				= cfgLoadInt(L"config", L"pvr.rend", 0);

	settings.pvr.MaxThreads			= cfgLoadInt(L"config", L"pvr.MaxThreads", 3);
	settings.pvr.SynchronousRender	= cfgLoadInt(L"config", L"pvr.SynchronousRendering", 0);

	settings.debug.SerialConsole	= cfgLoadInt(L"config", L"Debug.SerialConsoleEnabled", 0) != 0;

	settings.bios.UseReios			= cfgLoadInt(L"config", L"bios.UseReios", 0);
	settings.reios.ElfFile			= cfgLoadStr(L"reios", L"ElfFile", L"");

	settings.validate.OpenGlChecks	= cfgLoadInt(L"validate", L"OpenGlChecks", 0) != 0;

	// Configured on a per-game basis
	settings.dynarec.safemode		= 0;
	settings.aica.DelayInterrupt	= 0;
	settings.rend.ModifierVolumes	= 1;
#endif

	settings.pvr.HashLogFile		= cfgLoadStr(L"testing", L"ta.HashLogFile", L"");
	settings.pvr.HashCheckFile		= cfgLoadStr(L"testing", L"ta.HashCheckFile", L"");

#if SUPPORT_DISPMANX
	settings.dispmanx.Width			= cfgLoadInt(L"dispmanx",L"width",640);
	settings.dispmanx.Height		= cfgLoadInt(L"dispmanx",L"height",480);
	settings.dispmanx.Keep_Aspect	= cfgLoadBool(L"dispmanx",L"maintain_aspect",true);
#endif

#if (HOST_OS != OS_LINUX || defined(_ANDROID) || defined(TARGET_PANDORA))
	settings.aica.BufferSize=2048;
#else
	settings.aica.BufferSize=1024;
#endif

#if USE_OMX
	settings.omx.Audio_Latency	= cfgLoadInt(L"omx","audio_latency",100);
	settings.omx.Audio_HDMI		= cfgLoadBool("omx","audio_hdmi",true);
#endif

/*
	//make sure values are valid
	settings.dreamcast.cable	= min(max(settings.dreamcast.cable,    0),3);
	settings.dreamcast.region	= min(max(settings.dreamcast.region,   0),3);
	settings.dreamcast.broadcast= min(max(settings.dreamcast.broadcast,0),4);
*/
}

void LoadCustom()
{	
	const size_t maxDiskIdSize = 128;
	const size_t maxNameSize = 128;	
	
	wstring _d_id = toWString(reios_disk_id());			// these were for testing, i'm just lazy to change below back to using temp objects -Z
	wstring _sw_n = toWString(reios_software_name);

	// Get reios_disk_id and reios_software-name and convert to wstring
	const wchar_t* disk_id = _d_id.c_str();
	const wchar_t* software_name = _sw_n.c_str();
	
	// create wchar_t array to copy wide chars from const wchar_t*
	wchar_t reios_id_untrimmed[maxDiskIdSize] = { '\0' };
	wchar_t reios_software_name_untrimmed[maxNameSize] = { '\0' };

	// determine max number of chars to copy from disk_id and software_name
	size_t idLength = wcsnlen(disk_id, maxDiskIdSize - 1);
	size_t nameLength = wcsnlen(software_name, maxNameSize - 1);

	// copy chars from const wchar_t* to wchart_t array
	wcsncpy(reios_id_untrimmed, disk_id, idLength);
	wcsncpy(reios_software_name_untrimmed, software_name, nameLength);

	// null terminate based on length
	reios_id_untrimmed[idLength] = '\0';
	reios_software_name_untrimmed[nameLength] = '\0';

	// All this just so we can now trim whitespace
	wchar_t* reios_id = trim_ws(reios_id_untrimmed);
	wchar_t* sw_name = trim_ws(reios_software_name_untrimmed);

#ifndef TARGET_UWP
	cfgSaveStr(reios_id, L"software.name", sw_name);
#endif
	settings.dynarec.Enable			= cfgLoadInt(reios_id,L"Dynarec.Enabled",		settings.dynarec.Enable ? 1 : 0) != 0;
	settings.dynarec.idleskip		= cfgGameInt(reios_id,L"Dynarec.idleskip",		settings.dynarec.idleskip ? 1 : 0) != 0;
	settings.dynarec.unstable_opt	= cfgGameInt(reios_id,L"Dynarec.unstable-opt",	settings.dynarec.unstable_opt);
	settings.dynarec.safemode		= cfgGameInt(reios_id,L"Dynarec.safemode",		settings.dynarec.safemode);
	settings.aica.DelayInterrupt	= cfgLoadInt(reios_id,L"aica.DelayInterrupt",	settings.aica.DelayInterrupt);
	settings.rend.ModifierVolumes	= cfgGameInt(reios_id,L"rend.ModifierVolumes",	settings.rend.ModifierVolumes);
	settings.rend.Clipping			= cfgGameInt(reios_id,L"rend.Clipping",			settings.rend.Clipping);

	settings.pvr.subdivide_transp	= cfgGameInt(reios_id,L"pvr.Subdivide",			settings.pvr.subdivide_transp);

	settings.pvr.ta_skip			= cfgGameInt(reios_id,L"ta.skip",	settings.pvr.ta_skip);
	settings.pvr.rend				= cfgGameInt(reios_id,L"pvr.rend",	settings.pvr.rend);

	settings.pvr.MaxThreads			= cfgGameInt(reios_id,L"pvr.MaxThreads", settings.pvr.MaxThreads);
	settings.pvr.SynchronousRender	= cfgGameInt(reios_id,L"pvr.SynchronousRendering", settings.pvr.SynchronousRender);
}

void SaveSettings()
{
	cfgSaveInt(L"config",L"Dynarec.Enabled",	settings.dynarec.Enable);
	cfgSaveInt(L"config",L"Dreamcast.Cable",	settings.dreamcast.cable);
	cfgSaveInt(L"config",L"Dreamcast.RTC",		settings.dreamcast.RTC);
	cfgSaveInt(L"config",L"Dreamcast.Region",	settings.dreamcast.region);
	cfgSaveInt(L"config",L"Dreamcast.Broadcast",settings.dreamcast.broadcast);
}


