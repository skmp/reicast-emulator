#ifndef LIBRETRO_CORE_OPTIONS_H__
#define LIBRETRO_CORE_OPTIONS_H__

#include <stdlib.h>
#include <string.h>

#include <libretro.h>
#include <retro_inline.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COLORS_STRING \
      {"BLACK 02",      NULL}, \
      {"BLUE 03",       NULL }, \
      {"LIGHT_BLUE 04", NULL }, \
      {"GREEN 05",      NULL }, \
      {"CYAN 06",       NULL }, \
      {"CYAN_BLUE 07",  NULL }, \
      {"LIGHT_GREEN 08",NULL }, \
      {"CYAN_GREEN 09", NULL }, \
      {"LIGHT_CYAN 10", NULL }, \
      {"RED 11",        NULL }, \
      {"PURPLE 12",     NULL }, \
      {"LIGHT_PURPLE 13",NULL}, \
      {"YELLOW 14",     NULL }, \
      {"GRAY 15",       NULL }, \
      {"LIGHT_PURPLE_2 16", NULL }, \
      {"LIGHT_GREEN_2 17",  NULL }, \
      {"LIGHT_GREEN_3 18",  NULL }, \
      {"LIGHT_CYAN_2 19",   NULL }, \
      {"LIGHT_RED_2 20",    NULL }, \
      {"MAGENTA 21",        NULL }, \
      {"LIGHT_PURPLE_2 22", NULL }, \
      {"LIGHT_ORANGE 23",   NULL }, \
      {"ORANGE 24",         NULL }, \
      {"LIGHT_PURPLE_3 25", NULL }, \
      {"LIGHT_YELLOW 26",   NULL }, \
      {"LIGHT_YELLOW_2 27", NULL }, \
      {"WHITE 28",          NULL }, \
      { NULL, NULL },

#define CORE_OPTION_NAME "reicast"

#define VMU_SCREEN_PARAMS(num) \
{ \
   CORE_OPTION_NAME "_vmu" #num "_screen_display", \
   "VMU Screen " #num " Display", \
   "", \
   { \
      { "disabled", NULL }, \
      { "enabled",  NULL }, \
      { NULL, NULL }, \
   }, \
   "disabled", \
}, \
{ \
CORE_OPTION_NAME "_vmu" #num "_screen_position", \
"VMU Screen " #num " Position", \
"", \
{ \
   { "Upper Left",  NULL }, \
   { "Upper Right", NULL }, \
   { "Lower Left",  NULL }, \
   { "Lower Right", NULL }, \
   { NULL, NULL }, \
}, \
   "Upper Left", \
}, \
{ \
CORE_OPTION_NAME "_vmu" #num "_screen_size_mult", \
"VMU Screen " #num " Size", \
"", \
{ \
   { "1x", NULL }, \
   { "2x", NULL }, \
   { "3x", NULL }, \
   { "4x", NULL }, \
   { "5x", NULL }, \
   { NULL, NULL }, \
}, \
   "1x", \
}, \
{ \
CORE_OPTION_NAME "_vmu" #num "_pixel_on_color", \
"VMU Screen " #num " Pixel On Color", \
"", \
{ \
   { "DEFAULT_ON 00", NULL }, \
   { "DEFAULT_OFF 01", NULL }, \
COLORS_STRING \
}, \
   "DEFAULT_ON 00", \
}, \
{ \
CORE_OPTION_NAME "_vmu" #num "_pixel_off_color", \
"VMU Screen " #num " Pixel Off Color", \
"", \
{ \
   { "DEFAULT_OFF 01", NULL }, \
   { "DEFAULT_ON 00",  NULL }, \
COLORS_STRING \
}, \
   "DEFAULT_OFF 01", \
}, \
{ \
CORE_OPTION_NAME "_vmu" #num "_screen_opacity", \
"VMU Screen " #num " Opacity", \
"", \
{ \
   {"10%",  NULL}, \
   {"20%",  NULL}, \
   {"30%",  NULL}, \
   {"40%",  NULL}, \
   {"50%",  NULL}, \
   {"60%",  NULL}, \
   {"70%",  NULL}, \
   {"80%",  NULL}, \
   {"90%",  NULL}, \
   {"100%", NULL}, \
   {NULL,   NULL}, \
}, \
   "100%", \
},

