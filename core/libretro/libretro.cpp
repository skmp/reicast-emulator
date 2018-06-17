#include <cstdio>
#include <cstdarg>
#include <math.h>
#include "types.h"

#include <sys/stat.h>
#include <retro_stat.h>

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
#include <glsm/glsm.h>
#endif
#include "../rend/rend.h"

#include "libretro.h"

u32 fskip;
extern int screen_width;
extern int screen_height;
bool boot_to_bios;

static int astick_deadzone = 0;
static int trigger_deadzone = 0;
static bool digital_triggers = false;

u16 kcode[4] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
u8 rt[4] = {0, 0, 0, 0};
u8 lt[4] = {0, 0, 0, 0};
u32 vks[4];
s8 joyx[4], joyy[4];

bool enable_purupuru = true;

bool inside_loop     = true;
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
static retro_rumble_interface rumble;

int dc_init(int argc,wchar* argv[]);
void dc_run();

static int co_argc;
static wchar** co_argv;

char *game_data;
char game_dir[1024];
char game_dir_no_slash[1024];

void co_dc_yield(void)
{
   if (settings.UpdateMode || settings.UpdateModeForced)
      return;
   inside_loop = false;
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

void retro_set_environment(retro_environment_t cb)
{
   environ_cb = cb;

   struct retro_variable variables[] = {
#if ((FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_X86) || (HOST_CPU == CPU_ARM) || (HOST_CPU == CPU_X64) || TARGET_NO_JIT)
      {
         "reicast_cpu_mode",
         "CPU Mode (restart); "
#if (FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_X86) || (HOST_CPU == CPU_ARM) || (HOST_CPU == CPU_X64)
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
         "reicast_internal_resolution",
         "Internal resolution (restart); 640x480|1280x960|1920x1440|2560x1920|3200x2400|3840x2880|4480x3360|5120x3840|5760x4320|6400x4800|7040x5280|7680x5760|8320x6240|8960x6720|9600x7200|10240x7680|10880x8160|11520x8640|12160x9120|12800x9600",
      },
      {
         "reicast_mipmapping",
         "Mipmapping; enabled|disabled",
      },
#ifndef HAVE_OIT
      {
         "reicast_multipass",
         "Multipass support (restart); disabled|enabled",
      },
#endif
      {
         "reicast_volume_modifier_enable",
         "Volume modifier; enabled|disabled",
      },
      {
         "reicast_widescreen_hack",
         "Widescreen hack; disabled|enabled",
      },
      {
         "reicast_audio_buffer_size",
         "Audio buffer size; 1024|2048",
      },
      {
         "reicast_cable_type",
         "Cable type; TV (RGB)|TV (VBS/Y+S/C)|VGA (RGB)",
      },
      {
         "reicast_broadcast",
         "Broadcast; PAL_M|PAL_N|NTSC|PAL|Default",
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
      {
         "reicast_precompile_shaders",
         "Precompile shaders; disabled|enabled",
      },
      {
         "reicast_enable_rtt",
         "Enable RTT (Render To Texture); enabled|disabled",
      },
      {
         "reicast_enable_rttb",
         "Enable RTT (Render To Texture) Buffer; disabled|enabled",
      },
      {
         "reicast_enable_purupuru",
         "Purupuru Pack (restart); enabled|disabled"
      },
      { NULL, NULL },
   };

   cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);
}


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
}

// TODO/FIXME - forward declarations
void dc_term(void);
void rend_terminate();
void ngen_terminate();

void retro_deinit(void)
{
   first_run = true;
}

static bool is_dupe = false;

static void update_variables(bool first_startup)
{
   struct retro_variable var = {
      .key = "reicast_internal_resolution",
   };

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

#ifndef HAVE_OIT
   if (first_startup)
   {
      var.key = "reicast_multipass";

      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      {
         if (!strcmp(var.value, "enabled"))
            settings.rend.Multipass      = true;
         else if (!strcmp(var.value, "disabled"))
            settings.rend.Multipass      = false;
      }
      else
         settings.rend.Multipass         = false;
   }
#endif

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

   var.key = "reicast_audio_buffer_size";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      settings.aica.BufferSize = atoi(var.value);
   }
   else
      settings.aica.BufferSize = 1024;

   var.key = "reicast_cable_type";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp("VGA (RGB)", var.value))
         settings.dreamcast.cable = 0;
      else if (!strcmp("TV (RGB)", var.value))
         settings.dreamcast.cable = 2;
      else if (!strcmp("TV (VBS/Y+S/C)", var.value))
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

   var.key = "reicast_precompile_shaders";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp("disabled", var.value))
         settings.pvr.Emulation.precompile_shaders = false;
      else
         settings.pvr.Emulation.precompile_shaders = true;
   }

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

   var.key = "reicast_enable_purupuru";
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      enable_purupuru = (strcmp("enabled", var.value) == 0);

   var.key = "reicast_analog_stick_deadzone";
   var.value = NULL;

   if ( environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value )
      input_set_deadzone_stick( atoi( var.value ) );

   var.key = "reicast_trigger_deadzone";
   var.value = NULL;

   if ( environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value )
      input_set_deadzone_trigger( atoi( var.value ) );

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
}

