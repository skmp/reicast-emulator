using System;
namespace Reidbg
{
    // Copied from refrend_debug.cpp
    // Keep in sync
    public enum RRI_DebugCommands
    {
        RRIBC_BAD = 0,
        RRIBC_OK = 1,

        RRIBC_Hello = 2,

        RRIBC_SetStep,
        RRIBC_GetBufferData,
        RRIBC_GetTextureData,

        RRIBC_StepNotification,
        RRIBC_TileNotification,
        RRIBC_FrameNotification,

        RRIBC_ClearBuffers,
        RRIBC_PeelBuffers,
        RRIBC_SummarizeStencilOr,
        RRIBC_SummarizeStencilAnd,
        RRIBC_ClearPixelsDrawn,
        RRIBC_GetPixelsDrawn,
        RRIBC_AddFpuEntry,
        RRIBC_ClearFpuEntries,
        RRIBC_GetColorOutputBuffer,
        RRIBC_RenderParamTags,
        RRIBC_RasterizeTriangle,
    };

    // Keep in sync with refrend_base.h
    public struct DrawParams
    {
        public UInt32 isp;
        public UInt32 tsp;
        public UInt32 tcw;
        public UInt32 tsp2;
        public UInt32 tcw2;
    }

    // Keep in sync with ta_ctx.h
    public struct Vertex
    {
        public float x, y, z;

        public UInt32 col;
        public UInt32 spc;

        public float u, v;

        public UInt32 col1;
        public UInt32 spc1;

        public float u1, v1;
    }
}
