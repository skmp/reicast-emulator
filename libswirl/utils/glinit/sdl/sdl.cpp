#include <map>
#include "types.h"
#include "cfg/cfg.h"
#include "linux-dist/main.h"
#include "sdl/sdl.h"
#include "gui/gui.h"
#ifndef GLES
#include "khronos/GL3/gl3w.h"
#endif
#endif
#include "hw/maple/maple_devs.h"
#include "sdl_gamepad.h"
#include "sdl_keyboard.h"

void sdlgl_CreateWindow(bool gles)
{
	if (SDL_WasInit(SDL_INIT_VIDEO) == 0)
	{
		if(SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
		{
			die("error initializing SDL Joystick subsystem");
		}
	}

	int window_width  = cfgLoadInt("x11","width", WINDOW_WIDTH);
	int window_height = cfgLoadInt("x11","height", WINDOW_HEIGHT);

	int flags = SDL_WINDOW_OPENGL;
	#ifdef TARGET_PANDORA
		flags |= SDL_FULLSCREEN;
	#else
		flags |= SDL_SWSURFACE | SDL_WINDOW_RESIZABLE;
	#endif

	if (gles)
	{
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	}
	else
	{
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	}

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	window = SDL_CreateWindow("Reicast Emulator", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,	window_width, window_height, flags);
	if (!window)
	{
		die("error creating SDL window");
	}

	glcontext = SDL_GL_CreateContext(window);
	if (!glcontext)
	{
		die("Error creating SDL GL context");
	}
	SDL_GL_MakeCurrent(window, NULL);

	SDL_GL_GetDrawableSize(window, &screen_width, &screen_height);

	float ddpi, hdpi, vdpi;
	if (!SDL_GetDisplayDPI(SDL_GetWindowDisplayIndex(window), &ddpi, &hdpi, &vdpi))
		screen_dpi = (int)roundf(max(hdpi, vdpi));

	printf("Created SDL Window (%ix%i) and GL Context successfully\n", window_width, window_height);
}

bool sdlgl_Init(void* wind, void* disp)
{
	SDL_GL_MakeCurrent(window, glcontext);
	#ifdef GLES
		return true;
	#else
		return gl3wInit() != -1 && gl3wIsSupported(3, 1);
	#endif
}

void sdlgl_Swap()
{
	SDL_GL_SwapWindow(window);

	/* Check if drawable has been resized */
	SDL_GL_GetDrawableSize(window, &screen_width, &screen_height);
}

void sdlgl_Term()
{
	SDL_GL_DeleteContext(glcontext);
}
