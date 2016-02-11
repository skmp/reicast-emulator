#ifndef LIBRETRO_SDK_GLSM_SYM_H
#define LIBRETRO_SDK_GLSM_SYM_H

#include "glsm.h"

#ifdef __cplusplus
extern "C" {
#endif

#define glDisable(T) rglDisable(S##T)
#define glEnable(T)  rglEnable(S##T)
#define glUseProgram rglUseProgram
#define glDepthMask  rglDepthMask

void rglDisable(GLenum cap);
void rglEnable(GLenum cap);
void rglUseProgram(GLuint program);
void rglDepthMask(GLboolean flag);

#ifdef __cplusplus
}
#endif

#endif
