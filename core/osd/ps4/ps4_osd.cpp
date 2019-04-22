/*
**	$OSD$/ps4/ps4_osd.cpp
*/
#include "types.h"
#include "cfg/cfg.h"
#include "dc_arch/maple/maple_cfg.h"
#include "audiobackend/audiostream.h"

#include "ps4.h"

#if 0

// Assign 256 MiB as flexible memory
#define TOTAL_FMEM_SIZE (4 * 1024 * 1024 * 1024UL)
SCE_KERNEL_FLEXIBLE_MEMORY_SIZE(TOTAL_FMEM_SIZE);
//SCE_KERNEL_MAIN_DMEM_SIZE


//#define LIBC_HEAP_SIZE
//#define LIBC_HEAP_SIZE_INITIAL = 

#define _MB MB

#if 1
extern "C" {

	size_t sceLibcHeapSize = SCE_LIBC_HEAP_SIZE_EXTENDED_ALLOC_NO_LIMIT;
	size_t sceLibcHeapInitialSize = _MB(512);
//	unsigned int sceLibcHeapDelayedAlloc  = 0;
	unsigned int sceLibcHeapExtendedAlloc = 1;
//	size_t sceLibcHeapHighAddressAlloc;
//	unsigned int sceLibcHeapMemoryLock;
	//unsigned int sceLibcHeapDebugFlags = SCE_LIBC_HEAP_DEBUG_SHORTAGE;



	//SceLibcMallocManagedSize m_mmsize;

};
#endif

#endif // 0



//int32_t sceSystemServiceGetStatus(SceSystemServiceStatus* status);
//int32_t sceSystemServiceReceiveEvent(SceSystemServiceEvent* event);

//dbgbreak __builtin_trap()

void dc_stop();

void os_DoEvents()
{
	int32_t rS = SCE_OK;
	SceSystemServiceEvent sse; // = { 0 };
	SceSystemServiceStatus sss = { 0 };


	memset(&sss, 0, sizeof(SceSystemServiceStatus));
	rS = sceSystemServiceGetStatus(&sss);
	if (SCE_OK != rS)
		printf("os_DoEvents(): sceSystemServiceGetStatus() Failed!");


	/*
	**	Polling is really the only method of finding out we're supposed to stop?
	**		Fucked up when the emulator destroys the OS if you don't stop...
	*/
	if (sss.isInBackgroundExecution)
	{
		printf("os_DoEvents(): ! STOP \n");
		dc_stop();
		printf("os_DoEvents(): STOP ! \n");
	}

	do {
		/*
			eventNum					Number of events that can be received with sceSystemServiceReceiveEvent()
			isSystemUiOverlaid			Flag indicating whether the system software UI is being overlaid or not
			isInBackgroundExecution		Flag indicating whether the application is running in the background or not
			isCpuMode7CpuNormal			Flag indicating whether the CPU mode is 7CPU(NORMAL) mode or not
			isGameLiveStreamingOnAir	Flag indicating whether live streaming is currently being performed or not
		*/


		memset(&sse, 0, sizeof(SceSystemServiceEvent));
		rS = sceSystemServiceReceiveEvent(&sse);
		if (SCE_OK == rS) {

			if (SCE_SYSTEM_SERVICE_EVENT_ON_RESUME == sse.eventType)
				printf("SCE EVENT - RESUME!\n"); // dc_run()
			else
				printf("SCE EVENT - Type: %08X \n", sse.eventType);
		}

		if (sss.eventNum-- < 0) {
			printf("SCE EVENT - OVERFLOW? \n"); break;
		}
	} while (SCE_OK == rS);



}



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

