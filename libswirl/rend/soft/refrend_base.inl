/*
	This file is part of libswirl
*/
#include "license/bsd"

/*
    REFSW: Reference-style software rasterizer

    An attempt to model CLX2's CORE/SPG/RAMDAC at the lowest functional level

    Rasterizer structure
    ===

    Reads tile lists in CORE format, generated from a LLE TA implementation or software running from sh4,
    renders them in 32x32 tiles, reads out to VRAM and displays framebuffer from VRAM.

    CORE high level overview
    ===

    CORE Renders based on the REGION ARRAY, which is a flag-terminated list of tiles. Each RegionArrayEntry
    contains the TILE x/y position, control flags for Z clear/Write out/Presort and pointers to OBJECT LISTS.

    OBJECT LISTS are inline linked lists containing ObjectListEntries. Each ObjectListEntry has a small
    descriptor for the entry type and vertex size, and a pointer to the OBJECT DATA.

    OBJECT DATA contains the PARAMETERS for the OBJECT (ISP, TSP, TCW, optional TSP2 and TCW2) and vertixes.

    There are 3 OBJECT DATA TYPES
    - Triangle Strips (PARAMETERS, up to 8 VTXs) x 1
    - Triangle Arrays (PARAMETERS, 3 vtx) x Num_of_primitives
    - Quad Arrays (PARAMETERS, 4 vtx) x Num_of_primitives

    CORE renders the OBJECTS to its internal TILE BUFFERS, scales and filters the output (SCL)
    and writes out to VRAM.

    CORE Rendering details
    ===

    CORE has four main components, FPU (triangle setup) ISP (Rasterization, depth, stencil), TSP (Texutre + Shading)
    and SCL (tile writeout + scaling). There are three color rendering modes: DEPTH FIRST, DEPTH + COLOR and LAYER PEELING.

    OPAQUE OBJECTS are rendered using the DEPTH FIRST mode.
    PUNCH THROUGH OBJECTS are rendered using the DEPTH + COLOR mode.
    TRANSPARENT OBJECTS are rendered using either the DEPTH + COLOR mode or the LAYER PEELING mode.
    
    DEPTH FIRST mode
    ---
    OBJECTS are first rendered by ISP in the depth and tag buffers, 32 pixels (?) at a time. then the SPAN SORTER collects spans with the
    same tag and sends them to TSP for shading processing, one pixel at a time.

    DEPTH + COLOR mode
    ---
    OBJECTS are rendered by ISP and TSP at the same time, one pixel (?) at a time. ALPHA TEST feedback from TSP modifies the Z-write behavior.

    LAYER PEELING mode
    ---

    OBJECTS are first rendered by ISP in the depth and tag buffers, using a depth pass and a depth test buffer. SPAN SORTER collects spans with
    the same tag and sends them to TSP for shading processing. The process repeats itself until all layers have been indepedently rendered. On
    each pass, only the pixels with the lowest depth value that pass the depth pass buffer are rendered.
*/


#include <omp.h>
#include "hw/pvr/Renderer_if.h"
#include "hw/pvr/pvr_mem.h"
#include "oslib/oslib.h"
#include "rend/TexCache.h"

#include <mmintrin.h>
#include <xmmintrin.h>
#include <emmintrin.h>
#include <smmintrin.h>

#include <cmath>
#include <float.h>

#include "rend/gles/gles.h"
#include "gui/gui.h"

#include <memory>
#include <atomic>
#include <queue>

extern u32 decoded_colors[3][65536];

#pragma pack(push, 1) 
union RegionArrayEntryControl {
    struct {
        u32 res0 : 2;
        u32 tilex : 6;
        u32 tiley : 6;
        u32 res1 : 14;
        u32 no_writeout : 1;
        u32 pre_sort : 1;
        u32 z_keep : 1;
        u32 last_region : 1;
    };
    u32 full;
};

typedef u32 pvr32addr_t;

union ListPointer {
    struct
    {
        u32 pad0 : 2;
        u32 ptr_in_words : 22;
        u32 pad1 : 7;
        u32 empty : 1;
    };
    u32 full;
};

union ObjectListEntry {
    struct {
        u32 pad0 : 31;
        u32 is_not_triangle_strip : 1;
    };

    struct {
        u32 pad1 : 29;
        u32 type : 3;
    };

