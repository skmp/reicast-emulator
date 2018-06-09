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
#define VERTEX_COL_BASE1_ARRAY 4
#define VERTEX_COL_OFFS1_ARRAY 5
#define VERTEX_UV1_ARRAY 6

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
   GLuint sp_FOG_COL_RAM,sp_FOG_COL_VERT,sp_FOG_DENSITY,sp_LOG_FOG_COEFS;
   GLuint shade_scale_factor;
	GLuint screen_size;
   GLuint blend_mode;
   GLuint pp_Number;
   GLuint pp_Stencil;
   GLuint blend_mode0, blend_mode1;
	GLuint use_alpha0, use_alpha1;
	GLuint ignore_tex_alpha0, ignore_tex_alpha1;
	GLuint shading_instr0, shading_instr1;
	GLuint fog_control0, fog_control1;

   //
	u32 cp_AlphaTest;
   s32 pp_ClipTestMode;
   u32 pp_Texture, pp_UseAlpha, pp_IgnoreTexA, pp_ShadInstr, pp_Offset, pp_FogCtrl;
   int pass;
   bool pp_TwoVolumes;
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
							u32 pp_FogCtrl, bool two_volumes, int pass);
void SetCull(u32 CulliMode);

typedef struct _ShaderUniforms_t
{
	float PT_ALPHA;
	float scale_coefs[4];
	float depth_coefs[4];
	float fog_den_float;
	float ps_FOG_COL_RAM[3];
	float ps_FOG_COL_VERT[3];
   float fog_coefs[2];
   int poly_number;
   u32 stencil;
   TSP tsp0;
	TSP tsp1;
	TCW tcw0;
	TCW tcw1;

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

      if (s->sp_LOG_FOG_COEFS!=-1)
			glUniform2fv(s->sp_LOG_FOG_COEFS,1, fog_coefs);

      if (s->screen_size != -1)
			glUniform2f(s->screen_size, (float)screen_width, (float)screen_height);

		if (s->shade_scale_factor != -1)
			glUniform1f(s->shade_scale_factor, FPU_SHAD_SCALE.scale_factor / 256.f);
      if (s->blend_mode0 != -1) {
			u32 blend_mode[2] = { tsp0.SrcInstr, tsp0.DstInstr };
			glUniform2uiv(s->blend_mode0, 1, blend_mode);
		}
		if (s->blend_mode1 != -1) {
			u32 blend_mode[2] = { tsp1.SrcInstr, tsp1.DstInstr };
			glUniform2uiv(s->blend_mode1, 1, blend_mode);
		}

		if (s->use_alpha0 != -1)
			glUniform1i(s->use_alpha0, tsp0.UseAlpha);
		if (s->use_alpha1 != -1)
			glUniform1i(s->use_alpha1, tsp1.UseAlpha);

		if (s->ignore_tex_alpha0 != -1)
			glUniform1i(s->ignore_tex_alpha0, tsp0.IgnoreTexA);
		if (s->ignore_tex_alpha1 != -1)
			glUniform1i(s->ignore_tex_alpha1, tsp1.IgnoreTexA);

		if (s->shading_instr0 != -1)
			glUniform1i(s->shading_instr0, tsp0.ShadInstr);
		if (s->shading_instr1 != -1)
			glUniform1i(s->shading_instr1, tsp1.ShadInstr);

		if (s->fog_control0 != -1)
			glUniform1i(s->fog_control0, tsp0.FogCtrl);
		if (s->fog_control1 != -1)
			glUniform1i(s->fog_control1, tsp1.FogCtrl);

      if (s->pp_Number != -1)
			glUniform1i(s->pp_Number, poly_number);

      if (s->pp_Stencil != -1)
			glUniform1ui(s->pp_Stencil, stencil);
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

// Must match!
// in bytes
#define ABUFFER_SIZE 256*1024*1024
#define ABUFFER_SIZE_STR "(256u * 1024u * 1024u)"

#define SHADER_HEADER "#version 140 \n\
#extension GL_EXT_shader_image_load_store : enable \n\
#extension GL_ARB_shader_storage_buffer_object : enable \n\
#extension GL_ARB_shader_atomic_counters : enable \n\
#extension GL_ARB_shader_image_size : enable \n\
#extension GL_ARB_shading_language_420pack : enable \n\
\n\
#define ABUFFER_SIZE " ABUFFER_SIZE_STR " \n\
coherent uniform layout(size1x32, binding = 4) uimage2D abufferPointerImg; \n\
struct Pixel { \n\
	mediump vec4 color; \n\
	mediump float depth; \n\
	int seq_num; \n\
	uint blend_stencil; \n\
	uint next; \n\
}; \n\
#define EOL 0xFFFFFFFFu \n\
layout (binding = 0, std430) buffer PixelBuffer { \n\
	Pixel pixels[]; \n\
}; \n\
layout(binding = 0, offset = 0) uniform atomic_uint buffer_index; \n\
\n\
void setFragDepth(void) \n\
{ \n\
	highp float w = 100000.0 * gl_FragCoord.w; \n\
	gl_FragDepth = 1 - log2(1.0 + w) / 34; \n\
} \n\
"

void SetupModvolVBO();
