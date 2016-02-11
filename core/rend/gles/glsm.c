#include <glsym/glsym.h>
#include "glsm.h"

struct retro_hw_render_callback hw_render;
static int glsm_stop;

/* GL wrapper-side */

void rglDisable(GLenum cap)
{
   glDisable(gl_state.cap_translate[cap]);
   gl_state.cap_state[cap] = 0;
}

void rglEnable(GLenum cap)
{
   glEnable(gl_state.cap_translate[cap]);
   gl_state.cap_state[cap] = 1;
}

void rglUseProgram(GLuint program)
{
   gl_state.program = program;
   glUseProgram(program);
}

void rglDepthMask(GLboolean flag)
{
   gl_state.depthmask = flag;
   glDepthMask(flag);
}

void rglStencilMask(GLenum mask)
{
   gl_state.stencilmask = mask;
   glStencilMask(mask);
}

void rglBufferData(GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage)
{
   glBufferData(target, size, data, usage);
}

void rglBindBuffer(GLenum target, GLuint buffer)
{
   glBindBuffer(target, buffer);
}

void rglLinkProgram(GLuint program)
{
   glLinkProgram(program);
}

void rglFramebufferTexture2D(GLenum target, GLenum attachment,
      GLenum textarget, GLuint texture, GLint level)
{
   glFramebufferTexture2D(target, attachment, textarget, texture, level);
}

void rglFramebufferRenderbuffer(GLenum target, GLenum attachment,
      GLenum renderbuffertarget, GLuint renderbuffer)
{
   glFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);
}

void rglDeleteFramebuffers(GLsizei n, GLuint *framebuffers)
{
   glDeleteFramebuffers(n, framebuffers);
}

void rglRenderbufferStorage(GLenum target, GLenum internalFormat,
      GLsizei width, GLsizei height)
{
   glRenderbufferStorage(target, internalFormat, width, height);
}

void rglBindRenderbuffer(GLenum target, GLuint renderbuffer)
{
   glBindRenderbuffer(target, renderbuffer);
}

void rglDeleteRenderbuffers(GLsizei n, GLuint *renderbuffers)
{
   glDeleteRenderbuffers(n, renderbuffers);
}

void rglGenRenderbuffers(GLsizei n, GLuint *renderbuffers)
{
   glGenRenderbuffers(n, renderbuffers);
}

void rglGenFramebuffers(GLsizei n, GLuint *ids)
{
   glGenFramebuffers(n, ids);
}

void rglBindFramebuffer(GLenum target, GLuint framebuffer)
{
   if (!glsm_stop)
      glBindFramebuffer(GL_FRAMEBUFFER, framebuffer ? framebuffer : hw_render.get_current_framebuffer());
}

void rglGenerateMipmap(GLenum target)
{
   glGenerateMipmap(target);
}

GLenum rglCheckFramebufferStatus(GLenum target)
{
   return glCheckFramebufferStatus(target);
}

void rglBindFragDataLocation(GLuint program, GLuint colorNumber,
                                   const char * name)
{
   glBindFragDataLocation(program, colorNumber, name);
}

void rglBindAttribLocation(GLuint program, GLuint index, const GLchar *name)
{
   glBindAttribLocation(program, index, name);
}

void rglGetProgramiv(GLuint shader, GLenum pname, GLint *params)
{
   glGetProgramiv(shader, pname, params);
}

void rglGetShaderiv(GLuint shader, GLenum pname, GLint *params)
{
   glGetShaderiv(shader, pname, params);
}

void rglAttachShader(GLuint program, GLuint shader)
{
   glAttachShader(program, shader);
}

void rglShaderSource(GLuint shader, GLsizei count,
      const GLchar **string, const GLint *length)
{
   return glShaderSource(shader, count, string, length);
}

void rglCompileShader(GLuint shader)
{
   glCompileShader(shader);
}

GLuint rglCreateProgram(void)
{
   return glCreateProgram();
}

