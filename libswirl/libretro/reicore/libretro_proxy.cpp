
#include "libretro-common/include/libretro.h"
#include "libretro_proxy.h"
#include <Windows.h>

extern "C" {
#ifdef _WIN32
#ifdef BUILD_RETROARCH_CORE
#define LIBRETRO_PROXY_STUB_TYPE RETRO_API
#define trace_plugin(__s__) MessageBoxA(nullptr,__s__,"Trace",MB_OK)
#else
#define LIBRETRO_PROXY_STUB_TYPE
#define trace_plugin(__s__) 
#endif
#else
#define LIBRETRO_PROXY_STUB_TYPE
#define trace_plugin(__s__) printf("DummyTrace:%s\n",__s__)
#endif
}

//Private bridge module contexts
struct surface_dims_t {
    int32_t base_w, base_h;
    int32_t max_w, max_h;
    float aspect;
};

//Private bridge module variables
static retro_log_printf_t log_cb;
static retro_environment_t environ_cb;
static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;
static struct retro_rumble_interface rumble;
static struct retro_log_callback logging;
static bool g_b_init_done = false;
static surface_dims_t g_surf_dims{ 1024,768,1024,1024,0.0f };

#ifdef BUILD_RETROARCH_CORE
extern void retro_reicast_entry_point();
#endif

//Private bridge module functions
static void __emu_log_provider(enum retro_log_level level, const char* fmt, ...) {
    (void)level;
    va_list va;
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    va_end(va);
}

void LIBRETRO_PROXY_STUB_TYPE retro_init(void) {
    trace_plugin("retro_init");

    if (!g_b_init_done) {
        //TODO
        g_b_init_done = true;
    }

#ifdef BUILD_RETROARCH_CORE
    retro_reicast_entry_point();
#endif
}

void LIBRETRO_PROXY_STUB_TYPE retro_deinit(void) {
    trace_plugin("retro_deinit");

    retro_unload_game();

    if (g_b_init_done) {
        // TODO
        g_b_init_done = false;
    }
}

unsigned LIBRETRO_PROXY_STUB_TYPE retro_api_version(void) {
    trace_plugin("retro_api_version");
    return RETRO_API_VERSION;
}

void LIBRETRO_PROXY_STUB_TYPE retro_set_controller_port_device(unsigned port, unsigned device) {
    trace_plugin("retro_set_controller_port_device");
}

void LIBRETRO_PROXY_STUB_TYPE retro_get_system_info(struct retro_system_info* info) {
    trace_plugin("retro_get_system_info");

    memset(info, 0, sizeof(*info));
    info->library_name = "Reicast";
    info->library_version = "1";
    info->need_fullpath = true;
    info->valid_extensions =   "bin|gdi|chd|cue|cdi";
}

void LIBRETRO_PROXY_STUB_TYPE retro_get_system_av_info(struct retro_system_av_info* info) {
    trace_plugin("retro_get_system_av_info");
 
    float sampling_rate = 30000.0f;

    info->geometry.base_width = g_surf_dims.base_w;
    info->geometry.base_height = g_surf_dims.base_h;
    info->geometry.max_width = g_surf_dims.max_w;
    info->geometry.max_height = g_surf_dims.max_h;
    info->geometry.aspect_ratio = g_surf_dims.aspect;
    info->timing.sample_rate = sampling_rate;
}

void LIBRETRO_PROXY_STUB_TYPE retro_set_environment(retro_environment_t cb) {
    trace_plugin("retro_set_environment");

    environ_cb = cb;

    if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging))
        log_cb = logging.log;
    else
        log_cb = __emu_log_provider;

    static const struct retro_controller_description controllers[] = {
       { "Nintendo DS", RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_NONE, 0) },
    };

    static const struct retro_controller_info ports[] = {
       { controllers, 1 },
       { NULL, 0 },
    };

    cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports);
    retro_init();
}

void LIBRETRO_PROXY_STUB_TYPE retro_set_audio_sample(retro_audio_sample_t cb) {
    trace_plugin("retro_set_audio_sample");
    audio_cb = cb;
}

void LIBRETRO_PROXY_STUB_TYPE retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) {
    trace_plugin("retro_set_audio_sample_batch");
    audio_batch_cb = cb;
}

void LIBRETRO_PROXY_STUB_TYPE retro_set_input_poll(retro_input_poll_t cb) {
    trace_plugin("retro_set_input_poll");   
    input_poll_cb = cb;
}

void LIBRETRO_PROXY_STUB_TYPE retro_set_input_state(retro_input_state_t cb) {
    trace_plugin("retro_set_input_state");
    input_state_cb = cb;
}

void LIBRETRO_PROXY_STUB_TYPE retro_set_video_refresh(retro_video_refresh_t cb) {
    trace_plugin("retro_set_video_refresh");
    video_cb = cb;
}

void LIBRETRO_PROXY_STUB_TYPE retro_reset(void) {
    trace_plugin("retro_reset");
}

void LIBRETRO_PROXY_STUB_TYPE retro_run(void) {
    trace_plugin("retro_run");
}

bool LIBRETRO_PROXY_STUB_TYPE retro_load_game(const struct retro_game_info* info) {
    trace_plugin("retro_load_game");
    return true;
}

void LIBRETRO_PROXY_STUB_TYPE retro_unload_game(void) {
    trace_plugin("retro_unload_game");
}

unsigned LIBRETRO_PROXY_STUB_TYPE retro_get_region(void) {
    trace_plugin("retro_get_region");
    return RETRO_REGION_NTSC;
}

bool LIBRETRO_PROXY_STUB_TYPE retro_load_game_special(unsigned type, const struct retro_game_info* info, size_t num) {
    trace_plugin("retro_load_game_special");

    struct retro_input_descriptor desc[] = {
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "Left" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "Up" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "Down" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },
   { 0 },
    };

    environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);

    /*
    enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
    if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
    {
        log_cb(RETRO_LOG_INFO, "XRGB8888 is not supported.\n");
        return false;
    }

    snprintf(retro_game_path, sizeof(retro_game_path), "%s", info->path);
    struct retro_audio_callback audio_cb = { audio_callback, audio_set_state };
    use_audio_cb = environ_cb(RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK, &audio_cb);
    */
    return false;
}

size_t LIBRETRO_PROXY_STUB_TYPE retro_serialize_size(void) {
    trace_plugin("retro_serialize_size");
    return false;
}

bool LIBRETRO_PROXY_STUB_TYPE retro_serialize(void* data_, size_t size) {
    trace_plugin("retro_serialize");
    return true;
}

bool LIBRETRO_PROXY_STUB_TYPE  retro_unserialize(const void* data_, size_t size) {
    trace_plugin("retro_unserialize");
    return true;
}

/*
void* LIBRETRO_PROXY_STUB_TYPE retro_get_memory_data(unsigned id) {
    trace_plugin("retro_get_memory_data");
    return NULL;
}*/

size_t LIBRETRO_PROXY_STUB_TYPE retro_get_memory_size(unsigned id) {
    trace_plugin("retro_get_memory_size");
    return 0;
}

void LIBRETRO_PROXY_STUB_TYPE retro_cheat_reset(void) {
    trace_plugin("retro_cheat_reset");
}

void LIBRETRO_PROXY_STUB_TYPE retro_cheat_set(unsigned index, bool enabled, const char* code) {
    trace_plugin("retro_cheat_set");
}
