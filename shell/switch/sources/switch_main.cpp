//
//  ios_main.m
//  emulator
//
//  Created by admin on 12/17/14.
//  Copyright (c) 2014 Karen Tsai (angelXwind). All rights reserved.
//


#include <assert.h>
#include <poll.h>
//#include <curses.h>
#include <fcntl.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/param.h>
#include <sys/time.h>
#include "hw/sh4/dyna/blockmanager.h"
#include <unistd.h>
#include "hw/maple/maple_cfg.h"
#include "stdclass.h"
#include <switch.h>

#include <EGL/egl.h>    // EGL library
#include <GLES2/gl2.h>  // OpenGL ES 2.0 library

#define TRACE printf

void dc_stop(void);
#define key_CONT_B            (1 << 1)
#define key_CONT_A            (1 << 2)
#define key_CONT_START        (1 << 3)
#define key_CONT_DPAD_UP      (1 << 4)
#define key_CONT_DPAD_DOWN    (1 << 5)
#define key_CONT_DPAD_LEFT    (1 << 6)
#define key_CONT_DPAD_RIGHT   (1 << 7)
#define key_CONT_Y            (1 << 9)
#define key_CONT_X            (1 << 10)

static int s_nxlinkSock = -1;

static void initNxLink()
{
	if (R_FAILED(socketInitializeDefault()))
		return;

	s_nxlinkSock = nxlinkStdio();
	if (s_nxlinkSock >= 0)
		printf("printf output now goes to nxlink server");
	else
		socketExit();
}

static void deinitNxLink()
{
	if (s_nxlinkSock >= 0)
	{
		close(s_nxlinkSock);
		socketExit();
		s_nxlinkSock = -1;
	}
}

extern "C" void userAppInit()
{
	initNxLink();
}

extern "C" void userAppExit()
{
	deinitNxLink();
}


void egl_stealcntx();
static EGLDisplay s_display;
static EGLContext s_context;
static EGLSurface s_surface;

static bool initEgl(NWindow* win)
{
    // Connect to the EGL default display
    s_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (!s_display)
    {
        printf("Could not connect to display! error: %d", eglGetError());
        goto _fail0;
    }

    // Initialize the EGL display connection
    eglInitialize(s_display, NULL, NULL);

    // Get an appropriate EGL framebuffer configuration
    EGLConfig config;
    EGLint numConfigs;
    static const EGLint framebufferAttributeList[] =
    {
        EGL_RED_SIZE,     8,
        EGL_GREEN_SIZE,   8,
        EGL_BLUE_SIZE,    8,
        EGL_ALPHA_SIZE,   8,
        EGL_DEPTH_SIZE,   24,
        EGL_STENCIL_SIZE, 8,
        EGL_NONE
    };
    eglChooseConfig(s_display, framebufferAttributeList, &config, 1, &numConfigs);
    if (numConfigs == 0)
    {
        TRACE("No config found! error: %d", eglGetError());
        goto _fail1;
    }

    // Create an EGL window surface
    s_surface = eglCreateWindowSurface(s_display, config, win, NULL);
    if (!s_surface)
    {
        TRACE("Surface creation failed! error: %d", eglGetError());
        goto _fail1;
    }

    // Create an EGL rendering context
    static const EGLint contextAttributeList[] =
    {
        EGL_CONTEXT_CLIENT_VERSION, 2, // request OpenGL ES 2.x
        EGL_NONE
    };
    s_context = eglCreateContext(s_display, config, EGL_NO_CONTEXT, contextAttributeList);
    if (!s_context)
    {
        TRACE("Context creation failed! error: %d", eglGetError());
        goto _fail2;
    }

    // Connect the context to the surface
    eglMakeCurrent(s_display, s_surface, s_surface, s_context);
    return true;

_fail2:
    eglDestroySurface(s_display, s_surface);
    s_surface = NULL;
_fail1:
    eglTerminate(s_display);
    s_display = NULL;
_fail0:
    return false;
}


static void setMesaConfig()
{
    // Uncomment below to disable error checking and save CPU time (useful for production):
    //setenv("MESA_NO_ERROR", "1", 1);

    // Uncomment below to enable Mesa logging:
    //setenv("EGL_LOG_LEVEL", "debug", 1);
    //setenv("MESA_VERBOSE", "all", 1);
    //setenv("NOUVEAU_MESA_DEBUG", "1", 1);

    // Uncomment below to enable shader debugging in Nouveau:
    //setenv("NV50_PROG_OPTIMIZE", "0", 1);
    //setenv("NV50_PROG_DEBUG", "1", 1);
    //setenv("NV50_PROG_CHIPSET", "0x120", 1);
}

int msgboxf(const wchar* text,unsigned int type,...)
{
    va_list args;

    wchar temp[2048];
    va_start(args, type);
    vsprintf(temp, text, args);
    va_end(args);

    //printf(NULL,temp,VER_SHORTNAME,type | MB_TASKMODAL);
    puts(temp);
    return 0;
}

