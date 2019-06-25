#include <cstdio>
#include <cstdarg>
#include <math.h>
#include "types.h"
#ifndef _WIN32
#include <sys/time.h>
#endif

#include <sys/stat.h>
#include <retro_stat.h>

#include <libretro.h>

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
#include <glsm/glsm.h>
#endif
#include "../rend/rend.h"
#include "../hw/sh4/sh4_mem.h"
#include "../hw/sh4/sh4_sched.h"
#include "keyboard_map.h"
#include "hw/maple/maple_cfg.h"
#include "hw/maple/maple_if.h"
#include "../hw/pvr/spg.h"
#include "../hw/naomi/naomi_cart.h"
#include "../imgread/common.h"
#include "../hw/aica/dsp.h"

#if defined(_XBOX) || defined(_WIN32)
char slash = '\\';
#else
char slash = '/';
#endif

#define RETRO_DEVICE_TWINSTICK				RETRO_DEVICE_SUBCLASS( RETRO_DEVICE_JOYPAD, 1 )
#define RETRO_DEVICE_TWINSTICK_SATURN		RETRO_DEVICE_SUBCLASS( RETRO_DEVICE_JOYPAD, 2 )
#define RETRO_DEVICE_ASCIISTICK				RETRO_DEVICE_SUBCLASS( RETRO_DEVICE_JOYPAD, 3 )

#define RETRO_ENVIRONMENT_RETROARCH_START_BLOCK 0x800000

#define RETRO_ENVIRONMENT_SET_SAVE_STATE_IN_BACKGROUND (2 | RETRO_ENVIRONMENT_RETROARCH_START_BLOCK)
                                            /* bool * --
                                            * Boolean value that tells the front end to save states in the
                                            * background or not.
                                            */

#define RETRO_ENVIRONMENT_GET_CLEAR_ALL_THREAD_WAITS_CB (3 | RETRO_ENVIRONMENT_RETROARCH_START_BLOCK)
                                            /* retro_environment_t * --
                                            * Provides the callback to the frontend method which will cancel
                                            * all currently waiting threads.  Used when coordination is needed
                                            * between the core and the frontend to gracefully stop all threads.
                                            */


u32 fskip;
extern int screen_width;
extern int screen_height;
char save_dir[PATH_MAX];
char eeprom_file[PATH_MAX];
char nvmem_file[PATH_MAX];
char nvmem_file2[PATH_MAX];		// AtomisWave
bool boot_to_bios;

static bool devices_need_refresh = false;
static int device_type[4] = {-1,-1,-1,-1};
static int astick_deadzone = 0;
static int trigger_deadzone = 0;
static bool digital_triggers = false;
static bool allow_service_buttons = false;

static bool libretro_supports_bitmasks = false;

u32 kcode[4] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
u8 rt[4] = {0, 0, 0, 0};
u8 lt[4] = {0, 0, 0, 0};
u32 vks[4];
s8 joyx[4], joyy[4];
s8 joyrx[4], joyry[4];
extern f32 mo_x_abs[4];
extern f32 mo_y_abs[4];
extern u32 mo_buttons[4];
extern f32 mo_x_delta[4];
extern f32 mo_y_delta[4];
extern f32 mo_wheel_delta[4];

bool enable_purupuru = true;
static u32 vib_stop_time[4];
static double vib_strength[4];
static double vib_delta[4];

unsigned per_content_vmus = 0;

static bool first_run = true;

enum DreamcastController
{
   DC_BTN_C       = 1,
   DC_BTN_B       = 1<<1,
   DC_BTN_A       = 1<<2,
   DC_BTN_START   = 1<<3,
   DC_DPAD_UP     = 1<<4,
   DC_DPAD_DOWN   = 1<<5,
   DC_DPAD_LEFT   = 1<<6,
   DC_DPAD_RIGHT  = 1<<7,
   DC_BTN_Z       = 1<<8,
   DC_BTN_Y       = 1<<9,
   DC_BTN_X       = 1<<10,
   DC_BTN_D       = 1<<11,
   DC_DPAD2_UP    = 1<<12,
   DC_DPAD2_DOWN  = 1<<13,
   DC_DPAD2_LEFT  = 1<<14,
   DC_DPAD2_RIGHT = 1<<15,

   DC_AXIS_LT = 0X10000,
   DC_AXIS_RT = 0X10001,
   DC_AXIS_X  = 0X20000,
   DC_AXIS_Y  = 0X20001,
};

struct retro_perf_callback perf_cb;
retro_get_cpu_features_t perf_get_cpu_features_cb = NULL;

// Callbacks
retro_log_printf_t         log_cb = NULL;
retro_video_refresh_t      video_cb = NULL;
retro_input_poll_t         poll_cb = NULL;
retro_input_state_t        input_cb = NULL;
retro_audio_sample_batch_t audio_batch_cb = NULL;
retro_environment_t        environ_cb = NULL;
retro_environment_t        frontend_clear_thread_waits_cb = NULL;
static retro_rumble_interface rumble;

int dc_init(int argc,wchar* argv[]);
void dc_reset();
void dc_run();
void dc_term(void);
void dc_stop();
void dc_start();
void FlushCache();	// Arm dynarec (arm and x86 only)
bool dc_is_running();
extern Renderer* renderer;
bool rend_single_frame();
void rend_cancel_emu_wait();
bool acquire_mainloop_lock();

static void refresh_devices(bool descriptors_only);
static void init_disk_control_interface(const char *initial_image_path);

static bool read_m3u(const char *file);

static int co_argc;
static wchar** co_argv;

char *game_data;
char g_base_name[128];
char game_dir[1024];
char game_dir_no_slash[1024];
char vmu_dir_no_slash[PATH_MAX];
char content_name[PATH_MAX];
char g_roms_dir[PATH_MAX];
static bool emu_in_thread = false;
static bool performed_serialization = false;
#if !defined(TARGET_NO_THREADS)
static void *emu_thread_func(void *);
static cThread emu_thread(&emu_thread_func, 0);
static cMutex mtx_serialization ;
static cMutex mtx_mainloop ;
static bool gl_ctx_resetting = false;
bool reset_requested;

// Disk swapping
static struct retro_disk_control_callback retro_disk_control_cb;
static unsigned disk_index = 0;
static std::vector<std::string> disk_paths;
static bool disc_tray_open = false;

static void *emu_thread_func(void *)
{
    emu_in_thread = true ;
    while ( true )
    {
    	performed_serialization = false ;
    	mtx_mainloop.Lock() ;
    	rend_cancel_emu_wait() ;
        dc_run();
        mtx_mainloop.Unlock() ;

    	mtx_serialization.Lock() ;
    	mtx_serialization.Unlock() ;

    	if (!performed_serialization && !reset_requested)
    		break ;
    	if (reset_requested)
    	{
    		dc_reset();
    		reset_requested = false;
    	}
    }

	rend_cancel_emu_wait() ;
    dc_term();

    emu_in_thread = false ;

    return NULL;
}
#endif


void co_dc_yield(void)
{
#if !defined(TARGET_NO_THREADS)
    if (!settings.rend.ThreadedRendering)
#endif
    {
    	if (settings.UpdateMode || settings.UpdateModeForced)
    		return;
    	dc_stop();
    }
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
   video_cb = cb;
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
   // Nothing to do here
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
   audio_batch_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
   poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb)
{
   input_cb = cb;
}

static void input_set_deadzone_stick( int percent )
{
   if ( percent >= 0 && percent <= 100 )
      astick_deadzone = (int)( percent * 0.01f * 0x8000);
}

static void input_set_deadzone_trigger( int percent )
{
   if ( percent >= 0 && percent <= 100 )
      trigger_deadzone = (int)( percent * 0.01f * 0x8000);
}

#define COLORS_STRING "BLACK 02|" \
		"BLUE 03|" \
		"LIGHT_BLUE 04|" \
		"GREEN 05|" \
		"CYAN 06|" \
		"CYAN_BLUE 07|" \
		"LIGHT_GREEN 08|" \
		"CYAN_GREEN 09|" \
		"LIGHT_CYAN 10|" \
		"RED 11|" \
		"PURPLE 12|" \
		"LIGHT_PURPLE 13|" \
		"YELLOW 14|" \
		"GRAY 15|" \
		"LIGHT_PURPLE_2 16|" \
		"LIGHT_GREEN_2 17|" \
		"LIGHT_GREEN_3 18|" \
		"LIGHT_CYAN_2 19|" \
		"LIGHT_RED_2 20|" \
		"MAGENTA 21|" \
		"LIGHT_PURPLE_2 22|" \
		"LIGHT_ORANGE 23|" \
		"ORANGE 24|" \
		"LIGHT_PURPLE_3 25|" \
		"LIGHT_YELLOW 26|" \
		"LIGHT_YELLOW_2 27|" \
		"WHITE 28" \

#define VMU_SCREEN_PARAMS(num)       { \
"reicast_vmu" #num "_screen_display", \
"VMU Screen " #num " Display; disabled|enabled" \
}, \
{ \
"reicast_vmu" #num "_screen_position", \
"VMU Screen " #num " Position; Upper Left|Upper Right|Lower Left|Lower Right" \
}, \
{ \
"reicast_vmu" #num "_screen_size_mult", \
"VMU Screen " #num " Size; 1x|2x|3x|4x|5x" \
}, \
{ \
"reicast_vmu" #num "_pixel_on_color", \
"VMU Screen " #num " Pixel On Color; " "DEFAULT_ON 00|DEFAULT_OFF 01|" \
COLORS_STRING \
}, \
{ \
"reicast_vmu" #num "_pixel_off_color", \
"VMU Screen " #num " Pixel Off Color; " "DEFAULT_OFF 01|DEFAULT_ON 00|" \
COLORS_STRING \
}, \
{ \
"reicast_vmu" #num "_screen_opacity", \
"VMU Screen " #num " Opacity; 100%|90%|80%|70%|60%|50%|40%|30%|20%|10%" \
}, \


#define LIGHTGUN_PARAMS(num)       { \
"reicast_lightgun" #num "_crosshair", \
"Gun Crosshair " #num " Display; disabled|White|Red|Green|Blue" \
}, \



void retro_set_environment(retro_environment_t cb)
{
   environ_cb = cb;

   struct retro_variable variables[] = {
#if ((FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_X86) || (HOST_CPU == CPU_ARM) || (HOST_CPU == CPU_ARM64) || (HOST_CPU == CPU_X64) || TARGET_NO_JIT)
      {
         "reicast_cpu_mode",
         "CPU Mode (restart); "
#if (FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_X86) || (HOST_CPU == CPU_ARM) || (HOST_CPU == CPU_ARM64) || (HOST_CPU == CPU_X64)
            "dynamic_recompiler"
#endif
#ifdef TARGET_NO_JIT
            "|"
            "generic_recompiler"
#endif
            ,
      },
#endif
      {
         "reicast_boot_to_bios",
         "Boot to BIOS (restart); disabled|enabled",
      },
      {
         "reicast_system",
         "System type (restart); auto|dreamcast|naomi|atomiswave",
      },
#ifdef HAVE_OIT
      {
         "reicast_oit_abuffer_size",
         "Accumulation pixel buffer size (restart); 512MB|1GB|2GB",
      },
#endif
      {
         "reicast_internal_resolution",
#ifdef LOW_RES
         "Internal resolution (restart); 320x240|640x480|1280x960|1440x1080|1920x1440|2560x1920|2880x2160|3200x2400|3840x2880|4480x3360|5120x3840|5760x4320|6400x4800|7040x5280|7680x5760|8320x6240|8960x6720|9600x7200|10240x7680|10880x8160|11520x8640|12160x9120|12800x9600",
#else
         "Internal resolution (restart); 640x480|320x240|1280x960|1440x1080|1920x1440|2560x1920|2880x2160|3200x2400|3840x2880|4480x3360|5120x3840|5760x4320|6400x4800|7040x5280|7680x5760|8320x6240|8960x6720|9600x7200|10240x7680|10880x8160|11520x8640|12160x9120|12800x9600",	
#endif
      },
      {
         "reicast_screen_rotation",
         "Screen orientation; horizontal|vertical",
      },
      {
    	 "reicast_alpha_sorting",
#ifdef LOW_END
         "Alpha sorting; per-strip (fast, least accurate)|per-triangle (normal)",
#else
#ifdef HAVE_OIT
         "Alpha sorting; per-triangle (normal)|per-strip (fast, least accurate)|per-pixel (accurate)",
#else
         "Alpha sorting; per-triangle (normal)|per-strip (fast, least accurate)",
#endif
#endif
      },
      {
         "reicast_gdrom_fast_loading",
#ifdef LOW_END
         "GDROM Fast Loading (inaccurate); enabled|disabled",
#else
         "GDROM Fast Loading (inaccurate); disabled|enabled",	
#endif
      },
      {
         "reicast_mipmapping",
         "Mipmapping; enabled|disabled",
      },
      {
         "reicast_volume_modifier_enable",
         "Volume modifier; enabled|disabled",
      },
      {
         "reicast_widescreen_hack",
         "Widescreen hack (restart); disabled|enabled",
      },
      {
         "reicast_cable_type",
#ifdef LOW_END
         "Cable type; VGA (RGB)|TV (RGB)|TV (Composite)",	
#else
         "Cable type; TV (RGB)|TV (Composite)|VGA (RGB)",
#endif
      },
      {
         "reicast_broadcast",
         "Broadcast; Default|PAL_M|PAL_N|NTSC|PAL",
      },
      {
         "reicast_framerate",
         "Framerate; fullspeed|normal",
      },
      {
         "reicast_region",
         "Region; Default|Japan|USA|Europe",
      },
      {
         "reicast_language",
         "Language; Default|Japanese|English|German|French|Spanish|Italian",
      },
      {
         "reicast_div_matching",
#ifdef LOW_END
         "DIV matching (performance, less accurate); enabled|disabled|auto",
#else
         "DIV matching (performance, less accurate); disabled|enabled|auto",	
#endif
      },
      {
         "reicast_analog_stick_deadzone",
         "Analog Stick Deadzone; 15%|20%|25%|30%|0%|5%|10%"
      },
      {
         "reicast_trigger_deadzone",
         "Trigger Deadzone; 0%|5%|10%|15%|20%|25%|30%"
      },
      {
         "reicast_digital_triggers",
         "Digital Triggers; disabled|enabled",
      },
      LIGHTGUN_PARAMS(1)
      LIGHTGUN_PARAMS(2)
      LIGHTGUN_PARAMS(3)
      LIGHTGUN_PARAMS(4)
      {
         "reicast_enable_dsp",
#ifdef LOW_END
         "Enable DSP; disabled|enabled",
#else
         "Enable DSP; enabled|disabled",
#endif
      },
#ifdef HAVE_TEXUPSCALE
      {
         "reicast_texupscale",
         "Texture upscaling (xBRZ); off|2x|4x|6x",
      },
      {
         "reicast_texupscale_max_filtered_texture_size",
         "Texture upscaling max filtered size; 256|512|1024",
      },
#endif
      {
         "reicast_enable_rtt",
         "Enable RTT (Render To Texture); enabled|disabled",
      },
      {
         "reicast_enable_rttb",
         "Enable RTT (Render To Texture) Buffer; disabled|enabled",
      },
      {
         "reicast_render_to_texture_upscaling",
         "Render To Texture Upscaling; 1x|2x|3x|4x|8x",
      },
#if !defined(TARGET_NO_THREADS)
      {
         "reicast_threaded_rendering",
         "Threaded rendering (restart); enabled|disabled",
      },
      {
         "reicast_synchronous_rendering",
         "Synchronous rendering; disabled|enabled",
      },
#endif
      {
         "reicast_frame_skipping",
         "Frame skipping; disabled|1|2|3|4|5|6",
      },
      {
         "reicast_enable_purupuru",
         "Purupuru Pack; enabled|disabled"
      },
      {
         "reicast_allow_service_buttons",
         "Allow Naomi service buttons; disabled|enabled"
      },
      {
         "reicast_custom_textures",
         "Load custom textures; disabled|enabled"
      },
      {
         "reicast_dump_textures",
         "Dump textures; disabled|enabled"
      },
      {
         "reicast_per_content_vmus",
         "Per-game VMUs; disabled|VMU A1|All VMUs"
      },
	  VMU_SCREEN_PARAMS(1)
	  VMU_SCREEN_PARAMS(2)
	  VMU_SCREEN_PARAMS(3)
	  VMU_SCREEN_PARAMS(4)
      { NULL, NULL },
   };

   cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);

   static const struct retro_controller_description ports_default[] =
   {
		 { "Controller",	RETRO_DEVICE_JOYPAD },
		 { "Arcade Stick",	RETRO_DEVICE_ASCIISTICK },
		 { "Keyboard",		RETRO_DEVICE_KEYBOARD },
		 { "Mouse",			RETRO_DEVICE_MOUSE },
		 { "Light Gun",		RETRO_DEVICE_LIGHTGUN },
		 { "Twin Stick",	RETRO_DEVICE_TWINSTICK },
		 { "Saturn Twin-Stick",	RETRO_DEVICE_TWINSTICK_SATURN },
		 { 0 },
   };
   static const struct retro_controller_info ports[] = {
           { ports_default,  7 },
           { ports_default,  7 },
           { ports_default,  7 },
           { ports_default,  7 },
           { 0 },
   };
   environ_cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports);
}

