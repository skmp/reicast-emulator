#include <cstdio>
#include <cstdarg>
#include "types.h"

#include <libco.h>

#if defined(GL) || defined(GLES)
#include <glsym/rglgen.h>
#include <glsym/glsym.h>
#endif

#include "libretro.h"

int screen_width;
int screen_height;

u16 kcode[4] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
u8 rt[4] = {0, 0, 0, 0};
u8 lt[4] = {0, 0, 0, 0};
u32 vks[4];
s8 joyx[4], joyy[4];

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

// Callbacks
retro_log_printf_t         log_cb = NULL;
retro_video_refresh_t      video_cb = NULL;
retro_input_poll_t         poll_cb = NULL;
retro_input_state_t        input_cb = NULL;
retro_audio_sample_batch_t audio_batch_cb = NULL;
retro_environment_t        environ_cb = NULL;

#if defined(GL) || defined(GLES)
struct retro_hw_render_callback hw_render;
#endif

void common_linux_setup();
int dc_init(int argc,wchar* argv[]);
void dc_run();

static cothread_t ct_main;
static cothread_t ct_dc;

static int co_argc;
static wchar** co_argv;

char *game_data;
char game_dir[1024];
char game_dir_no_slash[1024];

static void co_dc_thread(void)
{
	co_switch(ct_main);
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
}

void co_dc_run(void)
{
   //puts("ENTER LOOP");
	co_switch(ct_dc);
}

void co_dc_yield(void)
{
	co_switch(ct_main);
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

void retro_set_environment(retro_environment_t cb)
{
   environ_cb = cb;

   struct retro_variable variables[] = {
      {
         "reicast_internal_resolution",
         "Internal resolution (restart); 640x480|720x576|800x600|960x720|1024x768|1280x720|1280x960|1280x1024|1600x1200|1920x1080|1920x1440|2048x2048|2560x1920|3200x2400|4096x4096",
      },
      {
         "reicast_widescreen_hack",
         "Widescreen hack; disabled|enabled",
      },
      {
         "reicast_cable_type",
         "Cable type; TV (VBS/Y+S/C)|TV (RGB)|VGA (0)(RGB)|VGA (1)(RGB)",
      },
      { NULL, NULL },
   };

   cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);
}


void install_fault_handler (void);//TODO: this is in core/linux/common.cpp and should not be referenced from libretro

// Now comes the interesting stuff
void retro_init(void)
{
   // Logging
   struct retro_log_callback log;
   if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
      log_cb = log.log;
   else
      log_cb = NULL;

   // Set color mode
   unsigned color_mode = RETRO_PIXEL_FORMAT_XRGB8888;
   environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &color_mode);

   install_fault_handler();

   co_dc_init(0, NULL);
}

void retro_deinit(void)
{
   //TODO
}

static void update_variables(void)
{
   static bool widescreen_set = false;
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

   var.key = "reicast_widescreen_hack";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "enabled"))
         settings.rend.WideScreen = true;
      else
         settings.rend.WideScreen = false;
   }

   var.key = "reicast_cable_type";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp("VGA (0)(RGB)", var.value))
         settings.dreamcast.cable = 0;
      else if (!strcmp("VGA (1)(RGB)", var.value))
         settings.dreamcast.cable = 1;
      else if (!strcmp("TV (RGB)", var.value))
         settings.dreamcast.cable = 2;
      else if (!strcmp("TV (VBS/Y+S/C)", var.value))
         settings.dreamcast.cable = 3;
   }
}

void retro_run (void)
{
   bool updated = false;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
      update_variables();

   glBindFramebuffer(GL_FRAMEBUFFER, hw_render.get_current_framebuffer());
   co_dc_run();
   glBindFramebuffer(GL_FRAMEBUFFER, 0);

#if defined(GL) || defined(GLES)
   video_cb(RETRO_HW_FRAME_BUFFER_VALID, screen_width, screen_height, 0);
#endif

}

void retro_reset (void)
{
   //TODO
}

#if defined(GL) || defined(GLES)
static void context_reset(void)
{
   printf("context_reset.\n");
   rglgen_resolve_symbols(hw_render.get_proc_address);
   co_switch(ct_dc);
}

