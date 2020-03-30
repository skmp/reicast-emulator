#include "rend/gles/glcache.h"

#include "libretro-common/include/libretro.h"
#include "libretro_proxy.h"
#include "utils/glwrap/gl3w.h"
#include "types.h"
#include "gui/gui.h"
#include "gui/gui_renderer.h"
#include <chrono>
#include "perf_tools/moving_average.hpp"
#include <libswirl/gui/gui.h>
#include <libswirl/libswirl.h>
#include <libswirl/input/gamepad.h>
#include <libswirl/input/gamepad_device.h>
#include <utils/bit_utils.hpp>

using namespace bit_utils;

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
#define trace_plugin(__s__)  //printf("RAW_TRACE:%s\n",__s__)
#endif
#else
#define LIBRETRO_PROXY_STUB_TYPE
#define trace_plugin(__s__) //printf("RAW_TRACE:%s\n",__s__)
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
    { "Keyboard",		RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_KEYBOARD, 0)  },
      { "Controller",	RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 1)  },
      
      { "Mouse",		RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_MOUSE ,2)  },
      { "Light Gun",	RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_LIGHTGUN, 3)  },
      { nullptr },
};

static const size_t k_ctl_port_count = (sizeof(k_ctl_ports_default) / sizeof(k_ctl_ports_default[0])) - 1U; // Minus the null

static const struct retro_controller_info k_static_ctl_ports[] = {  
        { k_ctl_ports_default,  k_ctl_port_count },
        { NULL,0}
       
};
 
 
static uint32_t input_states[k_ctl_port_count] = {0}; 

//Private bridge module variables
static retro_log_printf_t log_cb;
static retro_environment_t environ_cb;
static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
retro_audio_sample_batch_t audio_batch_cb = nullptr;
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

int rfb = 0; //XXX HACK!
extern int screen_width;
extern int screen_height;
static bool retro_init_gl_hw_ctx();
static void update_vars() ;
 


//Private bridge module functions
static void __emu_log_provider(enum retro_log_level level, const char* fmt, ...) {
    (void)level;
    va_list va;
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    va_end(va);
}

LIBRETRO_PROXY_STUB_TYPE void  retro_init(void) {
    trace_plugin("retro_init");

    screen_width = 640  ;
    screen_height = 480 ;  
    if (!g_b_init_done) {
        //TODO
        g_b_init_done = true;
    }
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
    info->library_version = "1.0.0.3";
    info->need_fullpath = true;
    info->block_extract = true;
    info->valid_extensions =   "bin|gdi|chd|cue|cdi";
}

LIBRETRO_PROXY_STUB_TYPE void  retro_get_system_av_info(struct retro_system_av_info* info) {
    trace_plugin("retro_get_system_av_info");
 
    info->timing.fps = 60; 
    info->timing.sample_rate = 44100;

    info->geometry.base_height = screen_width,
    info->geometry.base_height = screen_height,
    info->geometry.max_width = screen_width,
    info->geometry.max_height = screen_height,
    info->geometry.aspect_ratio =   4.0 / 3.0;  //0 == auto
}

LIBRETRO_PROXY_STUB_TYPE void  retro_set_environment(retro_environment_t cb) {
    trace_plugin("retro_set_environment");

    environ_cb = cb;

    if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging))
        log_cb = logging.log;
    else
        log_cb = __emu_log_provider;
 
 
    cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)k_static_ctl_ports);

    bool no_rom = false;
    cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_rom);

       struct retro_variable variables[] = {
      {
         "libreicast_resolution",
#ifdef HAVE_OPENGLES
         "Internal resolution; 320x240|360x480|480x272|512x384|512x512|640x240|640x448|640x480|720x576|800x600|960x720|1024x768",
#else
         "Internal resolution; 640x480|720x576|800x600|960x720|1024x768|1024x1024|1280x720|1280x960|1600x1200|1920x1080|1920x1440|1920x1600|2048x2048",
#endif
      },
#ifdef CORE
      { "libreicast_multisample", "Multisampling; 1x|2x|4x" },
#endif
      { NULL, NULL },
   };
    cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);
}

static void update_vars() {

    bool updated = false;
    environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) ;
    if (  !updated)
        return;

    struct retro_variable var = {
      .key = "libreicast_resolution",
   };

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      char *pch;
      char str[256];
      snprintf(str, sizeof(str), "%s", var.value);
      printf("Val = %s\n",var.value);
      pch = strtok(str, "x");
      if (pch)
         screen_width = strtoul(pch, NULL, 0);
      pch = strtok(NULL, "x");
      if (pch)
         screen_height = strtoul(pch, NULL, 0);

      printf( "Gt size: %u x %u.\n", screen_width, screen_height);
   }
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

