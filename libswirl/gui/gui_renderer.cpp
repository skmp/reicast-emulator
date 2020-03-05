/*
	This file is part of libswirl
*/
#include "license/bsd"



#include <atomic>

#include "imgui/imgui.h"
#include "rend/gles/imgui_impl_opengl3.h"

#include "gui_renderer.h"
#include "oslib/oslib.h"
#include "input/gamepad.h"
#include "gui/gui_partials.h"

std::unique_ptr<GUIRenderer> g_GUIRenderer;

#include "rend/gles/gles.h"
#include "rend/gles/glcache.h"
#include "android/Android.h"

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
        if (settings.pvr.ForceGLES2)
            gl.gl_major = 2;

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


//TODO: FIXME HAVING THESE HERE IS A BIT OF A HACK
// THIS FILE IS SUPPOSED TO BE MOSTLY PORTABLE

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// OSD


const char* OSD_VertexShader =
        "\
%s \n\
#define TARGET_GL %s \n\
 \n\
#define GLES2 0 \n\
#define GLES3 1 \n\
#define GL2 2 \n\
#define GL3 3 \n\
 \n\
#if TARGET_GL == GL2 \n\
#define highp \n\
#define lowp \n\
#define mediump \n\
#endif \n\
#if TARGET_GL == GLES2 || TARGET_GL == GL2 \n\
#define in attribute \n\
#define out varying \n\
#endif \n\
 \n\
uniform highp vec4      scale; \n\
 \n\
in highp vec4    in_pos; \n\
in lowp vec4     in_base; \n\
in mediump vec2  in_uv; \n\
 \n\
out lowp vec4 vtx_base; \n\
out mediump vec2 vtx_uv; \n\
 \n\
void main() \n\
{ \n\
	vtx_base = in_base; \n\
	vtx_uv = in_uv; \n\
	highp vec4 vpos = in_pos; \n\
	\n\
	vpos.w = 1.0; \n\
	vpos.z = vpos.w; \n\
	vpos.xy = vpos.xy * scale.xy - scale.zw;  \n\
	gl_Position = vpos; \n\
}";

const char* OSD_Shader =
        "\
%s \n\
#define TARGET_GL %s \n\
 \n\
#define GLES2 0 \n\
#define GLES3 1 \n\
#define GL2 2 \n\
#define GL3 3 \n\
 \n\
#if TARGET_GL == GL2 \n\
#define highp \n\
#define lowp \n\
#define mediump \n\
#endif \n\
#if TARGET_GL != GLES2 && TARGET_GL != GL2 \n\
out highp vec4 FragColor; \n\
#define gl_FragColor FragColor \n\
#else \n\
#define in varying \n\
#define texture texture2D \n\
#endif \n\
 \n\
in lowp vec4 vtx_base; \n\
in mediump vec2 vtx_uv; \n\
/* Vertex input*/ \n\
uniform sampler2D tex; \n\
void main() \n\
{ \n\
	mediump vec2 uv=vtx_uv; \n\
	uv.y=1.0-uv.y; \n\
	gl_FragColor = vtx_base*texture(tex,uv.st); \n\
}";


extern u16 kcode[4];
extern u8 rt[4],lt[4];

#if defined(_ANDROID)
extern float vjoy_pos[14][8];
#else

float vjoy_pos[14][8]=
{
	{24+0,24+64,64,64},     //LEFT
	{24+64,24+0,64,64},     //UP
	{24+128,24+64,64,64},   //RIGHT
	{24+64,24+128,64,64},   //DOWN

	{440+0,280+64,64,64},   //X
	{440+64,280+0,64,64},   //Y
	{440+128,280+64,64,64}, //B
	{440+64,280+128,64,64}, //A

	{320-32,360+32,64,64},  //Start

	{440,200,90,64},        //RT
	{542,200,90,64},        //LT

	{-24,128+224,128,128},  //ANALOG_RING
	{96,320,64,64},         //ANALOG_POINT
	{1}
};
#endif // !_ANDROID

struct
{
    GLuint program;
    GLuint scaleLocation;
    GLuint tex;
    GLuint vao;
    GLuint geom;
} gl_osd;


void gl_load_osd_resources()
{
    char vshader[8192];
    char fshader[8192];

    sprintf(vshader, OSD_VertexShader, gl.glsl_version_header, gl.gl_version);
    sprintf(fshader, OSD_Shader, gl.glsl_version_header, gl.gl_version);

    gl_osd.program = gl_CompileAndLink(vshader, fshader);
    gl_osd.scaleLocation = glGetUniformLocation(gl_osd.program, "scale");
    glUniform1i(glGetUniformLocation(gl_osd.program, "tex"), 0);		//bind osd texture to slot 0

#ifdef _ANDROID
    int w, h;
    if (gl_osd.tex == 0)
        gl_osd.tex = loadPNG(get_readonly_data_path(DATA_PATH "buttons.png"), w, h);
#endif
}

