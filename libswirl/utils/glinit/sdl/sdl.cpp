#include <map>
#include "types.h"
#include "cfg/cfg.h"
#include "linux-dist/main.h"
#include "sdl/sdl.h"

#include "utils/glwrap/gl3w.h"
#include "hw/pvr/Renderer_if.h"

static bool sdl_gles;
static SDL_Window* window;
static SDL_GLContext glcontext;

bool sdlgl_CreateWindow(bool gles, int flags, int window_width, int window_height, void** wind, void** disp)
{
	sdl_gles = gles;

	flags |= SDL_WINDOW_OPENGL;

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

	auto win = SDL_CreateWindow("Reicast Emulator", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,	window_width, window_height, flags);
	if (!win)
	{
		return false;
	}

	auto glc = SDL_GL_CreateContext(window);
	if (!glc)
	{
		SDL_DestroyWindow(win);
		return false;
	}

	*wind = win;
	*disp = glc;

	return true;
}

bool sdlgl_Init(void* wind, void* disp)
{
	window = reinterpret_cast<SDL_Window*>(wind);
	glcontext = reinterpret_cast<SDL_GLContext>(disp);

	SDL_GL_MakeCurrent(window, glcontext);

	if (sdl_gles)
	{
		return load_gles_symbols();
	}
	else
	{
		return gl3wInit() != -1 && gl3wIsSupported(3, 1);
	}
}

bool sdlgl_Swap()
{
	SDL_GL_SwapWindow(window);

	/* Check if drawable has been resized */
	int w;
	int h;

	SDL_GL_GetDrawableSize(window, &w, &h);

	rend_resize(w, h);

	//TODO: FIXME we don't handle context lost here
	return true;
}

void sdlgl_Term()
{
	SDL_GL_DeleteContext(glcontext);
}
