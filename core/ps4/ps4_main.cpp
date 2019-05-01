/*
**	reicast: ps4_main.cpp
*/
#include "types.h"
#include "cfg/cfg.h"
#include "hw/maple/maple_cfg.h"

#if 1 // defined(TARGET_PS4) || BUILD_OS==OS_PS4_BSD


#include <kernel_ex.h>
#include <sysmodule_ex.h>
#include <system_service_ex.h>
#include <shellcore_util.h>
#include <piglet.h>
#include <pad.h>
#include <audioout.h>

#include <kernel.h>
#include <video_out.h>
#include <user_service.h>
#include <system_service.h>
#include <gnm.h>


using namespace sce;


#include <sys/cdefs.h>
#include <sys/dmem.h>





SceWindow render_window = { 0, 1920,1080 }; // width, height };
void* libPvr_GetRenderTarget() { return &render_window; }
void* libPvr_GetRenderSurface() { return (void*)EGL_DEFAULT_DISPLAY; }



int get_mic_data(u8* buffer) { return 0; }
int push_vmu_screen(u8* buffer) { return 0; }



void common_linux_setup();
int reicast_init(int argc, char* argv[]);
void *rend_thread(void *);

void dc_term();
void dc_stop();


void ps4_module_init();


int main(int argc, char* argv[])
{
	printf("-- REICAST BETA -- \n");
	//atexit(&cleanup);

	ps4_module_init();


	printf("common_setup()\n");

	/* Set directories */
	set_user_data_dir(PS4_DIR_DATA);
	set_user_config_dir(PS4_DIR_CFG);

	printf("Config dir is: %s\n", get_writable_config_path("/").c_str());
	printf("Data dir is:   %s\n", get_writable_data_path("/").c_str());
	printf("RO Data dir is:   %s\n", get_readonly_data_path("/").c_str());

	common_linux_setup();



	settings.profile.run_counts = 0;

	printf("dc_init();\n");

	if (reicast_init(argc, argv) != 0)
		die("Reicast initialization failed");

	
	printf("rend_thread();\n");

	rend_thread(NULL);

	printf("dc_term();\n");
	dc_term();




err:
	return 0;
}


extern "C" void catchReturnFromMain(int exit_code)
{
	printf("ReturnFromMain(%d)\n",exit_code);
	for(;1;) sceKernelSleep(1);
}



void os_CreateWindow() { }

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







extern SceUserServiceUserId userId;

int32_t pad1_H = 0;



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


void os_SetupInput() {

	mcfg_CreateDevices();

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





void UpdateInputState(u32 port)
{
	ScePadData data;
	
	int ret = scePadReadState(pad1_H, &data);
	if (ret != SCE_OK || !data.connected) {
		printf("Warning, Controller is not connected or failed to read data! \n");
		return;
	}

	if (port != 0) return;	// *FIXME* multiplayer

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
	s8 lsX = data.leftStick.x - 128; lsX = (lsX < 0) ? std::min(lsX, (s8)-8) : std::max(lsX, (s8)7);
	s8 lsY = data.leftStick.y - 128; lsY = (lsY < 0) ? std::min(lsY, (s8)-8) : std::max(lsY, (s8)7);

	joyx[port] = lsX;
	joyy[port] = lsY;

	s8 rsX = data.rightStick.x - 128; rsX = (rsX < 0) ? std::min(rsX, (s8)-8) : std::max(rsX, (s8)7);
	s8 rsY = data.rightStick.y - 128; rsY = (rsY < 0) ? std::min(rsY, (s8)-8) : std::max(rsY, (s8)7);

	if (rsY < -120)		kcode[port] &= ~Btn_DPAD2_UP;
	if (rsY > 119)		kcode[port] &= ~Btn_DPAD2_DOWN;
	if (rsX < -120)		kcode[port] &= ~Btn_DPAD2_LEFT;
	if (rsX > 119)		kcode[port] &= ~Btn_DPAD2_RIGHT;

}

void UpdateVibration(u32 port, u32 value)
{
	ScePadVibrationParam vibParam;
	vibParam.largeMotor = (uint8_t)value;
	vibParam.smallMotor = 0;

	scePadSetVibration(pad1_H, &vibParam);
}






#define USE_CUSTOM_ALLOCATOR 1
#if USE_CUSTOM_ALLOCATOR //defined(TARGET_PS4)		




#include <mspace.h>

bool msp_init=false;
SceLibcMspace msp;




off_t phyAddr = 0;


static size_t maxheap=MB(512), heapsize=0;
static void	*heapbase=nullptr;



void initheap()
{
	int32_t res = SCE_OK;
	if (SCE_OK != (res = sceKernelAllocateMainDirectMemory(MB(512), KB(64), SCE_KERNEL_WB_ONION, &phyAddr))) {
		die("sceKernelAllocateMainDirectMemory() Failed"); // with 0x%08X \n", (unat)res);
		return;
	}

	if (SCE_OK != (res = sceKernelMapDirectMemory(&heapbase,MB(512),SCE_KERNEL_PROT_CPU_RW,0,phyAddr,KB(64)))) {
		die("sceKernelMapDirectMemory() Failed"); // with 0x%08X \n", (unat)res);
		return;
	}
	
	printf("@@@@@@@@@@@@@@@ CUSTOM HEAP %p - %p ####################\n", heapbase, (void*)((unat)heapbase+maxheap));
	//setheap() not needed
}



void InitMSpace()
{
	initheap();

	bzero(&msp,sizeof(SceLibcMspace));
	msp = sceLibcMspaceCreate("ps4mspace", heapbase, maxheap, 0);

	msp_init=true;
}


// sceLibcMspaceMalloc, sceLibcMspaceCalloc, sceLibcMspaceMemalign, sceLibcMspacePosixMemalign, sceLibcMspaceRealloc, sceLibcMspaceReallocalign, and sceLibcMspaceFree





////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


extern "C" void* zmalloc(unsigned long size)
{
	if (!msp_init) InitMSpace();

	return sceLibcMspaceMalloc(msp, size);
}

extern "C" void* z_calloc(size_t nelem, size_t size)	// libz has a zcalloc / global ns clash
{
	if (!msp_init) InitMSpace();

	return sceLibcMspaceCalloc(msp, nelem, size);
}


extern "C" void* zrealloc(void* ptr, unsigned long size)
{
	if (!msp_init) InitMSpace();

	return sceLibcMspaceRealloc(msp, ptr, size);
}

extern "C" int zmemalign(void **ptr, unsigned long alignment, unsigned long size)
{
	if (!msp_init) InitMSpace();

	return sceLibcMspacePosixMemalign(msp, ptr, alignment, size);

}

extern "C" void  zfree(void* ptr)
{
	////////// *FIXME* !!!!!!!!!!!!!!!!
	/// this is a hack !

	if ( ((unat)ptr >= (unat)heapbase) && ((unat)ptr < ((unat)heapbase + maxheap)) )
		sceLibcMspaceFree(msp,ptr);


	//else free(ptr);	//// WHY can't this just work ?! - 
}


void* operator new(size_t size)
{
	return zmalloc(size);
}

void operator delete(void* ptr)
{
	if ( ((unat)ptr < (unat)heapbase) || ((unat)ptr >= ((unat)heapbase + maxheap)) )
		zpf(" >>> IS DELETE @ %p !! \n", ptr);

	zfree(ptr);
}








// Evidently LibcInternal doesn't handle these either ... fml

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




#endif // USE_CUSTOM_ALLOCATOR




#endif // TARGET_PS4
