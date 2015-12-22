#pragma once
#include "hw/pvr/ta_ctx.h"
#include "hw/pvr/Renderer_if.h"

#if defined(GL) || defined(GLES)
#include <glsym/glsym.h>

#ifndef SGL_CAP_MAX
#define SGL_CAP_MAX 8
#endif

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
   GLenum cullmode;
   GLuint framebuf;
   GLuint program; 
   int cap_state[SGL_CAP_MAX];
   int cap_translate[SGL_CAP_MAX];
};

extern gl_cached_state gl_state;
#endif