void gl_free_osd_resources()
{
    if (gl_osd.program != 0) {
        glcache.DeleteProgram(gl_osd.program);
        gl_osd.program = 0;
    }

    if (gl_osd.tex != 0) {
        glcache.DeleteTextures(1, &gl_osd.tex);
        gl_osd.tex = 0;
    }
}


static vector<Vertex> osd_vertices;

#ifdef _ANDROID
static const float vjoy_sz[2][14] = {
        { 64,64,64,64, 64,64,64,64, 64, 90,90, 128, 64 },
        { 64,64,64,64, 64,64,64,64, 64, 64,64, 128, 64 },
};
#endif

void HideOSD()
{
    vjoy_pos[13][0] = 0;
    vjoy_pos[13][1] = 0;
    vjoy_pos[13][2] = 0;
    vjoy_pos[13][3] = 0;
}

static void DrawButton(float* xy, u32 state)
{
    Vertex vtx;

    vtx.z = 1;

    vtx.col[0]=vtx.col[1]=vtx.col[2]=(0x7F-0x40*state/255)*vjoy_pos[13][0];

    vtx.col[3]=0xA0*vjoy_pos[13][4];

    vjoy_pos[13][4]+=(vjoy_pos[13][0]-vjoy_pos[13][4])/2;



    vtx.x = xy[0]; vtx.y = xy[1];
    vtx.u=xy[4]; vtx.v=xy[5];
    osd_vertices.push_back(vtx);

    vtx.x = xy[0] + xy[2]; vtx.y = xy[1];
    vtx.u=xy[6]; vtx.v=xy[5];
    osd_vertices.push_back(vtx);

    vtx.x = xy[0]; vtx.y = xy[1] + xy[3];
    vtx.u=xy[4]; vtx.v=xy[7];
    osd_vertices.push_back(vtx);

    vtx.x = xy[0] + xy[2]; vtx.y = xy[1] + xy[3];
    vtx.u=xy[6]; vtx.v=xy[7];
    osd_vertices.push_back(vtx);
}

static void DrawButton2(float* xy, bool state) { DrawButton(xy,state?0:255); }

static void osd_gen_vertices()
{
    osd_vertices.clear();
    DrawButton2(vjoy_pos[0],kcode[0] & DC_DPAD_LEFT);
    DrawButton2(vjoy_pos[1],kcode[0] & DC_DPAD_UP);
    DrawButton2(vjoy_pos[2],kcode[0] & DC_DPAD_RIGHT);
    DrawButton2(vjoy_pos[3],kcode[0] & DC_DPAD_DOWN);

    DrawButton2(vjoy_pos[4],kcode[0] & DC_BTN_X);
    DrawButton2(vjoy_pos[5],kcode[0] & DC_BTN_Y);
    DrawButton2(vjoy_pos[6],kcode[0] & DC_BTN_B);
    DrawButton2(vjoy_pos[7],kcode[0] & DC_BTN_A);

    DrawButton2(vjoy_pos[8],kcode[0] & DC_BTN_START);

    DrawButton(vjoy_pos[9],lt[0]);

    DrawButton(vjoy_pos[10],rt[0]);

    DrawButton2(vjoy_pos[11],1);
    DrawButton2(vjoy_pos[12],0);
}

#define OSD_TEX_W 512
#define OSD_TEX_H 256

void OSD_DRAW(bool clear_screen)
{
#ifdef _ANDROID
    if (gl_osd.tex == 0)
        gl_load_osd_resources();
    if (gl_osd.tex != 0)
    {
        osd_gen_vertices();

        float u=0;
        float v=0;

        for (int i=0;i<13;i++)
        {
            //umin,vmin,umax,vmax
            vjoy_pos[i][4]=(u+1)/OSD_TEX_W;
            vjoy_pos[i][5]=(v+1)/OSD_TEX_H;

            vjoy_pos[i][6]=((u+vjoy_sz[0][i]-1))/OSD_TEX_W;
            vjoy_pos[i][7]=((v+vjoy_sz[1][i]-1))/OSD_TEX_H;

            u+=vjoy_sz[0][i];
            if (u>=OSD_TEX_W)
            {
                u-=OSD_TEX_W;
                v+=vjoy_sz[1][i];
            }
            //v+=vjoy_pos[i][3];
        }

        verify(glIsProgram(gl_osd.program));
        glcache.UseProgram(gl_osd.program);

        float scale_h = screen_height / 480.f;
        float offs_x = (screen_width - scale_h * 640.f) / 2.f;
        float scale[4];
        scale[0] = 2.f / (screen_width / scale_h);
        scale[1]= -2.f / 480.f;
        scale[2]= 1.f - 2.f * offs_x / screen_width;
        scale[3]= -1.f;
        glUniform4fv(gl_osd.scaleLocation, 1, scale);

        glActiveTexture(GL_TEXTURE0);
        glcache.BindTexture(GL_TEXTURE_2D, gl_osd.tex);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glBufferData(GL_ARRAY_BUFFER, osd_vertices.size() * sizeof(Vertex), &osd_vertices[0], GL_STREAM_DRAW); glCheck();

        glcache.Enable(GL_BLEND);
        glcache.Disable(GL_DEPTH_TEST);
        glcache.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glcache.DepthMask(false);
        glcache.DepthFunc(GL_ALWAYS);

        glcache.Disable(GL_CULL_FACE);
        glcache.Disable(GL_SCISSOR_TEST);
        glViewport(0, 0, screen_width, screen_height);

        if (clear_screen)
        {
            glcache.ClearColor(0.7f, 0.7f, 0.7f, 1.f);
            glClear(GL_COLOR_BUFFER_BIT);
        }
        int dfa = osd_vertices.size() / 4;

        for (int i = 0; i < dfa; i++)
            glDrawArrays(GL_TRIANGLE_STRIP, i * 4, 4);
    }
#endif
    //g_GUI->RenderOSD();
}