void retro_keyboard_event(bool down, unsigned keycode, uint32_t character, uint16_t key_modifiers);

// Now comes the interesting stuff
void retro_init(void)
{
   screen_width  = 640;
   screen_height = 480;
   // Logging
   struct retro_log_callback log;
   if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
      log_cb = log.log;
   else
      log_cb = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_PERF_INTERFACE, &perf_cb))
      perf_get_cpu_features_cb = perf_cb.get_cpu_features;
   else
      perf_get_cpu_features_cb = NULL;

   // Set color mode
   unsigned color_mode = RETRO_PIXEL_FORMAT_XRGB8888;
   environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &color_mode);

   environ_cb(RETRO_ENVIRONMENT_GET_CLEAR_ALL_THREAD_WAITS_CB, &frontend_clear_thread_waits_cb);

   init_kb_map();
   struct retro_keyboard_callback kb_callback = { &retro_keyboard_event };
   environ_cb(RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK, &kb_callback);

   if (environ_cb(RETRO_ENVIRONMENT_GET_INPUT_BITMASKS, NULL))
      libretro_supports_bitmasks = true;
}

void retro_deinit(void)
{
   first_run = true;

   //When auto-save states are enabled this is needed to prevent the core from shutting down before
   //any save state actions are still running - which results in partial saves
   mtx_serialization.Lock() ;
   mtx_serialization.Unlock() ;

   libretro_supports_bitmasks = false;
}

static bool is_dupe = false;
extern int GDROM_TICK;
static bool rotate_screen = false;

static void update_variables(bool first_startup)
{
   struct retro_variable var;
   int i ;
   char key[256] ;

   var.key = "reicast_per_content_vmus";
   unsigned previous_per_content_vmus = per_content_vmus;
   per_content_vmus = 0;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp("VMU A1", var.value))
         per_content_vmus = 1;
      else if (!strcmp("All VMUs", var.value))
         per_content_vmus = 2;
   }

   if ((per_content_vmus != previous_per_content_vmus) &&
       (settings.System == DC_PLATFORM_DREAMCAST))
   {
      mcfg_DestroyDevices();
      mcfg_CreateDevices();
   }

   var.key = "reicast_widescreen_hack";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "enabled"))
         settings.rend.WideScreen = 1;
      else
         settings.rend.WideScreen = 0;
   }
   else
      settings.rend.WideScreen = 0;

   var.key = "reicast_screen_rotation";
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value && !strcmp("vertical", var.value))
   {
	  rotate_screen = true;
	  settings.rend.WideScreen = 0;
   }

   var.key = "reicast_internal_resolution";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      char *pch;
      char str[100];
      snprintf(str, sizeof(str), "%s", var.value);

      pch = strtok(str, "x");
      if (pch)
         screen_width = strtoul(pch, NULL, 0);
      pch = strtok(NULL, "x");
      if (pch)
         screen_height = strtoul(pch, NULL, 0);

      if (settings.rend.WideScreen)
      {
         if (!strcmp(var.value, "640x480"))
         {
            screen_width  = 854;
         }
         else if (!strcmp(var.value, "1280x960"))
         {
            screen_width  = 1536;
         }
         else if (!strcmp(var.value, "1920x1440"))
            screen_height = 1200;
         else if (!strcmp(var.value, "2560x1920"))
            screen_width = 3200;
      }

      fprintf(stderr, "[reicast]: Got size: %u x %u.\n", screen_width, screen_height);
   }


   var.key = "reicast_cpu_mode";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "dynamic_recompiler"))
         settings.dynarec.Type = 0;
      else if (!strcmp(var.value, "generic_recompiler"))
         settings.dynarec.Type = 1;
   }

   var.key = "reicast_boot_to_bios";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "enabled"))
         boot_to_bios = true;
      else if (!strcmp(var.value, "disabled"))
         boot_to_bios = false;
   }
   else
      boot_to_bios = false;

   var.key = "reicast_gdrom_fast_loading";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "enabled"))
         GDROM_TICK      = 0001000;
      else if (!strcmp(var.value, "disabled"))
         GDROM_TICK      = 1500000;
   }
   else
      GDROM_TICK      = 1500000;

   var.key = "reicast_alpha_sorting";
   int previous_renderer = settings.pvr.rend;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "per-strip (fast, least accurate)"))
      {
    	 settings.pvr.rend = 0;
         settings.pvr.Emulation.AlphaSortMode = 1;
      }
      else if (!strcmp(var.value, "per-triangle (normal)"))
      {
    	 settings.pvr.rend = 0;
         settings.pvr.Emulation.AlphaSortMode = 0;
      }
      else if (!strcmp(var.value, "per-pixel (accurate)"))
      {
    	 settings.pvr.rend = 3;
         settings.pvr.Emulation.AlphaSortMode = 0;	// Not used
      }
   }
   else
   {
	  settings.pvr.rend = 0;
      settings.pvr.Emulation.AlphaSortMode = 0;
   }
   if (!first_startup && previous_renderer != settings.pvr.rend)
	  renderer_changed = true;

   var.key = "reicast_mipmapping";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "enabled"))
         settings.rend.UseMipmaps      = 1;
      else if (!strcmp(var.value, "disabled"))
         settings.rend.UseMipmaps      = 0;
   }
   else
      settings.rend.UseMipmaps      = 1;

   if (first_startup)
   {
      var.key = "reicast_system";

      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      {
         if (!strcmp(var.value, "auto"))
            settings.System = DC_PLATFORM_DREAMCAST;
         else if (!strcmp(var.value, "dreamcast"))
            settings.System = DC_PLATFORM_DREAMCAST;
         else if (!strcmp(var.value, "naomi"))
            settings.System = DC_PLATFORM_NAOMI;
         else if (!strcmp(var.value, "atomiswave"))
            settings.System = DC_PLATFORM_ATOMISWAVE;
      }
      else
         settings.System = DC_PLATFORM_DREAMCAST;

#ifdef HAVE_OIT
      extern GLuint pixel_buffer_size;
      var.key = "reicast_oit_abuffer_size";

      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      {
         if (!strcmp(var.value, "512MB"))
            pixel_buffer_size = 0x20000000u;
         else if (!strcmp(var.value, "1GB"))
            pixel_buffer_size = 0x40000000u;
         else if (!strcmp(var.value, "2GB"))
            pixel_buffer_size = 0x80000000u;
         else
            pixel_buffer_size = 0x20000000u;
      }
      else
         pixel_buffer_size = 0x20000000u;
#endif
   }

   var.key = "reicast_volume_modifier_enable";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "disabled"))
         settings.pvr.Emulation.ModVol      = false;
      else if (!strcmp(var.value, "enabled"))
         settings.pvr.Emulation.ModVol      = true;
   }
   else
      settings.pvr.Emulation.ModVol      = true;

   var.key = "reicast_cable_type";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp("VGA (RGB)", var.value))
         settings.dreamcast.cable = 0;
      else if (!strcmp("TV (RGB)", var.value))
         settings.dreamcast.cable = 2;
      else if (!strcmp("TV (Composite)", var.value))
         settings.dreamcast.cable = 3;
   }

   var.key = "reicast_broadcast";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp("NTSC", var.value))
         settings.dreamcast.broadcast = 0;
      else if (!strcmp("PAL", var.value))
         settings.dreamcast.broadcast = 1;
      else if (!strcmp("PAL_M", var.value))
         settings.dreamcast.broadcast = 2;
      else if (!strcmp("PAL_N", var.value))
         settings.dreamcast.broadcast = 3;
      else if (!strcmp("Default", var.value))
         settings.dreamcast.broadcast = 4;
   }
   else
         settings.dreamcast.broadcast = 4;

   var.key = "reicast_framerate";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp("normal", var.value))
         settings.UpdateMode = 0;
      else if (!strcmp("fullspeed", var.value))
         settings.UpdateMode = 1;
   }
   else
      settings.UpdateMode = 0;

   var.key = "reicast_region";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp("Japan", var.value))
         settings.dreamcast.region = 0;
      else if (!strcmp("USA", var.value))
         settings.dreamcast.region = 1;
      else if (!strcmp("Europe", var.value))
         settings.dreamcast.region = 2;
      else if (!strcmp("Default", var.value))
         settings.dreamcast.region = 3;
   }
   else
         settings.dreamcast.region = 3;

   var.key = "reicast_language";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp("Japanese", var.value))
         settings.dreamcast.language = 0;
      else if (!strcmp("English", var.value))
         settings.dreamcast.language = 1;
      else if (!strcmp("German", var.value))
         settings.dreamcast.language = 2;
      else if (!strcmp("French", var.value))
         settings.dreamcast.language = 3;
      else if (!strcmp("Spanish", var.value))
         settings.dreamcast.language = 4;
      else if (!strcmp("Italian", var.value))
         settings.dreamcast.language = 5;
      else if (!strcmp("Default", var.value))
         settings.dreamcast.language = 6;
   }
   else
         settings.dreamcast.language = 6;

   var.key = "reicast_div_matching";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp("auto", var.value))
      {
         settings.dynarec.DisableDivMatching = 0;
         settings.dynarec.AutoDivMatching = true;
      }
      else if (!strcmp("enabled", var.value))
      {
         settings.dynarec.DisableDivMatching = 0;
         settings.dynarec.AutoDivMatching = false;
      }
      else if (!strcmp("disabled", var.value))
      {
         settings.dynarec.DisableDivMatching = 1;
         settings.dynarec.AutoDivMatching = false;
      }
   }

#ifdef HAVE_TEXUPSCALE
   var.key = "reicast_texupscale";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp("off", var.value))
         settings.rend.TextureUpscale = 1;
      else if (!strcmp("2x", var.value))
         settings.rend.TextureUpscale = 2;
      else if (!strcmp("4x", var.value))
         settings.rend.TextureUpscale = 4;
      else if (!strcmp("6x", var.value))
         settings.rend.TextureUpscale = 6;
   }
   else if (first_startup)
      settings.rend.TextureUpscale = 1;

   var.key = "reicast_texupscale_max_filtered_texture_size";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp("256", var.value))
         settings.rend.MaxFilteredTextureSize = 256;
      else if (!strcmp("512", var.value))
         settings.rend.MaxFilteredTextureSize = 512;
      else if (!strcmp("1024", var.value))
         settings.rend.MaxFilteredTextureSize = 1024;
   }
   else
      settings.rend.MaxFilteredTextureSize = 256;
#endif

   var.key = "reicast_enable_rtt";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp("enabled", var.value))
         settings.rend.RenderToTexture = true;
      else
         settings.rend.RenderToTexture = false;
   }
   else
         settings.rend.RenderToTexture = true;

   var.key = "reicast_enable_rttb";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp("enabled", var.value))
         settings.rend.RenderToTextureBuffer = true;
      else
         settings.rend.RenderToTextureBuffer = false;
   }
   else
      settings.rend.RenderToTextureBuffer = false;

   var.key = "reicast_render_to_texture_upscaling";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      char *pch;
      char str[100];
      sprintf(str, "%s", var.value);

      pch = strtok(str, "x");
      if (pch)
         settings.rend.RenderToTextureUpscale = strtoul(pch, NULL, 0);
   }
   else if (first_startup)
      settings.rend.RenderToTextureUpscale = 1;

#if !defined(TARGET_NO_THREADS)
   if (first_startup)
   {
	   bool save_state_in_background = true ;
	   var.key = "reicast_threaded_rendering";

	   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
	   {
		   if (!strcmp("enabled", var.value))
			   settings.rend.ThreadedRendering = true;
		   else
			   settings.rend.ThreadedRendering = false;
	   }
	   else
		   settings.rend.ThreadedRendering = false;

	   if ( settings.rend.ThreadedRendering  )
		   environ_cb(RETRO_ENVIRONMENT_SET_SAVE_STATE_IN_BACKGROUND, &save_state_in_background);

   }