    struct {
        u32 param_offs_in_words : 21;
        u32 skip : 3;
        u32 shadow : 1;
        u32 mask : 6;
    } tstrip;

    struct {
        u32 param_offs_in_words : 21;
        u32 skip : 3;
        u32 shadow : 1;
        u32 prims : 4;
    } tarray;

    struct {
        u32 param_offs_in_words : 21;
        u32 skip : 3;
        u32 shadow : 1;
        u32 prims : 4;
    } qarray;

    struct {
        u32 pad3 : 2;
        u32 next_block_ptr_in_words : 22;
        u32 pad4 : 4;
        u32 end_of_list : 1;
    } link;

    u32 full;
};

#pragma pack(pop)

struct RegionArrayEntry {
    RegionArrayEntryControl control;
    ListPointer opaque;
    ListPointer opaque_mod;
    ListPointer trans;
    ListPointer trans_mod;
    ListPointer puncht;
};

struct DrawParameters
{
    ISP_TSP isp;
    TSP tsp;
    TCW tcw;
    TSP tsp2;
    TSP tcw2;
};

enum RenderMode {
    RM_OPAQUE,
    RM_PUNCHTHROUGH,
    RM_TRANSLUCENT,
    RM_MODIFIER,
    RM_COUNT
};

#if HOST_OS != OS_WINDOWS
struct RECT {
    int left, top, right, bottom;
};

#include     <X11/Xlib.h>
#endif

#if HOST_OS == OS_WINDOWS
static BITMAPINFOHEADER bi = { sizeof(BITMAPINFOHEADER), 0, 0, 1, 32, BI_RGB };
#endif

bool gles_init();

typedef u32 parameter_tag_t;

struct RefRendInterface
{
    // backend specific init
    virtual bool Init() = 0;

    // Clear the buffers
    virtual void ClearBuffers(u32 paramValue, float depthValue, u32 stencilValue) = 0;

    // Clear and set DEPTH2 for peeling
    virtual void PeelBuffers(u32 paramValue, float depthValue, u32 stencilValue) = 0;

    // Summarize tile after rendering modvol (inside)
    virtual void SummarizeStencilOr() = 0;
    
    // Summarize tile after rendering modvol (outside)
    virtual void SummarizeStencilAnd() = 0;

    // Clear the pixel drawn counter
    virtual void ClearPixelsDrawn() = 0;

    // Get the pixel drawn counter. Used during layer peeling to determine when to stop processing
    virtual u32 GetPixelsDrawn() = 0;

    // Add an entry to the fpu parameters list
    virtual parameter_tag_t AddFpuEntry(DrawParameters* params, Vertex* vtx, RenderMode render_mode) = 0;

    // Clear the fpu parameters list
    virtual void ClearFpuEntries() = 0;

    // Get the final output of the 32x32 tile. Used to write to the VRAM framebuffer
    virtual u8* GetColorOutputBuffer() = 0;

    // Render to ACCUM from TAG buffer
    // TAG holds references to triangles, ACCUM is the tile framebuffer
    virtual void RenderParamTags(RenderMode rm, int tileX, int tileY) = 0;

    // RasterizeTriangle for each rendering mode
    #define RasterizeTriangleMode(mode) \
        virtual void RasterizeTriangle_##mode(DrawParameters* params, parameter_tag_t tag, int vertex_offset, const Vertex& v1, const Vertex& v2, const Vertex& v3, RECT* area) = 0

    RasterizeTriangleMode(OPAQUE);
    RasterizeTriangleMode(PUNCHTHROUGH);
    RasterizeTriangleMode(TRANSLUCENT);
    RasterizeTriangleMode(MODIFIER);

    #undef RasterizeTriangleMode
    
};


#define MAX_CPU_COUNT 64

/*
    Main renderer class
*/
struct refrend : Renderer
{
    std::function<RefRendInterface*()> createBackend;

    u8* vram;
    vector<cThread> threads;
    vector<queue<function<void(RefRendInterface*)>>> queues;
    atomic<bool> running;

    cMutex queue_lock;
    

    static void* ThreadPoolEntry(void* func) {
        auto fn = reinterpret_cast<function<void()>*>(func);

        (*fn)();

        delete fn;

        return nullptr;
    }

    void EnqueueTile(int tileId, function<void(RefRendInterface*)> fn) {
        queue_lock.Lock();
        queues[tileId % queues.size()].push(fn);
        queue_lock.Unlock();
    }

