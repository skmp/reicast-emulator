#ifndef LIBRETRO_CORE_OPTIONS_INTL_H__
#define LIBRETRO_CORE_OPTIONS_INTL_H__

#if defined(_MSC_VER) && (_MSC_VER >= 1500 && _MSC_VER < 1900)
/* https://support.microsoft.com/en-us/kb/980263 */
#pragma execution_character_set("utf-8")
#pragma warning(disable:4566)
#endif

#include <libretro.h>

#include "libretro_core_option_defines.h"

/*
 ********************************
 * VERSION: 1.3
 ********************************
 *
 * - 1.3: Move translations to libretro_core_options_intl.h
 *        - libretro_core_options_intl.h includes BOM and utf-8
 *          fix for MSVC 2010-2013
 *        - Added HAVE_NO_LANGEXTRA flag to disable translations
 *          on platforms/compilers without BOM support
 * - 1.2: Use core options v1 interface when
 *        RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION is >= 1
 *        (previously required RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION == 1)
 * - 1.1: Support generation of core options v0 retro_core_option_value
 *        arrays containing options with a single value
 * - 1.0: First commit
*/

#ifdef __cplusplus
extern "C" {
#endif

/*
 ********************************
 * Core Option Definitions
 ********************************
*/

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

#define COLORS_STRING_TR \
      { "BLACK 02",          "Black" }, \
      { "BLUE 03",           "Blue" }, \
      { "LIGHT_BLUE 04",     "Light Blue" }, \
      { "GREEN 05",          "Green" }, \
      { "CYAN 06",           "Cyan" }, \
      { "CYAN_BLUE 07",      "Cyan Blue" }, \
      { "LIGHT_GREEN 08",    "Light Green" }, \
      { "CYAN_GREEN 09",     "Cyan Green" }, \
      { "LIGHT_CYAN 10",     "Light Cyan" }, \
      { "RED 11",            "Red" }, \
      { "PURPLE 12",         "Purple" }, \
      { "LIGHT_PURPLE 13",   "Light Purple" }, \
      { "YELLOW 14",         "Yellow" }, \
      { "GRAY 15",           "Gray" }, \
      { "LIGHT_PURPLE_2 16", "Light Purple (2)" }, \
      { "LIGHT_GREEN_2 17",  "Light Green (2)" }, \
      { "LIGHT_GREEN_3 18",  "Light Green (3)" }, \
      { "LIGHT_CYAN_2 19",   "Light Cyan (2)" }, \
      { "LIGHT_RED_2 20",    "Light Red (2)" }, \
      { "MAGENTA 21",        "Magenta" }, \
      { "LIGHT_PURPLE_2 22", "Light Purple (2)" }, \
      { "LIGHT_ORANGE 23",   "Light Orange" }, \
      { "ORANGE 24",         "Orange" }, \
      { "LIGHT_PURPLE_3 25", "Light Purple (3)" }, \
      { "LIGHT_YELLOW 26",   "Light Yellow" }, \
      { "LIGHT_YELLOW_2 27", "Light Yellow (2)" }, \
      { "WHITE 28",          "White" }, \
      { NULL, NULL },

#define VMU_SCREEN_PARAMS_TR(num) \
{ \
   CORE_OPTION_NAME "_vmu" #num "_screen_display", \
   "VMU Screen " #num " Display", \
   "", \
   { \
      { NULL, NULL }, \
   }, \
   NULL, \
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
   NULL, \
}, \
{ \
   CORE_OPTION_NAME "_vmu" #num "_screen_size_mult", \
   "VMU Screen " #num " Size", \
   "", \
   { \
      { NULL, NULL }, \
   }, \
   NULL, \
}, \
{ \
   CORE_OPTION_NAME "_vmu" #num "_pixel_on_color", \
   "VMU Screen " #num " Pixel On Color", \
   "", \
   { \
      { "DEFAULT_ON 00",  "Default ON" }, \
      { "DEFAULT_OFF 01", "Default OFF" }, \
      COLORS_STRING_TR \
   }, \
   NULL, \
}, \
{ \
   CORE_OPTION_NAME "_vmu" #num "_pixel_off_color", \
   "VMU Screen " #num " Pixel Off Color", \
   "", \
   { \
      { "DEFAULT_OFF 01", "Default OFF" }, \
      { "DEFAULT_ON 00",  "Default ON" }, \
      COLORS_STRING_TR \
   }, \
   NULL, \
}, \
{ \
   CORE_OPTION_NAME "_vmu" #num "_screen_opacity", \
   "VMU Screen " #num " Opacity", \
   "", \
   { \
      { NULL,   NULL }, \
   }, \
   NULL, \
},

#define LIGHTGUN_PARAMS_TR(num) \
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
   NULL, \
},