static void context_destroy(void)
{
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
   const char *dir = NULL;

   game_data = strdup(game->path);

   extract_directory(game_dir, game->path, sizeof(game_dir));

   if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &dir) && dir)
   {
      snprintf(game_dir, sizeof(game_dir), "%s/dc/", dir);
      snprintf(game_dir_no_slash, sizeof(game_dir_no_slash), "%s/dc", dir);
   }
   else
   {
      strcat(game_dir, "/data/");
      strcat(game_dir_no_slash, "/data");
   }

   settings.dreamcast.cable = 3;
   screen_width  = 640;
   screen_height = 480;
   update_variables();

#if defined(GL) || defined(GLES)
#ifdef GLES
#if defined(GLES31)
   hw_render.context_type = RETRO_HW_CONTEXT_OPENGLES_VERSION;
   hw_render.version_major = 3;
   hw_render.version_minor = 1;
#elif defined(GLES3)
   hw_render.context_type = RETRO_HW_CONTEXT_OPENGLES3;
#else
   hw_render.context_type = RETRO_HW_CONTEXT_OPENGLES2;
#endif
#else
#ifdef CORE
   hw_render.context_type = RETRO_HW_CONTEXT_OPENGL_CORE;
   hw_render.version_major = 3;
   hw_render.version_minor = 1;
#else
   hw_render.context_type = RETRO_HW_CONTEXT_OPENGL;
#endif
#endif
   hw_render.context_reset      = context_reset;
   hw_render.context_destroy    = context_destroy;
   hw_render.depth              = true;
   hw_render.stencil            = true;
   hw_render.bottom_left_origin = true;

   if (!environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render))
      return false;
#else
   co_switch(ct_dc);
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
   info->library_name = "Reicast";
   info->library_version = "0.1";
   info->valid_extensions = "cdi|gdi|chd";
   info->need_fullpath = true;
   info->block_extract = false;
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   info->geometry.base_width   = screen_width;
   info->geometry.base_height  = screen_height;
   info->geometry.max_width    = screen_width;
   info->geometry.max_height   = screen_height;
   info->geometry.aspect_ratio = 4.0 / 3.0;
   info->timing.fps = 60.0; //FIXME: This might differ for non-NTSC games
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
}


// API version (to detect version mismatch)
unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}

//Reicast stuff
void os_DoEvents()
{
   poll_cb();
}

void os_CreateWindow()
{
   // Nothing to do here
}

void UpdateInputState(u32 port)
{
   int id;
   static const uint16_t joymap[] = {
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
      /* JOYPAD_L      */ 0, /* half L */
      /* JOYPAD_R      */ 0, /* half R */
      /* JOYPAD_L2     */ 0, /* full L */
      /* JOYPAD_R2     */ 0, /* full R */
      /* JOYPAD_L3     */ 0,
      /* JOYPAD_R3     */ 0,
   };


   rt[port] = 0;
   lt[port] = 0;

   for (id = RETRO_DEVICE_ID_JOYPAD_B; id < RETRO_DEVICE_ID_JOYPAD_R3+1; ++id)
   {
      uint16_t dc_key = joymap[id];
      bool is_down = input_cb(port, RETRO_DEVICE_JOYPAD, 0, id);

      switch (id)
      {
         case RETRO_DEVICE_ID_JOYPAD_L:  lt[port] |= 0x7f * is_down; break;
         case RETRO_DEVICE_ID_JOYPAD_R:  rt[port] |= 0x7f * is_down; break;
         case RETRO_DEVICE_ID_JOYPAD_L2: lt[port] |= 0xff * is_down; break;
         case RETRO_DEVICE_ID_JOYPAD_R2: rt[port] |= 0xff * is_down; break;
         default:
            if (is_down)
               kcode[port] &= ~dc_key;
            else
               kcode[port] |= dc_key;
      }
   }

   joyx[port] = input_cb(port, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X) / 256;
   joyy[port] = input_cb(port, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y) / 256;
}

void* libPvr_GetRenderTarget()
{
   return NULL;
}

void* libPvr_GetRenderSurface()
{
   return NULL;
}

void os_SetWindowText(const char * text)
{
   // Nothing to do here
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