#define LIGHTGUN_PARAMS(num) \
{ \
CORE_OPTION_NAME "_lightgun" #num "_crosshair", \
"Gun Crosshair " #num " Display", \
"", \
{ \
   { "disabled", NULL }, \
   { "White",    NULL }, \
   { "Red",      NULL }, \
   { "Green",    NULL }, \
   { "Blue",     NULL }, \
   { NULL,       NULL }, \
}, \
   "disabled", \
},




/*
 ********************************
 * Core Option Definitions
 ********************************
*/

/* RETRO_LANGUAGE_ENGLISH */

/* Default language:
 * - All other languages must include the same keys and values
 * - Will be used as a fallback in the event that frontend language
 *   is not available
 * - Will be used as a fallback for any missing entries in
 *   frontend language definition */

struct retro_core_option_definition option_defs_us[] = {
#if ((FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_X86) || (HOST_CPU == CPU_ARM) || (HOST_CPU == CPU_ARM64) || (HOST_CPU == CPU_X64)) && defined(TARGET_NO_JIT)
   {
      CORE_OPTION_NAME "_cpu_mode",
      "CPU Mode (Restart)",
      "",
      {
#if (FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_X86) || (HOST_CPU == CPU_ARM) || (HOST_CPU == CPU_ARM64) || (HOST_CPU == CPU_X64)
         {"dynamic_recompiler", NULL},
#endif
#ifdef TARGET_NO_JIT
         {"generic_recompiler", NULL},
#endif
         { NULL, NULL},
      },
#if (FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_X86) || (HOST_CPU == CPU_ARM) || (HOST_CPU == CPU_ARM64) || (HOST_CPU == CPU_X64)
      "dynamic_recompiler",
#elif defined(TARGET_NO_JIT)
      "generic_recompiler",
#endif
   },
#endif
   {
      CORE_OPTION_NAME "_boot_to_bios",
      "Boot to BIOS (Restart)",
      "Boot directly into the Dreamcast BIOS menu.",
      {
         { "disabled",  NULL },
         { "enabled",  NULL },
         { NULL, NULL},
      },
      "disabled",
   },
   {
      CORE_OPTION_NAME "_system",
      "System type (Restart)",
      "",
      {
         { "auto",  NULL },
         { "dreamcast",  NULL },
         { "naomi",  NULL },
         { "atomiswave",  NULL },
         { NULL, NULL},
      },
      "auto",
   },
   {
      CORE_OPTION_NAME "_hle_bios",
      "HLE BIOS",
      "Force use of high-level emulation BIOS.",
      {
         { "disabled",  NULL },
         { "enabled",  NULL },
         { NULL, NULL},
      },
      "disabled",
   },
#ifdef HAVE_OIT
   {
      CORE_OPTION_NAME "_oit_abuffer_size",
      "Accumulation pixel buffer size (Restart)",
      "",
      {
         { "512MB",  NULL },
         { "1GB",  NULL },
         { "2GB",  NULL },
         { NULL, NULL},
      },
      "512MB",
   },
#endif
   {
      CORE_OPTION_NAME "_internal_resolution",
      "Internal Resolution (Restart)",
      "Modify the resolution. Requires a restart.",
      {
         { "320x240",    NULL },
         { "640x480",    NULL },
         { "800x600",    NULL },
         { "960x720",    NULL },
         { "1024x768",   NULL },
         { "1280x720",   NULL },
         { "1280x960",   NULL },
         { "1280x1024",  NULL },
         { "1440x1080",  NULL },
         { "1600x900",   NULL },
         { "1600x1200",  NULL },
         { "1920x1080",  NULL },
         { "1920x1440",  NULL },
         { "2560x1440",  NULL },
         { "2560x1920",  NULL },
         { "2880x2160",  NULL },
         { "3200x2400",  NULL },
         { "3440x1400",  NULL },
         { "3840x2160",  NULL },
         { "3840x2880",  NULL },
         { "4096x2160",  NULL },
         { "4480x3360",  NULL },
         { "5120x3840",  NULL },
         { "5760x4320",  NULL },
         { "6400x4800",  NULL },
         { "7040x5280",  NULL },
         { "7680x5760",  NULL },
         { "8320x6240",  NULL },
         { "8960x6720",  NULL },
         { "9600x7200",  NULL },
         { "10240x7680",  NULL },
         { "10880x8160",  NULL },
         { "11520x8640",  NULL },
         { "12160x9120",  NULL },
         { "12800x9600",  NULL },
         { NULL, NULL},
      },
#ifdef LOW_RES
      "320x240",
#else
      "640x480",
#endif
   },
   {
      CORE_OPTION_NAME "_screen_rotation",
      "Screen orientation",
      "",
      {
         { "horizontal",  NULL },
         { "vertical",  NULL },
         { NULL, NULL},
      },
      "horizontal",
   },
   {
      CORE_OPTION_NAME "_alpha_sorting",
      "Alpha sorting",
      "",
      {
         { "per-strip (fast, least accurate)",  "Per-strip (fast, least accurate)" },
         { "per-triangle (normal)",  "Per-triangle (normal)" },
#ifdef HAVE_OIT
         { "per-pixel (accurate)",  "Per-pixel (accurate, but slowest)" },
#endif
         { NULL, NULL},
      },
#if defined(LOW_END)
      "per-strip (fast, least accurate)",
#else
      "per-triangle (normal)",
#endif
   },
   {
      CORE_OPTION_NAME "_gdrom_fast_loading",
      "GDROM Fast Loading (inaccurate)",
      "",
      {
         { "disabled",  NULL },
         { "enabled",  NULL },
         { NULL, NULL},
      },
#ifdef LOW_END
      "enabled",
#else
      "disabled",
#endif
   },
   {
      CORE_OPTION_NAME "_mipmapping",
      "Mipmapping",
      "",
      {
         { "enabled",  NULL },
         { "disabled",  NULL },
         { NULL, NULL},
      },
      "enabled",
   },
   {
      CORE_OPTION_NAME "_volume_modifier_enable",
      "Volume modifier",
      "A Dreamcast GPU feature that is typically used by games to draw shadows of objects. You should typically leave this on - performance impact should be minimal to negligible.",
      {
         { "enabled",  NULL },
         { "disabled",  NULL },
         { NULL, NULL},
      },
      "enabled",
   },
   {
      CORE_OPTION_NAME "_widescreen_hack",
      "Widescreen hack (Restart)",
      "",
      {
         { "disabled",  NULL },
         { "enabled",  NULL },
         { NULL, NULL},
      },
      "disabled",
   },
   {
      CORE_OPTION_NAME "_cable_type",
      "Cable type",
      "",
      {
         { "TV (RGB)",  NULL },
         { "TV (Composite)",  NULL },
         { "VGA (RGB)",  NULL },
         { NULL, NULL},
      },
#ifdef LOW_END
      "VGA (RGB)",
#else
      "TV (RGB)",
#endif
   },
   {
      CORE_OPTION_NAME "_broadcast",
      "Broadcast",
      "",
      {
         { "Default",  NULL },
         { "PAL_M",  NULL },
         { "PAL_N",  NULL },
         { "NTSC",  NULL },
         { "PAL",  NULL },
         { NULL, NULL},
      },
      "Default",
   },
   {
      CORE_OPTION_NAME "_framerate",
      "Framerate",
      "",
      {
         { "fullspeed",  NULL },
         { "normal",  NULL },
         { NULL, NULL},
      },
      "fullspeed",
   },
   {
      CORE_OPTION_NAME "_region",
      "Region",
      "",
      {
         { "Default",  NULL },
         { "Japan",  NULL },
         { "USA",  NULL },
         { "Europe",  NULL },
         { NULL, NULL},
      },
      "Default",
   },
   {
      CORE_OPTION_NAME "_language",
      "Language",
      "",
      {
         { "Default",  NULL },
         { "Japanese",  NULL },
         { "English",  NULL },
         { "German",  NULL },
         { "French",  NULL },
         { "Spanish",  NULL },
         { "Italian",  NULL },
         { NULL, NULL},
      },
      "Default",
   },
   {
      CORE_OPTION_NAME "_div_matching",
      "DIV matching (performance, less accurate)",
      "",
      {
         { "disabled",  NULL },
         { "enabled",  NULL },
         { "auto",  NULL },
         { NULL, NULL},
      },
#ifdef LOW_END
      "enabled",
#else
      "disabled",
#endif
   },
   {
      CORE_OPTION_NAME "_analog_stick_deadzone",
      "Analog Stick Deadzone",
      "",
      {
         { "0%",  NULL },
         { "5%",  NULL },
         { "10%",  NULL },
         { "15%",  NULL },
         { "20%",  NULL },
         { "25%",  NULL },
         { "30%",  NULL },
         { NULL, NULL},
      },
      "15%",
   },
   {
      CORE_OPTION_NAME "_trigger_deadzone",
      "Trigger Deadzone",
      "",
      {
         { "0%",  NULL },
         { "5%",  NULL },
         { "10%",  NULL },
         { "15%",  NULL },
         { "20%",  NULL },
         { "25%",  NULL },
         { "30%",  NULL },
         { NULL, NULL},
      },
      "0%",
   },
   {
      CORE_OPTION_NAME "_digital_triggers",
      "Digital Triggers",
      "",
      {
         { "disabled",  NULL },
         { "enabled",  NULL },
         { NULL, NULL},
      },
      "disabled",
   },
   {
      CORE_OPTION_NAME "_enable_dsp",
      "Enable DSP",
      "Enable the Dreamcast's audio DSP. This will emulate the sound output more accurately at the expense of performance.",
      {
         { "disabled",  NULL },
         { "enabled",  NULL },
         { NULL, NULL},
      },
#ifdef LOW_END
      "disabled",
#else
      "enabled",
#endif
   },
#ifdef HAVE_TEXUPSCALE
   {
      CORE_OPTION_NAME "_texupscale",
      "Texture upscaling (xBRZ)",
      "",
      {
         { "off",  NULL },
         { "2x",  NULL },
         { "4x",  NULL },
         { "6x",  NULL },
         { NULL, NULL},
      },
      "off",
   },
   {
      CORE_OPTION_NAME "_texupscale_max_filtered_texture_size",
      "Texture upscaling max filtered size",
      "",
      {
         { "256",  NULL },
         { "512",  NULL },
         { "1024",  NULL },
         { NULL, NULL},
      },
      "256",
   },
#endif
   {
      CORE_OPTION_NAME "_enable_rtt",
      "Enable RTT (Render To Texture)",
      "",
      {
         { "disabled",  NULL },
         { "enabled",  NULL },
         { NULL, NULL},
      },
      "enabled",
   },
   {
      CORE_OPTION_NAME "_enable_rttb",
      "Enable RTT (Render To Texture) Buffer",
      "",
      {
         { "disabled",  NULL },
         { "enabled",  NULL },
         { NULL, NULL},
      },
      "disabled",
   },
   {
      CORE_OPTION_NAME "_render_to_texture_upscaling",
      "Render To Texture Upscaling",
      "",
      {
         { "1x",  NULL },
         { "2x",  NULL },
         { "3x",  NULL },
         { "4x",  NULL },
         { "8x",  NULL },
         { NULL, NULL},
      },
      "1x",
   },
#if !defined(TARGET_NO_THREADS)
   {
      CORE_OPTION_NAME "_threaded_rendering",
      "Threaded rendering (Restart)",
      "",
      {
         { "disabled",  NULL },
         { "enabled",  NULL },
         { NULL, NULL},
      },
      "disabled",
   },
   {
      CORE_OPTION_NAME "_synchronous_rendering",
      "Synchronous rendering",
      "",
      {
         { "disabled",  NULL },
         { "enabled",  NULL },
         { NULL, NULL},
      },
      "disabled",
   },
#endif
   {
      CORE_OPTION_NAME "_frame_skipping",
      "Frame skipping",
      "",
      {
         { "disabled",  NULL },
         { "1",  NULL },
         { "2",  NULL },
         { "3",  NULL },
         { "4",  NULL },
         { "5",  NULL },
         { "6",  NULL },
         { NULL, NULL},
      },
      "disabled",
   },
   {
      CORE_OPTION_NAME "_enable_purupuru",
      "Purupuru Pack/Vibration Pack",
      "",
      {
         { "disabled",  NULL },
         { "enabled",  NULL },
         { NULL, NULL},
      },
      "enabled",
   },
   {
      CORE_OPTION_NAME "_allow_service_buttons",
      "Allow NAOMI service buttons",
      "",
      {
         { "disabled",  NULL },
         { "enabled",  NULL },
         { NULL, NULL},
      },
      "disabled",
   },
   {
      CORE_OPTION_NAME "_enable_naomi_15khz_dipswitch",
      "Enable NAOMI 15KHz dipswitch (480i)",
      "",
      {
         { "disabled",  NULL },
         { "enabled",  NULL },
         { NULL, NULL},
      },
      "disabled",
   },
   {
      CORE_OPTION_NAME "_custom_textures",
      "Load custom textures",
      "",
      {
         { "disabled",  NULL },
         { "enabled",  NULL },
         { NULL, NULL},
      },
      "disabled",
   },
   {
      CORE_OPTION_NAME "_dump_textures",
      "Dump textures",
      "",
      {
         { "disabled",  NULL },
         { "enabled",  NULL },
         { NULL, NULL},
      },
      "disabled",
   },
   {
      CORE_OPTION_NAME "_per_content_vmus",
      "Per-game VMUs",
      "",
      {
         { "disabled",  NULL },
         { "VMU A1",  NULL },
         { "All VMUs",  NULL },
         { NULL, NULL},
      },
      "disabled",
   },
	  VMU_SCREEN_PARAMS(1)
	  VMU_SCREEN_PARAMS(2)
	  VMU_SCREEN_PARAMS(3)
	  VMU_SCREEN_PARAMS(4)
     LIGHTGUN_PARAMS(1)
     LIGHTGUN_PARAMS(2)
     LIGHTGUN_PARAMS(3)
     LIGHTGUN_PARAMS(4)
   { NULL, NULL, NULL, { NULL, NULL }, NULL },
};

