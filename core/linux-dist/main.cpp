#include "types.h"
#include "cfg/cfg.h"

#include "hw/sh4/dyna/blockmanager.h"
#include <unistd.h>

#include <libco.h>

int screen_width  = 640;
int screen_height = 480;

void emit_WriteCodeCache(void);
void SetupInput(void);

void common_linux_setup();
int dc_init(int argc,wchar* argv[]);
void dc_run();

static cothread_t ct_main;
static cothread_t ct_dc;

static int co_argc;
static wchar** co_argv;

static void co_dc_thread(void)
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

void co_dc_run(void)
{
   puts("ENTER LOOP");
	co_switch(ct_dc);
}

void co_dc_yield(void)
{
	co_switch(ct_main);
}

string find_user_config_dir(void)
{
	// Unable to detect config dir, use the current folder
	return ".";
}

string find_user_data_dir(void)
{
	// Unable to detect config dir, use the current folder
	return ".";
}

void main_init(int argc, wchar *argv[] )
{
   /* Set directories */
   set_user_config_dir(find_user_config_dir());
   set_user_data_dir(find_user_data_dir());
   printf("Config dir is: %s\n", get_writable_config_path("/").c_str());
   printf("Data dir is:   %s\n", get_writable_data_path("/").c_str());

   common_linux_setup();

   settings.profile.run_counts=0;

   co_dc_init(argc,argv);

   SetupInput();
}

void main_run(int argc, wchar *argv[] )
{
   main_init(argc, argv);

   while (true)
      co_dc_run();
}

int get_mic_data(u8* buffer) { return 0; }
int push_vmu_screen(u8* buffer) { return 0; }

void os_DebugBreak(void)
{
   printf("DEBUGBREAK!\n");
   exit(-1);
}