    refrend(u8* vram, std::function<RefRendInterface*()> createBackend) : vram(vram), createBackend(createBackend) {
        running = true;

        queues.reserve(MAX_CPU_COUNT);
        threads.reserve(MAX_CPU_COUNT);

        for (int i = 0; i < MAX_CPU_COUNT; i++) {
            auto func = new function<void()>([=]() {
                unique_ptr<RefRendInterface> backend;

                backend.reset(createBackend());

                backend->Init();

                while(running) {
                    queue_lock.Lock();
                    if (queues[i].size() == 0)
                    {
                        queue_lock.Unlock();
                        SleepMs(1);
                        continue;    
                    }
                    auto item = queues[i].front();
                    queues[i].pop();
                    queue_lock.Unlock();

                    item(backend.get());
                }

                backend.reset();
            });

            queues.push_back( queue<function<void(RefRendInterface*)>>() );
            threads.push_back(cThread(ThreadPoolEntry, func));

            threads[threads.size()-1].Start();
        }
    }

    template<RenderMode render_mode>
    void RenderTriangle(RefRendInterface* backend, DrawParameters* params, parameter_tag_t tag, int vertex_offset, const Vertex& v1, const Vertex& v2, const Vertex& v3, RECT* area)
    {
        switch(render_mode)
        {
            #define RasterizeTriangleCase(mode) \
                case RM_##mode: backend->RasterizeTriangle_##mode(params, tag, vertex_offset, v1, v2, v3, area); break

            RasterizeTriangleCase(OPAQUE);
            RasterizeTriangleCase(PUNCHTHROUGH);
            RasterizeTriangleCase(TRANSLUCENT);
            RasterizeTriangleCase(MODIFIER);

            #undef RasterizeTriangleCase
        }
        

        if (render_mode == RM_MODIFIER)
        {          
            if (params->isp.modvol.VolumeMode == 1 ) 
            {
                backend->SummarizeStencilOr();
            }
            else if (params->isp.modvol.VolumeMode == 2) 
            {
                backend->SummarizeStencilAnd();
            }
        }
    }

    void SetFBScale(float x, float y)
    {
        
    }

    virtual bool Process(TA_context* ctx) {

        // no-op for now
        return true;
    }


    // called on vblank
    virtual bool RenderFramebuffer() {
        Present();
        return false;
    }

    u32 ReadRegionArrayEntry(u32 base, RegionArrayEntry* entry) 
    {
        bool fmt_v1 = (((FPU_PARAM_CFG >> 21) & 1) == 0);

        entry->control.full     = vri(vram, base);
        entry->opaque.full      = vri(vram, base + 4);
        entry->opaque_mod.full  = vri(vram, base + 8);
        entry->trans.full       = vri(vram, base + 12);
        entry->trans_mod.full   = vri(vram, base + 16);

        if (fmt_v1)
        {
            entry->puncht.full = 0x80000000;
            return 5 * 4;
        }
        else
        {
            entry->puncht.full = vri(vram, base + 20);
            return 6 * 4;
        }
    }

    f32 f16(u16 v)
    {
        u32 z=v<<16;
        return *(f32*)&z;
    }

	#define vert_packed_color_(to,src) \
		{ \
		u32 t=src; \
		to[2] = (u8)(t);t>>=8;\
		to[1] = (u8)(t);t>>=8;\
		to[0] = (u8)(t);t>>=8;\
		to[3] = (u8)(t);      \
		}

    //decode a vertex in the native pvr format
    void decode_pvr_vertex(DrawParameters* params, pvr32addr_t ptr,Vertex* cv)
    {
        //XYZ
        //UV
        //Base Col
        //Offset Col

        //XYZ are _allways_ there :)
        cv->x=vrf(vram, ptr);ptr+=4;
        cv->y=vrf(vram, ptr);ptr+=4;
        cv->z=vrf(vram, ptr);ptr+=4;

        if (params->isp.Texture)
        {	//Do texture , if any
            if (params->isp.UV_16b)
            {
                u32 uv=vri(vram, ptr);
                cv->u = f16((u16)uv);
                cv->v = f16((u16)(uv >> 16));
                ptr+=4;
            }
            else
            {
                cv->u=vrf(vram, ptr);ptr+=4;
                cv->v=vrf(vram, ptr);ptr+=4;
            }
        }

        //Color
        u32 col=vri(vram, ptr);ptr+=4;
        vert_packed_color_(cv->col,col);
        if (params->isp.Offset)
        {
            //Intensity color (can be missing too ;p)
            u32 col=vri(vram, ptr);ptr+=4;
            vert_packed_color_(cv->spc,col);
        }
    }