#endif

   var.key = "reicast_synchronous_rendering";
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
	   if (!strcmp("enabled", var.value))
		   settings.pvr.SynchronousRendering = 1;
	   else
		   settings.pvr.SynchronousRendering = 0;
   }
   else
	   settings.pvr.SynchronousRendering = 0;

   var.key = "reicast_frame_skipping";
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
	   if (!strcmp("disabled", var.value))
		   settings.pvr.ta_skip = 0;
	   else {
		   settings.pvr.ta_skip = max(0, min(6, var.value[0] - '0'));
	   }
   }
   else
	   settings.pvr.ta_skip = 0;

   var.key = "reicast_enable_purupuru";
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (enable_purupuru != (strcmp("enabled", var.value) == 0) && settings.System == DC_PLATFORM_DREAMCAST)
      {
      	enable_purupuru = (strcmp("enabled", var.value) == 0);
      	if (!first_startup)
      		maple_ReconnectDevices();
	else
	{
      		mcfg_DestroyDevices();
      		mcfg_CreateDevices();
	}
      }
   }

   var.key = "reicast_analog_stick_deadzone";
   var.value = NULL;

   if ( environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value )
      input_set_deadzone_stick( atoi( var.value ) );

   var.key = "reicast_trigger_deadzone";
   var.value = NULL;

   if ( environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value )
      input_set_deadzone_trigger( atoi( var.value ) );

   var.key = "reicast_enable_dsp";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp("enabled", var.value))
      {
         settings.aica.DSPEnabled = true;
         settings.aica.NoBatch    = 1;
      }
      else
      {
         settings.aica.DSPEnabled = false;
         settings.aica.NoBatch    = 0;
      }
   }
   else if (first_run)
   {
      settings.aica.DSPEnabled = true;
      settings.aica.NoBatch    = 1;
   }

   var.key = "reicast_digital_triggers";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp("enabled", var.value))
         digital_triggers = true;
      else
         digital_triggers = false;
   }
   else
      digital_triggers = false;

   var.key = "reicast_allow_service_buttons";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp("enabled", var.value))
         allow_service_buttons = true;
      else
         allow_service_buttons = false;
   }
   else
      allow_service_buttons = false;

   var.key = "reicast_custom_textures";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp("enabled", var.value))
         settings.rend.CustomTextures = true;
      else
    	 settings.rend.CustomTextures = false;
   }
   else
	  settings.rend.CustomTextures = false;

   var.key = "reicast_dump_textures";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp("enabled", var.value))
         settings.rend.DumpTextures = true;
      else
    	 settings.rend.DumpTextures = false;
   }
   else
	  settings.rend.DumpTextures = false;

   key[0] = '\0' ;

   var.key = key ;
   for ( i = 0 ; i < 4 ; i++)
   {
      lightgun_params[i].offscreen = true;	   
      lightgun_params[i].x = 0;	   
      lightgun_params[i].y = 0;	   
      lightgun_params[i].dirty = true;	   
      lightgun_params[i].colour = i+1;	   

      snprintf(key, sizeof(key), "reicast_lightgun%d_crosshair", i+1) ;

      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value  )
      {
         if (!strcmp("disabled", var.value))
        	 lightgun_params[i].colour = LIGHTGUN_COLOR_OFF;
         else if (!strcmp("White", var.value))
        	 lightgun_params[i].colour = LIGHTGUN_COLOR_WHITE;
         else if (!strcmp("Red", var.value))
        	 lightgun_params[i].colour = LIGHTGUN_COLOR_RED;
         else if (!strcmp("Green", var.value))
        	 lightgun_params[i].colour = LIGHTGUN_COLOR_GREEN;
         else if (!strcmp("Blue", var.value))
        	 lightgun_params[i].colour = LIGHTGUN_COLOR_BLUE;
      }
	   
      vmu_screen_params[i].vmu_screen_display = false ;
      vmu_screen_params[i].vmu_screen_position = UPPER_LEFT ;
      vmu_screen_params[i].vmu_screen_size_mult = 1 ;
      vmu_screen_params[i].vmu_pixel_on_R = VMU_SCREEN_COLOR_MAP[VMU_DEFAULT_ON].r ;
      vmu_screen_params[i].vmu_pixel_on_G = VMU_SCREEN_COLOR_MAP[VMU_DEFAULT_ON].g ;
      vmu_screen_params[i].vmu_pixel_on_B = VMU_SCREEN_COLOR_MAP[VMU_DEFAULT_ON].b ;
      vmu_screen_params[i].vmu_pixel_off_R = VMU_SCREEN_COLOR_MAP[VMU_DEFAULT_OFF].r ;
      vmu_screen_params[i].vmu_pixel_off_G = VMU_SCREEN_COLOR_MAP[VMU_DEFAULT_OFF].g ;
      vmu_screen_params[i].vmu_pixel_off_B = VMU_SCREEN_COLOR_MAP[VMU_DEFAULT_OFF].b ;
      vmu_screen_params[i].vmu_screen_opacity = 0xFF ;
      vmu_screen_params[i].vmu_screen_needs_update = true ;

      snprintf(key, sizeof(key), "reicast_vmu%d_screen_display", i+1) ;

      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value && !strcmp("enabled", var.value) )
       	 vmu_screen_params[i].vmu_screen_display = true ;


      snprintf(key, sizeof(key), "reicast_vmu%d_screen_position", i+1) ;

      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value  )
      {
         if (!strcmp("Upper Left", var.value))
        	 vmu_screen_params[i].vmu_screen_position = UPPER_LEFT;
         else if (!strcmp("Upper Right", var.value))
        	 vmu_screen_params[i].vmu_screen_position = UPPER_RIGHT;
         else if (!strcmp("Lower Left", var.value))
        	 vmu_screen_params[i].vmu_screen_position = LOWER_LEFT;
         else if (!strcmp("Lower Right", var.value))
        	 vmu_screen_params[i].vmu_screen_position = LOWER_RIGHT;
      }

      snprintf(key, sizeof(key), "reicast_vmu%d_screen_size_mult", i+1) ;

      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value  )
      {
         if (!strcmp("1x", var.value))
        	 vmu_screen_params[i].vmu_screen_size_mult = 1;
         else if (!strcmp("2x", var.value))
        	 vmu_screen_params[i].vmu_screen_size_mult = 2;
         else if (!strcmp("3x", var.value))
        	 vmu_screen_params[i].vmu_screen_size_mult = 3;
         else if (!strcmp("4x", var.value))
        	 vmu_screen_params[i].vmu_screen_size_mult = 4;
         else if (!strcmp("5x", var.value))
        	 vmu_screen_params[i].vmu_screen_size_mult = 5;
      }

      snprintf(key, sizeof(key), "reicast_vmu%d_screen_opacity", i+1) ;

      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value  )
      {
         if (!strcmp("100%", var.value))
        	 vmu_screen_params[i].vmu_screen_opacity = 255;
         else if (!strcmp("90%", var.value))
        	 vmu_screen_params[i].vmu_screen_opacity = 9*25.5;
         else if (!strcmp("80%", var.value))
        	 vmu_screen_params[i].vmu_screen_opacity = 8*25.5;
         else if (!strcmp("70%", var.value))
        	 vmu_screen_params[i].vmu_screen_opacity = 7*25.5;
         else if (!strcmp("60%", var.value))
        	 vmu_screen_params[i].vmu_screen_opacity = 6*25.5;
         else if (!strcmp("50%", var.value))
        	 vmu_screen_params[i].vmu_screen_opacity = 5*25.5;
         else if (!strcmp("40%", var.value))
        	 vmu_screen_params[i].vmu_screen_opacity = 4*25.5;
         else if (!strcmp("30%", var.value))
        	 vmu_screen_params[i].vmu_screen_opacity = 3*25.5;
         else if (!strcmp("20%", var.value))
        	 vmu_screen_params[i].vmu_screen_opacity = 2*25.5;
         else if (!strcmp("10%", var.value))
        	 vmu_screen_params[i].vmu_screen_opacity = 1*25.5;
      }

      snprintf(key, sizeof(key), "reicast_vmu%d_pixel_on_color", i+1) ;

      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value && strlen(var.value)>1 )
      {
    	  int color_idx = atoi(var.value+(strlen(var.value)-2)) ;
    	  vmu_screen_params[i].vmu_pixel_on_R = VMU_SCREEN_COLOR_MAP[color_idx].r ;
    	  vmu_screen_params[i].vmu_pixel_on_G = VMU_SCREEN_COLOR_MAP[color_idx].g ;
    	  vmu_screen_params[i].vmu_pixel_on_B = VMU_SCREEN_COLOR_MAP[color_idx].b ;
      }

      snprintf(key, sizeof(key), "reicast_vmu%d_pixel_off_color", i+1) ;

      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value && strlen(var.value)>1 )
      {
    	  int color_idx = atoi(var.value+(strlen(var.value)-2)) ;
    	  vmu_screen_params[i].vmu_pixel_off_R = VMU_SCREEN_COLOR_MAP[color_idx].r ;
    	  vmu_screen_params[i].vmu_pixel_off_G = VMU_SCREEN_COLOR_MAP[color_idx].g ;
    	  vmu_screen_params[i].vmu_pixel_off_B = VMU_SCREEN_COLOR_MAP[color_idx].b ;
      }


   }

}

void retro_run (void)
{
   bool fastforward = false;
   bool updated     = false;

   if (environ_cb(RETRO_ENVIRONMENT_GET_FASTFORWARDING, &fastforward) && settings.rend.ThreadedRendering)
      settings.aica.LimitFPS = !fastforward;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
      update_variables(false);

   refresh_devices(false);

#if !defined(TARGET_NO_THREADS)
   if (settings.rend.ThreadedRendering)
   {
	   // On the first call, we start the emulator thread
	   if (first_run)
	   {
		   emu_thread.Start();
		   first_run = false;
	   }

	   poll_cb();

	   glsm_ctl(GLSM_CTL_STATE_BIND, NULL);

	   // Render
	   is_dupe = !rend_single_frame();

	   glsm_ctl(GLSM_CTL_STATE_UNBIND, NULL);
   }
   else
#endif
   {
	   dc_run();
   }
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
   video_cb(is_dupe ? 0 : RETRO_HW_FRAME_BUFFER_VALID, screen_width, screen_height, 0);
#endif
#if !defined(TARGET_NO_THREADS)
   if (!settings.rend.ThreadedRendering)
#endif
	   is_dupe = true;
}

void retro_reset (void)
{
#if !defined(TARGET_NO_THREADS)
   mtx_serialization.Lock();
   if (settings.rend.ThreadedRendering)
   {
	  dc_stop();
	  if (!acquire_mainloop_lock())
	  {
		 dc_start();
		 mtx_serialization.Unlock();
		 return;
	  }
   }
#endif

   settings.dreamcast.cable = 3;
   settings.dreamcast.RTC = GetRTC_now();
   update_variables(false);
   dc_reset();

#if !defined(TARGET_NO_THREADS)
   if (settings.rend.ThreadedRendering)
   {
	  performed_serialization = true;
	  mtx_mainloop.Unlock();
   }
   mtx_serialization.Unlock();
#endif
}

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
static void context_reset(void)
{
   printf("context_reset.\n");
   gl_ctx_resetting = false;
   glsm_ctl(GLSM_CTL_STATE_CONTEXT_RESET, NULL);
   glsm_ctl(GLSM_CTL_STATE_SETUP, NULL);
}

static void context_destroy(void)
{
   gl_ctx_resetting = true;
   renderer_changed = true;
   glsm_ctl(GLSM_CTL_STATE_CONTEXT_DESTROY, NULL);
}
#endif

static void extract_directory(char *buf, const char *path, size_t size)
{
   strncpy(buf, path, size - 1);
   buf[size - 1] = '\0';

   char *base = strrchr(buf, '/');
   if (!base)
      base = strrchr(buf, '\\');

   if (base)
      *base = '\0';
   else
      buf[0] = '\0';
}

extern void dc_prepare_system(void);

