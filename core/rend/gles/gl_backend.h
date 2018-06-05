#pragma once
#include "rend/rend.h"

#define VERTEX_POS_ARRAY      0
#define VERTEX_COL_BASE_ARRAY 1
#define VERTEX_COL_OFFS_ARRAY 2
#define VERTEX_UV_ARRAY       3

//vertex types
extern u32 gcflip;
extern float scale_x, scale_y;

struct PipelineShader
{
	GLuint program;
	GLuint scale;
   GLuint depth_scale;
	GLuint pp_ClipTest;
   GLuint cp_AlphaTestValue;
	GLuint sp_FOG_COL_RAM;
   GLuint sp_FOG_COL_VERT;
   GLuint sp_FOG_DENSITY;
	u32 cp_AlphaTest;
   s32 pp_ClipTestMode;
	u32 pp_Texture;
   u32 pp_UseAlpha;
   u32 pp_IgnoreTexA;
   u32 pp_ShadInstr;
   u32 pp_Offset;
   u32 pp_FogCtrl;
};

GLuint gl_GetTexture(TSP tsp,TCW tcw);
struct text_info {
	u16* pdata;
	u32 width;
	u32 height;
	u32 textype; // 0 565, 1 1555, 2 4444
};

text_info raw_GetTexture(TSP tsp, TCW tcw);
void CollectCleanup();
void DoCleanup();
void SortPParams(int first, int count);

void BindRTT(u32 addy, u32 fbw, u32 fbh, u32 channels, u32 fmt);
void ReadRTTBuffer();
int GetProgramID(u32 cp_AlphaTest, u32 pp_ClipTestMode,
							u32 pp_Texture, u32 pp_UseAlpha, u32 pp_IgnoreTexA, u32 pp_ShadInstr, u32 pp_Offset,
							u32 pp_FogCtrl);

bool CompilePipelineShader(PipelineShader* s);