    // decode an object (params + vertexes)
    u32 decode_pvr_vetrices(DrawParameters* params, pvr32addr_t base, u32 skip, u32 shadow, Vertex* vtx, int count)
    {
        bool PSVM=FPU_SHAD_SCALE.intensity_shadow == 0;

        if (!PSVM) {
            shadow = 0; // no double volume stuff
        }

        params->isp.full=vri(vram, base);
        params->tsp.full=vri(vram, base+4);
        params->tcw.full=vri(vram, base+8);

        base += 12;
        if (shadow) {
            params->tsp2.full=vri(vram, base+0);
            params->tcw2.full=vri(vram, base+4);
            base += 8;
        }

        for (int i = 0; i < count; i++) {
            decode_pvr_vertex(params,base, &vtx[i]);
            base += (3 + skip * (shadow+1)) * 4;
        }

        return base;
    }


    // render a triangle strip object list entry
    template<RenderMode render_mode>
    void RenderTriangleStrip(RefRendInterface* backend, ObjectListEntry obj, RECT* rect)
    {
        Vertex vtx[8];
        DrawParameters params;

        u32 param_base = PARAM_BASE & 0xF00000;

        decode_pvr_vetrices(&params, param_base + obj.tstrip.param_offs_in_words * 4, obj.tstrip.skip, obj.tstrip.shadow, vtx, 8);


        for (int i = 0; i < 6; i++)
        {
            if (obj.tstrip.mask & (1 << (5-i)))
            {

                parameter_tag_t tag = backend->AddFpuEntry(&params, &vtx[i], render_mode);

                RenderTriangle<render_mode>(backend, &params, tag, i&1, vtx[i+0], vtx[i+1], vtx[i+2], rect);
            }
        }
    }


    // render a triangle array object list entry
    template<RenderMode render_mode>
    void RenderTriangleArray(RefRendInterface* backend, ObjectListEntry obj, RECT* rect)
    {
        auto triangles = obj.tarray.prims + 1;
        u32 param_base = PARAM_BASE & 0xF00000;


        u32 param_ptr = param_base + obj.tarray.param_offs_in_words * 4;
        
        for (int i = 0; i<triangles; i++)
        {
            DrawParameters params;
            Vertex vtx[3];

            param_ptr = decode_pvr_vetrices(&params, param_ptr, obj.tarray.skip, obj.tarray.shadow, vtx, 3);
            
            parameter_tag_t tag = 0;
            if (render_mode != RM_MODIFIER)
            {
                tag = backend->AddFpuEntry(&params, &vtx[0], render_mode);
            }

            RenderTriangle<render_mode>(backend, &params, tag, 0, vtx[0], vtx[1], vtx[2], rect);
        }
    }

    // render a quad array object list entry
    template<RenderMode render_mode>
    void RenderQuadArray(RefRendInterface* backend, ObjectListEntry obj, RECT* rect)
    {
        auto quads = obj.qarray.prims + 1;
        u32 param_base = PARAM_BASE & 0xF00000;


        u32 param_ptr = param_base + obj.qarray.param_offs_in_words * 4;
        
        for (int i = 0; i<quads; i++)
        {
            DrawParameters params;
            Vertex vtx[4];

            param_ptr = decode_pvr_vetrices(&params, param_ptr, obj.qarray.skip, obj.qarray.shadow, vtx, 4);
            
            parameter_tag_t tag = backend->AddFpuEntry(&params, &vtx[0], render_mode);

            //TODO: FIXME
            RenderTriangle<render_mode>(backend, &params, tag, 0, vtx[0], vtx[1], vtx[2], rect);
            RenderTriangle<render_mode>(backend, &params, tag, 1, vtx[0], vtx[3], vtx[2], rect);
        }
    }

