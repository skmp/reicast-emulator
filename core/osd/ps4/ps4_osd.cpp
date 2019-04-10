/*
**	$OSD$/ps4/ps4_osd.cpp
*/
#include "types.h"
#include "cfg/cfg.h"



#include <kernel_ex.h>
#include <sysmodule_ex.h>
#include <system_service_ex.h>
#include <shellcore_util.h>
#include <piglet.h>


extern "C" int syscall(int num, ...);


//64      AUE_NULL        COMPAT{ int getpagesize(void); } getpagesize \ getpagesize_args int
//33      AUE_ACCESS      STD{ int access(char *path, int flags); }
extern "C" int access(char *path, int flags)
{
	return syscall(33, path, flags);
}

//482     AUE_SHMOPEN     STD{ int shm_open(const char *path, int flags, mode_t mode); }
//483     AUE_SHMUNLINK   STD{ int shm_unlink(const char *path); }


extern "C" int shm_open(const char *path, int flags, mode_t mode)
{
	return syscall(482, path, flags, mode);
}
extern "C" int shm_unlink(const char *path)
{
	return syscall(483, path);
}



u16 kcode[4];
u32 vks[4];
s8 joyx[4], joyy[4];
u8 rt[4], lt[4];

int get_mic_data(u8* buffer) { return 0; }
int push_vmu_screen(u8* buffer) { return 0; }

void os_SetWindowText(const char * text) {
	puts(text);
}

void os_DoEvents() {

}

void os_SetupInput() {
	//mcfg_CreateDevicesFromConfig();
}

//void os_DebugBreak() { return; } // SIGNAL()


void UpdateInputState(u32 port) {

}

void UpdateVibration(u32 port, u32 value) {

}

void os_CreateWindow() { }


SceWindow render_window = { 0, 1920,1080 }; // width, height };
void* libPvr_GetRenderTarget() { return &render_window; }
void* libPvr_GetRenderSurface() { return (void*)(u64)EGL_DEFAULT_DISPLAY; }


int __cdecl msgboxf(const wchar_t* text, unsigned int type, ...)
{
	va_list args;

	char temp[2048];
	va_start(args, type);
	vsprintf(temp, text, args);
	va_end(args);

	wprintf(L"----------------------------------------------\n");
	wprintf(L"msgbox(\" %s \" ); \n", temp);
	wprintf(L"----------------------------------------------\n\n");
//	QMessageBox::information(0, "", temp, QMessageBox::Ok);
	return 0;
}


#define RENDER_WIDTH 1920
#define RENDER_HEIGHT 1080

#define PIGLET_MODULE_NAME "libScePigletv2VSH.sprx"
#define SHCOMP_MODULE_NAME "libSceShaccVSH.sprx"


#define PS4_PKG

#ifdef PS4_PKG			// These are RO ofc, use unjailed /data for W

# define PS4_DIR_CFG	"/app0/reicast"	
# define PS4_DIR_DATA	"/app0/reicast/data"

# define MODULE_PATH_PREFIX "/app0/sce_module"

#else

# define PS4_DIR_CFG	"/mnt/usb0/reicast/"
# define PS4_DIR_DATA	"/mnt/usb0/reicast/data"

# define MODULE_PATH_PREFIX "/data/self/system/common/lib"

#endif

#define EPRINTF wprintf


static SceKernelModule s_piglet_module = -1;
static SceKernelModule s_shcomp_module = -1;




const wchar_t* g_sandbox_word = NULL;

static bool load_modules(void) {
	int ret;

	ret = sceKernelLoadStartModule(MODULE_PATH_PREFIX "/" PIGLET_MODULE_NAME, 0, NULL, 0, NULL, NULL);
	if (ret < 0) {
		EPRINTF("sceKernelLoadStartModule(%s) failed: 0x%08X\n", PIGLET_MODULE_NAME, ret);
		goto err;
	}
	s_piglet_module = ret;

	ret = sceKernelLoadStartModule(MODULE_PATH_PREFIX "/" SHCOMP_MODULE_NAME, 0, NULL, 0, NULL, NULL);
	if (ret < 0) {
		EPRINTF("sceKernelLoadStartModule(%s) failed: 0x%08X\n", SHCOMP_MODULE_NAME, ret);
		goto err;
	}
	s_shcomp_module = ret;

	return true;

err_unload_shcompt:
	ret = sceKernelStopUnloadModule(s_shcomp_module, 0, NULL, 0, NULL, NULL);
	if (ret < 0) {
		EPRINTF("sceKernelStopUnloadModule(%s) failed: 0x%08X\n", SHCOMP_MODULE_NAME, ret);
	}
	else {
		s_shcomp_module = -1;
	}

err_unload_piglet:
	ret = sceKernelStopUnloadModule(s_piglet_module, 0, NULL, 0, NULL, NULL);
	if (ret < 0) {
		EPRINTF("sceKernelStopUnloadModule(%s) failed: 0x%08X\n", PIGLET_MODULE_NAME, ret);
	}
	else {
		s_piglet_module = -1;
	}

err:
	return false;
}

