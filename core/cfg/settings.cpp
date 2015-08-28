#include "types.h"
#include "cfg.h"

#if (HOST_OS != OS_LINUX || defined(_ANDROID) || defined(TARGET_PANDORA))
	#define AICA_BUFFER_SIZE 2048
#else
	#define AICA_BUFFER_SIZE 1024
#endif

void LoadSettings(settings_t* p_settings)
{
	#ifndef _ANDROID
		p_settings->dynarec.Enable           = cfgLoadInt("config", "Dynarec.Enabled", 1) != 0;
		p_settings->dynarec.idleskip         = cfgLoadInt("config", "Dynarec.idleskip", 1) != 0;
		p_settings->dynarec.unstable_opt     = cfgLoadInt("config", "Dynarec.unstable-opt", 0);
		//disable_nvmem can't be loaded, because nvmem init is before cfg load
		p_settings->dreamcast.cable          = cfgLoadInt("config", "Dreamcast.Cable", 3);
		p_settings->dreamcast.RTC            = cfgLoadInt("config", "Dreamcast.RTC", GetRTC_now());
		p_settings->dreamcast.region         = cfgLoadInt("config", "Dreamcast.Region", 3);
		p_settings->dreamcast.broadcast      = cfgLoadInt("config", "Dreamcast.Broadcast", 4);
		p_settings->aica.LimitFPS            = cfgLoadInt("config", "aica.LimitFPS", 1);
		p_settings->aica.NoBatch             = cfgLoadInt("config", "aica.NoBatch", 0);
		p_settings->aica.NoSound             = cfgLoadInt("config", "aica.NoSound", 0);
		p_settings->rend.UseMipmaps          = cfgLoadInt("config", "rend.UseMipmaps", 1);
		p_settings->rend.WideScreen          = cfgLoadInt("config", "rend.WideScreen", 0);
		p_settings->pvr.subdivide_transp     = cfgLoadInt("config", "pvr.Subdivide", 0);
		p_settings->pvr.ta_skip              = cfgLoadInt("config", "ta.skip", 0);
		p_settings->pvr.rend                 = cfgLoadInt("config", "pvr.rend", 0);
		p_settings->pvr.MaxThreads           = cfgLoadInt("config", "pvr.MaxThreads", 3);
		p_settings->pvr.SynchronousRendering = cfgLoadInt("config", "pvr.SynchronousRendering", 0);
		p_settings->debug.SerialConsole      = cfgLoadInt("config", "Debug.SerialConsoleEnabled", 0) != 0;
		p_settings->reios.ElfFile            = cfgLoadStr("reios", "ElfFile", "");
		p_settings->validate.OpenGlChecks    = cfgLoadInt("validate", "OpenGlChecks", 0) != 0;
	#endif

	p_settings->bios.UseReios = cfgLoadInt("config", "bios.UseReios", 0);
	p_settings->aica.BufferSize = AICA_BUFFER_SIZE;

	/*
	//make sure values are valid
	p_settings->dreamcast.cable    = min(max(p_settings->dreamcast.cable,     0), 3);
	p_settings->dreamcast.region   = min(max(p_settings->dreamcast.region,    0), 3);
	p_settings->dreamcast.broadcast= min(max(p_settings->dreamcast.broadcast, 0), 4);
	*/
}
void SaveSettings(settings_t* p_settings)
{
	cfgSaveInt("config","Dynarec.Enabled",     p_settings->dynarec.Enable);
	cfgSaveInt("config","Dreamcast.Cable",     p_settings->dreamcast.cable);
	cfgSaveInt("config","Dreamcast.RTC",       p_settings->dreamcast.RTC);
	cfgSaveInt("config","Dreamcast.Region",    p_settings->dreamcast.region);
	cfgSaveInt("config","Dreamcast.Broadcast", p_settings->dreamcast.broadcast);
}