static uint32_t map_gamepad_button(unsigned device, unsigned id)
{
   static const uint32_t dc_joymap[] =
   {
      /* JOYPAD_B      */ DC_BTN_A,
      /* JOYPAD_Y      */ DC_BTN_X,
      /* JOYPAD_SELECT */ 0,
      /* JOYPAD_START  */ DC_BTN_START,
      /* JOYPAD_UP     */ DC_DPAD_UP,
      /* JOYPAD_DOWN   */ DC_DPAD_DOWN,
      /* JOYPAD_LEFT   */ DC_DPAD_LEFT,
      /* JOYPAD_RIGHT  */ DC_DPAD_RIGHT,
      /* JOYPAD_A      */ DC_BTN_B,
      /* JOYPAD_X      */ DC_BTN_Y,
   };

   static const uint32_t dc_lg_joymap[] =
   {
	  /* deprecated */ 			0,
	  /* deprecated */ 			0,
	  /* LIGHTGUN_TRIGGER */	DC_BTN_A,
	  /* LIGHTGUN_AUX_A */		DC_BTN_B,
	  /* LIGHTGUN_AUX_B */ 		0,
	  /* deprecated */ 			0,
	  /* LIGHTGUN_START */		DC_BTN_START,
	  /* LIGHTGUN_SELECT */ 	0,
	  /* LIGHTGUN_AUX_C */		0,
	  /* LIGHTGUN_UP   */ 		DC_DPAD_UP,
	  /* LIGHTGUN_DOWN   */ 	DC_DPAD_DOWN,
	  /* LIGHTGUN_LEFT   */ 	DC_DPAD_LEFT,
	  /* LIGHTGUN_RIGHT  */ 	DC_DPAD_RIGHT,
   };

   static const uint32_t aw_joymap[] =
   {
      /* JOYPAD_B      */ AWAVE_BTN0_KEY, /* BTN1 */
      /* JOYPAD_Y      */ AWAVE_BTN2_KEY, /* BTN3 */
      /* JOYPAD_SELECT */ AWAVE_COIN_KEY,
      /* JOYPAD_START  */ AWAVE_START_KEY,
      /* JOYPAD_UP     */ AWAVE_UP_KEY,
      /* JOYPAD_DOWN   */ AWAVE_DOWN_KEY,
      /* JOYPAD_LEFT   */ AWAVE_LEFT_KEY,
      /* JOYPAD_RIGHT  */ AWAVE_RIGHT_KEY,
      /* JOYPAD_A      */ AWAVE_BTN1_KEY, /* BTN2 */
      /* JOYPAD_X      */ AWAVE_BTN3_KEY, /* BTN4 */
      /* JOYPAD_L      */ 0,
      /* JOYPAD_R      */ AWAVE_BTN4_KEY, /* BTN5 */
      /* JOYPAD_L2     */ 0,
      /* JOYPAD_R2     */ 0,
      /* JOYPAD_L3     */ AWAVE_TEST_KEY,
      /* JOYPAD_R3     */ AWAVE_SERVICE_KEY,
   };

   static const uint32_t aw_lg_joymap[] =
   {
	   /* deprecated */ 			0,
	   /* deprecated */ 			0,
	   /* LIGHTGUN_TRIGGER */	AWAVE_TRIGGER_KEY,
	   /* LIGHTGUN_AUX_A */		AWAVE_BTN0_KEY,
	   /* LIGHTGUN_AUX_B */ 	AWAVE_BTN1_KEY,
	   /* deprecated */ 			0,
	   /* LIGHTGUN_START */		AWAVE_START_KEY,
	   /* LIGHTGUN_SELECT */ 	AWAVE_COIN_KEY,
	   /* LIGHTGUN_AUX_C */		AWAVE_BTN2_KEY,
	   /* LIGHTGUN_UP   */ 		AWAVE_UP_KEY,
	   /* LIGHTGUN_DOWN   */ 	AWAVE_DOWN_KEY,
	   /* LIGHTGUN_LEFT   */ 	AWAVE_LEFT_KEY,
	   /* LIGHTGUN_RIGHT  */ 	AWAVE_RIGHT_KEY,
   };

   static const uint32_t nao_joymap[] =
   {
      /* JOYPAD_B      */ NAOMI_BTN0_KEY, /* BTN1 */
      /* JOYPAD_Y      */ NAOMI_BTN2_KEY, /* BTN3 */
      /* JOYPAD_SELECT */ NAOMI_COIN_KEY,
      /* JOYPAD_START  */ NAOMI_START_KEY,
      /* JOYPAD_UP     */ NAOMI_UP_KEY,
      /* JOYPAD_DOWN   */ NAOMI_DOWN_KEY,
      /* JOYPAD_LEFT   */ NAOMI_LEFT_KEY,
      /* JOYPAD_RIGHT  */ NAOMI_RIGHT_KEY,
      /* JOYPAD_A      */ NAOMI_BTN1_KEY, /* BTN2 */
      /* JOYPAD_X      */ NAOMI_BTN3_KEY, /* BTN4 */
      /* JOYPAD_L      */ NAOMI_BTN5_KEY, /* BTN6 */
      /* JOYPAD_R      */ NAOMI_BTN4_KEY, /* BTN5 */
      /* JOYPAD_L2     */ NAOMI_BTN7_KEY, /* BTN8 */
      /* JOYPAD_R2     */ NAOMI_BTN6_KEY, /* BTN7 */
      /* JOYPAD_L3     */ NAOMI_TEST_KEY,
      /* JOYPAD_R3     */ NAOMI_SERVICE_KEY,
   };

   static const uint32_t nao_lg_joymap[] =
   {
	   /* deprecated */ 			0,
	   /* deprecated */ 			0,
	   /* LIGHTGUN_TRIGGER */	NAOMI_BTN0_KEY,
	   /* LIGHTGUN_AUX_A */		NAOMI_BTN1_KEY,
	   /* LIGHTGUN_AUX_B */ 	NAOMI_BTN2_KEY,
	   /* deprecated */ 			0,
	   /* LIGHTGUN_START */		NAOMI_START_KEY,
	   /* LIGHTGUN_SELECT */ 	NAOMI_COIN_KEY,
	   /* LIGHTGUN_AUX_C */		NAOMI_BTN3_KEY,
	   /* LIGHTGUN_UP   */ 		NAOMI_UP_KEY,
	   /* LIGHTGUN_DOWN   */ 	NAOMI_DOWN_KEY,
	   /* LIGHTGUN_LEFT   */ 	NAOMI_LEFT_KEY,
	   /* LIGHTGUN_RIGHT  */ 	NAOMI_RIGHT_KEY,
   };

   const uint32_t *joymap;
   size_t joymap_size;

   switch (settings.System)
   {
   case DC_PLATFORM_DREAMCAST:
   case DC_PLATFORM_DEV_UNIT:
	  switch (device)
	  {
	  case RETRO_DEVICE_JOYPAD:
		 joymap = dc_joymap;
		 joymap_size = ARRAY_SIZE(dc_joymap);
		 break;
	  case RETRO_DEVICE_LIGHTGUN:
		 joymap = dc_lg_joymap;
		 joymap_size = ARRAY_SIZE(dc_lg_joymap);
		 break;
	  default:
		 return 0;
	  }
	  break;

   case DC_PLATFORM_NAOMI:
	  switch (device)
	  {
	  case RETRO_DEVICE_JOYPAD:
		 joymap = nao_joymap;
		 joymap_size = ARRAY_SIZE(nao_joymap);
		 break;
	  case RETRO_DEVICE_LIGHTGUN:
		 joymap = nao_lg_joymap;
		 joymap_size = ARRAY_SIZE(nao_lg_joymap);
		 break;
	  default:
		 return 0;
	  }
	  break;

   case DC_PLATFORM_ATOMISWAVE:
	  switch (device)
	  {
	  case RETRO_DEVICE_JOYPAD:
		 joymap = aw_joymap;
		 joymap_size = ARRAY_SIZE(aw_joymap);
		 break;
	  case RETRO_DEVICE_LIGHTGUN:
		 joymap = aw_lg_joymap;
		 joymap_size = ARRAY_SIZE(aw_lg_joymap);
		 break;
	  default:
		 return 0;
	  }
	  break;

   default:
	  return 0;
   }

   if (id >= joymap_size)
	  return 0;
   uint32_t mapped = joymap[id];
   // Hack to bind Button 9 instead of Service when not used
   if (id == RETRO_DEVICE_ID_JOYPAD_R3 && device == RETRO_DEVICE_JOYPAD && settings.System == DC_PLATFORM_NAOMI && !allow_service_buttons)
	  mapped = NAOMI_BTN8_KEY;
   return mapped;
}

static const char *get_button_name(unsigned device, unsigned id, const char *default_name)
{
   if (naomi_game_inputs == NULL)
	  return default_name;
   uint32_t mask = map_gamepad_button(device, id);
   if (mask == 0)
	  return NULL;
   for (int i = 0; naomi_game_inputs->buttons[i].mask != 0; i++)
	  if (naomi_game_inputs->buttons[i].mask == mask)
		 return naomi_game_inputs->buttons[i].name;
   return NULL;
}

static const char *get_axis_name(unsigned index, const char *default_name)
{
   if (naomi_game_inputs == NULL)
	  return default_name;
   for (int i = 0; naomi_game_inputs->axes[i].name != NULL; i++)
	  if (i == index)
		 return naomi_game_inputs->axes[i].name;
   return NULL;
}