static void unload_modules(void) {
	int ret;

	if (s_shcomp_module > 0) {
		ret = sceKernelStopUnloadModule(s_shcomp_module, 0, NULL, 0, NULL, NULL);
		if (ret < 0) {
			EPRINTF("sceKernelStopUnloadModule(%s) failed: 0x%08X\n", SHCOMP_MODULE_NAME, ret);
		}
		else {
			s_shcomp_module = -1;
		}
	}

	if (s_piglet_module > 0) {
		ret = sceKernelStopUnloadModule(s_piglet_module, 0, NULL, 0, NULL, NULL);
		if (ret < 0) {
			EPRINTF("sceKernelStopUnloadModule(%s) failed: 0x%08X\n", PIGLET_MODULE_NAME, ret);
		}
		else {
			s_piglet_module = -1;
		}
	}
}

/* XXX: patches below are given for Piglet module from 4.74 Devkit PUP */
static void pgl_patches_cb(void* arg, uint8_t* base, uint64_t size) {
	/* Patch runtime compiler check */
	const uint8_t p_set_eax_to_1[] = {
		0x31, 0xC0, 0xFF, 0xC0, 0x90,
	};
	memcpy(base + 0x5451F, p_set_eax_to_1, sizeof(p_set_eax_to_1));

	/* Tell that runtime compiler exists */
	*(uint8_t*)(base + 0xB2DEC) = 0;
	*(uint8_t*)(base + 0xB2DED) = 0;
	*(uint8_t*)(base + 0xB2DEE) = 1;
	*(uint8_t*)(base + 0xB2E21) = 1;

	/* Inform Piglet that we have shader compiler module loaded */
	*(int32_t*)(base + 0xB2E24) = s_shcomp_module;
}


/// util.h

SceKernelModule load_module_from_sandbox(const wchar_t* name, size_t args, const void* argp, unsigned int flags, const SceKernelLoadModuleOpt* opts, int* res);

bool get_module_base(const wchar_t* name, uint64_t* base, uint64_t* size);

typedef void module_patch_cb_t(void* arg, uint8_t* base, uint64_t size);
bool patch_module(const wchar_t* name, module_patch_cb_t* cb, void* arg);

void hexdump(const void* data, size_t size);

void send_notify(const wchar_t* format, ...);

//// end util.h


static bool do_patches(void) {
	if (!patch_module(PIGLET_MODULE_NAME, &pgl_patches_cb, NULL)) {
		EPRINTF("Unable to patch PGL module.\n");
		goto err;
	}

	return true;

err:
	return false;
}

static void cleanup(void) {
	int ret;

	unload_modules();

	ret = sceSysmoduleUnloadModuleInternal(SCE_SYSMODULE_INTERNAL_SYSTEM_SERVICE);
	if (ret) {
		EPRINTF("sceSysmoduleUnloadModuleInternal(%s) failed: 0x%08X\n", "SCE_SYSMODULE_INTERNAL_SYSTEM_SERVICE", ret);
	}
}












void common_linux_setup();
int dc_init(int argc, wchar_t* argv[]);
void dc_run();
void dc_term();

int main(int argc, wchar_t* argv[])
{
	wprintf(L"\n\n\n\n -- REICAST BETA -- \n\n\n\n");

	atexit(&cleanup);

wprintf(L"sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_SYSTEM_SERVICE);\n");

	int ret = sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_SYSTEM_SERVICE);
	if (ret) {
		EPRINTF("sceSysmoduleLoadModuleInternal(%s) failed: 0x%08X\n", "SCE_SYSMODULE_INTERNAL_SYSTEM_SERVICE", ret);
		goto err;
	}

	g_sandbox_word = sceKernelGetFsSandboxRandomWord();
	if (!g_sandbox_word) {
		EPRINTF("sceKernelGetFsSandboxRandomWord failed.\n");
		goto err;
	}