void rglGetShaderInfoLog(GLuint shader, GLsizei maxLength,
      GLsizei *length, GLchar *infoLog)
{
   glGetShaderInfoLog(shader, maxLength, length, infoLog);
}

void rglGetProgramInfoLog(GLuint shader, GLsizei maxLength,
      GLsizei *length, GLchar *infoLog)
{
   glGetProgramInfoLog(shader, maxLength, length, infoLog);
}

GLboolean rglIsProgram(GLuint program)
{
   glIsProgram(program);
}

void rglEnableVertexAttribArray(GLuint index)
{
   gl_state.vertex_attrib_pointer.enabled[index] = 1;
   glEnableVertexAttribArray(index);
}

void rglDisableVertexAttribArray(GLuint index)
{
   gl_state.vertex_attrib_pointer.enabled[index] = 0;
   glDisableVertexAttribArray(index);
}

void rglVertexAttribPointer(GLuint name, GLint size,
      GLenum type, GLboolean normalized, GLsizei stride,
      const GLvoid* pointer)
{
   glVertexAttribPointer(name, size, type, normalized, stride, pointer);
}

GLuint rglCreateShader(GLenum shaderType)
{
   return glCreateShader(shaderType);
}

void rglDeleteShader(GLuint shader)
{
   glDeleteShader(shader);
}

GLint rglGetUniformLocation(GLuint program, const GLchar *name)
{
   return glGetUniformLocation(program, name);
}

void rglGenBuffers(GLsizei n, GLuint *buffers)
{
   glGenBuffers(n, buffers);
}

void rglUniform1f(GLint location, GLfloat v0)
{
   glUniform1f(location, v0);
}

void rglUniform1i(GLint location, GLint v0)
{
   glUniform1i(location, v0);
}

void rglUniform2fv(GLint location, GLsizei count, const GLfloat *value)
{
   glUniform2fv(location, count, value);
}

void rglUniform3fv(GLint location, GLsizei count, const GLfloat *value)
{
   glUniform3fv(location, count, value);
}

void rglUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3)
{
   glUniform4f(location, v0, v1, v2, v3);
}

void rglUniform4fv(GLint location, GLsizei count, const GLfloat *value)
{
   glUniform4fv(location, count, value);
}

/* GLSM-side */

static void glsm_state_setup(void)
{
   unsigned i;

   glsm_stop = 0;
   
   gl_state.cap_translate[SGL_DEPTH_TEST]           = GL_DEPTH_TEST;
   gl_state.cap_translate[SGL_BLEND]                = GL_BLEND;
   gl_state.cap_translate[SGL_POLYGON_OFFSET_FILL]  = GL_POLYGON_OFFSET_FILL;
   gl_state.cap_translate[SGL_FOG]                  = GL_FOG;
   gl_state.cap_translate[SGL_CULL_FACE]            = GL_CULL_FACE;
   gl_state.cap_translate[SGL_ALPHA_TEST]           = GL_ALPHA_TEST;
   gl_state.cap_translate[SGL_SCISSOR_TEST]         = GL_SCISSOR_TEST;
   gl_state.cap_translate[SGL_STENCIL_TEST]         = GL_STENCIL_TEST;

   for (i = 0; i < MAX_ATTRIB; i++)
      gl_state.vertex_attrib_pointer.enabled[i] = 0;

   gl_state.framebuf                    = hw_render.get_current_framebuffer();
   gl_state.cullmode                    = GL_BACK;
   gl_state.frontface.mode              = GL_CCW; 

   gl_state.blendfunc_separate.used     = false;
   gl_state.blendfunc_separate.srcRGB   = GL_ONE;
   gl_state.blendfunc_separate.dstRGB   = GL_ZERO;
   gl_state.blendfunc_separate.srcAlpha = GL_ONE;
   gl_state.blendfunc_separate.dstAlpha = GL_ZERO;

   gl_state.depthfunc.used              = false;
   
   gl_state.colormask.used              = false;
   gl_state.colormask.red               = GL_TRUE;
   gl_state.colormask.green             = GL_TRUE;
   gl_state.colormask.blue              = GL_TRUE;
   gl_state.colormask.alpha             = GL_TRUE;

   gl_state.polygonoffset.used          = false;

   gl_state.depthfunc.func              = GL_LESS;

#ifdef CORE
   glGenVertexArrays(1, &gl_state.vao);
#endif
}