void DrawOSD(bool clear_screen)
{

    if (gl.gl_major >= 3)
        glBindVertexArray(gl_osd.vao);

    glBindBuffer(GL_ARRAY_BUFFER, gl_osd.geom); glCheck();
    glEnableVertexAttribArray(VERTEX_POS_ARRAY);
    glVertexAttribPointer(VERTEX_POS_ARRAY, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex,x));

    glEnableVertexAttribArray(VERTEX_COL_BASE_ARRAY);
    glVertexAttribPointer(VERTEX_COL_BASE_ARRAY, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex,col));

    glEnableVertexAttribArray(VERTEX_UV_ARRAY);
    glVertexAttribPointer(VERTEX_UV_ARRAY, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex,u));

    OSD_DRAW(clear_screen);
}
// GUIRenderer

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
                DrawOSD(false);
                g_GUI->RenderOSD();
                if (!os_gl_swap()) // must re-create context
                {
                    rv = false;
                }
            }
        }
        else  if (g_GUI->IsOpen())
        {
            g_GUI->RenderUI();

            if (!os_gl_swap()) // must re-create context
            {
                rv = false;
            }
        }
        else if (g_GUI->IsVJoyEdit())
        {
            DrawOSD(true);

            if (!os_gl_swap()) // must re-create context
            {
                rv = false;
            }
        }

        return rv;
    }

    virtual void UIFrame() {
        if (!tryUIFrame()) {
            printf("UIFRAME: Re-creating context...\n");
            SleepMs(10);
            DestroyContext();
            if (!CreateContext())
            {
#if !defined(_ANDROID)
                die("UIFrame: Failed to recover gl context")
#else
                msgboxf("Your graphics driver has crashed. Will try to recover.\n You can force a GLES2 context from the settings to avoid this message.\n\n Please upgrade your OS.", MBX_ICONEXCLAMATION);
                printf("UIFRAME: Failed to create context - recreating view\n");
                Stop();
                android_RecreateView();
                settings.pvr.ForceGLES2 = true;
                SaveSettings();
#endif
            }
        }
    }

    void CreateOSDResources()
    {
        glGenVertexArrays(1, &gl_osd.vao);

        //create vbos
        glGenBuffers(1, &gl_osd.geom);

        gl_load_osd_resources();
    }

    void DestroyOSDResources()
    {
        gl_free_osd_resources();
        glDeleteBuffers(1, &gl_osd.geom);
        gl_osd.geom = 0;

        glDeleteVertexArrays(1, &gl_osd.vao);
        gl_osd.vao = 0;
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

        CreateOSDResources();
        //rend_init_renderer(sh4_cpu->vram.data); // is lazy init in rend_frame, sh4_cpu might not be known at this point
        printf("CreateContext Done\n");
        return true;
    }

    void DestroyContext() {
        printf("DestroyContext\n");
        rend_term_renderer();	// also cleans texcache
        DestroyOSDResources();
        ImGui_ImplOpenGL3_Shutdown();
        os_gl_term();
        printf("DestroyContext Done\n");
    }

    virtual void UILoop() {
        if (!CreateContext()) {
            msgboxf("Failed to create rendering context\n", MBX_ICONERROR);
            return;
        }

#if defined(TARGET_EMSCRIPTEN)
        emscripten_set_main_loop_arg(STATIC_FORWARD(GUIRenderer_impl, UIFrame), this, 60, true);
#else
        while (keepRunning) {
            UIFrame();
            SleepMs(1); // don't sleep too much
        }
#endif
        DestroyContext();
    }

    virtual void QueueEmulatorFrame(std::function<bool()> cb) {
        callback_mutex.Lock();
        frameDone = false;
        #if FEAT_TA == TA_LLE
            verify(callback == nullptr);
        #endif
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