void common_linux_setup();
int dc_init(int argc,wchar* argv[]);
void dc_run();

u16 kcode[4];
u32 vks[4];
s8 joyx[4],joyy[4];
u8 rt[4],lt[4];

extern "C" int main(int argc, wchar* argv[])
{
    //consoleInit(NULL);
    //printf("Hello World!");

    // Set mesa configuration (useful for debugging)
    setMesaConfig();

    // Initialize EGL on the default window
    if (!initEgl(nwindowGetDefault()))
        return EXIT_FAILURE;

	egl_stealcntx();

    //if (argc==2)
    //ndcid=atoi(argv[1]);

    std::string homedir = "/reicast";
    set_user_config_dir(homedir);
    set_user_data_dir(homedir);

    // freopen( (homedir + "/log.txt").c_str(), "wb", stdout);
	// setbuf(stdout, 0);
    printf("Config dir is: %s\n", get_writable_config_path("/").c_str());
    printf("Data dir is:   %s\n", get_writable_data_path("/").c_str());

    //common_linux_setup();

    settings.profile.run_counts=0;

    dc_init(argc,argv);

	while(appletMainLoop())
    {
        //Scan all the inputs. This should be done once for each frame
        //hidScanInput();

        //hidKeysDown returns information about which buttons have been just pressed (and they weren't in the previous frame)
        //u64 kDown = hidKeysDown(CONTROLLER_P1_AUTO);

        //if (kDown & KEY_PLUS) break; // break in order to return to hbmenu

		dc_run();

        //consoleUpdate(NULL);
    }

    //consoleExit(NULL);


    return 0;
}

void os_DoEvents() {

}


u32  os_Push(void*, u32, bool) {
    return 1;
}

void os_SetWindowText(const char* t) {
    puts(t);
}

void os_CreateWindow() {

}

void os_SetupInput() {
	mcfg_CreateDevicesFromConfig();
}

void UpdateInputState(u32 port) {
    hidScanInput();
    u32 kDown = hidKeysHeld(CONTROLLER_P1_AUTO);
	kcode[port]=0xFFFF;
    if (kDown & KEY_B)
		kcode[port]&=~key_CONT_A;
    if (kDown & KEY_A)
		kcode[port]&=~key_CONT_B;
    if (kDown & KEY_X)
		kcode[port]&=~key_CONT_Y;
    if (kDown & KEY_Y)
		kcode[port]&=~key_CONT_X;

    if (kDown & KEY_DLEFT)
		kcode[port]&=~key_CONT_DPAD_LEFT;
    if (kDown & KEY_DRIGHT)
		kcode[port]&=~key_CONT_DPAD_RIGHT;
    if (kDown & KEY_DUP)
		kcode[port]&=~key_CONT_DPAD_UP;
    if (kDown & KEY_DDOWN)
		kcode[port]&=~key_CONT_DPAD_DOWN;

    if (kDown & KEY_PLUS)
		kcode[port]&=~key_CONT_START;
    if (kDown & KEY_MINUS)
		dc_stop();

	lt[port]=(kDown & (KEY_ZL | KEY_L)) ? 255 : 0;
	rt[port]=(kDown & (KEY_ZR | KEY_R)) ? 255 : 0;

	JoystickPosition pos_left;
    hidJoystickRead(&pos_left, CONTROLLER_P1_AUTO, JOYSTICK_LEFT);
	joyx[port] = pos_left.dx / 257;
	joyy[port] = -pos_left.dy / 257;
}

void UpdateVibration(u32 port, u32 value) {

}

int get_mic_data(u8* buffer) {

}

void* libPvr_GetRenderTarget() {
    return 0;
}

void* libPvr_GetRenderSurface() {
    return 0;

}
/*
bool gl_init(void*, void*) {
    return true;
}

void gl_term() {

}

void gl_swap() {

}*/

void os_DebugBreak() { exit(-1); }

//cResetEvent Calss
cResetEvent::cResetEvent(bool State,bool Auto)
{
}
cResetEvent::~cResetEvent()
{
}
void cResetEvent::Set()//Signal
{
}
void cResetEvent::Reset()//reset
{
}
void cResetEvent::Wait(u32 msec)//Wait for signal , then reset
{
}
void cResetEvent::Wait()//Wait for signal , then reset
{
}


/*void   eventLoadRemote(Event* t, Handle handle, bool autoclear);
void   eventClose(Event* t);

/// Returns whether the Event is initialized.
static inline bool eventActive(Event* t)
{
    return t->revent != INVALID_HANDLE;
}

Result eventWait(Event* t, u64 timeout);
Result eventFire(Event* t);
Result eventClear(Event* t);
*/

double os_GetSeconds()
{
	timeval a;
	gettimeofday (&a,0);
	static u64 tvs_base=a.tv_sec;
	return a.tv_sec-tvs_base+a.tv_usec/1000000.0;
}

int push_vmu_screen(u8* buffer) { return 0; }

void VArray2::LockRegion(u32 offset,u32 size) {}
void VArray2::UnLockRegion(u32 offset,u32 size) {}