struct retro_core_option_definition option_defs_tr[] = {
#if ((FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_X86) || (HOST_CPU == CPU_ARM) || (HOST_CPU == CPU_ARM64) || (HOST_CPU == CPU_X64)) && defined(TARGET_NO_JIT)
   {
      CORE_OPTION_NAME "_cpu_mode",
      "CPU Modu (Yeniden Başlatma Gerektirir)",
      "",
      {
#if (FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_X86) || (HOST_CPU == CPU_ARM) || (HOST_CPU == CPU_ARM64) || (HOST_CPU == CPU_X64)
         { "dynamic_recompiler", "Dynamic Recompiler" },
#endif
#ifdef TARGET_NO_JIT
         { "generic_recompiler", "Generic Recompiler" },
#endif
         { NULL, NULL },
      },
      NULL,
   },
#endif
   {
      CORE_OPTION_NAME "_boot_to_bios",
      "BIOS'a önyükleme (Yeniden Başlatma Gerektirir)",
      "Doğrudan Dreamcast BIOS menüsüne önyükleme yapın.",
      {
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_system",
      "Sistem Tipi (Yeniden Başlatma Gerektirir)",
      "",
      {
         { "auto",       "Otomatik" },
         { "dreamcast",  "Dreamcast" },
         { "naomi",      "NAOMI" },
         { "atomiswave", "Atomiswave" },
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_hle_bios",
      "HLE BIOS",
      "Üst düzey öykünmüş BIOS kullanımını zorla.",
      {
         { NULL, NULL},
      },
      NULL,
   },
#ifdef HAVE_OIT
   {
      CORE_OPTION_NAME "_oit_abuffer_size",
      "Birikim Piksel Arabellek Boyutu (Yeniden Başlatma Gerektirir)",
      "",
      {
         { NULL, NULL },
      },
      NULL,
   },
#endif
   {
      CORE_OPTION_NAME "_internal_resolution",
      "Dahili Çözünürlük (Yeniden Başlat Gerektirir)",
      "Render çözünürlüğünü değiştirin. Yeniden başlatma gerektirir.",
      {
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_screen_rotation",
      "Ekran Yönü",
      "",
      {
         { "horizontal", "Yatay" },
         { "vertical",   "Dikey" },
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_alpha_sorting",
      "Alfa Sıralama",
      "",
      {
         { "per-strip (fast, least accurate)", "Şerit Başına (hızlı, en az doğru)" },
         { "per-triangle (normal)",            "Üçgen Başına (normal)" },
#if defined(HAVE_OIT) || defined(HAVE_VULKAN)
         { "per-pixel (accurate)",             "Piksel Başına (doğru, ancak en yavaş)" },
#endif
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_gdrom_fast_loading",
      "GDROM Hızlı Yükleme (kusurlu)",
      "GD-ROM yüklemesini hızlandırır.",
      {
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_mipmapping",
      "Mipmapping",
      "",
      {
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_volume_modifier_enable",
      "Hacim Değiştirici",
      "Nesne gölgeleri çizmek için genellikle oyunlar tarafından kullanılan bir Dreamcast GPU özelliği. Bu normalde etkinleştirilmelidir - performansın etkisi ihmal edilebilir düzeyde genellikle minimum düzeydedir.",
      {
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_widescreen_hack",
      "Geniş ekran kesmesi (Yeniden Başlatma Gerektirir)",
      "",
      {
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_cable_type",
      "Kablo Tipi",
      "",
      {
         { "TV (RGB)",       NULL },
         { "TV (Composite)", NULL },
         { "VGA (RGB)",      NULL },
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_broadcast",
      "Yayın",
      "",
      {
         { "Default", "Varsayılan" },
         { "PAL_M",   "PAL-M (Brazil)" },
         { "PAL_N",   "PAL-N (Argentina, Paraguay, Uruguay)" },
         { "NTSC",    NULL },
         { "PAL",     "PAL (World)" },
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_framerate",
      "Kare Hızı",
      "Emülatörün ön uçla nasıl etkileşimde bulunduğunu etkiler. 'Tam Hız' - emülatör, bir kare oluşturulduğunda, kontrolü RetroArch'a geri döndürür. 'Normal' - emülatör, V-blank kesmesi her üretildiğinde kontrolü RetroArch'a döndürür. Çoğu durumda 'Tam Hız' kullanılmalıdır. 'Normal' bazı sistemlerde kare ilerleme hızını iyileştirebilir, ancak ekran statik olduğunda (örneğin, yükleme/duraklatma ekranları) yanıt vermeyen girişe neden olabilir.",
      {
         { "fullspeed", "Tam Hız" },
         { "normal",    "Normal" },
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_region",
      "Bölge",
      "",
      {
         { "Default", "Varsayılan" },
         { "Japan",   NULL },
         { "USA",     NULL },
         { "Europe",  NULL },
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_language",
      "Dil",
      "",
      {
         { "Default",  "Varsayılan" },
         { "Japanese", NULL },
         { "English",  NULL },
         { "German",   NULL },
         { "French",   NULL },
         { "Spanish",  NULL },
         { "Italian",  NULL },
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_div_matching",
      "DIV Eşleştirme (performans, daha az doğru)",
      "",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { "auto",     "Otomatik" },
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_force_wince",
      "Force Windows CE Mode",
      "Enable full MMU emulation and other settings for Windows CE games",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_analog_stick_deadzone",
      "Analog Çubuğu Ölü Bölge",
      "",
      {
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_trigger_deadzone",
      "Tetik Ölü Bölge",
      "",
      {
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_digital_triggers",
      "Dijital Tetikleyiciler",
      "",
      {
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_enable_dsp",
      "DSP'yi Etkinleştir",
      "Dreamcast'in ses DSP'sinin (dijital sinyal işlemcisi) öykünmesini etkinleştirin. Üretilen sesin doğruluğunu arttırır, ancak performans gereksinimlerini artırır.",
      {
         { NULL, NULL },
      },
      NULL,
   },
#ifdef HAVE_TEXUPSCALE
   {
      CORE_OPTION_NAME "_texupscale",
      "Doku Büyütme (xBRZ)",
      "",
      {
         { "off", "Devre Dışı" },
         { "2x",  NULL },
         { "4x",  NULL },
         { "6x",  NULL },
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_texupscale_max_filtered_texture_size",
      "Doku Yükseltme Maks. Filtre boyutu",
      "",
      {
         { NULL, NULL },
      },
      NULL,
   },
#endif
   {
      CORE_OPTION_NAME "_enable_rttb",
      "RTT'yi etkinleştirme (Dokuya Render'i) ara belleği",
      "",
      {
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_render_to_texture_upscaling",
      "Doku Yükseltme İşlemine Render",
      "",
      {
         { NULL, NULL },
      },
      NULL,
   },
#if !defined(TARGET_NO_THREADS)
   {
      CORE_OPTION_NAME "_threaded_rendering",
      "İşlem Parçacığı Renderlama (Yeniden Başlatma Gerektirir)",
      "",
      {
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_synchronous_rendering",
      "Senkronize İşleme",
      "",
      {
         { NULL, NULL },
      },
      NULL,
   },
#endif
   {
      CORE_OPTION_NAME "_frame_skipping",
      "Kare Atlama",
      "",
      {
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_enable_purupuru",
      "Purupuru Paketi / Titreşim Paketi",
      "Denetleyici geri bildirimini etkinleştirir.",
      {
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_allow_service_buttons",
      "Allow NAOMI Service Buttons",
      "Kabin ayarlarına girmek için NAOMI'nin SERVİS düğmesini etkinleştirir.",
      {
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_enable_naomi_15khz_dipswitch",
      "NAOMI 15KHz Dipswitch'i etkinleştir",
      "Bu, 240p, 480i'de gösterimi zorlayabilir veya oyuna bağlı olarak hiçbir etkisi olmayabilir.",
      {
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_custom_textures",
      "Özel Dokular Yükle",
      "",
      {
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_dump_textures",
      "Dokuları Göm",
      "",
      {
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_per_content_vmus",
      "Oyun Başına VMU'lar",
      "Devre dışı bırakıldığında, tüm oyunlar RetroArch'ın sistem dizininde bulunan 4 VMU kaydetme dosyasını (A1, B1, C1, D1) paylaşır. 'VMU A1' ayarı, RetroArch'ın başlattığı her oyun için kaydetme dizininde benzersiz bir VMU 'A1' dosyası oluşturur. 'Tüm VMU'lar' ayarı, başlatılan her oyun için 4 benzersiz VMU dosyası (A1, B1, C1, D1) oluşturur.",
      {
         { "disabled", "Devre Dışı" },
         { "VMU A1",   NULL },
         { "All VMUs", "Tüm VMU'lar" },
         { NULL, NULL},
      },
      NULL,
   },
   VMU_SCREEN_PARAMS_TR(1)
   VMU_SCREEN_PARAMS_TR(2)
   VMU_SCREEN_PARAMS_TR(3)
   VMU_SCREEN_PARAMS_TR(4)
   LIGHTGUN_PARAMS_TR(1)
   LIGHTGUN_PARAMS_TR(2)
   LIGHTGUN_PARAMS_TR(3)
   LIGHTGUN_PARAMS_TR(4)
   { NULL, NULL, NULL, {{0}}, NULL },
};

#ifdef __cplusplus
}
#endif

#endif
