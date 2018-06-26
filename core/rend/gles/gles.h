#pragma once
#include "rend/rend.h"
#include <glsm/glsm.h>
#include <glsm/glsmsym.h>
#include "glcache.h"

#define VERTEX_POS_ARRAY      0
#define VERTEX_COL_BASE_ARRAY 1
#define VERTEX_COL_OFFS_ARRAY 2
#define VERTEX_UV_ARRAY       3

//vertex types
extern float scale_x, scale_y;

void DrawStrips(void);

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



struct gl_ctx
{
	struct
	{
		GLuint program;

		GLuint scale,depth_scale;
		GLuint sp_ShaderColor;

	} modvol_shader;

	PipelineShader program_table[768*2];

	struct
	{
		GLuint geometry,modvols,idxs,idxs2;
#ifndef GLES
		GLuint vao;
#endif
	} vbo;

	//GLuint matrix;
};

GLuint gl_GetTexture(TSP tsp,TCW tcw);
struct text_info {
	u16* pdata;
	u32 width;
	u32 height;
	u32 textype; // 0 565, 1 1555, 2 4444
};

extern gl_ctx gl;

struct modvol_shader_type
{
   GLuint program;
   GLuint scale;
   GLuint depth_scale;
   GLuint sp_ShaderColor;
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
void vertex_buffer_unmap(void);

bool CompilePipelineShader(PipelineShader* s);
enum ModifierVolumeMode { Xor, Or, Inclusion, Exclusion, ModeCount };
