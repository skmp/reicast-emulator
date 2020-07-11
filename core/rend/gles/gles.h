#pragma once
#include <unordered_map>
#include <atomic>
#include <libretro.h>
#include "rend/rend.h"
#include "rend/TexCache.h"
#include <glsm/glsm.h>
#include <glsm/glsmsym.h>
#include "glcache.h"

#ifndef TEXTURE_MAX_ANISOTROPY_EXT
#define TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#endif
#ifndef MAX_TEXTURE_MAX_ANISOTROPY_EXT
#define MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif
#ifndef GL_TEXTURE_BASE_LEVEL
#define GL_TEXTURE_BASE_LEVEL                   0x813C
#endif
#ifndef GL_TEXTURE_MAX_LEVEL
#define GL_TEXTURE_MAX_LEVEL                    0x813D
#endif


#define VERTEX_POS_ARRAY      0
#define VERTEX_COL_BASE_ARRAY 1
#define VERTEX_COL_OFFS_ARRAY 2
#define VERTEX_UV_ARRAY       3
// OIT only
#define VERTEX_COL_BASE1_ARRAY 4
#define VERTEX_COL_OFFS1_ARRAY 5
#define VERTEX_UV1_ARRAY 6

#define glCheck()

//vertex types
extern u32 gcflip;
extern float scale_x, scale_y;

void DrawStrips(void);

struct PipelineShader
{
   GLuint program;
   GLint scale,depth_scale;
   GLint extra_depth_scale;
   GLint pp_ClipTest;
   GLint cp_AlphaTestValue;
   GLint sp_FOG_COL_RAM;
   GLint sp_FOG_COL_VERT;
   GLint sp_FOG_DENSITY;
   GLint trilinear_alpha;
   GLint fog_clamp_min, fog_clamp_max;
   GLint palette_index;
   
   //
   bool cp_AlphaTest;
   s32 pp_ClipTestMode;
   bool pp_Texture;
   bool pp_UseAlpha;
   bool pp_IgnoreTexA;
   u32 pp_ShadInstr;
   bool pp_Offset;
   u32 pp_FogCtrl;
   bool pp_Gouraud;
   bool pp_BumpMap;
   bool fog_clamping;
   bool trilinear;
   bool palette;
};



struct gl_ctx
{
	struct
	{
		GLuint program;

		GLint scale,depth_scale;
      GLint extra_depth_scale;
		GLint sp_ShaderColor;

	} modvol_shader;

	std::unordered_map<u32, PipelineShader> shaders;

	struct
	{
		GLuint geometry,modvols,idxs,idxs2;
	} vbo;

	struct
	{
		u32 TexAddr;
		GLuint depthb;
		GLuint tex;
		GLuint fbo;
	} rtt;

   const char *gl_version;
   const char *glsl_version_header;
   int gl_major;
   int gl_minor;
   bool is_gles;
   GLuint single_channel_format;
   GLenum index_type;
   bool stencil_present;
   f32 max_anisotropy;

   size_t get_index_size() { return index_type == GL_UNSIGNED_INT ? sizeof(u32) : sizeof(u16); }
};

extern gl_ctx gl;
extern GLuint fbTextureId;
extern float fb_scale_x, fb_scale_y;

u64 gl_GetTexture(TSP tsp,TCW tcw);
struct text_info {
	u16* pdata;
	u32 width;
	u32 height;
	u32 textype; // 0 565, 1 1555, 2 4444
};

enum ModifierVolumeMode { Xor, Or, Inclusion, Exclusion, ModeCount };

bool ProcessFrame(TA_context* ctx);
void UpdateFogTexture(u8 *fog_table, GLenum texture_slot, GLint fog_image_format);
void UpdatePaletteTexture(GLenum texture_slot);
text_info raw_GetTexture(TSP tsp, TCW tcw);
void DoCleanup();
void SortPParams(int first, int count);
void SetCull(u32 CullMode);
s32 SetTileClip(u32 val, GLint uniform);
void SetMVS_Mode(ModifierVolumeMode mv_mode, ISP_Modvol ispc);

void BindRTT(u32 addy, u32 fbw, u32 fbh, u32 channels, u32 fmt);
void ReadRTTBuffer();
void RenderFramebuffer();
void DrawFramebuffer(float w, float h);

PipelineShader *GetProgram(u32 cp_AlphaTest, u32 pp_ClipTestMode,
							u32 pp_Texture, u32 pp_UseAlpha, u32 pp_IgnoreTexA, u32 pp_ShadInstr, u32 pp_Offset,
							u32 pp_FogCtrl, bool pp_Gouraud, bool pp_BumpMap, bool fog_clamping, bool trilinear,
							bool palette);
void vertex_buffer_unmap(void);

void findGLVersion();
GLuint gl_CompileShader(const char* shader, GLuint type);
GLuint gl_CompileAndLink(const char* VertexShader, const char* FragmentShader);
bool CompilePipelineShader(PipelineShader* s);
void co_dc_yield(void);
void vertex_buffer_unmap();

extern GLuint vmuTextureId[4];
extern GLuint lightgunTextureId[4];
void UpdateVmuTexture(int vmu_screen_number);

extern struct ShaderUniforms_t
{
	float PT_ALPHA;
	float scale_coefs[4];
	float depth_coefs[4];
   float extra_depth_scale;
	float fog_den_float;
	float ps_FOG_COL_RAM[3];
	float ps_FOG_COL_VERT[3];
	float trilinear_alpha;
   float fog_clamp_min[4];
	float fog_clamp_max[4];
	float palette_index;

	void Set(const PipelineShader* s)
	{
		if (s->cp_AlphaTestValue!=-1)
			glUniform1f(s->cp_AlphaTestValue,PT_ALPHA);

		if (s->scale!=-1)
			glUniform4fv( s->scale, 1, scale_coefs);

		if (s->depth_scale!=-1)
			glUniform4fv( s->depth_scale, 1, depth_coefs);

      if (s->extra_depth_scale != -1)
			glUniform1f(s->extra_depth_scale, extra_depth_scale);

		if (s->sp_FOG_DENSITY!=-1)
			glUniform1f(s->sp_FOG_DENSITY, fog_den_float);

		if (s->sp_FOG_COL_RAM!=-1)
			glUniform3fv( s->sp_FOG_COL_RAM, 1, ps_FOG_COL_RAM);

		if (s->sp_FOG_COL_VERT!=-1)
			glUniform3fv( s->sp_FOG_COL_VERT, 1, ps_FOG_COL_VERT);

      if (s->fog_clamp_min != -1)
			glUniform4fv(s->fog_clamp_min, 1, fog_clamp_min);
		if (s->fog_clamp_max != -1)
			glUniform4fv(s->fog_clamp_max, 1, fog_clamp_max);

		if (s->palette_index != -1)
			glUniform1f(s->palette_index, palette_index);
	}

} ShaderUniforms;

class TextureCacheData : public BaseTextureCacheData
{
public:
	GLuint texID;   //gl texture
	u16* pData;
	virtual std::string GetId() override { return std::to_string(texID); }
	virtual void UploadToGPU(int width, int height, u8 *temp_tex_buffer, bool mipmapped, bool mipmapsIncluded = false) override;
	virtual bool Delete() override;
};

class GlTextureCache : public BaseTextureCache<TextureCacheData>
{
};
extern GlTextureCache TexCache;

extern const u32 Zfunction[8];
extern const u32 SrcBlendGL[], DstBlendGL[];

extern "C" struct retro_hw_render_callback hw_render;