static void glsm_state_bind(void)
{
   unsigned i;

   for (i = 0; i < MAX_ATTRIB; i++)
   {
      if (gl_state.vertex_attrib_pointer.enabled[i])
         glEnableVertexAttribArray(i);
      else
         glDisableVertexAttribArray(i);
   }

   glBindFramebuffer(
         RARCH_GL_FRAMEBUFFER,
         hw_render.get_current_framebuffer());
   glBlendFunc(
         gl_state.blendfunc.sfactor,
         gl_state.blendfunc.dfactor);
   if (gl_state.blendfunc_separate.used)
      glBlendFuncSeparate(
            gl_state.blendfunc_separate.srcRGB,
            gl_state.blendfunc_separate.dstRGB,
            gl_state.blendfunc_separate.srcAlpha,
            gl_state.blendfunc_separate.dstAlpha
            );
   glClearColor(
         gl_state.clear_color.r,
         gl_state.clear_color.g,
         gl_state.clear_color.b,
         gl_state.clear_color.a);
   if (gl_state.depthfunc.used)
      glDepthFunc(gl_state.depthfunc.func);
   if (gl_state.colormask.used)
      glColorMask(
            gl_state.colormask.red,
            gl_state.colormask.green,
            gl_state.colormask.blue,
            gl_state.colormask.alpha);
   glCullFace(gl_state.cullmode);
   glDepthMask(gl_state.depthmask);
   if (gl_state.polygonoffset.used)
      glPolygonOffset(
            gl_state.polygonoffset.factor,
            gl_state.polygonoffset.units);
   glScissor(
         gl_state.scissor.x,
         gl_state.scissor.y,
         gl_state.scissor.w,
         gl_state.scissor.h);
   glUseProgram(gl_state.program);
   glViewport(
         gl_state.viewport.x,
         gl_state.viewport.y,
         gl_state.viewport.w,
         gl_state.viewport.h);
#ifdef CORE
   glBindVertexArray(gl_state.vao);
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
   glStencilFunc(
         gl_state.stencilfunc.func,
         gl_state.stencilfunc.ref,
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
   if (gl_state.colormask.used)
      glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
   if (gl_state.blendfunc_separate.used)
      glBlendFuncSeparate(GL_ONE, GL_ZERO, GL_ONE, GL_ZERO);
   glCullFace(GL_BACK);
   glDepthMask(GL_TRUE);
   if (gl_state.polygonoffset.used)
      glPolygonOffset(0, 0);
   glUseProgram(0);
   glClearColor(0,0,0,0.0f);
   glStencilMask(1);
   glFrontFace(GL_CCW);
   if (gl_state.depthfunc.used)
      glDepthFunc(GL_LESS);
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
   hw_render.context_type       = RETRO_HW_CONTEXT_OPENGLES_VERSION;
   hw_render.version_major      = 3;
   hw_render.version_minor      = 1;
#elif defined(GLES3)
   hw_render.context_type       = RETRO_HW_CONTEXT_OPENGLES3;
#else
   hw_render.context_type       = RETRO_HW_CONTEXT_OPENGLES2;
#endif
#else
#ifdef CORE
   hw_render.context_type       = RETRO_HW_CONTEXT_OPENGL_CORE;
   hw_render.version_major      = 3;
   hw_render.version_minor      = 1;
#else
   hw_render.context_type       = RETRO_HW_CONTEXT_OPENGL;
#endif
#endif
   hw_render.context_reset      = params->context_reset;
   hw_render.context_destroy    = params->context_destroy;
   hw_render.stencil            = params->stencil;
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