static void set_input_descriptors()
{
   struct retro_input_descriptor desc[22 * 4 + 1];
   int descriptor_index = 0;
   if (settings.System == DC_PLATFORM_NAOMI || settings.System == DC_PLATFORM_ATOMISWAVE)
   {
	  const char *name;

      for (unsigned i = 0; i < MAPLE_PORTS; i++)
      {
    	 switch (maple_devices[i])
    	 {
    	 case MDT_LightGun:
    		name = get_button_name(RETRO_DEVICE_LIGHTGUN, RETRO_DEVICE_ID_LIGHTGUN_DPAD_LEFT, "D-Pad Left");
    		if (name != NULL)
    		   desc[descriptor_index++] = { i, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_DPAD_LEFT, name };
    		name = get_button_name(RETRO_DEVICE_LIGHTGUN, RETRO_DEVICE_ID_LIGHTGUN_DPAD_UP, "D-Pad Up");
    		if (name != NULL)
    		   desc[descriptor_index++] = { i, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_DPAD_UP, name };
    		name = get_button_name(RETRO_DEVICE_LIGHTGUN, RETRO_DEVICE_ID_LIGHTGUN_DPAD_DOWN, "D-Pad Down");
    		if (name != NULL)
    		   desc[descriptor_index++] = { i, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_DPAD_DOWN, name };
    		name = get_button_name(RETRO_DEVICE_LIGHTGUN, RETRO_DEVICE_ID_LIGHTGUN_DPAD_RIGHT, "D-Pad Right") ;
    		if (name != NULL)
    		   desc[descriptor_index++] = { i, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_DPAD_RIGHT, name };
    		name = get_button_name(RETRO_DEVICE_LIGHTGUN, RETRO_DEVICE_ID_LIGHTGUN_TRIGGER, "Trigger");
    		if (name != NULL)
    		   desc[descriptor_index++] = { i, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_TRIGGER, name };
    		name = get_button_name(RETRO_DEVICE_LIGHTGUN, RETRO_DEVICE_ID_LIGHTGUN_AUX_A, "Button 1");
    		if (name != NULL)
    		   desc[descriptor_index++] = { i, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_AUX_A, name };
    		name = get_button_name(RETRO_DEVICE_LIGHTGUN, RETRO_DEVICE_ID_LIGHTGUN_AUX_B, "Button 2");
    		if (name != NULL)
    		   desc[descriptor_index++] = { i, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_AUX_B, name };
    		name = get_button_name(RETRO_DEVICE_LIGHTGUN, RETRO_DEVICE_ID_LIGHTGUN_AUX_C, "Button 3");
    		if (name != NULL)
    		   desc[descriptor_index++] = { i, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_AUX_C, name };
    		desc[descriptor_index++] = { i, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_RELOAD, "Reload" };
    		name = get_button_name(RETRO_DEVICE_LIGHTGUN, RETRO_DEVICE_ID_LIGHTGUN_SELECT, "Coin");
    		if (name != NULL)
    		   desc[descriptor_index++] = { i, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SELECT, name };
    		name = get_button_name(RETRO_DEVICE_LIGHTGUN, RETRO_DEVICE_ID_LIGHTGUN_START, "Start");
    		if (name != NULL)
    		   desc[descriptor_index++] = { i, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_START, name };
    		break;

    	 case MDT_SegaController:
    		name = get_button_name(RETRO_DEVICE_JOYPAD, RETRO_DEVICE_ID_JOYPAD_LEFT, "D-Pad Left");
    		if (name != NULL)
    		   desc[descriptor_index++] = { i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, name };
    		name = get_button_name(RETRO_DEVICE_JOYPAD, RETRO_DEVICE_ID_JOYPAD_UP, "D-Pad Up");
    		if (name != NULL)
    		   desc[descriptor_index++] = { i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, name };
    		name = get_button_name(RETRO_DEVICE_JOYPAD, RETRO_DEVICE_ID_JOYPAD_DOWN, "D-Pad Down");
    		if (name != NULL)
    		   desc[descriptor_index++] = { i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  name };
    		name = get_button_name(RETRO_DEVICE_JOYPAD, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right");
    		if (name != NULL)
    		   desc[descriptor_index++] = { i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, name };
    		name = get_button_name(RETRO_DEVICE_JOYPAD, RETRO_DEVICE_ID_JOYPAD_B, "Button 1");
    		if (name != NULL)
    		   desc[descriptor_index++] = { i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, name };
    		name = get_button_name(RETRO_DEVICE_JOYPAD, RETRO_DEVICE_ID_JOYPAD_A, "Button 2");
    		if (name != NULL)
    		   desc[descriptor_index++] = { i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, name };
    		name = get_button_name(RETRO_DEVICE_JOYPAD, RETRO_DEVICE_ID_JOYPAD_Y, "Button 3");
    		if (name != NULL)
    		   desc[descriptor_index++] = { i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, name };
    		name = get_button_name(RETRO_DEVICE_JOYPAD, RETRO_DEVICE_ID_JOYPAD_X, "Button 4");
    		if (name != NULL)
    		   desc[descriptor_index++] = { i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, name };
    		name = get_button_name(RETRO_DEVICE_JOYPAD, RETRO_DEVICE_ID_JOYPAD_R, "Button 5");
    		if (name != NULL)
    		   desc[descriptor_index++] = { i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, name };
    		name = get_button_name(RETRO_DEVICE_JOYPAD, RETRO_DEVICE_ID_JOYPAD_L, "Button 6");
    		if (name != NULL)
    		   desc[descriptor_index++] = { i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, name };
    		name = get_button_name(RETRO_DEVICE_JOYPAD, RETRO_DEVICE_ID_JOYPAD_R2, "Button 7");
    		if (name != NULL)
    		   desc[descriptor_index++] = { i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2, name };
    		name = get_button_name(RETRO_DEVICE_JOYPAD, RETRO_DEVICE_ID_JOYPAD_L2, "Button 8");
    		if (name != NULL)
    		   desc[descriptor_index++] = { i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2, name };
    		name = get_button_name(RETRO_DEVICE_JOYPAD, RETRO_DEVICE_ID_JOYPAD_START, "Start");
    		if (name != NULL)
    		   desc[descriptor_index++] = { i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, name };
    		name = get_button_name(RETRO_DEVICE_JOYPAD, RETRO_DEVICE_ID_JOYPAD_SELECT, "Coin");
    		if (name != NULL)
    		   desc[descriptor_index++] = { i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, name };
    		name = get_button_name(RETRO_DEVICE_JOYPAD, RETRO_DEVICE_ID_JOYPAD_L3, "Test");
    		if (name != NULL)
    		   desc[descriptor_index++] = { i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3, name };
    		name = get_button_name(RETRO_DEVICE_JOYPAD, RETRO_DEVICE_ID_JOYPAD_R3, "Service");
    		if (name != NULL)
    		   desc[descriptor_index++] = { i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3, name };
    		name = get_axis_name(0, "Axis 1");
    		if (name != NULL && name[0] != '\0')
    		   desc[descriptor_index++] = { i, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, name };
    		name = get_axis_name(1, "Axis 2");
    		if (name != NULL && name[0] != '\0')
    		   desc[descriptor_index++] = { i, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, name };
    		name = get_axis_name(2, "Axis 3");
    		if (name != NULL && name[0] != '\0')
    		   desc[descriptor_index++] = { i, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X, name };
    		name = get_axis_name(3, "Axis 4");
    		if (name != NULL && name[0] != '\0')
    		   desc[descriptor_index++] = { i, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y, name };
    		name = get_axis_name(4, NULL);
    		if (name != NULL && name[0] != '\0')
    		   desc[descriptor_index++] = { i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2, name };
    		name = get_axis_name(5, NULL);
    		if (name != NULL && name[0] != '\0')
    		   desc[descriptor_index++] = { i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2, name };
    		break;
    	 }
  	 
      }
   }
   else
   {
      for (unsigned i = 0; i < MAPLE_PORTS; i++)
      {
    	 switch (maple_devices[i])
    	 {
    	 case MDT_SegaController:
    		desc[descriptor_index++] = { i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "D-Pad Left" };
    		desc[descriptor_index++] = { i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "D-Pad Up" };
    		desc[descriptor_index++] = { i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "D-Pad Down" };
    		desc[descriptor_index++] = { i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" };
    		desc[descriptor_index++] = { i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "A" };
    		desc[descriptor_index++] = { i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "B" };
    		desc[descriptor_index++] = { i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,     "Y" };
    		desc[descriptor_index++] = { i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,     "X" };
    		desc[descriptor_index++] = { i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2,    "L Trigger" };
    		desc[descriptor_index++] = { i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2,    "R Trigger" };
    		desc[descriptor_index++] = { i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START,    "Start" };
    		desc[descriptor_index++] = { i, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, "Analog X" };
    		desc[descriptor_index++] = { i, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, "Analog Y" };
    		break;
 
 		case MDT_TwinStick:
    		desc[descriptor_index++] = { i, RETRO_DEVICE_TWINSTICK, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "L-Stick Left" };
    		desc[descriptor_index++] = { i, RETRO_DEVICE_TWINSTICK, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "L-Stick Up" };
    		desc[descriptor_index++] = { i, RETRO_DEVICE_TWINSTICK, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "L-Stick Down" };
    		desc[descriptor_index++] = { i, RETRO_DEVICE_TWINSTICK, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "L-Stick Right" };
    		desc[descriptor_index++] = { i, RETRO_DEVICE_TWINSTICK, 0, RETRO_DEVICE_ID_JOYPAD_B,     "R-Stick Down" };
    		desc[descriptor_index++] = { i, RETRO_DEVICE_TWINSTICK, 0, RETRO_DEVICE_ID_JOYPAD_A,     "R-Stick Right" };
    		desc[descriptor_index++] = { i, RETRO_DEVICE_TWINSTICK, 0, RETRO_DEVICE_ID_JOYPAD_X,     "R-Stick Up" };
    		desc[descriptor_index++] = { i, RETRO_DEVICE_TWINSTICK, 0, RETRO_DEVICE_ID_JOYPAD_Y,     "R-Stick Left" };
    		desc[descriptor_index++] = { i, RETRO_DEVICE_TWINSTICK, 0, RETRO_DEVICE_ID_JOYPAD_L,    "L Turbo" };
    		desc[descriptor_index++] = { i, RETRO_DEVICE_TWINSTICK, 0, RETRO_DEVICE_ID_JOYPAD_R,    "R Turbo" };
    		desc[descriptor_index++] = { i, RETRO_DEVICE_TWINSTICK, 0, RETRO_DEVICE_ID_JOYPAD_L2,    "L Trigger" };
    		desc[descriptor_index++] = { i, RETRO_DEVICE_TWINSTICK, 0, RETRO_DEVICE_ID_JOYPAD_R2,    "R Trigger" };
    		desc[descriptor_index++] = { i, RETRO_DEVICE_TWINSTICK, 0, RETRO_DEVICE_ID_JOYPAD_START,    "Start" };
    		desc[descriptor_index++] = { i, RETRO_DEVICE_TWINSTICK, 0, RETRO_DEVICE_ID_JOYPAD_SELECT,    "Special" };
    		break;
 
 		case MDT_AsciiStick:
    		desc[descriptor_index++] = { i, RETRO_DEVICE_TWINSTICK, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "Stick Left" };
    		desc[descriptor_index++] = { i, RETRO_DEVICE_TWINSTICK, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "Stick Up" };
    		desc[descriptor_index++] = { i, RETRO_DEVICE_TWINSTICK, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "Stick Down" };
    		desc[descriptor_index++] = { i, RETRO_DEVICE_TWINSTICK, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Stick Right" };
    		desc[descriptor_index++] = { i, RETRO_DEVICE_TWINSTICK, 0, RETRO_DEVICE_ID_JOYPAD_B,     "A" };
    		desc[descriptor_index++] = { i, RETRO_DEVICE_TWINSTICK, 0, RETRO_DEVICE_ID_JOYPAD_A,     "B" };
    		desc[descriptor_index++] = { i, RETRO_DEVICE_TWINSTICK, 0, RETRO_DEVICE_ID_JOYPAD_X,     "Y" };
    		desc[descriptor_index++] = { i, RETRO_DEVICE_TWINSTICK, 0, RETRO_DEVICE_ID_JOYPAD_Y,     "X" };
    		desc[descriptor_index++] = { i, RETRO_DEVICE_TWINSTICK, 0, RETRO_DEVICE_ID_JOYPAD_L,    "C" };
    		desc[descriptor_index++] = { i, RETRO_DEVICE_TWINSTICK, 0, RETRO_DEVICE_ID_JOYPAD_R,    "Z" };
    		desc[descriptor_index++] = { i, RETRO_DEVICE_TWINSTICK, 0, RETRO_DEVICE_ID_JOYPAD_START,    "Start" };
    		break;

    	 case MDT_LightGun:
    		desc[descriptor_index++] = { i, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_DPAD_LEFT,  "D-Pad Left" };
    		desc[descriptor_index++] = { i, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_DPAD_UP,    "D-Pad Up" };
    		desc[descriptor_index++] = { i, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_DPAD_DOWN,  "D-Pad Down" };
    		desc[descriptor_index++] = { i, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_DPAD_RIGHT, "D-Pad Right" };
    		desc[descriptor_index++] = { i, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_TRIGGER,	   "A" };
    		desc[descriptor_index++] = { i, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_START,      "Start" };
    		desc[descriptor_index++] = { i, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_AUX_A,      "B" };
    		break;
    	 }
      }
   }
   desc[descriptor_index++] = { 0 };

   environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);
}

static void extract_basename(char *buf, const char *path, size_t size)
{
   const char *base = strrchr(path, slash);
   if (!base)
      base = path;

   if (*base == slash)
      base++;

   strncpy(buf, base, size - 1);
   buf[size - 1] = '\0';
}

static void remove_extension(char *buf, const char *path, size_t size)
{
   char *base;
   strncpy(buf, path, size - 1);
   buf[size - 1] = '\0';

   base = strrchr(buf, '.');

   if (base)
      *base = '\0';
}

// Loading/unloading games
bool retro_load_game(const struct retro_game_info *game)
{
   glsm_ctx_params_t params = {0};
   const char *dir = NULL;
   const char *vmu_dir = NULL;
#ifdef _WIN32
   char slash = '\\';
#else
   char slash = '/';
#endif

   extract_basename(g_base_name, game->path, sizeof(g_base_name));
   extract_directory(game_dir, game->path, sizeof(game_dir));

   // Storing rom dir for later use
   snprintf(g_roms_dir, sizeof(g_roms_dir), "%s%c", game_dir, slash);

   if (environ_cb(RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE, &rumble) && log_cb)
        log_cb(RETRO_LOG_INFO, "Rumble interface supported!\n");

   if (!(environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &dir) && dir))
      dir = game_dir;

   snprintf(game_dir, sizeof(game_dir), "%s%cdc%c", dir, slash, slash);
   snprintf(game_dir_no_slash, sizeof(game_dir_no_slash), "%s%cdc", dir, slash);

   // Per-content VMU additions START
   // > Get save directory
   if (!(environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &vmu_dir) && vmu_dir))
      vmu_dir = game_dir;

   snprintf(vmu_dir_no_slash, sizeof(vmu_dir_no_slash), "%s", vmu_dir);

   // > Get content name
   remove_extension(content_name, g_base_name, sizeof(content_name));

   if (content_name[0] == '\0')
      snprintf(content_name, sizeof(content_name), "vmu_save");
   // Per-content VMU additions END

   settings.dreamcast.cable = 3;
   update_variables(true);

   char *ext = strrchr(g_base_name, '.');

   {
      /* Check for extension .lst, .bin, .dat or .zip. If found, we will set the system type
       * automatically to Naomi or AtomisWave. */
      if (ext)
      {
         log_cb(RETRO_LOG_INFO, "File extension is: %s\n", ext);
         if (!strcmp(".lst", ext)
        	   || !strcmp(".bin", ext) || !strcmp(".BIN", ext)
        	   || !strcmp(".dat", ext) || !strcmp(".DAT", ext)
        	   || !strcmp(".zip", ext) || !strcmp(".ZIP", ext)
        	   || !strcmp(".7z", ext) || !strcmp(".7Z", ext))
         {
            settings.System = naomi_cart_GetSystemType(game->path);
         }
         // If m3u playlist found load the paths into array
         else if (!strcmp(".m3u", ext) || !strcmp(".M3U", ext))
         {
            if (!read_m3u(game->path))
            {
               if (log_cb)
                  log_cb(RETRO_LOG_ERROR, "%s\n", "[libretro]: failed to read m3u file ...\n");
               return false;
            }
         }
      }
   }

   if (game->path[0] == '\0')
   {
	  if (settings.System == DC_PLATFORM_DREAMCAST)
		 boot_to_bios = true;
	  else
		 return false;
   }

   if (!boot_to_bios)
   {
      // if an m3u file was loaded, disk_paths will already be populated so load the game from there
      if (disk_paths.size() > 0)
      {
         disk_index = 0;
         game_data = strdup(disk_paths[disk_index].c_str());
      }
      else
      {
         disk_paths.push_back(game->path);
         game_data = strdup(game->path);
      } 
   }

   {
      char data_dir[1024];

      snprintf(data_dir, sizeof(data_dir), "%s%s", game_dir, "data");

      printf("Creating dir: %s\n", data_dir);
      struct stat buf;
      if (stat(data_dir, &buf) < 0)
      {
         mkdir_norecurse(data_dir);
      }
   }

   params.context_type          = RETRO_HW_CONTEXT_NONE;

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
   params.context_reset         = context_reset;
   params.context_destroy       = context_destroy;
   params.environ_cb            = environ_cb;
#ifdef TARGET_NO_STENCIL
   params.stencil               = false;
#else
   params.stencil               = true;
#endif
   params.imm_vbo_draw          = NULL;
   params.imm_vbo_disable       = NULL;
#ifdef HAVE_OIT
   if (settings.pvr.rend == 3)
   {
	  params.context_type          = RETRO_HW_CONTEXT_OPENGL_CORE;
	  params.major                 = 4;
	  params.minor                 = 3;
   }
   else
   {
	   params.context_type          = RETRO_HW_CONTEXT_OPENGL_CORE;
	   params.major                 = 3;
	   params.minor                 = 0;
   }
#elif defined(HAVE_GL3)
   params.context_type          = RETRO_HW_CONTEXT_OPENGL_CORE;
   params.major                 = 3;
   params.minor                 = 0;
#endif

   if (!glsm_ctl(GLSM_CTL_STATE_CONTEXT_INIT, &params))
   {
#if defined(HAVE_GL3)
      params.context_type       = RETRO_HW_CONTEXT_OPENGL_CORE;
      params.major              = 3;
      params.minor              = 0;
#else
      params.context_type       = RETRO_HW_CONTEXT_OPENGL;
      params.major              = 0;
      params.minor              = 0;
#endif
      if (!glsm_ctl(GLSM_CTL_STATE_CONTEXT_INIT, &params))
         return false;
   }
#endif

   if (settings.System != DC_PLATFORM_DREAMCAST)
   {
      if (environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &dir) && dir) {
         char g_save_dir[PATH_MAX];
         strncpy(g_save_dir, dir, sizeof(g_save_dir));
         if(strcmp(g_save_dir,g_roms_dir) != 0)
            snprintf(save_dir, sizeof(save_dir), "%s%creicast%c", g_save_dir, slash, slash);
         else
            strncpy(save_dir, g_roms_dir, sizeof(save_dir));
         struct stat buf;
         if (stat(save_dir, &buf) < 0)
         {
            log_cb(RETRO_LOG_INFO, "Creating dir: %s\n", save_dir);
            mkdir_norecurse(save_dir);
         }
      } else {
         strncpy(save_dir, g_roms_dir, sizeof(save_dir));
      }
      log_cb(RETRO_LOG_INFO, "Setting save dir to %s\n", save_dir);
      snprintf(eeprom_file, sizeof(eeprom_file), "%s%s.eeprom", save_dir, g_base_name);
      snprintf(nvmem_file, sizeof(nvmem_file), "%s%s.nvmem", save_dir, g_base_name);
      snprintf(nvmem_file2, sizeof(nvmem_file2), "%s%s.nvmem2", save_dir, g_base_name);
   }

   dc_prepare_system();

   if (dc_init(co_argc,co_argv))
   {
	  if (log_cb)
		 log_cb(RETRO_LOG_ERROR, "Flycast emulator initialization failed\n");
	  return false;
   }
   int rotation = rotate_screen ? 3 : 0;
   if (naomi_cart_GetRotation() == 3)
      rotation = rotate_screen ? 0 : 1;
   environ_cb(RETRO_ENVIRONMENT_SET_ROTATION, &rotation);
   init_disk_control_interface(game->path);
   refresh_devices(true);

   return true;
}

bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info)
{
   return false; //TODO (?)
}

void retro_unload_game(void)
{
	printf("reicast unloading game\n") ;
   if (game_data)
      free(game_data);
   game_data = NULL;

   dc_stop();
#if !defined(TARGET_NO_THREADS)
   if (settings.rend.ThreadedRendering)
   {
	   rend_cancel_emu_wait();
	   printf("Waiting for emu thread......\n");
	   if ( emu_in_thread )
	   {
		   frontend_clear_thread_waits_cb(1,NULL) ;
		   printf("Waiting for emu thread to end...\n");
		   emu_thread.WaitToEnd();
		   frontend_clear_thread_waits_cb(0,NULL) ;
	   }
	   printf("...Done\n");
   }
   else
#endif
	   dc_term();
}


// Memory/Serialization
void *retro_get_memory_data(unsigned type)
{
   if ( type == RETRO_MEMORY_SYSTEM_RAM )
      return mem_b.data;
   return 0; //TODO
}

size_t retro_get_memory_size(unsigned type)
{
   if ( type == RETRO_MEMORY_SYSTEM_RAM )
      return mem_b.size;
   return 0; //TODO
}

size_t retro_serialize_size (void)
{
   unsigned int total_size = 0 ;
   void *data = NULL ;

   dc_serialize(&data, &total_size) ;

   return total_size;
}

bool wait_until_dc_running()
{
	retro_time_t start_time = perf_cb.get_time_usec() ;
	const retro_time_t FIVE_SECONDS = 5*1000000 ;
	while(!dc_is_running())
	{
		if ( start_time+FIVE_SECONDS < perf_cb.get_time_usec() )
		{
			//timeout elapsed - dc not getting a chance to run - just bail
			return false ;
		}
	}
	return true ;
}

bool acquire_mainloop_lock()
{
	bool result = false ;
	retro_time_t start_time = perf_cb.get_time_usec() ;
	const retro_time_t FIVE_SECONDS = 5*1000000 ;

    while ( ( start_time+FIVE_SECONDS > perf_cb.get_time_usec() ) && !(result = mtx_mainloop.TryLock())  )
   	{
    	rend_cancel_emu_wait();
   	}

    return result ;
}