#define Btn_C            (1 << 0)
#define Btn_B            (1 << 1)
#define Btn_A            (1 << 2)
#define Btn_START        (1 << 3)
#define Btn_DPAD_UP      (1 << 4)
#define Btn_DPAD_DOWN    (1 << 5)
#define Btn_DPAD_LEFT    (1 << 6)
#define Btn_DPAD_RIGHT   (1 << 7)
#define Btn_Z            (1 << 8)
#define Btn_Y            (1 << 9)
#define Btn_X            (1 << 10)
#define Btn_D            (1 << 11)
#define Btn_DPAD2_UP     (1 << 12)
#define Btn_DPAD2_DOWN   (1 << 13)
#define Btn_DPAD2_LEFT   (1 << 14)
#define Btn_DPAD2_RIGHT  (1 << 15)

#define PARAMS16 (SCE_AUDIO_OUT_PARAM_FORMAT_S16_STEREO << SCE_AUDIO_OUT_PARAM_FORMAT_SHIFT)

int get_mic_data(u8* buffer) { return 0; }
int push_vmu_screen(u8* buffer) { return 0; }

void os_SetWindowText(const char * text) {
	puts(text);
}



SceUserServiceUserId userId = SCE_USER_SERVICE_USER_ID_SYSTEM;


int32_t aOut_H = 0;
int32_t pad1_H = 0;
#if 0
int32_t vOut_H = 0;
#endif

static void aout_init()
{
	verify(SCE_OK == sceAudioOutInit());

	SceUserServiceUserId userId = SCE_USER_SERVICE_USER_ID_SYSTEM;
	aOut_H = sceAudioOutOpen(userId, SCE_AUDIO_OUT_PORT_TYPE_MAIN, 0, 256, 48000, PARAMS16);

	printf(" --aOut_H: %X\n", aOut_H);
}

static void aout_term() {
	sceAudioOutClose(aOut_H);
}

static u32 aout_push(void* frame, u32 samples, bool wait)
{
	/* Output audio */
	if (sceAudioOutOutput(aOut_H, frame) < 0) {
		/* Error handling */
	}

	/* Get output process time */
	uint64_t outputTime;
	if (sceAudioOutGetLastOutputTime(aOut_H, &outputTime) < 0) {
		/* Error handling */
	}

	/* Get port state */
	SceAudioOutPortState portState;
	if (sceAudioOutGetPortState(aOut_H, &portState) < 0) {
		/* Error handling */
	}

	/* Get system state */
	SceAudioOutSystemState aoss;
	if (sceAudioOutGetSystemState(&aoss) < 0) {
		/* Error handling */
	}

	return 1;
}

audiobackend_t audiobackend_ps4aout = {
		"aout", // Slug
		"PS4 AudioOut", // Name
		&aout_init,
		&aout_push,
		&aout_term
};





void os_SetupInput() {

	mcfg_CreateDevicesFromConfig();

	verify(SCE_OK == scePadInit());
	pad1_H = scePadOpen(userId, SCE_PAD_PORT_TYPE_STANDARD, 0, NULL);

	printf(" --UserId: %X\n", userId);
	printf(" --pad1_H: %X\n", pad1_H);


#if 1 // FUCK_YOU_SONY
	if (pad1_H >= 0) printf("pad1_H: %X\n", pad1_H); return;  // Winner Winner

	SceUserServiceLoginUserIdList userIdList;
	verify(SCE_OK == sceUserServiceGetLoginUserIdList(&userIdList));

	for (int i = 0; i < SCE_USER_SERVICE_MAX_LOGIN_USERS; i++) {
		if (userIdList.userId[i] != SCE_USER_SERVICE_USER_ID_INVALID) {
			pad1_H = scePadOpen(userIdList.userId[i], SCE_PAD_PORT_TYPE_STANDARD, 0, NULL);
			if (pad1_H >= 0) printf("Fixed pad1_H: %X\n", pad1_H); return;  // Winner Winner
		}
	}

	printf("Error, No DS4 or input pad found!\n");
#endif
}

//void os_DebugBreak() { return; } // SIGNAL()
//#define k(c) printf("Input: '%c'\n", (c));
#define mn(a,b) (((a)<(b))?(a):(b))
#define mx(a,b) (((a)>(b))?(a):(b))