    // Render an object list
    template<RenderMode render_mode>
    void RenderObjectList(RefRendInterface* backend, pvr32addr_t base, RECT* rect)
    {
        ObjectListEntry obj;

        for (;;) {
            obj.full = vri(vram, base);
            base += 4;

            if (!obj.is_not_triangle_strip) {
                RenderTriangleStrip<render_mode>(backend, obj, rect);
            } else {
                switch(obj.type) {
                    case 0b111: // link
                        if (obj.link.end_of_list)
                            return;

                            base = obj.link.next_block_ptr_in_words * 4;
                        break;

                    case 0b100: // triangle array
                        RenderTriangleArray<render_mode>(backend, obj, rect);
                        break;
                    
                    case 0b101: // quad array
                        RenderQuadArray<render_mode>(backend, obj, rect);
                        break;

                    default:
                        printf("RenderObjectList: Not handled object type: %d\n", obj.type);
                }
            }
        }
    }


    // Render a frame
    // Called on START_RENDER write
    virtual bool RenderPVR() {

        u32 base = REGION_BASE;

        RegionArrayEntry entry;
        int tilenum = 1;

        // Parse region array
        do {
            base += ReadRegionArrayEntry(base, &entry);
            EnqueueTile(entry.control.tiley * 64 + entry.control.tilex, [=](RefRendInterface* backend) {
                RECT rect;
                rect.top = entry.control.tiley * 32;
                rect.left = entry.control.tilex * 32;

                rect.bottom = rect.top + 32;
                rect.right = rect.left + 32;

                parameter_tag_t bgTag;

                // register BGPOLY to fpu
                {
                    DrawParameters params;
                    Vertex vtx[8];
                    decode_pvr_vetrices(&params, PARAM_BASE + ISP_BACKGND_T.tag_address * 4, ISP_BACKGND_T.skip, ISP_BACKGND_T.shadow, vtx, 8);
                    bgTag = backend->AddFpuEntry(&params, &vtx[ISP_BACKGND_T.tag_offset], RM_OPAQUE);
                }

                // Tile needs clear?
                if (!entry.control.z_keep)
                {
                    // Clear Param + Z + stencil buffers
                    backend->ClearBuffers(bgTag, ISP_BACKGND_D.f, 0);
                }

                // Render OPAQ to TAGS
                if (!entry.opaque.empty)
                {
                    RenderObjectList<RM_OPAQUE>(backend, entry.opaque.ptr_in_words * 4, &rect);
                }

                // render PT to TAGS
                if (!entry.puncht.empty)
                {
                    RenderObjectList<RM_PUNCHTHROUGH>(backend, entry.puncht.ptr_in_words * 4, &rect);
                }

                //TODO: Render OPAQ modvols
                if (!entry.opaque_mod.empty)
                {
                    RenderObjectList<RM_MODIFIER>(backend, entry.opaque_mod.ptr_in_words * 4, &rect);
                }

                // Render TAGS to ACCUM
                backend->RenderParamTags(RM_OPAQUE, rect.left, rect.top);

                // layer peeling rendering
                if (!entry.trans.empty)
                {

                    do
                    {
                        backend->ClearPixelsDrawn();

                        //copy depth test to depth reference buffer, clear depth test buffer, clear stencil, clear Param buffer
                        backend->PeelBuffers(0, FLT_MAX, 0);

                        // render to TAGS
                        RenderObjectList<RM_TRANSLUCENT>(backend, entry.trans.ptr_in_words * 4, &rect);

                        if (!entry.trans_mod.empty)
                        {
                            RenderObjectList<RM_MODIFIER>(backend, entry.trans_mod.ptr_in_words * 4, &rect);
                        }

                        // render TAGS to ACCUM
                        backend->RenderParamTags(RM_TRANSLUCENT, rect.left, rect.top);
                    } while (backend->GetPixelsDrawn() != 0);
                }

                // Copy to vram
                if (!entry.control.no_writeout)
                {

                    auto field = SCALER_CTL.fieldselect;
                    auto interlace = SCALER_CTL.interlace;

                    auto base = (interlace && field) ? FB_W_SOF2 : FB_W_SOF1;

                    // very few configurations supported here
                    verify(SCALER_CTL.hscale == 0);
                    verify(SCALER_CTL.interlace == 0); // write both SOFs
                    auto vscale = SCALER_CTL.vscalefactor;
                    verify(vscale == 0x401 || vscale == 0x400 || vscale == 0x800);

                    auto fb_packmode = FB_W_CTRL.fb_packmode;
                    verify(fb_packmode == 0x1); // 565 RGB16

                    auto bpp = 2;
                    auto offset_bytes = entry.control.tilex * 32 * bpp + entry.control.tiley * 32 * FB_W_LINESTRIDE.stride * 8;

                    auto src = backend->GetColorOutputBuffer();
                    for (int y = 0; y < 32; y++)
                    {
                        //auto base = (y&1) ? FB_W_SOF2 : FB_W_SOF1;
                        auto dst = base + offset_bytes + (y)*FB_W_LINESTRIDE.stride * 8;

                        for (int x = 0; x < 32; x++)
                        {
                            auto pixel = (((src[0] >> 3) & 0x1F) << 11) | (((src[1] >> 2) & 0x3F) << 5) | ((src[2] >> 3) & 0x1F);
                            pvr_write_area1_16(vram, dst, pixel);

                            dst += bpp;
                            src += 4; // skip alpha
                        }
                    }
                }
                // clear the tsp cache
                backend->ClearFpuEntries();
            });
        } while (!entry.control.last_region);

        for (int i = 0; i < MAX_CPU_COUNT; i++) {
            for(;;) {
                queue_lock.Lock();
                bool empty = queues[i].size() == 0;
                queue_lock.Unlock();

                if (empty)
                    break;
                else
                    SleepMs(1);
            }
        }

        return false;
    }

#if HOST_OS == OS_WINDOWS
    HWND hWnd;
    HBITMAP hBMP = 0, holdBMP;
    HDC hmem;
#endif


