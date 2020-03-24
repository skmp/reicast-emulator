
#include "libretro-common/include/libretro.h"
#include "libretro_proxy.h"
#include "utils/glwrap/gl3w.h"
#include "types.h"
#ifdef _WIN32
#include <Windows.h>

#endif

#define RARCH_GL_FRAMEBUFFER GL_FRAMEBUFFER
#define RARCH_GL_FRAMEBUFFER_COMPLETE GL_FRAMEBUFFER_COMPLETE
#define RARCH_GL_COLOR_ATTACHMENT0 GL_COLOR_ATTACHMENT0

extern "C" {
#ifdef _WIN32
#ifdef BUILD_RETROARCH_CORE
#define LIBRETRO_PROXY_STUB_TYPE RETRO_API
#define trace_plugin(__s__) MessageBoxA(nullptr,__s__,"Trace",MB_OK)
#else
#define LIBRETRO_PROXY_STUB_TYPE
#define trace_plugin(__s__)  printf("RAW_TRACE:%s\n",__s__)
#endif
#else
#define LIBRETRO_PROXY_STUB_TYPE
#define trace_plugin(__s__) printf("RAW_TRACE:%s\n",__s__)
#endif
}

//Private bridge module contexts
struct surface_dims_t {
    int32_t base_w, base_h;
    int32_t max_w, max_h;
    float aspect;
};

struct hw_libretro_accel_ctx_t {
    const char* alias;
    retro_hw_context_type val;
};

static hw_libretro_accel_ctx_t k_hw_accel_contexts[] = {
    {"HW_CTX_GL",RETRO_HW_CONTEXT_OPENGL},
    {"HW_CTX_GL_CORE",RETRO_HW_CONTEXT_OPENGL_CORE},
    {"HW_CTX_GL_ES2",RETRO_HW_CONTEXT_OPENGLES2},
    {"HW_CTX_GL_ES3",RETRO_HW_CONTEXT_OPENGLES3},
    {"HW_CTX_GL_ES",RETRO_HW_CONTEXT_OPENGLES_VERSION},
    {"HW_CTX_D3D",RETRO_HW_CONTEXT_DIRECT3D},
    {"HW_CTX_GL_FORCE_ANY",RETRO_HW_CONTEXT_DUMMY}
};

static const size_t k_libretro_hw_accellerated_count = sizeof(k_hw_accel_contexts) / sizeof(k_hw_accel_contexts[0]);

static const struct retro_controller_description k_ctl_ports_default[] =
{
      { "Controller",	RETRO_DEVICE_JOYPAD },
      { "Keyboard",		RETRO_DEVICE_KEYBOARD },
      { "Mouse",		RETRO_DEVICE_MOUSE },
      { "Light Gun",	RETRO_DEVICE_LIGHTGUN },
      { nullptr },
};

static const size_t k_ctl_port_count = (sizeof(k_ctl_ports_default) / sizeof(k_ctl_ports_default[0])) - 1U; // Minus the null

