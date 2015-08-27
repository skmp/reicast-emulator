#include "types.h"
#include "cfg.h"

#if (HOST_OS != OS_LINUX || defined(_ANDROID) || defined(TARGET_PANDORA))
	#define AICA_BUFFER_SIZE 2048
#else
	#define AICA_BUFFER_SIZE 1024
#endif

void LoadSettings()
{
	#ifndef _ANDROID
		settings.dynarec.Enable           = cfgLoadInt("config", "Dynarec.Enabled", 1) != 0;
		settings.dynarec.idleskip         = cfgLoadInt("config", "Dynarec.idleskip", 1) != 0;
		settings.dynarec.unstable_opt     = cfgLoadInt("config", "Dynarec.unstable-opt", 0);
		//disable_nvmem can't be loaded, because nvmem init is before cfg load
		settings.dreamcast.cable          = cfgLoadInt("config", "Dreamcast.Cable", 3);
		settings.dreamcast.RTC            = cfgLoadInt("config", "Dreamcast.RTC", GetRTC_now());
		settings.dreamcast.region         = cfgLoadInt("config", "Dreamcast.Region", 3);
		settings.dreamcast.broadcast      = cfgLoadInt("config", "Dreamcast.Broadcast", 4);
		settings.aica.LimitFPS            = cfgLoadInt("config", "aica.LimitFPS", 1);
		settings.aica.NoBatch             = cfgLoadInt("config", "aica.NoBatch", 0);
		settings.aica.NoSound             = cfgLoadInt("config", "aica.NoSound", 0);
		settings.rend.UseMipmaps          = cfgLoadInt("config", "rend.UseMipmaps", 1);
		settings.rend.WideScreen          = cfgLoadInt("config", "rend.WideScreen", 0);
		settings.pvr.subdivide_transp     = cfgLoadInt("config", "pvr.Subdivide", 0);
		settings.pvr.ta_skip              = cfgLoadInt("config", "ta.skip", 0);
		settings.pvr.rend                 = cfgLoadInt("config", "pvr.rend", 0);
		settings.pvr.MaxThreads           = cfgLoadInt("config", "pvr.MaxThreads", 3);
		settings.pvr.SynchronousRendering = cfgLoadInt("config", "pvr.SynchronousRendering", 0);
		settings.debug.SerialConsole      = cfgLoadInt("config", "Debug.SerialConsoleEnabled", 0) != 0;
		settings.reios.ElfFile            = cfgLoadStr("reios", "ElfFile", "");
		settings.validate.OpenGlChecks    = cfgLoadInt("validate", "OpenGlChecks", 0) != 0;
	#endif

	settings.bios.UseReios = cfgLoadInt("config", "bios.UseReios", 0);
	settings.aica.BufferSize = AICA_BUFFER_SIZE;

	/*
	//make sure values are valid
	settings.dreamcast.cable    = min(max(settings.dreamcast.cable,     0), 3);
	settings.dreamcast.region   = min(max(settings.dreamcast.region,    0), 3);
	settings.dreamcast.broadcast= min(max(settings.dreamcast.broadcast, 0), 4);
	*/
}
void SaveSettings()
{
	cfgSaveInt("config","Dynarec.Enabled",     settings.dynarec.Enable);
	cfgSaveInt("config","Dreamcast.Cable",     settings.dreamcast.cable);
	cfgSaveInt("config","Dreamcast.RTC",       settings.dreamcast.RTC);
	cfgSaveInt("config","Dreamcast.Region",    settings.dreamcast.region);
	cfgSaveInt("config","Dreamcast.Broadcast", settings.dreamcast.broadcast);
}
