#pragma once
#include "rend/rend.h"
#include <glsm/glsm.h>
#include <glsm/glsmsym.h>
#include <map>
#include "glcache.h"

#define VERTEX_POS_ARRAY      0
#define VERTEX_COL_BASE_ARRAY 1
#define VERTEX_COL_OFFS_ARRAY 2
#define VERTEX_UV_ARRAY       3

#define glCheck()

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
	GLuint sp_FOG_COL_RAM,sp_FOG_COL_VERT,sp_FOG_DENSITY;
   GLuint shade_scale_factor;
	GLuint screen_size;
   GLuint blend_mode;
   GLuint pp_Number;

   //
	u32 cp_AlphaTest;
   s32 pp_ClipTestMode;
   u32 pp_Texture, pp_UseAlpha, pp_IgnoreTexA, pp_ShadInstr, pp_Offset, pp_FogCtrl;
   int pass;
};



struct gl_ctx
{
	struct
	{
		GLuint program;

		GLuint scale,depth_scale;

	} modvol_shader;

   std::map<int, PipelineShader *> shaders;

	struct
	{
		GLuint geometry,modvols,idxs,idxs2;
#ifndef GLES
		GLuint vao;
#endif
	} vbo;

   PipelineShader *getShader(int programId) {
      PipelineShader *shader = shaders[programId];
		if (shader == NULL) {
			shader = new PipelineShader();
			shaders[programId] = shader;
			shader->program = -1;
		}
		return shader;
	}
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

extern int screen_width;
extern int screen_height;

void BindRTT(u32 addy, u32 fbw, u32 fbh, u32 channels, u32 fmt);
void ReadRTTBuffer();
int GetProgramID(u32 cp_AlphaTest, u32 pp_ClipTestMode,
							u32 pp_Texture, u32 pp_UseAlpha, u32 pp_IgnoreTexA, u32 pp_ShadInstr, u32 pp_Offset,
							u32 pp_FogCtrl, int pass);

typedef struct _ShaderUniforms_t
{
	float PT_ALPHA;
	float scale_coefs[4];
	float depth_coefs[4];
	float fog_den_float;
	float ps_FOG_COL_RAM[3];
	float ps_FOG_COL_VERT[3];
   GLuint blend_mode[2];
   int poly_number;

   void Set(PipelineShader* s)
   {
      if (s->cp_AlphaTestValue!=-1)
         glUniform1f(s->cp_AlphaTestValue, PT_ALPHA);

      if (s->scale!=-1)
         glUniform4fv( s->scale, 1, scale_coefs);

      if (s->depth_scale!=-1)
         glUniform4fv( s->depth_scale, 1, depth_coefs);

      if (s->sp_FOG_DENSITY!=-1)
         glUniform1f( s->sp_FOG_DENSITY, fog_den_float);

      if (s->sp_FOG_COL_RAM!=-1)
         glUniform3fv( s->sp_FOG_COL_RAM, 1, ps_FOG_COL_RAM);

      if (s->sp_FOG_COL_VERT!=-1)
         glUniform3fv( s->sp_FOG_COL_VERT, 1, ps_FOG_COL_VERT);

      if (s->screen_size != -1)
			glUniform2f(s->screen_size, (float)screen_width, (float)screen_height);

		if (s->shade_scale_factor != -1)
			glUniform1f(s->shade_scale_factor, FPU_SHAD_SCALE.scale_factor / 256.f);
      if (s->blend_mode != -1)
			glUniform2uiv(s->blend_mode, 1, blend_mode);

      if (s->pp_Number != -1)
			glUniform1i(s->pp_Number, poly_number);
   }
} _ShaderUniforms;
extern struct _ShaderUniforms_t ShaderUniforms;

extern const char *PixelPipelineShader;
bool CompilePipelineShader(PipelineShader* s, const char *source = PixelPipelineShader);

void vertex_buffer_unmap(void);

extern "C" struct retro_hw_render_callback hw_render;

extern GLuint stencilTexId;
extern GLuint depthTexId;
extern GLuint opaqueTexId;

#define ABUFFER_SIZE 32
#define ABUFFER_SIZE_STR "32"