    virtual bool Init() {

        gles_init();

#if HOST_OS == OS_WINDOWS
        hWnd = (HWND)libPvr_GetRenderTarget();

        bi.biWidth = 640;
        bi.biHeight = 480;

        RECT rect;

        GetClientRect(hWnd, &rect);

        HDC hdc = GetDC(hWnd);

        FillRect(hdc, &rect, (HBRUSH)(COLOR_BACKGROUND));

        bi.biSizeImage = bi.biWidth * bi.biHeight * 4;

        hBMP = CreateCompatibleBitmap(hdc, bi.biWidth, bi.biHeight);
        hmem = CreateCompatibleDC(hdc);
        holdBMP = (HBITMAP)SelectObject(hmem, hBMP);
        ReleaseDC(hWnd, hdc);
#endif
#define REP_16(x) ((x)* 16 + (x))
#define REP_32(x) ((x)* 8 + (x)/4)
#define REP_64(x) ((x)* 4 + (x)/16)

        for (int c = 0; c < 65536; c++) {
            //565
            decoded_colors[0][c] = 0xFF000000 | (REP_32((c >> 11) % 32) << 16) | (REP_64((c >> 5) % 64) << 8) | (REP_32((c >> 0) % 32) << 0);
            //1555
            decoded_colors[1][c] = ((c >> 0) % 2 * 255 << 24) | (REP_32((c >> 11) % 32) << 16) | (REP_32((c >> 6) % 32) << 8) | (REP_32((c >> 1) % 32) << 0);
            //4444
            decoded_colors[2][c] = (REP_16((c >> 0) % 16) << 24) | (REP_16((c >> 12) % 16) << 16) | (REP_16((c >> 8) % 16) << 8) | (REP_16((c >> 4) % 16) << 0);
        }

        return true;
    }

    virtual void Resize(int w, int h) {

    }

    ~refrend() {

        running = false;
        for (auto &thread: threads) {
            thread.WaitToEnd();
        }
#if HOST_OS == OS_WINDOWS
        if (hBMP) {
            DeleteObject(SelectObject(hmem, holdBMP));
            DeleteDC(hmem);
        }
#endif
    }