bool retro_serialize(void *data, size_t size)
{
   unsigned int total_size = 0 ;
   void *data_ptr = data ;
   bool result = false ;

#if !defined(TARGET_NO_THREADS)
	mtx_serialization.Lock() ;
    if (settings.rend.ThreadedRendering)
    {
    	if ( !wait_until_dc_running()) {
        	mtx_serialization.Unlock() ;
        	return false ;
    	}

  		dc_stop() ;
  		if ( !acquire_mainloop_lock() )
  		{
  			dc_start() ;
        	mtx_serialization.Unlock() ;
  			return false ;
  		}
    }
#endif

   result = dc_serialize(&data_ptr, &total_size) ;
   performed_serialization = true ;

#if !defined(TARGET_NO_THREADS)
    if (settings.rend.ThreadedRendering)
    {
    	mtx_mainloop.Unlock() ;
    }
	mtx_serialization.Unlock() ;
#endif

    return result ;
}

bool retro_unserialize(const void * data, size_t size)
{
   unsigned int total_size = 0 ;
   void *data_ptr = (void*)data ;
   bool result = false ;
   int i ;

#if !defined(TARGET_NO_THREADS)
    if (settings.rend.ThreadedRendering)
    {
    	mtx_serialization.Lock() ;
    	if ( !wait_until_dc_running()) {
        	mtx_serialization.Unlock() ;
        	return false ;
    	}
  		dc_stop() ;
  		if ( !acquire_mainloop_lock() )
  		{
  			dc_start() ;
        	mtx_serialization.Unlock() ;
  			return false ;
  		}
    }
#endif

    sh4_cpu.ResetCache();
#if FEAT_AREC == DYNAREC_JIT
    FlushCache();
#endif

    result = dc_unserialize(&data_ptr, &total_size, size) ;

    dsp.dyndirty = true;
    sh4_sched_ffts();
    CalculateSync();

    for ( i = 0 ; i < 4 ; i++)
    {
       vmu_screen_params[i].vmu_screen_needs_update = true ;
       lightgun_params[i].dirty = true ;
    }

    performed_serialization = true ;

#if !defined(TARGET_NO_THREADS)
    if (settings.rend.ThreadedRendering)
    {
    	mtx_mainloop.Unlock() ;
    	mtx_serialization.Unlock() ;
    }
#endif

    return result ;
}

// Cheats
void retro_cheat_reset(void)
{
   // Nothing to do here
}
void retro_cheat_set(unsigned unused, bool unused1, const char* unused2)
{
   // Nothing to do here
}


// Get info
const char* retro_get_system_directory(void)
{
   const char* dir;
   environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &dir);
   return dir ? dir : ".";
}

void retro_get_system_info(struct retro_system_info *info)
{
   info->library_name = "Flycast";
#ifndef GIT_VERSION
#define GIT_VERSION ""
#endif
   info->library_version = "0.1" GIT_VERSION;
   info->valid_extensions = "chd|cdi|iso|elf|cue|gdi|lst|bin|dat|zip|7z|m3u";
   info->need_fullpath = true;
   info->block_extract = true;
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   /*                        00=VGA    01=NTSC   10=PAL,   11=illegal/undocumented */
   const int spg_clks[4] = { 26944080, 13458568, 13462800, 26944080 };
   u32 pixel_clock= spg_clks[(SPG_CONTROL.full >> 6) & 3];

   info->geometry.aspect_ratio = settings.rend.WideScreen ? (16.0 / 9.0) : (4.0 / 3.0);
   if(naomi_cart_GetRotation() == 3)
      info->geometry.aspect_ratio = 1 / info->geometry.aspect_ratio;
   int maximum = screen_width > screen_height ? screen_width : screen_height;
   info->geometry.base_width   = screen_width;
   info->geometry.base_height  = screen_height;
   info->geometry.max_width    = maximum;
   info->geometry.max_height   = maximum;
   if (rotate_screen)
      info->geometry.aspect_ratio = 1 / info->geometry.aspect_ratio;

   switch (pixel_clock)
   {
      case 26944080:
         info->timing.fps = 60.00; /* (VGA  480 @ 60.00) */
         break;
      case 26917135:
         info->timing.fps = 59.94; /* (NTSC 480 @ 59.94) */
         break;
      case 13462800:
         info->timing.fps = 50.00; /* (PAL 240  @ 50.00) */
         break;
      case 13458568:
         info->timing.fps = 59.94; /* (NTSC 240 @ 59.94) */
         break;
      case 25925600:
         info->timing.fps = 50.00; /* (PAL 480  @ 50.00) */
         break;
   }

   info->timing.sample_rate = 44100.0;
}

unsigned retro_get_region (void)
{
   return RETRO_REGION_NTSC; //TODO
}

// Controller
void retro_set_controller_port_device(unsigned in_port, unsigned device)
{
	if (device_type[in_port] != device && in_port < MAPLE_PORTS)
	{
		devices_need_refresh = true;
		device_type[in_port] = device;
		switch (device)
		{
			case RETRO_DEVICE_JOYPAD:
				maple_devices[in_port] = MDT_SegaController;
				break;
			case RETRO_DEVICE_TWINSTICK:
			case RETRO_DEVICE_TWINSTICK_SATURN:
				maple_devices[in_port] = MDT_TwinStick;
				break;
			case RETRO_DEVICE_ASCIISTICK:
				maple_devices[in_port] = MDT_AsciiStick;
				break;
			case RETRO_DEVICE_KEYBOARD:
				maple_devices[in_port] = MDT_Keyboard;
				break;
			case RETRO_DEVICE_MOUSE:
				maple_devices[in_port] = MDT_Mouse;
				break;
			case RETRO_DEVICE_LIGHTGUN:
				maple_devices[in_port] = MDT_LightGun;
				break;
			default:
				maple_devices[in_port] = MDT_None;
				break;
		}
	}
}

static void refresh_devices(bool descriptors_only)
{
	if (devices_need_refresh)
	{
		devices_need_refresh = false;
		set_input_descriptors();

		if (!descriptors_only)
		{
			if (settings.System == DC_PLATFORM_DREAMCAST)
			{
				maple_ReconnectDevices();
			}

			if (rumble.set_rumble_state)
			{
				for(int i = 0; i < MAPLE_PORTS; i++)
				{
					rumble.set_rumble_state(i, RETRO_RUMBLE_STRONG, 0);
					rumble.set_rumble_state(i, RETRO_RUMBLE_WEAK,   0);
				}
			}
		}
		if (settings.System != DC_PLATFORM_DREAMCAST)
		{
			mcfg_DestroyDevices();
			mcfg_CreateDevices();
		}
	}
}

// API version (to detect version mismatch)
unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}

//Reicast stuff
void os_DoEvents(void)
{
#if !defined(TARGET_NO_THREADS)
	if (!settings.rend.ThreadedRendering)
#endif
	{
		is_dupe = false;
		poll_cb();

		if (settings.UpdateMode || settings.UpdateModeForced)
		{
			rend_end_render();
			dc_stop();
		}
	}
}

void os_CreateWindow()
{
   // Nothing to do here
}

static uint32_t get_time_ms()
{
   return (uint32_t)(os_GetSeconds() * 1000.0);
}

double os_GetSeconds()
{
#ifdef _WIN32
#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
  #define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else
  #define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif

   FILETIME ft;
   unsigned __int64 tmpres = 0;

   GetSystemTimeAsFileTime(&ft);

   tmpres |= ft.dwHighDateTime;
   tmpres <<= 32;
   tmpres |= ft.dwLowDateTime;

   tmpres /= 10;  /*convert into microseconds*/
   /*converting file time to unix epoch*/
   tmpres -= DELTA_EPOCH_IN_MICROSECS;

   return (double)tmpres / 1000000.0;	// microsecond -> second
#else
   struct timeval t;
   gettimeofday(&t, NULL);

   return (double)t.tv_sec + (double)t.tv_usec / 1000000.0;
#endif
}

static void get_analog_stick( retro_input_state_t input_state_cb,
                       int player_index,
                       int stick,
                       s8* p_analog_x,
                       s8* p_analog_y )
{
   int analog_x, analog_y;
   analog_x = input_state_cb( player_index, RETRO_DEVICE_ANALOG, stick, RETRO_DEVICE_ID_ANALOG_X );
   analog_y = input_state_cb( player_index, RETRO_DEVICE_ANALOG, stick, RETRO_DEVICE_ID_ANALOG_Y );

   // Analog stick deadzone (borrowed code from parallel-n64 core)
   if ( astick_deadzone > 0 )
   {
      static const int ASTICK_MAX = 0x8000;

      // Convert cartesian coordinate analog stick to polar coordinates
      double radius = sqrt(analog_x * analog_x + analog_y * analog_y);
      double angle = atan2(analog_y, analog_x);

      if (radius > astick_deadzone)
      {
         // Re-scale analog stick range to negate deadzone (makes slow movements possible)
         radius = (radius - astick_deadzone)*((float)ASTICK_MAX/(ASTICK_MAX - astick_deadzone));

         // Convert back to cartesian coordinates
         analog_x = (int)round(radius * cos(angle));
         analog_y = (int)round(radius * sin(angle));

         // Clamp to correct range
         if (analog_x > +32767) analog_x = +32767;
         if (analog_x < -32767) analog_x = -32767;
         if (analog_y > +32767) analog_y = +32767;
         if (analog_y < -32767) analog_y = -32767;
      }
      else
      {
         analog_x = 0;
         analog_y = 0;
      }
   }

   // output
   *p_analog_x = (s8)(analog_x >> 8);
   *p_analog_y = (s8)(analog_y >> 8);
}

static uint16_t apply_trigger_deadzone( uint16_t input )
{
   if ( trigger_deadzone > 0 )
   {
      if ( input > trigger_deadzone )
      {
         // Re-scale analog range
         static const int TRIGGER_MAX = 0x8000;
         const float scale = ((float)TRIGGER_MAX/(float)(TRIGGER_MAX - trigger_deadzone));
         float scaled      = (input - trigger_deadzone)*scale;

         input = (int)round(scaled);
         if (input > +32767)
            input = +32767;
      }
      else
         input = 0;
   }

   return input;
}

static uint16_t get_analog_trigger(
      int16_t ret,
      retro_input_state_t input_state_cb,
      int player_index,
      int id )
{
   // NOTE: Analog triggers were added Nov 2017. Not all front-ends support this
   // feature (or pre-date it) so we need to handle this in a graceful way.

   // First, try and get an analog value using the new libretro API constant
   uint16_t trigger = input_state_cb( player_index,
                       RETRO_DEVICE_ANALOG,
                       RETRO_DEVICE_INDEX_ANALOG_BUTTON,
                       id );

   if ( trigger == 0 )
   {
      // If we got exactly zero, we're either not pressing the button, or the front-end
      // is not reporting analog values. We need to do a second check using the classic
      // digital API method, to at least get some response - better than nothing.

      // NOTE: If we're really just not holding the trigger, we're still going to get zero.

      trigger = (ret & (1 << id)) ? 0x7FFF : 0;
   }
   else
   {
      // We got something, which means the front-end can handle analog buttons.
      // So we apply a deadzone to the input and use it.

      trigger = apply_trigger_deadzone( trigger );
   }

   return trigger;
}

#define setDeviceButtonState(port, deviceType, btnId) \
{ \
   uint32_t dc_key = map_gamepad_button(deviceType, btnId); \
   bool is_down = input_cb(port, deviceType, 0, btnId); \
   if (is_down) \
	  kcode[port] &= ~dc_key; \
   else \
	  kcode[port] |= dc_key; \
}

#define setDeviceButtonStateMacro(ret, port, deviceType, btnId) \
{ \
   uint32_t dc_key = map_gamepad_button(deviceType, btnId); \
   bool is_down    = ret & (1 << btnId); \
   if (is_down) \
	  kcode[port] &= ~dc_key; \
   else \
	  kcode[port] |= dc_key; \
}

// don't call map_gamepad_button, we supply the DC bit directly.
#define setDeviceButtonStateDirectMacro(ret, port, deviceType, btnId, dc_bit) \
{ \
   uint32_t dc_key = 1 << dc_bit; \
   bool is_down    = ret & (1 << btnId); \
   if (is_down) \
	  kcode[port] &= ~dc_key; \
   else \
	  kcode[port] |= dc_key; \
}