LIBRETRO_PROXY_STUB_TYPE void  retro_run(void) {
    trace_plugin("retro_run");
    std::clock_t c_start = std::clock();
    auto t_start = std::chrono::high_resolution_clock::now();
    static moving_average_c<double, 10> cpu_time_avg, time_avg;
    auto t_end = std::chrono::high_resolution_clock::now();
    std::clock_t c_end = std::clock();


    input_poll_cb();

    //RETRO_DEVICE_ID_JOYPAD_UP t

    for (uint32_t i = 0;i < 16;++i) //16 calls is UGLY!!
        input_states[0] |= (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i))<<i;

    extern u16 kcode[4];
    extern u32 vks[4];
    extern s8 joyx[4], joyy[4];
    extern u8 rt[4], lt[4];
 
    kcode[0]  = ~kcode[0];
    bit_set<u16>(kcode[0],(u16)DC_BTN_START, bit_isset((u16)input_states[0],(u16)RETRO_DEVICE_ID_JOYPAD_START));
    bit_set<u16>(kcode[0],(u16)DC_BTN_A, bit_isset((u16)input_states[0],(u16)RETRO_DEVICE_ID_JOYPAD_A));
    bit_set<u16>(kcode[0],(u16)DC_BTN_B , bit_isset((u16)input_states[0],(u16)RETRO_DEVICE_ID_JOYPAD_B ));
    bit_set<u16>(kcode[0],(u16)DC_BTN_X , bit_isset((u16)input_states[0],(u16)RETRO_DEVICE_ID_JOYPAD_X ));
    bit_set<u16>(kcode[0],(u16)DC_BTN_Y , bit_isset((u16)input_states[0],(u16)RETRO_DEVICE_ID_JOYPAD_Y ));

    bit_set<u16>(kcode[0],(u16)DC_DPAD_UP , bit_isset((u16)input_states[0],(u16)RETRO_DEVICE_ID_JOYPAD_UP ));
    bit_set<u16>(kcode[0],(u16)DC_DPAD_LEFT, bit_isset((u16)input_states[0],(u16)RETRO_DEVICE_ID_JOYPAD_LEFT ));
    bit_set<u16>(kcode[0],(u16)DC_DPAD_RIGHT, bit_isset((u16)input_states[0],(u16)RETRO_DEVICE_ID_JOYPAD_RIGHT ));
    bit_set<u16>(kcode[0],(u16)DC_DPAD_DOWN , bit_isset((u16)input_states[0],(u16)RETRO_DEVICE_ID_JOYPAD_DOWN ));
 
    bit_set<u16>(kcode[0],(u16)DC_AXIS_LT, bit_isset((u16)input_states[0],(u16)RETRO_DEVICE_ID_JOYPAD_L ));
    bit_set<u16>(kcode[0],(u16)DC_AXIS_RT , bit_isset((u16)input_states[0],(u16)RETRO_DEVICE_ID_JOYPAD_R ));

    //dump_bits(input_states[0],"ctl0:");
    //dump_bits(kcode[0],"inp0:");
    update_vars() ;

    rfb = hw_render.get_current_framebuffer();
   //glBindFramebuffer(RARCH_GL_FRAMEBUFFER, hw_render.get_current_framebuffer());
    glBindFramebuffer(RARCH_GL_FRAMEBUFFER, rfb);


    glcache.Reset();
    g_GUIRenderer->UIFrame();

    video_cb(RETRO_HW_FRAME_BUFFER_VALID, screen_width, screen_height, 0);

/*

    printf("Frame took : %lf/avg=%lf ms(cpu) or %lf/avg=%lf ms time\n", 1000.0 * (c_end - c_start) / CLOCKS_PER_SEC,
            cpu_time_avg.update(1000.0 * (c_end - c_start) / CLOCKS_PER_SEC),
            std::chrono::duration<double, std::milli>(t_end - t_start).count(),
            time_avg.update(std::chrono::duration<double, std::milli>(t_end - t_start).count()) ); */
 
    c_start = c_end;
    t_start = t_end;

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
    
    wchar* e[] = {"",(char*)game->path};
    libretro_prologue(2,e);
   				struct retro_message msg;
				// Sadly, this callback is only able to display short messages, so we can't give proper explanations...
				msg.msg = "Now starting..";
				msg.frames = 200;
				environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &msg);
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
