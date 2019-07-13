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

	#include "gl32defs.h"

#elif HOST_OS == OS_DARWIN
	#include <OpenGL/gl3.h>
#else
	#include "gl3wdefs.h"
#endif