static const struct retro_controller_info k_static_ctl_ports[] = {  
        { k_ctl_ports_default,  k_ctl_port_count },
        { k_ctl_ports_default,  k_ctl_port_count },
        { k_ctl_ports_default,  k_ctl_port_count },
        { k_ctl_ports_default,  k_ctl_port_count },
        { nullptr },
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
static struct retro_hw_render_callback hw_render;

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
    extern int screen_width;
    extern int screen_height;
static bool retro_init_gl_hw_ctx();
LIBRETRO_PROXY_STUB_TYPE void  retro_init(void) {
    trace_plugin("retro_init");

        screen_width = 640;
        screen_height = 480;
    if (!g_b_init_done) {

      //  g_b_init_done = true;
    }

#ifdef BUILD_RETROARCH_CORE
    //retro_reicast_entry_point();
#endif
}

LIBRETRO_PROXY_STUB_TYPE void  retro_deinit(void) {
    trace_plugin("retro_deinit");

    retro_unload_game();

    if (g_b_init_done) {
        // TODO
        g_b_init_done = false;
    }
}

LIBRETRO_PROXY_STUB_TYPE unsigned  retro_api_version(void) {
    trace_plugin("retro_api_version");
    return RETRO_API_VERSION;
}

LIBRETRO_PROXY_STUB_TYPE void  retro_set_controller_port_device(unsigned port, unsigned device) {
    trace_plugin("retro_set_controller_port_device");
}

LIBRETRO_PROXY_STUB_TYPE void  retro_get_system_info(struct retro_system_info* info) {
    trace_plugin("retro_get_system_info");

    memset(info, 0, sizeof(*info));
    info->library_name = "Reicast";
    info->library_version = "1.0.0.1";
    info->need_fullpath = true;
    info->block_extract = true;
    info->valid_extensions =   "bin|gdi|chd|cue|cdi";
}

LIBRETRO_PROXY_STUB_TYPE void  retro_get_system_av_info(struct retro_system_av_info* info) {
    trace_plugin("retro_get_system_av_info");
 

    info->timing.fps = 60; 
    info->timing.sample_rate = 0;

    info->geometry.base_height = screen_width,
        info->geometry.base_height = screen_height,
        info->geometry.max_width = screen_width,
        info->geometry.max_height = screen_height,
        info->geometry.aspect_ratio =  4.0 / 3.0;
     
}

LIBRETRO_PROXY_STUB_TYPE void  retro_set_environment(retro_environment_t cb) {
    trace_plugin("retro_set_environment");

    environ_cb = cb;

    if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging))
        log_cb = logging.log;
    else
        log_cb = __emu_log_provider;

   // retro_load_game(0);
   // uint32_t pref_hw_rend;
   // if (!environ_cb(RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER, &pref_hw_rend))
     //   pref_hw_rend = (uint32_t)-1U;

   // trace_plugin(std::string("ENV = " + std::to_string(pref_hw_rend)).c_str());
    cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)k_static_ctl_ports);

    bool no_rom = false;
    cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_rom);

}

LIBRETRO_PROXY_STUB_TYPE void  retro_set_audio_sample(retro_audio_sample_t cb) {
    trace_plugin("retro_set_audio_sample");
    audio_cb = cb;
}

LIBRETRO_PROXY_STUB_TYPE void  retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) {
    trace_plugin("retro_set_audio_sample_batch");
    audio_batch_cb = cb;
}

LIBRETRO_PROXY_STUB_TYPE void  retro_set_input_poll(retro_input_poll_t cb) {
    trace_plugin("retro_set_input_poll");   
    input_poll_cb = cb;
}

LIBRETRO_PROXY_STUB_TYPE void  retro_set_input_state(retro_input_state_t cb) {
    trace_plugin("retro_set_input_state");
    input_state_cb = cb;
}

LIBRETRO_PROXY_STUB_TYPE void  retro_set_video_refresh(retro_video_refresh_t cb) {
    trace_plugin("retro_set_video_refresh");
    video_cb = cb;
}

LIBRETRO_PROXY_STUB_TYPE void  retro_reset(void) {
    trace_plugin("retro_reset");
}

bool os_gl_init(void* hwnd, void* hdc)
{
    return true;
}

#include "gui/gui.h"
#include "gui/gui_renderer.h"

LIBRETRO_PROXY_STUB_TYPE void  retro_run(void) {
     trace_plugin("retro_run");

   //glBindFramebuffer(RARCH_GL_FRAMEBUFFER, hw_render.get_current_framebuffer());
         glBindFramebuffer(RARCH_GL_FRAMEBUFFER, hw_render.get_current_framebuffer());

   glClearColor(0.3, 0.4, 0.5, 1.0);
   glViewport(0, 0, screen_width, screen_height);
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

   /*
    glViewport(0, 0, 1024, 768);

    glClearColor(1, 0, 0, 1.0);
     
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);*/

    g_GUIRenderer->UIFrame();

/*
    glViewport(0, 0, 1024, 768);

    glClearColor(1, 0, 0, 1.0);
     
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   printf("gl err = %d\n",glGetError());*/
   //RETRO_HW_FRAME_BUFFER_VALID
      video_cb(RETRO_HW_FRAME_BUFFER_VALID, screen_width, screen_height, 0);

}


static void context_reset(void)
{
    trace_plugin("GOT Context");

    extern int gl3wInit2(GL3WGetProcAddressProc proc);
    int r = gl3wInit2(hw_render.get_proc_address);
    printf("RES = %d\n",r);
//   rglgen_resolve_symbols(hw_render.get_proc_address);


    g_GUIRenderer.reset(GUIRenderer::Create(g_GUI.get()));
    g_GUIRenderer->CreateContext();
    g_GUIRenderer->Start();

}

static void context_destroy(void)
{
    trace_plugin("DROPPED Context");
}