/* RETRO_LANGUAGE_JAPANESE */

/* RETRO_LANGUAGE_FRENCH */

/* RETRO_LANGUAGE_SPANISH */

/* RETRO_LANGUAGE_GERMAN */

/* RETRO_LANGUAGE_ITALIAN */

/* RETRO_LANGUAGE_DUTCH */

/* RETRO_LANGUAGE_PORTUGUESE_BRAZIL */

/* RETRO_LANGUAGE_PORTUGUESE_PORTUGAL */

/* RETRO_LANGUAGE_RUSSIAN */

/* RETRO_LANGUAGE_KOREAN */

/* RETRO_LANGUAGE_CHINESE_TRADITIONAL */

/* RETRO_LANGUAGE_CHINESE_SIMPLIFIED */

/* RETRO_LANGUAGE_ESPERANTO */

/* RETRO_LANGUAGE_POLISH */

/* RETRO_LANGUAGE_VIETNAMESE */

/* RETRO_LANGUAGE_ARABIC */

/* RETRO_LANGUAGE_GREEK */

/* RETRO_LANGUAGE_TURKISH */

/*
 ********************************
 * Language Mapping
 ********************************
*/

struct retro_core_option_definition *option_defs_intl[RETRO_LANGUAGE_LAST] = {
   option_defs_us, /* RETRO_LANGUAGE_ENGLISH */
   NULL,           /* RETRO_LANGUAGE_JAPANESE */
   NULL,           /* RETRO_LANGUAGE_FRENCH */
   NULL,           /* RETRO_LANGUAGE_SPANISH */
   NULL,           /* RETRO_LANGUAGE_GERMAN */
   NULL,           /* RETRO_LANGUAGE_ITALIAN */
   NULL,           /* RETRO_LANGUAGE_DUTCH */
   NULL,           /* RETRO_LANGUAGE_PORTUGUESE_BRAZIL */
   NULL,           /* RETRO_LANGUAGE_PORTUGUESE_PORTUGAL */
   NULL,           /* RETRO_LANGUAGE_RUSSIAN */
   NULL,           /* RETRO_LANGUAGE_KOREAN */
   NULL,           /* RETRO_LANGUAGE_CHINESE_TRADITIONAL */
   NULL,           /* RETRO_LANGUAGE_CHINESE_SIMPLIFIED */
   NULL,           /* RETRO_LANGUAGE_ESPERANTO */
   NULL,           /* RETRO_LANGUAGE_POLISH */
   NULL,           /* RETRO_LANGUAGE_VIETNAMESE */
   NULL,           /* RETRO_LANGUAGE_ARABIC */
   NULL,           /* RETRO_LANGUAGE_GREEK */
   NULL,           /* RETRO_LANGUAGE_TURKISH */
};