    virtual void Present()
    {

        if (FB_R_SIZE.fb_x_size == 0 || FB_R_SIZE.fb_y_size == 0)
            return;

        int width = (FB_R_SIZE.fb_x_size + 1) << 1; // in 16-bit words
        int height = FB_R_SIZE.fb_y_size + 1;
        int modulus = (FB_R_SIZE.fb_modulus - 1) << 1;

        int bpp;
        switch (FB_R_CTRL.fb_depth)
        {
        case fbde_0555:
        case fbde_565:
            bpp = 2;
            break;
        case fbde_888:
            bpp = 3;
            width = (width * 2) / 3;     // in pixels
            modulus = (modulus * 2) / 3; // in pixels
            break;
        case fbde_C888:
            bpp = 4;
            width /= 2;   // in pixels
            modulus /= 2; // in pixels
            break;
        default:
            die("Invalid framebuffer format\n");
            bpp = 4;
            break;
        }
        u32 addr = SPG_CONTROL.interlace && !SPG_STATUS.fieldnum ? FB_R_SOF2 : FB_R_SOF1;

        PixelBuffer<u32> pb;
        pb.init(width, height);
        u8 *dst = (u8 *)pb.data();

        switch (FB_R_CTRL.fb_depth)
        {
        case fbde_0555: // 555 RGB
            for (int y = 0; y < height; y++)
            {
                for (int i = 0; i < width; i++)
                {
                    u16 src = pvr_read_area1_16(sh4_cpu->vram.data, addr);
                    *dst++ = (((src >> 10) & 0x1F) << 3) + FB_R_CTRL.fb_concat;
                    *dst++ = (((src >> 5) & 0x1F) << 3) + FB_R_CTRL.fb_concat;
                    *dst++ = (((src >> 0) & 0x1F) << 3) + FB_R_CTRL.fb_concat;
                    *dst++ = 0xFF;
                    addr += bpp;
                }
                addr += modulus * bpp;
            }
            break;

        case fbde_565: // 565 RGB
            for (int y = 0; y < height; y++)
            {
                for (int i = 0; i < width; i++)
                {
                    u16 src = pvr_read_area1_16(sh4_cpu->vram.data, addr);
                    *dst++ = (((src >> 11) & 0x1F) << 3) + FB_R_CTRL.fb_concat;
                    *dst++ = (((src >> 5) & 0x3F) << 2) + (FB_R_CTRL.fb_concat >> 1);
                    *dst++ = (((src >> 0) & 0x1F) << 3) + FB_R_CTRL.fb_concat;
                    *dst++ = 0xFF;
                    addr += bpp;
                }
                addr += modulus * bpp;
            }
            break;
        case fbde_888: // 888 RGB
            for (int y = 0; y < height; y++)
            {
                for (int i = 0; i < width; i++)
                {
                    if (addr & 1)
                    {
                        u32 src = pvr_read_area1_32(sh4_cpu->vram.data, addr - 1);
                        *dst++ = src >> 16;
                        *dst++ = src >> 8;
                        *dst++ = src;
                    }
                    else
                    {
                        u32 src = pvr_read_area1_32(sh4_cpu->vram.data, addr);
                        *dst++ = src >> 24;
                        *dst++ = src >> 16;
                        *dst++ = src >> 8;
                    }
                    *dst++ = 0xFF;
                    addr += bpp;
                }
                addr += modulus * bpp;
            }
            break;
        case fbde_C888: // 0888 RGB
            for (int y = 0; y < height; y++)
            {
                for (int i = 0; i < width; i++)
                {
                    u32 src = pvr_read_area1_32(sh4_cpu->vram.data, addr);
                    *dst++ = src >> 16;
                    *dst++ = src >> 8;
                    *dst++ = src;
                    *dst++ = 0xFF;
                    addr += bpp;
                }
                addr += modulus * bpp;
            }
            break;
        }
        u32 *psrc = pb.data();

#if HOST_OS == OS_WINDOWS
        SetDIBits(hmem, hBMP, 0, 480, psrc, (BITMAPINFO*)& bi, DIB_RGB_COLORS);

        RECT clientRect;

        GetClientRect(hWnd, &clientRect);

        HDC hdc = GetDC(hWnd);
        int w = clientRect.right - clientRect.left;
        int h = clientRect.bottom - clientRect.top;
        int x = (w - 640) / 2;
        int y = (h - 480) / 2;

        StretchBlt(hdc, x, y+480, 640, -480, hmem, 0, 0, 640, 480, SRCCOPY);
        ReleaseDC(hWnd, hdc);
#elif defined(SUPPORT_X11)
        extern Window x11_win;
        extern Display* x11_disp;
        extern Visual* x11_vis;

        extern int x11_width;
        extern int x11_height;

        XImage* ximage = XCreateImage(x11_disp, x11_vis, 24, ZPixmap, 0, (char*)psrc, width, height, 32, width * 4);

        GC gc = XCreateGC(x11_disp, x11_win, 0, 0);
        XPutImage(x11_disp, x11_win, gc, ximage, 0, 0, (x11_width - width) / 2, (x11_height - height) / 2, width, height);
        XFree(ximage);
        XFreeGC(x11_disp, gc);
#else
        // TODO softrend without X11 (SDL f.e.)
    die("Softrend doesn't know how to update the screen");
#endif
    }
};