void UpdateInputState(u32 port)
{
	ScePadData data;
	
	int ret = scePadReadState(pad1_H, &data);
	if (ret == SCE_OK) {
		if (!data.connected)
			printf("Warning, Controller is not connected! \n");

		data.buttons;
		kcode[port] = 0xFFFF;
		joyx[port] = joyy[port] = 0;
		lt[port] = rt[port] = 0;


		if (data.buttons&SCE_PAD_BUTTON_CROSS)		kcode[port] &= ~Btn_A;
		if (data.buttons&SCE_PAD_BUTTON_CIRCLE)		kcode[port] &= ~Btn_B;
		if (data.buttons&SCE_PAD_BUTTON_SQUARE)		kcode[port] &= ~Btn_X;
		if (data.buttons&SCE_PAD_BUTTON_TRIANGLE)	kcode[port] &= ~Btn_Y;
		if (data.buttons&SCE_PAD_BUTTON_OPTIONS)	kcode[port] &= ~Btn_START;

		if (data.buttons&SCE_PAD_BUTTON_UP)			kcode[port] &= ~Btn_DPAD_UP;
		if (data.buttons&SCE_PAD_BUTTON_DOWN)		kcode[port] &= ~Btn_DPAD_DOWN;
		if (data.buttons&SCE_PAD_BUTTON_LEFT)		kcode[port] &= ~Btn_DPAD_LEFT;
		if (data.buttons&SCE_PAD_BUTTON_RIGHT)		kcode[port] &= ~Btn_DPAD_RIGHT;


		lt[port] = (data.buttons&SCE_PAD_BUTTON_L1) ? 255 : data.analogButtons.l2;
		rt[port] = (data.buttons&SCE_PAD_BUTTON_R1) ? 255 : data.analogButtons.r2;


		// Sticks are u8 !
		s8 lsX = data.leftStick.x - 128; lsX = (lsX < 0) ? mn(lsX, -8) : mx(lsX, 7);
		s8 lsY = data.leftStick.y - 128; lsY = (lsY < 0) ? mn(lsY, -8) : mx(lsY, 7);

		joyx[port] = lsX;
		joyy[port] = lsY;

		s8 rsX = data.rightStick.x - 128; rsX = (rsX < 0) ? mn(rsX, -8) : mx(rsX, 7);
		s8 rsY = data.rightStick.y - 128; rsY = (rsY < 0) ? mn(rsY, -8) : mx(rsY, 7);

		if (rsY < -120)		kcode[port] &= ~Btn_DPAD2_UP;
		if (rsY > 119)		kcode[port] &= ~Btn_DPAD2_DOWN;
		if (rsX < -120)		kcode[port] &= ~Btn_DPAD2_LEFT;
		if (rsX > 119)		kcode[port] &= ~Btn_DPAD2_RIGHT;



#if 0
		data.leftStick.x,y
		data.rightStick.x, y
			
		ScePadAnalogStick leftStick;
		ScePadAnalogStick rightStick;
		ScePadAnalogButtons analogButtons;

		SCE_PAD_BUTTON_ L3, R3, OPTIONS, 
			UP , RIGHT , DOWN ,	LEFT , L2 , R2 , L1 , R1 ,
			TRIANGLE , CIRCLE , CROSS , SQUARE ,
			TOUCH_PAD , INTERCEPTED 
#endif
	}

}

void UpdateVibration(u32 port, u32 value) {

}

void os_CreateWindow() { }


SceWindow render_window = { 0, 1920,1080 }; // width, height };
void* libPvr_GetRenderTarget() { return &render_window; }
void* libPvr_GetRenderSurface() { return (void*)EGL_DEFAULT_DISPLAY; }
//(void*)vOut_H; } // 

