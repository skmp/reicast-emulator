extern "C" void load_gles_symbols();
static bool created_context;

bool egl_makecurrent()
{
	if (gl.setup.surface == EGL_NO_SURFACE || gl.setup.context == EGL_NO_CONTEXT)
		return false;
	return eglMakeCurrent(gl.setup.display, gl.setup.surface, gl.setup.surface, gl.setup.context);
}

// Create a basic GLES context
bool os_gl_init(void* wind, void* disp)
{
	gl.setup.native_wind = (EGLNativeWindowType)wind;
	gl.setup.native_disp = (EGLNativeDisplayType)disp;

	//try to get a display
	gl.setup.display = eglGetDisplay(gl.setup.native_disp);

	//if failed, get the default display (this will not happen in win32)
	if (gl.setup.display == EGL_NO_DISPLAY)
		gl.setup.display = eglGetDisplay((EGLNativeDisplayType)EGL_DEFAULT_DISPLAY);


	// Initialise EGL
	EGLint maj, min;
	if (!eglInitialize(gl.setup.display, &maj, &min))
	{
		printf("EGL Error: eglInitialize failed\n");
		return false;
	}

	if (gl.setup.surface == 0)
	{
		EGLint pi32ConfigAttribs[] = {
				EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_SWAP_BEHAVIOR_PRESERVED_BIT,
				EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
				EGL_DEPTH_SIZE, 24,
				EGL_STENCIL_SIZE, 8,
				EGL_NONE
		};

		int num_config;

		EGLConfig config;
		if (!eglChooseConfig(gl.setup.display, pi32ConfigAttribs, &config, 1, &num_config) || (num_config != 1))
		{
			// Fall back to non preserved swap buffers
			EGLint pi32ConfigFallbackAttribs[] = {
					EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
					EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
					EGL_DEPTH_SIZE, 24,
					EGL_STENCIL_SIZE, 8,
					EGL_NONE
			};
			if (!eglChooseConfig(gl.setup.display, pi32ConfigFallbackAttribs, &config, 1, &num_config) || (num_config != 1))
			{
				printf("EGL Error: eglChooseConfig failed\n");
				return false;
			}
		}
#ifdef _ANDROID
		EGLint format;
		if (!eglGetConfigAttrib(gl.setup.display, config, EGL_NATIVE_VISUAL_ID, &format))
		{
			printf("eglGetConfigAttrib() returned error %x\n", eglGetError());
			return false;
		}
		ANativeWindow_setBuffersGeometry((ANativeWindow*)wind, 0, 0, format);
#endif
		gl.setup.surface = eglCreateWindowSurface(gl.setup.display, config, (EGLNativeWindowType)wind, NULL);

		if (gl.setup.surface == EGL_NO_SURFACE)
		{
			printf("EGL Error: eglCreateWindowSurface failed: %x\n", eglGetError());
			return false;
		}


		bool try_full_gl = true;
		if (!eglBindAPI(EGL_OPENGL_API))
		{
			printf("eglBindAPI(EGL_OPENGL_API) failed: %x\n", eglGetError());
			try_full_gl = false;
		}
		if (try_full_gl)
		{
			EGLint contextAttrs[] = { EGL_CONTEXT_MAJOR_VERSION_KHR, 3,
									  EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR,
									  EGL_NONE };
			gl.setup.context = eglCreateContext(gl.setup.display, config, NULL, contextAttrs);
			if (gl.setup.context != EGL_NO_CONTEXT)
			{
				egl_makecurrent();
				if (gl3wInit())
					printf("gl3wInit() failed\n");
			}
		}

		if (gl.setup.context == EGL_NO_CONTEXT)
		{
			if (!eglBindAPI(EGL_OPENGL_ES_API))
			{
				printf("eglBindAPI() failed: %x\n", eglGetError());
				return false;
			}
			EGLint contextAttrs[] = { EGL_CONTEXT_CLIENT_VERSION, 2 , EGL_NONE };

			gl.setup.context = eglCreateContext(gl.setup.display, config, NULL, contextAttrs);

			if (gl.setup.context == EGL_NO_CONTEXT)
			{
				printf("eglCreateContext() failed: %x\n", eglGetError());
				return false;
			}
#ifdef GLES
			// EGL only supports runtime loading with android? TDB
			load_gles_symbols();
#else
			egl_makecurrent();
			if (gl3wInit())
				printf("gl3wInit() failed\n");
#endif
		}
		created_context = true;
	}
	else if (glGetError == NULL)
	{
		// Needed when the context is not created here (Android, iOS)
		load_gles_symbols();
	}

	if (!egl_makecurrent())
	{
		printf("eglMakeCurrent() failed: %x\n", eglGetError());
		return false;
	}

	EGLint w, h;
	eglQuerySurface(gl.setup.display, gl.setup.surface, EGL_WIDTH, &w);
	eglQuerySurface(gl.setup.display, gl.setup.surface, EGL_HEIGHT, &h);

	screen_width = w;
	screen_height = h;

	// Required when doing partial redraws
	if (!eglSurfaceAttrib(gl.setup.display, gl.setup.surface, EGL_SWAP_BEHAVIOR, EGL_BUFFER_PRESERVED))
	{
		printf("Swap buffers are not preserved. Last frame copy enabled\n");
		gl.swap_buffer_not_preserved = true;
	}

	printf("EGL config: %p, %p, %p %dx%d\n", gl.setup.context, gl.setup.display, gl.setup.surface, w, h);
	return true;
}

void egl_stealcntx()
{
	gl.setup.context = eglGetCurrentContext();
	gl.setup.display = eglGetCurrentDisplay();
	gl.setup.surface = eglGetCurrentSurface(EGL_DRAW);
}

//swap buffers
void os_gl_swap()
{
#ifdef TARGET_PANDORA0
	if (fbdev >= 0)
	{
		int arg = 0;
		ioctl(fbdev, FBIO_WAITFORVSYNC, &arg);
	}
#endif
	eglSwapBuffers(gl.setup.display, gl.setup.surface);
}

void os_gl_term()
{
	if (!created_context)
		return;
	created_context = false;
	eglMakeCurrent(gl.setup.display, NULL, NULL, EGL_NO_CONTEXT);
#if HOST_OS == OS_WINDOWS
	ReleaseDC((HWND)gl.setup.native_wind, (HDC)gl.setup.native_disp);
#else
	if (gl.setup.context != NULL)
		eglDestroyContext(gl.setup.display, gl.setup.context);
	if (gl.setup.surface != NULL)
		eglDestroySurface(gl.setup.display, gl.setup.surface);
#ifdef TARGET_PANDORA
	if (gl.setup.display)
		eglTerminate(gl.setup.display);
	if (fbdev >= 0)
		close(fbdev);
	fbdev = -1;
#endif
#endif	// !OS_WINDOWS
	gl.setup.context = EGL_NO_CONTEXT;
	gl.setup.surface = EGL_NO_SURFACE;
	gl.setup.display = EGL_NO_DISPLAY;
}