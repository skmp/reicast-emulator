
// for gl binding before GLX
#include "utils/glwrap/gl3w.h"

#include <GL/glx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include "types.h"

// for rend_reize
#include <hw/pvr/Renderer_if.h>



static GLXContext x11_glc = NULL;
static Window* x11Window;
static Display* x11Display;

static GLXFBConfig bestFbc; // TODO: Remove this hack 

static int x11_error_handler(Display *, XErrorEvent *)
{
	return 0;
}

bool glx_ChooseVisual(Display* x11Display, long x11Screen, XVisualInfo** visual, int* depth)
{
	// Get a matching FB config
	static int visual_attribs[] =
	{
		GLX_X_RENDERABLE    , True,
		GLX_DRAWABLE_TYPE   , GLX_WINDOW_BIT,
		GLX_RENDER_TYPE     , GLX_RGBA_BIT,
		GLX_X_VISUAL_TYPE   , GLX_TRUE_COLOR,
		GLX_RED_SIZE        , 8,
		GLX_GREEN_SIZE      , 8,
		GLX_BLUE_SIZE       , 8,
		GLX_ALPHA_SIZE      , 8,
		GLX_DEPTH_SIZE      , 24,
		GLX_STENCIL_SIZE    , 8,
		GLX_DOUBLEBUFFER    , True,
		//GLX_SAMPLE_BUFFERS  , 1,
		//GLX_SAMPLES         , 4,
		None
	};

	int glx_major, glx_minor;

	// FBConfigs were added in GLX version 1.3.
	if (!glXQueryVersion(x11Display, &glx_major, &glx_minor) ||
			((glx_major == 1) && (glx_minor < 3)) || (glx_major < 1))
	{
		printf("GLX: Invalid version");
		return false;
	}

	int fbcount;
	GLXFBConfig* fbc = glXChooseFBConfig(x11Display, x11Screen, visual_attribs, &fbcount);
	if (!fbc)
	{
		printf("GLX: Failed to retrieve a framebuffer config\n");
		return false;
	}
	printf("GLX: Found %d matching FB configs.\n", fbcount);

	bestFbc = fbc[0];
	XFree(fbc);

	// Get a visual
	XVisualInfo *vi = glXGetVisualFromFBConfig(x11Display, bestFbc);
	printf("GLX: Chosen visual ID = 0x%lx\n", vi->visualid);


	*depth = vi->depth;
	*visual = vi;

	return true;
}

bool glx_Init(void* wind, void* disp)
{
	x11Window = (Window*)wind;
	x11Display = (Display*)disp;

	#define GLX_CONTEXT_MAJOR_VERSION_ARB       0x2091
	#define GLX_CONTEXT_MINOR_VERSION_ARB       0x2092
	typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);

	glXCreateContextAttribsARBProc glXCreateContextAttribsARB = 0;
	glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc)glXGetProcAddressARB((const GLubyte*)"glXCreateContextAttribsARB");
	verify(glXCreateContextAttribsARB != 0);
	int context_attribs[] =
	{
		GLX_CONTEXT_MAJOR_VERSION_ARB, 4,
		GLX_CONTEXT_MINOR_VERSION_ARB, 3,
#ifndef RELEASE
		GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_DEBUG_BIT_ARB,
#endif
		GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
		None
	};
	int (*old_handler)(Display *, XErrorEvent *) = XSetErrorHandler(&x11_error_handler);

	x11_glc = glXCreateContextAttribsARB(x11Display, bestFbc, 0, True, context_attribs);
	if (!x11_glc)
	{
		printf("GLX: Open GL 4.3 not supported\n");
		// Try GL 3.0
		context_attribs[1] = 3;
		context_attribs[3] = 0;
		x11_glc = glXCreateContextAttribsARB(x11Display, bestFbc, 0, True, context_attribs);
		if (!x11_glc)
		{
			die("GLX: Open GL 3.0 not supported\n");
		}
	}
	XSetErrorHandler(old_handler);
	XSync(x11Display, False);

	glXMakeCurrent(x11Display, (GLXDrawable)x11Window, x11_glc);

	auto init_ok = gl3wInit() != -1 && gl3wIsSupported(3, 1);
	if (init_ok)
	{
		printf("GLX: init OK\n");
	}

	return init_ok;
}



void glx_Swap()
{
	glXSwapBuffers(x11Display, (GLXDrawable)x11Window);

	Window win;
	int temp;
	unsigned int tempu, new_w, new_h;
	XGetGeometry(x11Display, (Drawable)x11Window,
		&win, &temp, &temp, &new_w, &new_h, &tempu, &tempu);

	//if resized, clear up the draw buffers, to avoid out-of-draw-area junk data

	rend_resize(new_w, new_h);
}

void glx_Term()
{
	if (x11_glc)
	{
		glXMakeCurrent(x11Display, None, NULL);
		glXDestroyContext(x11Display, (GLXContext)x11_glc);
		x11_glc = NULL;
	}
}