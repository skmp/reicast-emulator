#include <glsym/rglgen.h>

#include "glsm.h"

struct retro_hw_render_callback hw_render;

static void glsm_state_setup(void)
{
   gl_state.cap_translate[0] = GL_DEPTH_TEST;
   gl_state.cap_translate[1] = GL_BLEND;
   gl_state.cap_translate[2] = GL_POLYGON_OFFSET_FILL;
   gl_state.cap_translate[3] = GL_FOG;
   gl_state.cap_translate[4] = GL_CULL_FACE;
   gl_state.cap_translate[5] = GL_ALPHA_TEST;
   gl_state.cap_translate[6] = GL_SCISSOR_TEST;
   gl_state.cap_translate[7] = GL_STENCIL_TEST;

   gl_state.framebuf         = hw_render.get_current_framebuffer();
   gl_state.cullmode         = GL_BACK;
   gl_state.frontface.mode   = GL_CCW; 
}

static void glsm_state_bind(void)
{
   unsigned i;

   glBindFramebuffer(RARCH_GL_FRAMEBUFFER, hw_render.get_current_framebuffer());
   glBlendFunc(gl_state.blendfunc.sfactor, gl_state.blendfunc.dfactor);
   glClearColor(gl_state.clear_color.r, gl_state.clear_color.g, gl_state.clear_color.b, gl_state.clear_color.a);
   glCullFace(gl_state.cullmode);
   glDepthMask(gl_state.depthmask);
   glScissor(gl_state.scissor.x, gl_state.scissor.y, gl_state.scissor.w, gl_state.scissor.h);
   glUseProgram(gl_state.program);
   glViewport(gl_state.viewport.x, gl_state.viewport.y, gl_state.viewport.w, gl_state.viewport.h);
#ifdef CORE
   glBindVertexArray(vbo.vao);
#endif
   for(i = 0; i < SGL_CAP_MAX; i ++)
   {
      if (gl_state.cap_state[i])
         glEnable(gl_state.cap_translate[i]);
      else
         glDisable(gl_state.cap_translate[i]);
   }
   glFrontFace(gl_state.frontface.mode);
   glStencilMask(gl_state.stencilmask);
   glStencilOp(gl_state.stencilop.sfail,
         gl_state.stencilop.dpfail,
         gl_state.stencilop.dppass);
   glStencilFunc(gl_state.stencilfunc.func,gl_state.stencilfunc.ref,
         gl_state.stencilfunc.mask);
   glActiveTexture(gl_state.active_texture);
   glBindBuffer(GL_ARRAY_BUFFER, 0);
}

static void glsm_state_unbind(void)
{
   unsigned i;
#ifdef CORE
   glBindVertexArray(0);
#endif
   for (i = 0; i < SGL_CAP_MAX; i ++)
      glDisable(gl_state.cap_translate[i]);
   glDisable(GL_BLEND);
   glDisable(GL_CULL_FACE);
   glDisable(GL_SCISSOR_TEST);
   glDisable(GL_DEPTH_TEST);
   glBlendFunc(GL_ONE, GL_ZERO);
   glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
   glCullFace(GL_BACK);
   glDepthMask(GL_TRUE);
   glUseProgram(0);
   glClearColor(0,0,0,0.0f);
   glStencilMask(1);
   glFrontFace(GL_CCW);
   glStencilOp(GL_KEEP,GL_KEEP, GL_KEEP);
   glStencilFunc(GL_ALWAYS,0,1);

   /* Clear textures */
   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, 0);

   glBindFramebuffer(RARCH_GL_FRAMEBUFFER, 0);
}

static bool glsm_state_ctx_init(void *data)
{
   glsm_ctx_params_t *params = (glsm_ctx_params_t*)data;

   if (!params || !params->environ_cb)
      return false;

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
   hw_render.context_reset      = params->context_reset;
   hw_render.context_destroy    = params->context_destroy;
   hw_render.stencil            = true;
   hw_render.depth              = true;
   hw_render.bottom_left_origin = true;

   if (!params->environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render))
      return false;

   return true;
}

bool glsm_ctl(enum glsm_state_ctl state, void *data)
{
   switch (state)
   {
      case GLSM_CTL_STATE_CONTEXT_RESET:
         rglgen_resolve_symbols(hw_render.get_proc_address);
         break;
      case GLSM_CTL_STATE_CONTEXT_INIT:
         return glsm_state_ctx_init(data);
      case GLSM_CTL_STATE_SETUP:
         glsm_state_setup();
         break;
      case GLSM_CTL_STATE_UNBIND:
         glsm_state_unbind();
         break;
      case GLSM_CTL_STATE_BIND:
         glsm_state_bind();
         break;
      case GLSM_CTL_NONE:
      default:
         break;
   }

   return true;
}