wprintf(L"load_modules();\n");

	if (!load_modules()) {
		EPRINTF("Unable to load modules.\n");
		goto err;
	}
	if (!do_patches()) {
		EPRINTF("Unable to patch modules.\n");
		goto err;
	}

	ret = sceSystemServiceHideSplashScreen();
	if (ret) {
		EPRINTF("sceSystemServiceHideSplashScreen failed: 0x%08X\n", ret);
		goto err;
	}


///////////////////////////////////////////////////////////////////////////////
	wprintf(L"common_setup()\n");

	/* Set directories */
	set_user_config_dir(PS4_DIR_CFG);
	set_user_data_dir(PS4_DIR_DATA);

	wprintf(L"Config dir is: %s\n", get_writable_config_path("/").c_str());
	wprintf(L"Data dir is:   %s\n", get_writable_data_path("/").c_str());
	wprintf(L"RO Data dir is:   %s\n", get_readonly_data_path("/").c_str());

	common_linux_setup();

	settings.profile.run_counts = 0;

wprintf(L"dc_init();\n");
	dc_init(argc, argv);

wprintf(L"dc_run();\n");
	dc_run();

wprintf(L"dc_term();\n");
	dc_term();




err:
	return 0;
}

void catchReturnFromMain(int exit_code) {
	/* dummy */
}





//////// util.c



SceKernelModule load_module_from_sandbox(const wchar_t* name, size_t args, const void* argp, unsigned int flags, const SceKernelLoadModuleOpt* opts, int* res) {
	char file_path[SCE_KERNEL_MAX_NAME_LENGTH];
	SceKernelModule module;

	snprintf(file_path, sizeof(file_path), "/%s/common/lib/%s", g_sandbox_word, name);

	module = sceKernelLoadStartModule(file_path, args, argp, flags, opts, res);

	return module;
}

bool get_module_base(const wchar_t* name, uint64_t* base, uint64_t* size) {
	SceKernelModuleInfo moduleInfo;
	int ret;

	ret = sceKernelGetModuleInfoByName(name, &moduleInfo);
	if (ret) {
		EPRINTF("sceKernelGetModuleInfoByName(%s) failed: 0x%08X\n", name, ret);
		goto err;
	}

	if (base) {
		*base = (uint64_t)moduleInfo.segmentInfo[0].baseAddr;
	}
	if (size) {
		*size = moduleInfo.segmentInfo[0].size;
	}

	return true;

err:
	return false;
}

bool patch_module(const wchar_t* name, module_patch_cb_t* cb, void* arg) {
	uint64_t base, size;
	int ret;

	if (!get_module_base(name, &base, &size)) {
		goto err;
	}

	ret = sceKernelMprotect((void*)base, size, SCE_KERNEL_PROT_CPU_READ | SCE_KERNEL_PROT_CPU_WRITE | SCE_KERNEL_PROT_CPU_EXEC);
	if (ret) {
		EPRINTF("sceKernelMprotect(%s) failed: 0x%08X\n", name, ret);
		goto err;
	}

	if (cb) {
		(*cb)(arg, (uint8_t*)base, size);
	}

	return true;

err:
	return false;
}

void send_notify(const wchar_t* format, ...) {
	SceNotificationRequest req;
	va_list args;

	memset(&req, 0, sizeof(req));
	{
		req.unk_0x10 = -1;

		va_start(args, format);
		req.buf[0] = '%'; /* XXX: hackity hack */
		vsnprintf((wchar_t*)req.buf + 1, 0xB4, format, args);
		va_end(args);
	}

	sceKernelSendNotificationRequest(0, &req, sizeof(req), 1);
}

void hexdump(const void* data, size_t size) {
	const uint8_t* p = (const uint8_t*)data;
	const size_t n = 16;
	size_t i, j, k;
	for (i = 0; i < size; i += n) {
		k = (i + n) <= size ? n : (size - i);
		wprintf(L"%8p:", (uint8_t*)data + i);
		for (j = 0; j < k; ++j) {
			wprintf(L" %02x", p[i + j]);
		}
		for (j = k; j < n; ++j) {
			wprintf(L"   ");
		}
		wprintf(L"  ");
		for (j = 0; j < k; ++j) {
			wprintf(L"%c", isprint(p[i + j]) ? p[i + j] : '.');
		}
		for (j = k; j < n; ++j) {
			wprintf(L" ");
		}
		wprintf(L"\n");
	}
}