void retro_run (void)
{
   bool updated = false;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
      update_variables(false);

   if (first_run)
   {
      dc_init(co_argc,co_argv);
      dc_run();
      first_run = false;
      return;
   }

   dc_run();
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
   video_cb(is_dupe ? 0 : RETRO_HW_FRAME_BUFFER_VALID, screen_width, screen_height, 0);
#endif
   is_dupe     = true;
   inside_loop = true;
}

void retro_reset (void)
{
   //TODO
   dc_term();
   first_run = true;
   settings.dreamcast.cable = 3;
   update_variables(false);
}

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
static void context_reset(void)
{
   printf("context_reset.\n");
   glsm_ctl(GLSM_CTL_STATE_CONTEXT_RESET, NULL);
}

static void context_destroy(void)
{
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

// Loading/unloading games
bool retro_load_game(const struct retro_game_info *game)
{
   glsm_ctx_params_t params = {0};
   const char *dir = NULL;
#ifdef _WIN32
   char slash = '\\';
#else
   char slash = '/';
#endif

   struct retro_input_descriptor desc[] = {
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "D-Pad Left" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "D-Pad Up" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "D-Pad Down" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "A" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "B" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,     "Y" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,     "X" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2,    "L Trigger" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2,    "R Trigger" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START,    "Start" },
      { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, "Analog X" },
      { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, "Analog Y" },

      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "D-Pad Left" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "D-Pad Up" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "D-Pad Down" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "A" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "B" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,     "Y" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,     "X" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2,    "L Trigger" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2,    "R Trigger" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START,    "Start" },
      { 1, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, "Analog X" },
      { 1, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, "Analog Y" },

      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "D-Pad Left" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "D-Pad Up" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "D-Pad Down" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "A" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "B" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,     "Y" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,     "X" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2,    "L Trigger" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2,    "R Trigger" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START,    "Start" },
      { 2, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, "Analog X" },
      { 2, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, "Analog Y" },

      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "D-Pad Left" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "D-Pad Up" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "D-Pad Down" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "A" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "B" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,     "Y" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,     "X" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2,    "L Trigger" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2,    "R Trigger" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START,    "Start" },
      { 3, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, "Analog X" },
      { 3, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, "Analog Y" },

      { 0 },
   };

   extract_directory(game_dir, game->path, sizeof(game_dir));

   environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);

   if (environ_cb(RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE, &rumble) && log_cb)
        log_cb(RETRO_LOG_INFO, "Rumble interface supported!\n");

   if (!(environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &dir) && dir))
      dir = game_dir;

   snprintf(game_dir, sizeof(game_dir), "%s%cdc%c", dir, slash, slash);
   snprintf(game_dir_no_slash, sizeof(game_dir_no_slash), "%s%cdc", dir, slash);

   settings.dreamcast.cable = 3;
   update_variables(true);

   if (game->path[0] == '\0')
      boot_to_bios = true;

   if (!boot_to_bios)
      game_data = strdup(game->path);

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

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
   params.context_reset         = context_reset;
   params.context_destroy       = context_destroy;
   params.environ_cb            = environ_cb;
   params.stencil               = true;
   params.imm_vbo_draw          = NULL;
   params.imm_vbo_disable       = NULL;

   if (!glsm_ctl(GLSM_CTL_STATE_CONTEXT_INIT, &params))
      return false;
#endif


   return true;
}

bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info)
{
   return false; //TODO (?)
}

void retro_unload_game(void)
{
   if (game_data)
      free(game_data);
   game_data = NULL;

   rend_terminate();
   ngen_terminate();
   dc_term();
}


// Memory/Serialization
void *retro_get_memory_data(unsigned type)
{
   return 0; //TODO
}

size_t retro_get_memory_size(unsigned type)
{
   return 0; //TODO
}

size_t retro_serialize_size (void)
{
   return 0; //TODO
}

bool retro_serialize(void *data, size_t size)
{
   return false; //TODO
}

