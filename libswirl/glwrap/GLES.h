#pragma once

// Either Desktop 3+ or GLES2/GLES3

#include "types.h"


#ifdef GLES
#if defined(TARGET_IPHONE) //apple-specific ogles2 headers
//#include <APPLE/egl.h>
#include <OpenGLES/ES2/gl.h>
#include <OpenGLES/ES2/glext.h>
#endif
#include "khronos/GLES32/gl32.h"
#include "khronos/GLES32/gl2ext.h"
#ifndef GLES2
#include "gl32funcs.h"
#endif

#ifndef GL_NV_draw_path
//IMGTEC GLES emulation
#pragma comment(lib,"libEGL.lib")
#pragma comment(lib,"libGLESv2.lib")
#else /* NV gles emulation*/
#pragma comment(lib,"libGLES20.lib")
#endif

#elif HOST_OS == OS_DARWIN
#include <OpenGL/gl3.h>
#else
#include "gl3w.h"
#endif