static void updateMouseState(u32 port)
{
   mo_x_delta[port] = input_cb(port, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
   mo_y_delta[port] = input_cb(port, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);

   bool btn_state   = input_cb(port, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT);
   if (btn_state)
	  mo_buttons[port] &= ~(1 << 2);
   else
	  mo_buttons[port] |= 1 << 2;
   btn_state = input_cb(port, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT);
   if (btn_state)
	  mo_buttons[port] &= ~(1 << 1);
   else
	  mo_buttons[port] |= 1 << 1;
   btn_state = input_cb(port, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_MIDDLE);
   if (btn_state)
	  mo_buttons[port] &= ~(1 << 0);
   else
	  mo_buttons[port] |= 1 << 0;
   if (input_cb(port, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELDOWN))
	  mo_wheel_delta[port] -= 10;
   else if (input_cb(port, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELUP))
	  mo_wheel_delta[port] += 10;
}

static void UpdateInputStateNaomi(u32 port)
{
   int id;
   int max_id;

   switch (maple_devices[port])
   {
   case MDT_LightGun:
	  {
		 //
		 // -- buttons
		 setDeviceButtonState(port, RETRO_DEVICE_LIGHTGUN, RETRO_DEVICE_ID_LIGHTGUN_TRIGGER);
		 setDeviceButtonState(port, RETRO_DEVICE_LIGHTGUN, RETRO_DEVICE_ID_LIGHTGUN_AUX_A);
		 setDeviceButtonState(port, RETRO_DEVICE_LIGHTGUN, RETRO_DEVICE_ID_LIGHTGUN_AUX_B);
		 setDeviceButtonState(port, RETRO_DEVICE_LIGHTGUN, RETRO_DEVICE_ID_LIGHTGUN_AUX_C);
		 setDeviceButtonState(port, RETRO_DEVICE_LIGHTGUN, RETRO_DEVICE_ID_LIGHTGUN_START);
		 setDeviceButtonState(port, RETRO_DEVICE_LIGHTGUN, RETRO_DEVICE_ID_LIGHTGUN_SELECT);
		 setDeviceButtonState(port, RETRO_DEVICE_LIGHTGUN, RETRO_DEVICE_ID_LIGHTGUN_DPAD_UP);
		 setDeviceButtonState(port, RETRO_DEVICE_LIGHTGUN, RETRO_DEVICE_ID_LIGHTGUN_DPAD_DOWN);
		 setDeviceButtonState(port, RETRO_DEVICE_LIGHTGUN, RETRO_DEVICE_ID_LIGHTGUN_DPAD_LEFT);
		 setDeviceButtonState(port, RETRO_DEVICE_LIGHTGUN, RETRO_DEVICE_ID_LIGHTGUN_DPAD_RIGHT);

		 bool force_offscreen = false;

		 if (input_cb(port, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_RELOAD))
		 {
			force_offscreen = true;
			if (settings.System == DC_PLATFORM_NAOMI)
			   kcode[port] &= ~NAOMI_BTN0_KEY;
			else
			   kcode[port] &= ~AWAVE_TRIGGER_KEY;
		 }

		 if (force_offscreen || input_cb(port, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN))
		 {
			mo_x_abs[port] = -1;
			mo_y_abs[port] = -1;
		 }
		 else
		 {
			int x = input_cb(port, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X);
			int y = input_cb(port, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y);
			mo_x_abs[port] = (x + 0x8000) * 640.f / 0x10000;
			mo_y_abs[port] = (y + 0x8000) * 480.f / 0x10000;
		 }
	  }
	  break;

   default:
     int16_t ret = 0;
	  //
	  // -- buttons
     {
        if (libretro_supports_bitmasks)
           ret = input_cb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);
        else
        {
           for (id = RETRO_DEVICE_ID_JOYPAD_B; id <= RETRO_DEVICE_ID_JOYPAD_R3; ++id)
              if (input_cb(port, RETRO_DEVICE_JOYPAD, 0, id))
                 ret |= (1 << id);
        }

        for (id = RETRO_DEVICE_ID_JOYPAD_B; id <= RETRO_DEVICE_ID_JOYPAD_R3; ++id)
        {
           switch (id)
           {
              case RETRO_DEVICE_ID_JOYPAD_L3:
                 if (allow_service_buttons)
                    setDeviceButtonStateMacro(ret, port, RETRO_DEVICE_JOYPAD, id);
                 break;
              case RETRO_DEVICE_ID_JOYPAD_R3:
                 if (settings.System == DC_PLATFORM_NAOMI || allow_service_buttons)
                    setDeviceButtonStateMacro(ret, port, RETRO_DEVICE_JOYPAD, id);
                 break;
              default:
                 setDeviceButtonStateMacro(ret, port, RETRO_DEVICE_JOYPAD, id);
                 break;
           }
        }
     }
	  //
	  // -- analog stick

	  get_analog_stick( input_cb, port, RETRO_DEVICE_INDEX_ANALOG_LEFT, &(joyx[port]), &(joyy[port]) );
	  get_analog_stick( input_cb, port, RETRO_DEVICE_INDEX_ANALOG_RIGHT, &(joyrx[port]), &(joyry[port]));
	  lt[port] = get_analog_trigger(ret, input_cb, port, RETRO_DEVICE_ID_JOYPAD_L2) / 128;
	  rt[port] = get_analog_trigger(ret, input_cb, port, RETRO_DEVICE_ID_JOYPAD_R2) / 128;

	  if (naomi_game_inputs != NULL)
	  {
		 for (int i = 0; i < 4; i++)
		 {
			if (naomi_game_inputs->axes[i].name == NULL)
			   break;
			AxisType axis_type = naomi_game_inputs->axes[i].type;
			switch (i)
			{
			case 0:
			   if (axis_type == Half)
				  joyx[port] = max((int)joyx[port], 0) * 2;
			   else
				  joyx[port] += 128;
			   break;
			case 1:
			   if (axis_type == Half)
				  joyy[port] = max((int)joyy[port], 0) * 2;
			   else
				  joyy[port] += 128;
			   break;
			case 2:
			   if (axis_type == Half)
				  joyrx[port] = max((int)joyrx[port], 0) * 2;
			   else
				  joyrx[port] += 128;
			   break;
			case 3:
			   if (axis_type == Half)
				  joyry[port] = max((int)joyry[port], 0) * 2;
			   else
				  joyry[port] += 128;
			   break;
			case 4:
			   if (axis_type == Half)
				  rt[port] = max((int)rt[port], 0) * 2;
			   else
				  rt[port] += 128;
			   break;
			case 5:
			   if (axis_type == Half)
				  lt[port] = max((int)lt[port], 0) * 2;
			   else
				  lt[port] += 128;
			   break;
			}
		 }
	  }
	  else
	  {
		 // Make all axes full by default
		 joyx[port] += 128;
		 joyy[port] += 128;
		 joyrx[port] += 128;
		 joyry[port] += 128;
		 rt[port] += 128;
		 lt[port] += 128;
	  }

	  // -- mouse, for rotary encoders
	  updateMouseState(port);
	  break;
   }

   // Avoid Left+Right or Up+Down buttons being pressed together as this crashes some games
	if (settings.System == DC_PLATFORM_NAOMI)
	{
	   if ((kcode[port] & (NAOMI_UP_KEY|NAOMI_DOWN_KEY)) == 0)
		  kcode[port] |= NAOMI_UP_KEY|NAOMI_DOWN_KEY;
	   if ((kcode[port] & (NAOMI_LEFT_KEY|NAOMI_RIGHT_KEY)) == 0)
		  kcode[port] |= NAOMI_LEFT_KEY|NAOMI_RIGHT_KEY;
	}
	else
	{
	   if ((kcode[port] & (AWAVE_UP_KEY|AWAVE_DOWN_KEY)) == 0)
		  kcode[port] |= AWAVE_UP_KEY|AWAVE_DOWN_KEY;
	   if ((kcode[port] & (AWAVE_LEFT_KEY|AWAVE_RIGHT_KEY)) == 0)
		  kcode[port] |= AWAVE_LEFT_KEY|AWAVE_RIGHT_KEY;
	}
}

void UpdateInputState(u32 port)
{
   if (gl_ctx_resetting)
	  return;

   if (settings.System == DC_PLATFORM_NAOMI || settings.System == DC_PLATFORM_ATOMISWAVE)
   {
      UpdateInputStateNaomi(0);
      UpdateInputStateNaomi(1);
      UpdateInputStateNaomi(2);
      UpdateInputStateNaomi(3);
      return;
   }
   if (rumble.set_rumble_state != NULL && vib_stop_time[port] > 0)
   {
	  if (get_time_ms() >= vib_stop_time[port])
	  {
		 vib_stop_time[port] = 0;
		 rumble.set_rumble_state(port, RETRO_RUMBLE_STRONG, 0);
	  }
	  else if (vib_delta[port] > 0.0)
	  {
		 u32 rem_time = vib_stop_time[port] - get_time_ms();
		 rumble.set_rumble_state(port, RETRO_RUMBLE_STRONG, 65535 * vib_strength[port] * rem_time * vib_delta[port]);
	  }
   }
   
   lightgun_params[port].offscreen = true;	   

   switch (maple_devices[port])
   {
	  case MDT_SegaController:
	  {
		   int id;
         int16_t ret = 0;
		   //
		   // -- buttons
         
         if (libretro_supports_bitmasks)
            ret = input_cb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);
         else
         {
            for (id = RETRO_DEVICE_ID_JOYPAD_B; id <= RETRO_DEVICE_ID_JOYPAD_R3; ++id)
               if (input_cb(port, RETRO_DEVICE_JOYPAD, 0, id))
                  ret |= (1 << id);
         }

		   for (id = RETRO_DEVICE_ID_JOYPAD_B; id <= RETRO_DEVICE_ID_JOYPAD_X; ++id)
			  setDeviceButtonStateMacro(ret, port, RETRO_DEVICE_JOYPAD, id);

		   //
		   // -- analog stick

		   get_analog_stick( input_cb, port, RETRO_DEVICE_INDEX_ANALOG_LEFT, &(joyx[port]), &(joyy[port]) );


		   //
		   // -- triggers

		   if ( digital_triggers )
		   {
		      // -- digital left trigger
            if (ret & (1 << RETRO_DEVICE_ID_JOYPAD_L))
		         lt[port]=0xFF;
            else if (ret & (1 << RETRO_DEVICE_ID_JOYPAD_L2))
		         lt[port]=0x7F;
		      else
		         lt[port]=0;
		      // -- digital right trigger
            if (ret & (1 << RETRO_DEVICE_ID_JOYPAD_R))
		         rt[port]=0xFF;
            else if (ret & (1 << RETRO_DEVICE_ID_JOYPAD_R2))
		         rt[port]=0x7F;
		      else
		         rt[port]=0;
		   }
		   else
		   {
			   // -- analog triggers
			   lt[port] = get_analog_trigger(ret, input_cb, port, RETRO_DEVICE_ID_JOYPAD_L2 ) / 128;
			   rt[port] = get_analog_trigger(ret, input_cb, port, RETRO_DEVICE_ID_JOYPAD_R2 ) / 128;
		   }
	  }
	  break;
	  
	case MDT_AsciiStick:
	{
      int16_t ret = 0;

      if (libretro_supports_bitmasks)
         ret = input_cb(port, RETRO_DEVICE_ASCIISTICK, 0, RETRO_DEVICE_ID_JOYPAD_MASK);
      else
      {
         unsigned id;
         for (id = RETRO_DEVICE_ID_JOYPAD_B; id <= RETRO_DEVICE_ID_JOYPAD_R3; ++id)
            if (input_cb(port, RETRO_DEVICE_ASCIISTICK, 0, id))
               ret |= (1 << id);
      }

		kcode[port] = 0xFFFF; // active-low

		// stick
		setDeviceButtonStateDirectMacro(ret, port, RETRO_DEVICE_ASCIISTICK, RETRO_DEVICE_ID_JOYPAD_UP, 4 );
		setDeviceButtonStateDirectMacro(ret, port, RETRO_DEVICE_ASCIISTICK, RETRO_DEVICE_ID_JOYPAD_DOWN, 5 );
		setDeviceButtonStateDirectMacro(ret, port, RETRO_DEVICE_ASCIISTICK, RETRO_DEVICE_ID_JOYPAD_LEFT, 6 );
		setDeviceButtonStateDirectMacro(ret, port, RETRO_DEVICE_ASCIISTICK, RETRO_DEVICE_ID_JOYPAD_RIGHT, 7 );
		
		// buttons
		setDeviceButtonStateDirectMacro(ret, port, RETRO_DEVICE_ASCIISTICK, RETRO_DEVICE_ID_JOYPAD_B,  2 ); // A
		setDeviceButtonStateDirectMacro(ret, port, RETRO_DEVICE_ASCIISTICK, RETRO_DEVICE_ID_JOYPAD_A,  1 ); // B
		setDeviceButtonStateDirectMacro(ret, port, RETRO_DEVICE_ASCIISTICK, RETRO_DEVICE_ID_JOYPAD_Y, 10 ); // X
		setDeviceButtonStateDirectMacro(ret, port, RETRO_DEVICE_ASCIISTICK, RETRO_DEVICE_ID_JOYPAD_X,  9 ); // Y
		
		// Z
		{
		   uint32_t dc_key = 1 << 8; // Z
		   bool is_down = (ret & (1 << RETRO_DEVICE_ID_JOYPAD_L )) || 
		   				   (ret & (1 << RETRO_DEVICE_ID_JOYPAD_L2));
		   if (is_down)
			  kcode[port] &= ~dc_key;
		   else
			  kcode[port] |= dc_key;
		}

		// C
		{
		   uint32_t dc_key = 1 << 0; // C
		   bool is_down = (ret & (1 << RETRO_DEVICE_ID_JOYPAD_R)) || 
		   				   (ret & (1 << RETRO_DEVICE_ID_JOYPAD_R2));
		   if (is_down)
			  kcode[port] &= ~dc_key;
		   else
			  kcode[port] |= dc_key;
		}
		
		setDeviceButtonStateDirectMacro(ret, port, RETRO_DEVICE_ASCIISTICK, RETRO_DEVICE_ID_JOYPAD_START, 3 ); // Start
		
		// unused inputs
		lt[port]=0;
		rt[port]=0;
		joyx[port]=0;
		joyy[port]=0;
	}
	break;

	case MDT_TwinStick:
	{
      int16_t ret = 0;

		kcode[port] = 0xFFFF; // active-low

		
		if ( device_type[port] == RETRO_DEVICE_TWINSTICK_SATURN )
		{
			// NOTE: This is a remapping of the RetroPad layout in the block below to make using a real
			// Saturn Twin-Stick controller (via a USB adapter) less effort.			
			
			// The Saturn Twin-Stick identifies as a regular Saturn controller internally but with its controls
			// wired to the two sticks without much rhyme or reason. The mapping below untangles that layout
			// into DC compatible inputs, without requiring a change for the Reicast and Beetle Saturn cores.

			// Hope that makes sense!!
			
			// NOTE: the dc_bits below are the same, only the retro id values have been rearranged.
         
         if (libretro_supports_bitmasks)
            ret = input_cb(port, RETRO_DEVICE_TWINSTICK_SATURN, 0, RETRO_DEVICE_ID_JOYPAD_MASK);
         else
         {
            unsigned id;
            for (id = RETRO_DEVICE_ID_JOYPAD_B; id <= RETRO_DEVICE_ID_JOYPAD_R3; ++id)
               if (input_cb(port, RETRO_DEVICE_TWINSTICK_SATURN, 0, id))
                  ret |= (1 << id);
         }
			
			// left-stick
			setDeviceButtonStateDirectMacro(ret, port, RETRO_DEVICE_TWINSTICK_SATURN, RETRO_DEVICE_ID_JOYPAD_UP, 4 );
			setDeviceButtonStateDirectMacro(ret, port, RETRO_DEVICE_TWINSTICK_SATURN, RETRO_DEVICE_ID_JOYPAD_DOWN, 5 );
			setDeviceButtonStateDirectMacro(ret, port, RETRO_DEVICE_TWINSTICK_SATURN, RETRO_DEVICE_ID_JOYPAD_LEFT, 6 );
			setDeviceButtonStateDirectMacro(ret, port, RETRO_DEVICE_TWINSTICK_SATURN, RETRO_DEVICE_ID_JOYPAD_RIGHT, 7 );
			setDeviceButtonStateDirectMacro(ret, port, RETRO_DEVICE_TWINSTICK_SATURN, RETRO_DEVICE_ID_JOYPAD_L2, 10 ); // left-trigger
			setDeviceButtonStateDirectMacro(ret, port, RETRO_DEVICE_TWINSTICK_SATURN, RETRO_DEVICE_ID_JOYPAD_R2, 9 ); // left-turbo

			// right-stick
			setDeviceButtonStateDirectMacro(ret, port, RETRO_DEVICE_TWINSTICK_SATURN, RETRO_DEVICE_ID_JOYPAD_X, 12 ); // up
			setDeviceButtonStateDirectMacro(ret, port, RETRO_DEVICE_TWINSTICK_SATURN, RETRO_DEVICE_ID_JOYPAD_L, 15 ); // right
			setDeviceButtonStateDirectMacro(ret, port, RETRO_DEVICE_TWINSTICK_SATURN, RETRO_DEVICE_ID_JOYPAD_A, 13 ); // down
			setDeviceButtonStateDirectMacro(ret, port, RETRO_DEVICE_TWINSTICK_SATURN, RETRO_DEVICE_ID_JOYPAD_Y, 14 ); // left
			setDeviceButtonStateDirectMacro(ret, port, RETRO_DEVICE_TWINSTICK_SATURN, RETRO_DEVICE_ID_JOYPAD_B, 2 ); // right-trigger
			setDeviceButtonStateDirectMacro(ret, port, RETRO_DEVICE_TWINSTICK_SATURN, RETRO_DEVICE_ID_JOYPAD_R, 1 ); // right-turbo

			// misc control
			setDeviceButtonStateDirectMacro(ret, port, RETRO_DEVICE_TWINSTICK_SATURN, RETRO_DEVICE_ID_JOYPAD_START, 3 );
			setDeviceButtonStateDirectMacro(ret, port, RETRO_DEVICE_TWINSTICK_SATURN, RETRO_DEVICE_ID_JOYPAD_SELECT, 11 ); //D
		}
		else
		{	
			int analog;
			
			const int thresh = 11000; // about 33%, allows for 8-way movement

         if (libretro_supports_bitmasks)
            ret = input_cb(port, RETRO_DEVICE_TWINSTICK, 0, RETRO_DEVICE_ID_JOYPAD_MASK);
         else
         {
            unsigned id;
            for (id = RETRO_DEVICE_ID_JOYPAD_B; id <= RETRO_DEVICE_ID_JOYPAD_R3; ++id)
               if (input_cb(port, RETRO_DEVICE_TWINSTICK, 0, id))
                  ret |= (1 << id);
         }

			// LX
			analog = input_cb( port, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X );
			if ( analog < -thresh )
				kcode[port] &= ~( 1 << 6 ); // L
			else if ( analog > thresh )
				kcode[port] &= ~( 1 << 7 ); // R	
			else
			{
				// digital
				setDeviceButtonStateDirectMacro(ret, port, RETRO_DEVICE_TWINSTICK, RETRO_DEVICE_ID_JOYPAD_LEFT, 6 );
				setDeviceButtonStateDirectMacro(ret, port, RETRO_DEVICE_TWINSTICK, RETRO_DEVICE_ID_JOYPAD_RIGHT, 7 );
			}
			
			// LY
			analog = input_cb( port, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y );
			if ( analog < -thresh )
				kcode[port] &= ~( 1 << 4 ); // U
			else if ( analog > thresh )
				kcode[port] &= ~( 1 << 5 ); // D	
			else
			{
				// digital
				setDeviceButtonStateDirectMacro(ret, port, RETRO_DEVICE_TWINSTICK, RETRO_DEVICE_ID_JOYPAD_UP, 4 );
				setDeviceButtonStateDirectMacro(ret, port, RETRO_DEVICE_TWINSTICK, RETRO_DEVICE_ID_JOYPAD_DOWN, 5 );
			}

			// RX
			analog = input_cb( port, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X );
			if ( analog < -thresh )
				kcode[port] &= ~( 1 << 14 ); // L
			else if ( analog > thresh )
				kcode[port] &= ~( 1 << 15 ); // R	
			else
			{
				// digital
				setDeviceButtonStateDirectMacro(ret, port, RETRO_DEVICE_TWINSTICK, RETRO_DEVICE_ID_JOYPAD_Y, 14 ); // left
				setDeviceButtonStateDirectMacro(ret, port, RETRO_DEVICE_TWINSTICK, RETRO_DEVICE_ID_JOYPAD_A, 15 ); // right
			}
			
			// RY
			analog = input_cb( port, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y );
			if ( analog < -thresh )
				kcode[port] &= ~( 1 << 12 ); // U
			else if ( analog > thresh )
				kcode[port] &= ~( 1 << 13 ); // D
			else
			{
				// digital
				setDeviceButtonStateDirectMacro(ret, port, RETRO_DEVICE_TWINSTICK, RETRO_DEVICE_ID_JOYPAD_X, 12 ); // up
				setDeviceButtonStateDirectMacro(ret, port, RETRO_DEVICE_TWINSTICK, RETRO_DEVICE_ID_JOYPAD_B, 13 ); // down
			}

			// left-stick buttons
			setDeviceButtonStateDirectMacro(ret, port, RETRO_DEVICE_TWINSTICK, RETRO_DEVICE_ID_JOYPAD_L2, 10 ); // left-trigger
			setDeviceButtonStateDirectMacro(ret, port, RETRO_DEVICE_TWINSTICK, RETRO_DEVICE_ID_JOYPAD_L, 9 ); // left-turbo

			// right-stick buttons
			setDeviceButtonStateDirectMacro(ret, port, RETRO_DEVICE_TWINSTICK, RETRO_DEVICE_ID_JOYPAD_R2, 2 ); // right-trigger
			setDeviceButtonStateDirectMacro(ret, port, RETRO_DEVICE_TWINSTICK, RETRO_DEVICE_ID_JOYPAD_R, 1 ); // right-turbo

			// misc control
			setDeviceButtonStateDirectMacro(ret, port, RETRO_DEVICE_TWINSTICK, RETRO_DEVICE_ID_JOYPAD_START, 3 );
			setDeviceButtonStateDirectMacro(ret, port, RETRO_DEVICE_TWINSTICK, RETRO_DEVICE_ID_JOYPAD_SELECT, 11 ); //D
		}
		
		// unused inputs
		lt[port]=0;
		rt[port]=0;
		joyx[port]=0;
		joyy[port]=0;
	}
	
	break;
	  
	  case MDT_LightGun:
	  {
		 //
		 // -- buttons
		 setDeviceButtonState(port, RETRO_DEVICE_LIGHTGUN, RETRO_DEVICE_ID_LIGHTGUN_TRIGGER);
		 setDeviceButtonState(port, RETRO_DEVICE_LIGHTGUN, RETRO_DEVICE_ID_LIGHTGUN_AUX_A);
		 setDeviceButtonState(port, RETRO_DEVICE_LIGHTGUN, RETRO_DEVICE_ID_LIGHTGUN_START);
		 setDeviceButtonState(port, RETRO_DEVICE_LIGHTGUN, RETRO_DEVICE_ID_LIGHTGUN_DPAD_UP);
		 setDeviceButtonState(port, RETRO_DEVICE_LIGHTGUN, RETRO_DEVICE_ID_LIGHTGUN_DPAD_DOWN);
		 setDeviceButtonState(port, RETRO_DEVICE_LIGHTGUN, RETRO_DEVICE_ID_LIGHTGUN_DPAD_LEFT);
		 setDeviceButtonState(port, RETRO_DEVICE_LIGHTGUN, RETRO_DEVICE_ID_LIGHTGUN_DPAD_RIGHT);

		 bool force_offscreen = false;

		 if (input_cb(port, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_RELOAD))
		 {
			force_offscreen = true;
			kcode[port] &= ~DC_BTN_A;
		 }

		 if (force_offscreen || input_cb(port, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN))
		 {
			mo_x_abs[port] = -1;
			mo_y_abs[port] = -1;
		 }
		 else
		 {
			mo_x_abs[port] = (input_cb(port, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X) + 0x8000) * 640.f / 0x10000;
			mo_y_abs[port] = (input_cb(port, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y) + 0x8000) * 480.f / 0x10000;

			lightgun_params[port].offscreen = false;
			lightgun_params[port].x = mo_x_abs[port];
			lightgun_params[port].y = mo_y_abs[port];
		 }
	  }
	  break;

	  case MDT_Mouse:
		 updateMouseState(port);
		 break;
   }
}