static bool retro_init_gl_hw_ctx()
{
    trace_plugin("init GL CTX???");

    for (size_t i = 0; i < k_libretro_hw_accellerated_count; ++i) {
        hw_render.context_type = k_hw_accel_contexts[i].val;// RETRO_HW_CONTEXT_OPENGLES2; //force gles2 for now

 
        hw_render.context_reset = context_reset;
        hw_render.context_destroy = context_destroy;
        hw_render.depth = true;
        hw_render.bottom_left_origin = true;
        hw_render.depth = true;
        hw_render.stencil = true;
        hw_render.bottom_left_origin = true;
        if (environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render)) {
            trace_plugin("SET HW RENDER==");
            trace_plugin(k_hw_accel_contexts[i].alias);
            return true;
        }
     }
    return false;
}

#include <libswirl/gui/gui.h>
LIBRETRO_PROXY_STUB_TYPE bool  retro_load_game(const struct retro_game_info* game) {
    trace_plugin("retro_load_game");

   // uint32_t pref_hw_rend;
  //  if (!environ_cb(RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER, &pref_hw_rend))
      //  pref_hw_rend = (uint32_t)-1U;

    //trace_plugin(std::string("ENV = " + std::to_string(pref_hw_rend)).c_str());
  // bool no_rom = true;
   // environ_cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_rom);
    /*
    struct retro_variable variables[] = {
   {
      "testgl_resolution",
#ifdef HAVE_OPENGLES
         "Internal resolution; 320x240|360x480|480x272|512x384|512x512|640x240|640x448|640x480|720x576|800x600|960x720|1024x768",
#else
         "Internal resolution; 320x240|360x480|480x272|512x384|512x512|640x240|640x448|640x480|720x576|800x600|960x720|1024x768|1024x1024|1280x720|1280x960|1600x1200|1920x1080|1920x1440|1920x1600|2048x2048",
#endif
      },
#ifdef CORE
      { "testgl_multisample", "Multisampling; 1x|2x|4x" },
#endif
      { NULL, NULL },
    };
    environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);*/
    enum retro_pixel_format fmt = retro_pixel_format::RETRO_PIXEL_FORMAT_XRGB8888;
    if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
    {

        trace_plugin("XRGB8888 is not supported.\n");
        //return  ;
    } else trace_plugin("XRGB8888 is supported.\n");

    //TODO
    if (retro_init_gl_hw_ctx()) {
        trace_plugin("SUCCEED retro_init_gl_hw_ctx");
	}
	else trace_plugin("FAILED retro_init_gl_hw_ctx");



    extern int libretro_prologue(int argc, wchar* argv[]) ;
    wchar* e[] = {"",""};
    libretro_prologue(0,e);
 
  //  extern bool gui_start_game(const std::string& path);
//g_GUI->gui_start_game("/home/div22/reicast-roms/ika.chd");
    return true;
}

LIBRETRO_PROXY_STUB_TYPE void  retro_unload_game(void) {
    trace_plugin("retro_unload_game");
}

LIBRETRO_PROXY_STUB_TYPE unsigned  retro_get_region(void) {
    trace_plugin("retro_get_region");
    return RETRO_REGION_PAL;
}

LIBRETRO_PROXY_STUB_TYPE bool  retro_load_game_special(unsigned type, const struct retro_game_info* info, size_t num) {
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

LIBRETRO_PROXY_STUB_TYPE size_t  retro_serialize_size(void) {
    trace_plugin("retro_serialize_size");
    return false;
}

LIBRETRO_PROXY_STUB_TYPE bool  retro_serialize(void* data_, size_t size) {
    trace_plugin("retro_serialize");
    return true;
}

LIBRETRO_PROXY_STUB_TYPE bool   retro_unserialize(const void* data_, size_t size) {
    trace_plugin("retro_unserialize");
    return true;
}


LIBRETRO_PROXY_STUB_TYPE void*  retro_get_memory_data(unsigned id) {
    trace_plugin("retro_get_memory_data");
    return NULL;
}

LIBRETRO_PROXY_STUB_TYPE size_t  retro_get_memory_size(unsigned id) {
    trace_plugin("retro_get_memory_size");
    return 0;
}

LIBRETRO_PROXY_STUB_TYPE void  retro_cheat_reset(void) {
    trace_plugin("retro_cheat_reset");
}

LIBRETRO_PROXY_STUB_TYPE void  retro_cheat_set(unsigned index, bool enabled, const char* code) {
    trace_plugin("retro_cheat_set");
}
 #define trace_plugin(__s__)  printf("RAW_TRACE:%s\n",__s__)
