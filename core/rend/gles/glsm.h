#ifndef LIBRETRO_SDK_GLSM_H
#define LIBRETRO_SDK_GLSM_H

#include <glsym/glsym.h>
#include <boolean.h>
#include <libretro.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SGL_CAP_MAX
#define SGL_CAP_MAX 8
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

enum glsm_state_ctl
{
   GLSM_CTL_NONE = 0,
   GLSM_CTL_STATE_SETUP,
   GLSM_CTL_STATE_BIND,
   GLSM_CTL_STATE_UNBIND,
   GLSM_CTL_STATE_CONTEXT_RESET,
   GLSM_CTL_STATE_CONTEXT_INIT
};

struct gl_cached_state
{
   struct
   {
      GLuint r;
      GLuint g;
      GLuint b;
      GLuint a;
   } clear_color;
   struct
   {
      GLint x;
      GLint y;
      GLsizei w;
      GLsizei h;
   } scissor;
   struct
   {
      GLint x;
      GLint y;
      GLsizei w;
      GLsizei h;
   } viewport;
   struct
   {
      GLenum sfactor;
      GLenum dfactor;
   } blendfunc;
   struct
   {
      bool used;
      GLenum srcRGB;
      GLenum dstRGB;
      GLenum srcAlpha;
      GLenum dstAlpha;
   } blendfunc_separate;
   struct
   {
      bool used;
      GLboolean red;
      GLboolean green;
      GLboolean blue;
      GLboolean alpha;
   } colormask;
   struct
   {
      bool used;
      GLenum func;
   } depthfunc;
   struct
   {
      bool used;
      GLfloat factor;
      GLfloat units;
   } polygonoffset;
   struct
   {
      GLenum func;
      GLint ref;
      GLuint mask;
   } stencilfunc;
   struct
   {
      GLenum sfail;
      GLenum dpfail;
      GLenum dppass;
   } stencilop;
   struct
   {
      GLenum mode;
   } frontface;
   GLuint stencilmask;
   GLenum cullmode;
   GLuint framebuf;
   GLuint program; 
   GLboolean depthmask;
   GLenum active_texture;
   int cap_state[SGL_CAP_MAX];
   int cap_translate[SGL_CAP_MAX];
};

typedef struct glsm_ctx_params
{
   retro_hw_context_reset_t context_reset;
   retro_hw_context_reset_t context_destroy;
   retro_environment_t environ_cb;
   bool stencil;
   unsigned major;
   unsigned minor;
} glsm_ctx_params_t;

extern struct gl_cached_state gl_state;

bool glsm_ctl(enum glsm_state_ctl state, void *data);

#ifdef __cplusplus
}
#endif

#endif