void UpdateVibration(u32 port, u32 value, u32 max_duration)
{
   if (!rumble.set_rumble_state)
      return;

   u8 POW_POS = (value >> 8) & 0x7;
   u8 POW_NEG = (value >> 12) & 0x7;
   u8 FREQ = (value >> 16) & 0xFF;
   s16 INC = (value >> 24) & 0xFF;
   if (value & 0x8000)			// INH
	  INC = -INC;
   else if (!(value & 0x0800))	// EXH
	  INC = 0;
   bool CNT = value & 1;

   double pow = min((POW_POS + POW_NEG) / 7.0, 1.0);
   vib_strength[port] = pow;

   rumble.set_rumble_state(port, RETRO_RUMBLE_STRONG, (u16)(65535 * pow));

   if (FREQ > 0 && (!CNT || INC))
	  vib_stop_time[port] = get_time_ms() + min((int)(1000 * (INC ? abs(INC) * max(POW_POS, POW_NEG) : 1) / FREQ), (int)max_duration);
   else
	  vib_stop_time[port] = get_time_ms() + max_duration;
   if (INC == 0 || pow == 0)
	  vib_delta[port] = 0.0;
   else
	  vib_delta[port] = FREQ / (1000.0 * INC * max(POW_POS, POW_NEG));
}

extern u8 kb_shift; 		// shift keys pressed (bitmask)
extern u8 kb_key[6];		// normal keys pressed (up to 6)
static int kb_used;

static void release_key(unsigned dc_keycode)
{
   if (dc_keycode == 0)
	  return;

   if (kb_used > 0)
   {
	  for (int i = 0; i < 6; i++)
	  {
		 if (kb_key[i] == dc_keycode)
		 {
			kb_used--;
			for (int j = i; j < 5; j++)
			   kb_key[j] = kb_key[j + 1];
			kb_key[5] = 0;
		 }
	  }
   }
}

void retro_keyboard_event(bool down, unsigned keycode, uint32_t character, uint16_t key_modifiers)
{
   // Dreamcast keyboard emulation
   if (keycode == RETROK_LSHIFT || keycode == RETROK_RSHIFT)
	  if (!down)
		 kb_shift &= ~(0x02 | 0x20);
	  else
		 kb_shift |= (0x02 | 0x20);
   if (keycode == RETROK_LCTRL || keycode == RETROK_RCTRL)
	  if (!down)
		 kb_shift &= ~(0x01 | 0x10);
	  else
		 kb_shift |= (0x01 | 0x10);

   // Make sure modifier keys are released
   if ((key_modifiers & RETROKMOD_SHIFT) == 0)
   {
	  release_key(kb_map[RETROK_LSHIFT]);
	  release_key(kb_map[RETROK_LSHIFT]);
   }
   if ((key_modifiers & RETROKMOD_CTRL) == 0)
   {
	  release_key(kb_map[RETROK_LCTRL]);
	  release_key(kb_map[RETROK_RCTRL]);
   }

   u8 dc_keycode = kb_map[keycode];
   if (dc_keycode != 0)
   {
	  if (down)
	  {
		 if (kb_used < 6)
		 {
			bool found = false;
			for (int i = 0; !found && i < 6; i++)
			{
			   if (kb_key[i] == dc_keycode)
				  found = true;
			}
			if (!found)
			{
			   kb_key[kb_used] = dc_keycode;
			   kb_used++;
			}
		 }
	  }
	  else
	  {
		 release_key(dc_keycode);
	  }
   }
}

void* libPvr_GetRenderTarget()
{
   return NULL;
}

void* libPvr_GetRenderSurface()
{
   return NULL;
}

int msgboxf(const char* text, unsigned int type, ...)
{
   if (log_cb)
   {
      va_list args;

      char temp[2048];

      va_start(args, type);
      vsprintf(temp, text, args);
      va_end(args);

      log_cb(RETRO_LOG_INFO, temp);
   }
   return 0;
}

int get_mic_data(u8* buffer) { return 0; }
int push_vmu_screen(u8* buffer) { return 0; }

void os_DebugBreak(void)
{
   printf("DEBUGBREAK!\n");
   //exit(-1);
   __builtin_trap();
}

static bool retro_set_eject_state(bool ejected)
{
   disc_tray_open = ejected;
   if (ejected)
   {
      DiscOpenLid();
      return true;
   }
   else
   {
      return DiscSwap();
   }
}

static bool retro_get_eject_state()
{
   return disc_tray_open;
}

static unsigned retro_get_image_index()
{
   return disk_index;
}

static bool retro_set_image_index(unsigned index)
{
   settings.imgread.LoadDefaultImage = true;
   disk_index = index;
   if (disk_index >= disk_paths.size())
   {
	  // No disk in drive
	  settings.imgread.DefaultImage[0] = '\0';
	  return true;
   }
   strncpy(settings.imgread.DefaultImage, disk_paths[index].c_str(), sizeof(settings.imgread.DefaultImage));
   settings.imgread.DefaultImage[sizeof(settings.imgread.DefaultImage) - 1] = '\0';

   if (disc_tray_open)
      return true;
   else
      return DiscSwap();
}

static unsigned retro_get_num_images()
{
   return disk_paths.size();
}

static bool retro_add_image_index()
{
   disk_paths.push_back("");

   return true;
}

static bool retro_replace_image_index(unsigned index, const struct retro_game_info *info)
{
  if (info == nullptr)
  {
    if (index < disk_paths.size())
    {
      disk_paths.erase(disk_paths.begin() + index);
      if (disk_index >= index && disk_index > 0)
        disk_index--;
    }
  }
  else
    disk_paths[index] = info->path;

  return true;
}

static void init_disk_control_interface(const char *initial_image_path)
{
  retro_disk_control_cb.set_eject_state = retro_set_eject_state;
  retro_disk_control_cb.get_eject_state = retro_get_eject_state;
  retro_disk_control_cb.set_image_index = retro_set_image_index;
  retro_disk_control_cb.get_image_index = retro_get_image_index;
  retro_disk_control_cb.get_num_images  = retro_get_num_images;
  retro_disk_control_cb.add_image_index = retro_add_image_index;
  retro_disk_control_cb.replace_image_index = retro_replace_image_index;

  environ_cb(RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE, &retro_disk_control_cb);
}

static bool read_m3u(const char *file)
{
   char line[PATH_MAX];
   char name[PATH_MAX];
   FILE *f = fopen(file, "r");

   if (!f)
   {
      log_cb(RETRO_LOG_ERROR, "Could not read file\n");
      return false;
   }

   while (fgets(line, sizeof(line), f) && disk_index <= disk_paths.size())
   {
      if (line[0] == '#')
         continue;

      char *carriage_return = strchr(line, '\r');
      if (carriage_return)
         *carriage_return = '\0';

      char *newline = strchr(line, '\n');
      if (newline)
         *newline = '\0';

      // Remove any beginning and ending quotes as these can cause issues when feeding the paths into command line later
      if (line[0] == '"')
         memmove(line, line + 1, strlen(line));

      if (line[strlen(line) - 1] == '"')
         line[strlen(line) - 1]  = '\0';

      if (line[0] != '\0')
      {
         snprintf(name, sizeof(name), "%s%s", g_roms_dir, line);
         disk_paths.push_back(name);
         disk_index++;
      }
   }

   fclose(f);
   return (disk_index != 0);
}
