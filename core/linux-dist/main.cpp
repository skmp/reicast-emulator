#include "types.h"
#include "cfg/cfg.h"

#include <stdarg.h>
#include "hw/sh4/dyna/blockmanager.h"
#include <unistd.h>

#include <libco.h>

/* forward declaration(s) */
void SetupInput(void);

int msgboxf(const wchar* text, unsigned int type, ...)
{
	va_list args;

	wchar temp[2048];
	va_start(args, type);
	vsprintf(temp, text, args);
	va_end(args);

	//printf(NULL,temp,VER_SHORTNAME,type | MB_TASKMODAL);
	puts(temp);
	return MBX_OK;
}

u16 kcode[4] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
u8 rt[4] = {0, 0, 0, 0};
u8 lt[4] = {0, 0, 0, 0};
u32 vks[4];
s8 joyx[4], joyy[4];

void emit_WriteCodeCache();

void UpdateInputState(u32 port)
{
}

void common_linux_setup();
int dc_init(int argc,wchar* argv[]);
void dc_run();


static cothread_t ct_main;
static cothread_t ct_dc;

static int co_argc;
static wchar** co_argv;

static void co_dc_thread()
{
	dc_init(co_argc,co_argv);
	co_switch(ct_main);
	
	dc_run();
}

static void co_dc_init(int argc,wchar* argv[])
{
	ct_main = co_active();
	ct_dc = co_create(1024*1024/*why does libco demand me to know this*/, co_dc_thread);
	co_argc=argc;
	co_argv=argv;
	co_switch(ct_dc);
}

static void co_dc_run(void)
{
   puts("ENTER LOOP");
	co_switch(ct_dc);
}

void co_dc_yield(void)
{
	co_switch(ct_main);
}

static void retro_init(int argc, wchar *argv[] )
{
	common_linux_setup();

	settings.profile.run_counts=0;

	co_dc_init(argc,argv);

	SetupInput();
}

int main(int argc, wchar* argv[])
{
   retro_init(argc, argv);

   while (true)
      co_dc_run();

	return 0;
}

int get_mic_data(u8* buffer) { return 0; }
int push_vmu_screen(u8* buffer) { return 0; }

void os_DebugBreak()
{
   printf("DEBUGBREAK!\n");
   exit(-1);
}
