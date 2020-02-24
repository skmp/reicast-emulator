
#include "refrend_base.h"
#include <memory>

struct RefRendDebug: RefRendInterface
{
    unique_ptr<RefRendInterface> backend;
    RefRendDebug(RefRendInterface* backend) {
        this->backend.reset(backend);
    }

    // backend specific init
    virtual bool Init() {
        return backend->Init();
    }

    // Clear the buffers
    virtual void ClearBuffers(u32 paramValue, float depthValue, u32 stencilValue) {
        backend->ClearBuffers(paramValue, depthValue, stencilValue);
    }

    // Clear and set DEPTH2 for peeling
    virtual void PeelBuffers(u32 paramValue, float depthValue, u32 stencilValue) {
        backend->PeelBuffers(paramValue, depthValue, stencilValue);
    }

    // Summarize tile after rendering modvol (inside)
    virtual void SummarizeStencilOr() {
        backend->SummarizeStencilOr();
    }
    
    // Summarize tile after rendering modvol (outside)
    virtual void SummarizeStencilAnd() {
        backend->SummarizeStencilAnd();
    }

    // Clear the pixel drawn counter
    virtual void ClearPixelsDrawn() {
        backend->ClearPixelsDrawn();
    }

    // Get the pixel drawn counter. Used during layer peeling to determine when to stop processing
    virtual u32 GetPixelsDrawn() {
        return backend->GetPixelsDrawn();
    }

    // Add an entry to the fpu parameters list
    virtual parameter_tag_t AddFpuEntry(DrawParameters* params, Vertex* vtx, RenderMode render_mode) {
        return backend->AddFpuEntry(params, vtx, render_mode);
    }

    // Clear the fpu parameters list
    virtual void ClearFpuEntries() {
        backend->ClearFpuEntries();
    }

    // Get the final output of the 32x32 tile. Used to write to the VRAM framebuffer
    virtual u8* GetColorOutputBuffer() {
        return backend->GetColorOutputBuffer();
    }

    // Render to ACCUM from TAG buffer
    // TAG holds references to triangles, ACCUM is the tile framebuffer
    virtual void RenderParamTags(RenderMode rm, int tileX, int tileY) {
        backend->RenderParamTags(rm, tileX, tileY);
    }

    // RasterizeTriangle
    virtual void RasterizeTriangle(RenderMode render_mode, DrawParameters* params, parameter_tag_t tag, int vertex_offset, const Vertex& v1, const Vertex& v2, const Vertex& v3, const Vertex* v4, taRECT* area) {
        backend->RasterizeTriangle(render_mode, params, tag, vertex_offset, v1, v2, v3, v4, area);
    }

    virtual ~RefRendDebug() {
        backend.reset();    //explicit for order of de-init
    };
};

RefRendInterface*  rend_refred_debug(RefRendInterface* backend) {
    return new RefRendDebug(backend);
}