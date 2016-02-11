#ifndef LIBRETRO_SDK_GLSM_H
#define LIBRETRO_SDK_GLSM_H

#include <boolean.h>
#include <libretro.h>
#include <glsym/rglgen_headers.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(HAVE_OPENGLES2)
#define RARCH_GL_RENDERBUFFER GL_RENDERBUFFER
#define RARCH_GL_DEPTH24_STENCIL8 GL_DEPTH24_STENCIL8_OES
#define RARCH_GL_DEPTH_ATTACHMENT GL_DEPTH_ATTACHMENT
#define RARCH_GL_STENCIL_ATTACHMENT GL_STENCIL_ATTACHMENT
#elif defined(OSX_PPC)
#define RARCH_GL_RENDERBUFFER GL_RENDERBUFFER_EXT
#define RARCH_GL_DEPTH24_STENCIL8 GL_DEPTH24_STENCIL8_EXT
#define RARCH_GL_DEPTH_ATTACHMENT GL_DEPTH_ATTACHMENT_EXT
#define RARCH_GL_STENCIL_ATTACHMENT GL_STENCIL_ATTACHMENT_EXT
#elif defined(HAVE_PSGL)
#define RARCH_GL_RENDERBUFFER GL_RENDERBUFFER_OES
#define RARCH_GL_DEPTH24_STENCIL8 GL_DEPTH24_STENCIL8_SCE
#define RARCH_GL_DEPTH_ATTACHMENT GL_DEPTH_ATTACHMENT_OES
#define RARCH_GL_STENCIL_ATTACHMENT GL_STENCIL_ATTACHMENT_OES
#else
#define RARCH_GL_RENDERBUFFER GL_RENDERBUFFER
#define RARCH_GL_DEPTH24_STENCIL8 GL_DEPTH24_STENCIL8
#define RARCH_GL_DEPTH_ATTACHMENT GL_DEPTH_ATTACHMENT
#define RARCH_GL_STENCIL_ATTACHMENT GL_STENCIL_ATTACHMENT
#endif

#if defined(HAVE_PSGL)
#define RARCH_GL_FRAMEBUFFER GL_FRAMEBUFFER_OES
#define RARCH_GL_FRAMEBUFFER_COMPLETE GL_FRAMEBUFFER_COMPLETE_OES
#define RARCH_GL_COLOR_ATTACHMENT0 GL_COLOR_ATTACHMENT0_EXT
#elif defined(OSX_PPC)
#define RARCH_GL_FRAMEBUFFER GL_FRAMEBUFFER_EXT
#define RARCH_GL_FRAMEBUFFER_COMPLETE GL_FRAMEBUFFER_COMPLETE_EXT
#define RARCH_GL_COLOR_ATTACHMENT0 GL_COLOR_ATTACHMENT0_EXT
#else
#define RARCH_GL_FRAMEBUFFER GL_FRAMEBUFFER
#define RARCH_GL_FRAMEBUFFER_COMPLETE GL_FRAMEBUFFER_COMPLETE
#define RARCH_GL_COLOR_ATTACHMENT0 GL_COLOR_ATTACHMENT0
#endif

#define MAX_ATTRIB 8
#define MAX_TEXTURE 32

enum
{
   SGL_DEPTH_TEST             = 0,
   SGL_BLEND                  = 1,
   SGL_POLYGON_OFFSET_FILL    = 2,
   SGL_FOG                    = 3,
   SGL_CULL_FACE              = 4,
   SGL_ALPHA_TEST             = 5,
   SGL_SCISSOR_TEST           = 6,
   SGL_STENCIL_TEST           = 7,
   SGL_CAP_MAX
};

enum glsm_state_ctl
{
   GLSM_CTL_NONE = 0,
   GLSM_CTL_STATE_SETUP,
   GLSM_CTL_STATE_BIND,
   GLSM_CTL_STATE_UNBIND,
   GLSM_CTL_STATE_CONTEXT_RESET,
   GLSM_CTL_STATE_CONTEXT_INIT
};

typedef bool (*glsm_imm_vbo_draw)(void *);
typedef bool (*glsm_imm_vbo_disable)(void *);

typedef struct glsm_ctx_params
{
   glsm_imm_vbo_draw        imm_vbo_draw;
   glsm_imm_vbo_disable     imm_vbo_disable;
   retro_hw_context_reset_t context_reset;
   retro_hw_context_reset_t context_destroy;
   retro_environment_t environ_cb;
   bool stencil;
   unsigned major;
   unsigned minor;
} glsm_ctx_params_t;

bool glsm_ctl(enum glsm_state_ctl state, void *data);

#ifdef __cplusplus
}
#endif

#endif