/*
 ********************************
 * Functions
 ********************************
*/

/* Handles configuration/setting of core options.
 * Should only be called inside retro_set_environment().
 * > We place the function body in the header to avoid the
 *   necessity of adding more .c files (i.e. want this to
 *   be as painless as possible for core devs)
 */

static INLINE void libretro_set_core_options(retro_environment_t environ_cb)
{
   unsigned version = 0;

   if (!environ_cb)
      return;

   if (environ_cb(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &version) && (version == 1))
   {
      struct retro_core_options_intl core_options_intl;
      unsigned language = 0;

      core_options_intl.us    = option_defs_us;
      core_options_intl.local = NULL;

      if (environ_cb(RETRO_ENVIRONMENT_GET_LANGUAGE, &language) &&
          (language < RETRO_LANGUAGE_LAST) && (language != RETRO_LANGUAGE_ENGLISH))
         core_options_intl.local = option_defs_intl[language];

      environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL, &core_options_intl);
   }
   else
   {
      size_t i;
      size_t option_index              = 0;
      size_t num_options               = 0;
      struct retro_variable *variables = NULL;
      char **values_buf                = NULL;

      /* Determine number of options
       * > Note: We are going to skip a number of irrelevant
       *   core options when building the retro_variable array,
       *   but we'll allocate space for all of them. The difference
       *   in resource usage is negligible, and this allows us to
       *   keep the code 'cleaner' */
      while (true)
      {
         if (option_defs_us[num_options].key)
            num_options++;
         else
            break;
      }

      /* Allocate arrays */
      variables  = (struct retro_variable *)calloc(num_options + 1, sizeof(struct retro_variable));
      values_buf = (char **)calloc(num_options, sizeof(char *));

      if (!variables || !values_buf)
         goto error;

      /* Copy parameters from option_defs_us array */
      for (i = 0; i < num_options; i++)
      {
         const char *key                        = option_defs_us[i].key;
         const char *desc                       = option_defs_us[i].desc;
         const char *default_value              = option_defs_us[i].default_value;
         struct retro_core_option_value *values = option_defs_us[i].values;
         size_t buf_len                         = 3;
         size_t default_index                   = 0;

         values_buf[i] = NULL;

         /* Skip options that are irrelevant when using the
          * old style core options interface */
         if ((strcmp(key, "fceumm_advance_sound_options") == 0))
            continue;

         if (desc)
         {
            size_t num_values = 0;

            /* Determine number of values */
            while (true)
            {
               if (values[num_values].value)
               {
                  /* Check if this is the default value */
                  if (default_value)
                     if (strcmp(values[num_values].value, default_value) == 0)
                        default_index = num_values;

                  buf_len += strlen(values[num_values].value);
                  num_values++;
               }
               else
                  break;
            }

            /* Build values string */
            if (num_values > 1)
            {
               size_t j;

               buf_len += num_values - 1;
               buf_len += strlen(desc);

               values_buf[i] = (char *)calloc(buf_len, sizeof(char));
               if (!values_buf[i])
                  goto error;

               strcpy(values_buf[i], desc);
               strcat(values_buf[i], "; ");

               /* Default value goes first */
               strcat(values_buf[i], values[default_index].value);

               /* Add remaining values */
               for (j = 0; j < num_values; j++)
               {
                  if (j != default_index)
                  {
                     strcat(values_buf[i], "|");
                     strcat(values_buf[i], values[j].value);
                  }
               }
            }
         }

         variables[option_index].key   = key;
         variables[option_index].value = values_buf[i];
         option_index++;
      }

      /* Set variables */
      environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);

error:

      /* Clean up */
      if (values_buf)
      {
         for (i = 0; i < num_options; i++)
         {
            if (values_buf[i])
            {
               free(values_buf[i]);
               values_buf[i] = NULL;
            }
         }

         free(values_buf);
         values_buf = NULL;
      }

      if (variables)
      {
         free(variables);
         variables = NULL;
      }
   }
}

#ifdef __cplusplus
}
#endif

#endif
