#include <atomic>

#include "imgui/imgui.h"
#include "rend/gles/imgui_impl_opengl3.h"

#include "gui_renderer.h"
#include "oslib/oslib.h"

std::unique_ptr<GUIRenderer> g_GUIRenderer;

#include "rend/gles/gles.h"
#include "rend/gles/glcache.h"

#if defined(TARGET_EMSCRIPTEN)
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

static void findGLVersion()
{
    gl.index_type = GL_UNSIGNED_INT;
    gl.rpi4_workaround = false;

    while (true)
        if (glGetError() == GL_NO_ERROR)
            break;
    glGetIntegerv(GL_MAJOR_VERSION, &gl.gl_major);
    if (glGetError() == GL_INVALID_ENUM)
        gl.gl_major = 2;
    const char* version = (const char*)glGetString(GL_VERSION);
    printf("OpenGL version: %s\n", version);
    if (!strncmp(version, "OpenGL ES", 9))
    {
        gl.is_gles = true;
        if (gl.gl_major >= 3)
        {
            gl.gl_version = "GLES3";
            gl.glsl_version_header = "#version 300 es";
        }
        else
        {
            gl.gl_version = "GLES2";
            gl.glsl_version_header = "";
            gl.index_type = GL_UNSIGNED_SHORT;
        }
        gl.fog_image_format = GL_ALPHA;
        const char* extensions = (const char*)glGetString(GL_EXTENSIONS);
        if (strstr(extensions, "GL_OES_packed_depth_stencil") != NULL)
            gl.GL_OES_packed_depth_stencil_supported = true;
        if (strstr(extensions, "GL_OES_depth24") != NULL)
            gl.GL_OES_depth24_supported = true;
        if (!gl.GL_OES_packed_depth_stencil_supported)
            printf("Packed depth/stencil not supported: no modifier volumes when rendering to a texture\n");
    }
    else
    {
        gl.is_gles = false;
        if (gl.gl_major >= 3)
        {
            gl.gl_version = "GL3";
#if HOST_OS == OS_DARWIN
            gl.glsl_version_header = "#version 150";
#else
            gl.glsl_version_header = "#version 130";
#endif
            gl.fog_image_format = GL_RED;
        }
        else
        {
            gl.gl_version = "GL2";
            gl.glsl_version_header = "#version 120";
            gl.fog_image_format = GL_ALPHA;
        }
    }


    // workarounds

    auto renderer = (const char*)glGetString(GL_RENDERER);

    if (renderer && strstr(renderer, "V3D 4.2"))
    {
        printf("glesrend: Enabling rpi4_workaround\n");

        gl.rpi4_workaround = true;
    }
}

struct GUIRenderer_impl : GUIRenderer {
    std::atomic<bool> keepRunning;
    std::atomic<bool> frameDone;
    cMutex callback_mutex;
    cResetEvent pendingCallback;

    std::function<bool()> callback;

    GUIRenderer_impl() { 
        keepRunning = true; 
    }

    virtual void Stop() {
        keepRunning = false;
    }

    virtual void Start() {
        keepRunning = true;
    }

    virtual bool tryUIFrame() {

        bool rv = true;

        os_DoEvents();

        auto cb = callback;

        callback_mutex.Lock();
        {
            cb = callback;
            callback = nullptr;
            frameDone = cb == nullptr;
        }
        callback_mutex.Unlock();
        

        if (cb) {
            if (cb()) {
                if (!os_gl_swap()) // must re-create context
                {
                    rv = false;
                }
            }
        }
        else  if (g_GUI->IsOpen() || g_GUI->IsVJoyEdit())
        {
            g_GUI->RenderUI();

            if (!os_gl_swap()) // must re-create context
            {
                rv = false;
            }
        }

        return rv;
    }

    virtual void UIFrame() {
        if (!tryUIFrame()) {
            DestroyContext();

            if (!CreateContext()) {
                return;
            }
        }
    }
    
    bool CreateContext() {
        printf("CreateContext\n");
        if (!os_gl_init((void*)libPvr_GetRenderTarget(),
                        (void*)libPvr_GetRenderSurface()))
            return false;

        findGLVersion();

        glcache.EnableCache();
        renderer_changed = true;

        ImGui_ImplOpenGL3_Init();

        // rend_init_renderer(); // is lazy init on callback/rend_frame
        printf("CreateContext Done\n");
        return true;
    }

    void DestroyContext() {
        printf("DestroyContext\n");
        rend_term_renderer();	// also cleans texcache
        ImGui_ImplOpenGL3_Shutdown();
        os_gl_term();
        printf("DestroyContext Done\n");
    }

    virtual void UILoop() {
        if (!CreateContext())
            return;

#if defined(TARGET_EMSCRIPTEN)
        emscripten_set_main_loop_arg(STATIC_FORWARD(GUIRenderer_impl, UIFrame), this, 60, true);
#else
        while (keepRunning) {
            UIFrame();
        }
#endif
        DestroyContext();
    }

    virtual void QueueEmulatorFrame(std::function<bool()> cb) {
        callback_mutex.Lock();
        frameDone = false;
        callback = cb;
        callback_mutex.Unlock();
    }

    virtual void WaitQueueEmpty() {
        do {
            auto cb = callback;

            callback_mutex.Lock();
            {
                cb = callback;
            }
            callback_mutex.Unlock();
        } while (callback || !frameDone);
    }
};

GUIRenderer* GUIRenderer::Create(GUI* gui) {
    return new GUIRenderer_impl();
};