int __cdecl msgboxf(const char* text, unsigned int type, ...)
{
	va_list args;

	char temp[2048];
	va_start(args, type);
	vsprintf(temp, text, args);
	va_end(args);

	printf("----------------------------------------------\n");
	printf("msgbox(\" %s \" ); \n", temp);
	printf("----------------------------------------------\n\n");
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

#define EPRINTF printf


static SceKernelModule s_piglet_module = -1;
static SceKernelModule s_shcomp_module = -1;




const char* g_sandbox_word = NULL;

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

SceKernelModule load_module_from_sandbox(const char* name, size_t args, const void* argp, unsigned int flags, const SceKernelLoadModuleOpt* opts, int* res);

bool get_module_base(const char* name, uint64_t* base, uint64_t* size);

typedef void module_patch_cb_t(void* arg, uint8_t* base, uint64_t size);
bool patch_module(const char* name, module_patch_cb_t* cb, void* arg);

void hexdump(const void* data, size_t size);

void send_notify(const char* format, ...);

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



#include "ps4_video.h"

#define SM_AudioOut			0x80000001
#define SM_SystemService	0x80000010
#define SM_UserService		0x80000011
#define SM_Pad				0x80000024
#define SM_SysUtil			0x80000026

#define SM_ContentExport	SCE_SYSMODULE_CONTENT_EXPORT

static void ps4_init()
{
	verify(SCE_OK == sceSysmoduleLoadModuleInternal(SM_AudioOut));
	verify(SCE_OK == sceSysmoduleLoadModuleInternal(SM_SystemService));
	verify(SCE_OK == sceSysmoduleLoadModuleInternal(SM_UserService));
	verify(SCE_OK == sceSysmoduleLoadModuleInternal(SM_Pad));
	verify(SCE_OK == sceSysmoduleLoadModuleInternal(SM_SysUtil));

	verify(SCE_OK == sceUserServiceInitialize(NULL));
	sceUserServiceGetInitialUser(&userId);




	g_sandbox_word = sceKernelGetFsSandboxRandomWord();
	if (!g_sandbox_word) {
		EPRINTF("sceKernelGetFsSandboxRandomWord failed.\n");
		return;
	}

	printf("load_modules();\n");

	verify(load_modules());
	verify(do_patches());
	verify(SCE_OK == sceSystemServiceHideSplashScreen());

	

	ps4_video_init();


}






void common_linux_setup();
int dc_init(int argc, char* argv[]);
void dc_run();
void dc_term();

int main(int argc, char* argv[])
{
	printf("-- REICAST BETA -- \n");
	atexit(&cleanup);

	size_t mSize=0;
	sceKernelAvailableFlexibleMemorySize(&mSize);
	printf("Available Flexible Mem: %d KB \n", mSize/1024);
	
	off_t phy=0;
	sceKernelAvailableDirectMemorySize(0,SCE_KERNEL_MAIN_DMEM_SIZE,0,&phy,&mSize);
	printf("Available Direct Mem: %d KB @ %p \n", mSize/1024, (void*)phy);

	ps4_init();



///////////////////////////////////////////////////////////////////////////////
	printf("common_setup()\n");

	/* Set directories */
	set_user_config_dir(PS4_DIR_CFG);
	set_user_data_dir(PS4_DIR_DATA);

	printf("Config dir is: %s\n", get_writable_config_path("/").c_str());
	printf("Data dir is:   %s\n", get_writable_data_path("/").c_str());
	printf("RO Data dir is:   %s\n", get_readonly_data_path("/").c_str());

	common_linux_setup();

	settings.profile.run_counts = 0;

printf("dc_init();\n");
	dc_init(argc, argv);

printf("dc_run();\n");
	dc_run();

printf("dc_term();\n");
	dc_term();




err:
	return 0;
}

void catchReturnFromMain(int exit_code) {
	/* dummy */
}





//////// util.c



SceKernelModule load_module_from_sandbox(const char* name, size_t args, const void* argp, unsigned int flags, const SceKernelLoadModuleOpt* opts, int* res) {
	char file_path[SCE_KERNEL_MAX_NAME_LENGTH];
	SceKernelModule module;

	snprintf(file_path, sizeof(file_path), "/%s/common/lib/%s", g_sandbox_word, name);

	module = sceKernelLoadStartModule(file_path, args, argp, flags, opts, res);

	return module;
}

bool get_module_base(const char* name, uint64_t* base, uint64_t* size) {
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

bool patch_module(const char* name, module_patch_cb_t* cb, void* arg) {
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

void send_notify(const char* format, ...) {
	SceNotificationRequest req;
	va_list args;

	memset(&req, 0, sizeof(req));
	{
		req.unk_0x10 = -1;

		va_start(args, format);
		req.buf[0] = '%'; /* XXX: hackity hack */
		vsnprintf((char*)req.buf + 1, 0xB4, format, args);
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
		printf("%8p:", (uint8_t*)data + i);
		for (j = 0; j < k; ++j) {
			printf(" %02x", p[i + j]);
		}
		for (j = k; j < n; ++j) {
			printf("   ");
		}
		printf("  ");
		for (j = 0; j < k; ++j) {
			printf("%c", isprint(p[i + j]) ? p[i + j] : '.');
		}
		for (j = k; j < n; ++j) {
			printf(" ");
		}
		printf("\n");
	}
}


#if 0

static unsigned long zmtotal = 0;

void* zmalloc(unsigned long size)
{
	void *result = 0;

	const uint32_t psMask = (0x4000 - 1);
	int32_t mem_size = (size + psMask) & ~psMask;
	int32_t ret = sceKernelMapFlexibleMemory(&result, mem_size, SCE_KERNEL_PROT_CPU_RW | SCE_KERNEL_PROT_GPU_RW, 0);
	if (0 != result) {
		zmtotal += mem_size;
		printf("zmalloc(%d) allocated %d bytes to 0x%p :: total %d\n", size, mem_size, result, zmtotal);
	}
	else die("ERROR, zmalloc() : allocation failed!\n");
	return result;
}

void  zfree(void* ptr)
{
	sceKernelMunmap(ptr, 1024*1024*4);	// who cares atm, fucking choke on it
}

void  zfree2(void* ptr, unsigned long size)
{
	const uint32_t psMask = (0x4000 - 1);
	int32_t mem_size = (size + psMask) & ~psMask;
	sceKernelMunmap(ptr, mem_size);	// who cares atm, fucking choke on it

	zmtotal -= mem_size;
}
#endif


#if 0

extern "C" {

	int user_malloc_init(void) { printf("VM @@@@ %s", __FUNCTION__);  return 0; }

	int user_malloc_finalize(void) { printf("VM @@@@ %s", __FUNCTION__);  return 0; }

	void *user_malloc(size_t size) { printf("VM @@@@ %s", __FUNCTION__);  return 0; }

	void user_free(void *ptr) { printf("VM @@@@ %s", __FUNCTION__); }

	void *user_calloc(size_t nelem, size_t size) { printf("VM @@@@ %s", __FUNCTION__);  return 0; }

	void *user_realloc(void *ptr, size_t size) { printf("VM @@@@ %s", __FUNCTION__);  return 0; }

	void *user_memalign(size_t boundary, size_t size) { printf("VM @@@@ %s", __FUNCTION__);  return 0; }

	void *user_reallocalign(void *ptr, size_t size, size_t boundary) { printf("VM @@@@ %s", __FUNCTION__);  return 0; }

	int user_posix_memalign(void **ptr, size_t boundary, size_t size) { printf("VM @@@@ %s", __FUNCTION__);  return 0; }

	int user_malloc_stats(SceLibcMallocManagedSize *mmsize) { printf("VM @@@@ %s", __FUNCTION__);  return 0; }
	int user_malloc_stats_fast(SceLibcMallocManagedSize *mmsize) { printf("VM @@@@ %s", __FUNCTION__);  return 0; }

	size_t user_malloc_usable_size(void *ptr)
	{
		printf("VM @@@@ %s", __FUNCTION__);
		return 1 << 28;
	}


};

#endif










