#include <atomic>
#include <mutex>

#include "imgui/imgui.h"
#include "rend/gles/imgui_impl_opengl3.h"

#include "gui_renderer.h"
#include "oslib/oslib.h"

std::unique_ptr<GUIRenderer> g_GUIRenderer;

#include "rend/gles/gles.h"



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
    std::mutex callback_mutex;
    cResetEvent pendingCallback;

    std::function<bool(bool canceled)> callback;

    GUIRenderer_impl() { 
        keepRunning = true; 
    }

    virtual void Stop() {
        keepRunning = false;
    }

    virtual void UILoop() {
        if (!os_gl_init((void*)libPvr_GetRenderTarget(),
            (void*)libPvr_GetRenderSurface()))
            return;
        
        findGLVersion();
        ImGui_ImplOpenGL3_Init();


        while (keepRunning) {
            if (g_GUI->IsOpen() || g_GUI->IsVJoyEdit())
            {
                os_DoEvents();

                g_GUI->RenderUI();

                os_gl_swap();
            }
            else {
                auto cb = callback;

                callback_mutex.lock();
                {
                    cb = callback;
                    callback = nullptr;
                }
                callback_mutex.unlock();

                if (cb) {
                    if (cb(false)) {
                        os_gl_swap();
                    }
                }

                os_DoEvents();
            }
        }

        ImGui_ImplOpenGL3_Shutdown();
        os_gl_term();
    }

    virtual void QueueEmulatorFrame(std::function<bool(bool canceled)> cb) {
        callback_mutex.lock();
        callback = cb;
        callback_mutex.unlock();
    }

    virtual void FlushEmulatorQueue() {

        auto cb = callback;
        
        callback_mutex.lock();
        {
            cb = callback;
            callback = nullptr;
        }
        callback_mutex.unlock();

        if (cb) {
            cb(true);
        }
    }
};

GUIRenderer* GUIRenderer::Create(GUI* gui) {
    return new GUIRenderer_impl();
};