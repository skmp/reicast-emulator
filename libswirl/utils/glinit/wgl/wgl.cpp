/*
	This file is part of libswirl
*/
#include "license/bsd"


#define NOMINMAX 1

#include <windows.h>
#include <windowsx.h>

#include "rend/TexCache.h"

#include "oslib/oslib.h"
#include "oslib/audiostream.h"
#include "stdclass.h"
#include "cfg/cfg.h"

#include "hw/maple/maple_cfg.h"

#include <process.h>

#include "hw/sh4/dyna/ngen.h"
#include "hw/sh4/dyna/blockmanager.h"
#include "hw/mem/_vmem.h"

#include "hw/pvr/Renderer_if.h"

#include "utils/glwrap/gl3w.h"


#include "libswirl.h"
#include "hw/pvr/Renderer_if.h"

#include <wingdi.h>

#undef ARRAY_SIZE	// macros are evil


#define WGL_DRAW_TO_WINDOW_ARB         0x2001
#define WGL_ACCELERATION_ARB           0x2003
#define WGL_SWAP_METHOD_ARB            0x2007
#define WGL_SUPPORT_OPENGL_ARB         0x2010
#define WGL_DOUBLE_BUFFER_ARB          0x2011
#define WGL_PIXEL_TYPE_ARB             0x2013
#define WGL_COLOR_BITS_ARB             0x2014
#define WGL_DEPTH_BITS_ARB             0x2022
#define WGL_STENCIL_BITS_ARB           0x2023
#define WGL_FULL_ACCELERATION_ARB      0x2027
#define WGL_SWAP_EXCHANGE_ARB          0x2028
#define WGL_TYPE_RGBA_ARB              0x202B
#define WGL_CONTEXT_MAJOR_VERSION_ARB  0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB  0x2092
#define WGL_CONTEXT_FLAGS_ARB              0x2094

#define		WGL_CONTEXT_PROFILE_MASK_ARB  0x9126
#define 	WGL_CONTEXT_MAJOR_VERSION_ARB   0x2091
#define 	WGL_CONTEXT_MINOR_VERSION_ARB   0x2092
#define 	WGL_CONTEXT_LAYER_PLANE_ARB   0x2093
#define 	WGL_CONTEXT_FLAGS_ARB   0x2094
#define 	WGL_CONTEXT_DEBUG_BIT_ARB   0x0001
#define 	WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB   0x0002
#define 	ERROR_INVALID_VERSION_ARB   0x2095
#define		WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001

typedef BOOL(WINAPI * PFNWGLCHOOSEPIXELFORMATARBPROC) (HDC hdc, const int* piAttribIList, const FLOAT * pfAttribFList, UINT nMaxFormats,
	int* piFormats, UINT * nNumFormats);
typedef HGLRC(WINAPI* PFNWGLCREATECONTEXTATTRIBSARBPROC) (HDC hDC, HGLRC hShareContext, const int* attribList);
typedef BOOL(WINAPI* PFNWGLSWAPINTERVALEXTPROC) (int interval);

PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB;
PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB;
PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;


HDC ourWindowHandleToDeviceContext;
bool wgl_Init(void* hwnd, void* hdc)
{
	if (ourWindowHandleToDeviceContext)
		// Already initialized
		return true;

	PIXELFORMATDESCRIPTOR pfd =
	{
			sizeof(PIXELFORMATDESCRIPTOR),
			1,
			PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,    //Flags
			PFD_TYPE_RGBA,            //The kind of framebuffer. RGBA or palette.
			32,                        //Colordepth of the framebuffer.
			0, 0, 0, 0, 0, 0,
			0,
			0,
			0,
			0, 0, 0, 0,
			24,                        //Number of bits for the depthbuffer
			8,                        //Number of bits for the stencilbuffer
			0,                        //Number of Aux buffers in the framebuffer.
			PFD_MAIN_PLANE,
			0,
			0, 0, 0
	};

	/*HDC*/ ourWindowHandleToDeviceContext = (HDC)hdc;//GetDC((HWND)hwnd);

	int  letWindowsChooseThisPixelFormat;
	letWindowsChooseThisPixelFormat = ChoosePixelFormat(ourWindowHandleToDeviceContext, &pfd);
	SetPixelFormat(ourWindowHandleToDeviceContext, letWindowsChooseThisPixelFormat, &pfd);

	HGLRC ourOpenGLRenderingContext = wglCreateContext(ourWindowHandleToDeviceContext);
	wglMakeCurrent(ourWindowHandleToDeviceContext, ourOpenGLRenderingContext);

	bool rv = true;

	if (rv) {

		wglChoosePixelFormatARB = (PFNWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress("wglChoosePixelFormatARB");
		if (!wglChoosePixelFormatARB)
		{
			return false;
		}

		wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");
		if (!wglCreateContextAttribsARB)
		{
			return false;
		}

		wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
		if (!wglSwapIntervalEXT)
		{
			return false;
		}

		int attribs[] =
		{
			WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
			WGL_CONTEXT_MINOR_VERSION_ARB, 3,
			WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
			WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
			0
		};

		HGLRC m_hrc = wglCreateContextAttribsARB(ourWindowHandleToDeviceContext, 0, attribs);

		if (!m_hrc)
		{
			printf("Open GL 4.3 not supported\n");
			// Try Gl 3.1
			attribs[1] = 3;
			attribs[3] = 1;
			m_hrc = wglCreateContextAttribsARB(ourWindowHandleToDeviceContext, 0, attribs);
		}

		if (m_hrc)
			wglMakeCurrent(ourWindowHandleToDeviceContext, m_hrc);
		else
			rv = false;

		wglDeleteContext(ourOpenGLRenderingContext);
	}

	if (rv) {
		rv = gl3wInit() != -1 && gl3wIsSupported(3, 1);
	}

	RECT r;
	GetClientRect((HWND)hwnd, &r);

	rend_resize(r.right - r.left, r.bottom - r.top);

	return rv;
}

bool wgl_Swap()
{
	SwapBuffers(ourWindowHandleToDeviceContext);
	return true;
}

void wgl_Term()
{
}