bool retro_unserialize(const void * data, size_t size)
{
   return false; //TODO
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
#ifdef HAVE_OIT
   info->library_name = "Reicast OIT";
#else
   info->library_name = "Reicast";
#endif
#ifndef GIT_VERSION
#define GIT_VERSION ""
#endif
   info->library_version = "0.1" GIT_VERSION;
   info->valid_extensions = "cdi|gdi|chd|cue";
   info->need_fullpath = true;
   info->block_extract = false;
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   /*                        00=VGA    01=NTSC   10=PAL,   11=illegal/undocumented */
   const int spg_clks[4] = { 26944080, 13458568, 13462800, 26944080 };
   u32 pixel_clock= spg_clks[(SPG_CONTROL.full >> 6) & 3];

   info->geometry.base_width   = screen_width;
   info->geometry.base_height  = screen_height;
   info->geometry.max_width    = screen_width;
   info->geometry.max_height   = screen_height;
   info->geometry.aspect_ratio = 4.0 / 3.0;

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
   //TODO
   if (rumble.set_rumble_state)
   {
      rumble.set_rumble_state(in_port, RETRO_RUMBLE_STRONG, 0);
      rumble.set_rumble_state(in_port, RETRO_RUMBLE_WEAK,   0);
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
   is_dupe = false;
   poll_cb();

   if (settings.UpdateMode || settings.UpdateModeForced)
   {
      inside_loop = false;
      rend_end_render();
   }
}

void os_CreateWindow()
{
   // Nothing to do here
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
      static const int TRIGGER_MAX = 0x8000;
      const float scale = ((float)TRIGGER_MAX/(float)(TRIGGER_MAX - trigger_deadzone));

      if ( input > trigger_deadzone )
      {
         // Re-scale analog range
         float scaled = (input - trigger_deadzone)*scale;

         input = (int)round(scaled);
         if (input > +32767) {
            input = +32767;
         }
      }
      else
      {
         input = 0;
      }
   }

   return input;
}

static uint16_t get_analog_trigger( retro_input_state_t input_state_cb,
                           int player_index,
                           int id )
{
   uint16_t trigger;

   // NOTE: Analog triggers were added Nov 2017. Not all front-ends support this
   // feature (or pre-date it) so we need to handle this in a graceful way.

   // First, try and get an analog value using the new libretro API constant
   trigger = input_state_cb( player_index,
                       RETRO_DEVICE_ANALOG,
                       RETRO_DEVICE_INDEX_ANALOG_BUTTON,
                       id );

   if ( trigger == 0 )
   {
      // If we got exactly zero, we're either not pressing the button, or the front-end
      // is not reporting analog values. We need to do a second check using the classic
      // digital API method, to at least get some response - better than nothing.

      // NOTE: If we're really just not holding the trigger, we're still going to get zero.

      trigger = input_state_cb( player_index,
                          RETRO_DEVICE_JOYPAD,
                          0,
                          id ) ? 0x7FFF : 0;
   }
   else
   {
      // We got something, which means the front-end can handle analog buttons.
      // So we apply a deadzone to the input and use it.

      trigger = apply_trigger_deadzone( trigger );
   }

   return trigger;
}

void UpdateInputState(u32 port)
{
   int id;
   static const uint16_t joymap[] =
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


   //
   // -- buttons

   for (id = RETRO_DEVICE_ID_JOYPAD_B; id <= RETRO_DEVICE_ID_JOYPAD_X; ++id)
   {
      uint16_t dc_key = joymap[id];
      bool is_down = input_cb(port, RETRO_DEVICE_JOYPAD, 0, id);

      if ( is_down )
         kcode[port] &= ~dc_key;
      else
         kcode[port] |= dc_key;
   }

   //
   // -- analog stick

   get_analog_stick( input_cb, port, RETRO_DEVICE_INDEX_ANALOG_LEFT, &(joyx[port]), &(joyy[port]) );


   //
   // -- triggers

   if ( digital_triggers )
   {
      // -- digital left trigger
      if ( input_cb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L) )
         lt[port]=0xFF;
      else if ( input_cb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2) )
         lt[port]=0x7F;
      else
         lt[port]=0;
      // -- digital right trigger
      if ( input_cb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R) )
         rt[port]=0xFF;
      else if ( input_cb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2) )
         rt[port]=0x7F;
      else
         rt[port]=0;
   }
   else
   {
	   // -- analog triggers
	   lt[port] = get_analog_trigger( input_cb, port, RETRO_DEVICE_ID_JOYPAD_L2 ) / 128;
	   rt[port] = get_analog_trigger( input_cb, port, RETRO_DEVICE_ID_JOYPAD_R2 ) / 128;
   }
}

void UpdateVibration(u32 port, u32 value)
{
   if (!rumble.set_rumble_state)
      return;

   u8 POW_POS = (value >> 8) & 0x3;
   u8 POW_NEG = (value >> 12) & 0x3;
   u8 FREQ = (value >> 16) & 0xFF;

   double pow = (POW_POS + POW_NEG) / 7.0;
   double pow_l = pow * (0x3B - FREQ) / 17.0;
   double pow_r = pow * (FREQ - 0x07) / 15.0;

   if (pow_l > 1.0) pow_l = 1.0;
   if (pow_r > 1.0) pow_r = 1.0;

   rumble.set_rumble_state(port, RETRO_RUMBLE_STRONG, (u16)(65535 * pow_l));
   rumble.set_rumble_state(port, RETRO_RUMBLE_WEAK,   (u16)(65535 * pow_r));
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
   exit(-1);
}
