#include <math.h>

#include <memalign.h>

#include <algorithm>

#include <glsm/glsm.h>
#include <glsm/glsmsym.h>

#ifdef __SSE4_1__
#include <xmmintrin.h>
#endif

#include "gl_backend.h"
#include "../rend.h"
#include "../../libretro/libretro.h"
#include "../TexCache.h"
#include "glcache.h"

#include "../../hw/pvr/pvr.h"
#include "../../hw/mem/_vmem.h"
#include "../../hw/pvr/tr.h"

#ifndef GL_IMPLEMENTATION_COLOR_READ_TYPE
#define GL_IMPLEMENTATION_COLOR_READ_TYPE 0x8B9A
#endif

#ifndef GL_IMPLEMENTATION_COLOR_READ_FORMAT
#define GL_IMPLEMENTATION_COLOR_READ_FORMAT 0x8B9B
#endif

extern retro_environment_t environ_cb;
extern bool fog_needs_update;
extern bool enable_rtt;
bool KillTex=false;
GLCache glcache;

struct modvol_shader_type
{
   GLuint program;
   GLuint scale;
   GLuint depth_scale;
   GLuint sp_ShaderColor;
};

struct ShaderUniforms_t
{
	float PT_ALPHA;
	float scale_coefs[4];
	float depth_coefs[4];
	float fog_den_float;
	float ps_FOG_COL_RAM[3];
	float ps_FOG_COL_VERT[3];
	float fog_coefs[2];
} ShaderUniforms;

struct vbo_type
{
   GLuint geometry;
   GLuint modvols;
   GLuint idxs;
   GLuint idxs2;
};

vbo_type vbo;
modvol_shader_type modvol_shader;
PipelineShader program_table[768*2];

/*

Drawing and related state management
Takes vertex, textures and renders to the currently set up target
*/

const static u32 CullMode[]= 
{

	GL_NONE, //0    No culling          No culling
	GL_NONE, //1    Cull if Small       Cull if ( |det| < fpu_cull_val )

	GL_FRONT, //2   Cull if Negative    Cull if ( |det| < 0 ) or ( |det| < fpu_cull_val )
	GL_BACK,  //3   Cull if Positive    Cull if ( |det| > 0 ) or ( |det| < fpu_cull_val )
};
const static u32 Zfunction[]=
{
	GL_NEVER,      //GL_NEVER,              //0 Never
	GL_LESS,        //GL_LESS/*EQUAL*/,     //1 Less
	GL_EQUAL,       //GL_EQUAL,             //2 Equal
	GL_LEQUAL,      //GL_LEQUAL,            //3 Less Or Equal
	GL_GREATER,     //GL_GREATER/*EQUAL*/,  //4 Greater
	GL_NOTEQUAL,    //GL_NOTEQUAL,          //5 Not Equal
	GL_GEQUAL,      //GL_GEQUAL,            //6 Greater Or Equal
	GL_ALWAYS,      //GL_ALWAYS,            //7 Always
};

/*
0   Zero                  (0, 0, 0, 0)
1   One                   (1, 1, 1, 1)
2   Dither Color          (OR, OG, OB, OA) 
3   Inverse Dither Color  (1-OR, 1-OG, 1-OB, 1-OA)
4   SRC Alpha             (SA, SA, SA, SA)
5   Inverse SRC Alpha     (1-SA, 1-SA, 1-SA, 1-SA)
6   DST Alpha             (DA, DA, DA, DA)
7   Inverse DST Alpha     (1-DA, 1-DA, 1-DA, 1-DA)
*/

const static u32 DstBlendGL[] =
{
	GL_ZERO,
	GL_ONE,
	GL_SRC_COLOR,
	GL_ONE_MINUS_SRC_COLOR,
	GL_SRC_ALPHA,
	GL_ONE_MINUS_SRC_ALPHA,
	GL_DST_ALPHA,
	GL_ONE_MINUS_DST_ALPHA
};

const static u32 SrcBlendGL[] =
{
	GL_ZERO,
	GL_ONE,
	GL_DST_COLOR,
	GL_ONE_MINUS_DST_COLOR,
	GL_SRC_ALPHA,
	GL_ONE_MINUS_SRC_ALPHA,
	GL_DST_ALPHA,
	GL_ONE_MINUS_DST_ALPHA
};

Vertex* vtx_sort_base;

struct IndexTrig
{
	u16 id[3];
	u16 pid;
	f32 z;
};

struct SortTrigDrawParam
{
	PolyParam* ppid;
	u16 first;
	u16 count;
};

static vector<SortTrigDrawParam>	pidx_sort;
PipelineShader* CurrentShader;
u32 gcflip;

float fb_scale_x = 0.0f;
float fb_scale_y = 0.0f;
float scale_x, scale_y;

#define attr "attribute"
#define vary "varying"
#define FRAGCOL "gl_FragColor"
#define TEXLOOKUP "texture2D"

#ifdef HAVE_OPENGLES
#define HIGHP "highp"
#define MEDIUMP "mediump"
#define LOWP "lowp"
#else
#define HIGHP
#define MEDIUMP
#define LOWP
#endif

//Fragment and vertex shaders code
//pretty much 1:1 copy of the d3d ones for now
const char* VertexShaderSource =
#ifndef HAVE_OPENGLES
   "#version 120 \n"
#endif
"\
/* Vertex constants*/  \n\
uniform " HIGHP " vec4      scale; \n\
uniform " HIGHP " vec4      depth_scale; \n\
/* Vertex input */ \n\
" attr " " HIGHP " vec4    in_pos; \n\
" attr " " LOWP " vec4     in_base; \n\
" attr " " LOWP " vec4     in_offs; \n\
" attr " " MEDIUMP " vec2  in_uv; \n\
/* output */ \n\
" vary " " LOWP " vec4 vtx_base; \n\
" vary " " LOWP " vec4 vtx_offs; \n\
" vary " " MEDIUMP " vec2 vtx_uv; \n\
void main() \n\
{ \n\
	vtx_base=in_base; \n\
	vtx_offs=in_offs; \n\
	vtx_uv=in_uv; \n\
	vec4 vpos=in_pos; \n\
	vpos.w=1.0/vpos.z;  \n"
#ifndef GLES
	"\
   if (vpos.w < 0.0) { \n\
      gl_Position = vec4(0.0, 0.0, 0.0, vpos.w); \n\
         return; \n\
   } \n\
   vpos.z = vpos.w; \n"
#else
	"\
   vpos.z=depth_scale.x+depth_scale.y*vpos.w;  \n"
#endif
   "\
	vpos.xy=vpos.xy*scale.xy-scale.zw;  \n\
	vpos.xy*=vpos.w;  \n\
	gl_Position = vpos; \n\
}";

const char* PixelPipelineShader =
#ifndef HAVE_OPENGLES
      "#version 120 \n"
#endif
"\
\
#define cp_AlphaTest %d \n\
#define pp_ClipTestMode %d \n\
#define pp_UseAlpha %d \n\
#define pp_Texture %d \n\
#define pp_IgnoreTexA %d \n\
#define pp_ShadInstr %d \n\
#define pp_Offset %d \n\
#define pp_FogCtrl %d \n\
/* Shader program params*/ \n\
/* gles has no alpha test stage, so its emulated on the shader */ \n\
uniform " LOWP " float cp_AlphaTestValue; \n\
uniform " LOWP " vec4 pp_ClipTest; \n\
uniform " LOWP " vec3 sp_FOG_COL_RAM,sp_FOG_COL_VERT; \n\
uniform " HIGHP " float sp_FOG_DENSITY; \n\
uniform sampler2D tex,fog_table; \n\
/* Vertex input*/ \n\
" vary " " LOWP " vec4 vtx_base; \n\
" vary " " LOWP " vec4 vtx_offs; \n\
" vary " " MEDIUMP " vec2 vtx_uv; \n\
" LOWP " float fog_mode2(" HIGHP " float w) \n\
{ \n\
   " HIGHP " float z = clamp(w * sp_FOG_DENSITY, 1.0, 255.9999); \n\
   float exp         = floor(log2(z)); \n\
   " HIGHP " float m = z * 16.0 / pow(2.0, exp) - 16.0; \n\
   float idx         = floor(m) + exp * 16.0 + 0.5; \n\
   vec4 fog_coef = " TEXLOOKUP "(fog_table, vec2(idx / 128.0, 0.75 - (m - floor(m)) / 2.0)); \n\
   return fog_coef.a; \n\
} \n\
void main() \n\
{ \n\
	// Clip outside the box \n\
	#if pp_ClipTestMode==1 \n\
		if (gl_FragCoord.x < pp_ClipTest.x || gl_FragCoord.x > pp_ClipTest.z \n\
				|| gl_FragCoord.y < pp_ClipTest.y || gl_FragCoord.y > pp_ClipTest.w) \n\
			discard; \n\
	#endif \n\
	// Clip inside the box \n\
	#if pp_ClipTestMode==-1 \n\
		if (gl_FragCoord.x >= pp_ClipTest.x && gl_FragCoord.x <= pp_ClipTest.z \n\
				&& gl_FragCoord.y >= pp_ClipTest.y && gl_FragCoord.y <= pp_ClipTest.w) \n\
			discard; \n\
	#endif \n\
	\n\
   " LOWP " vec4 color=vtx_base; \n\
	#if pp_UseAlpha==0 \n\
		color.a=1.0; \n\
	#endif\n\
	#if pp_FogCtrl==3 \n\
		color=vec4(sp_FOG_COL_RAM.rgb,fog_mode2(gl_FragCoord.w)); \n\
	#endif\n\
	#if pp_Texture==1 \n\
	{ \n\
      " LOWP " vec4 texcol=" TEXLOOKUP "(tex,vtx_uv); \n\
		\n\
		#if pp_IgnoreTexA==1 \n\
			texcol.a=1.0;	 \n\
		#endif\n\
		\n\
      #if cp_AlphaTest == 1 \n\
         if (cp_AlphaTestValue>texcol.a) discard;\n\
      #endif \n\
		#if pp_ShadInstr==0 \n\
		{ \n\
         color=texcol; \n\
		} \n\
		#endif\n\
		#if pp_ShadInstr==1 \n\
		{ \n\
			color.rgb*=texcol.rgb; \n\
			color.a=texcol.a; \n\
		} \n\
		#endif\n\
		#if pp_ShadInstr==2 \n\
		{ \n\
			color.rgb=mix(color.rgb,texcol.rgb,texcol.a); \n\
		} \n\
		#endif\n\
		#if  pp_ShadInstr==3 \n\
		{ \n\
			color*=texcol; \n\
		} \n\
		#endif\n\
		\n\
		#if pp_Offset==1 \n\
		{ \n\
			color.rgb+=vtx_offs.rgb; \n\
			if (pp_FogCtrl==1) \n\
				color.rgb=mix(color.rgb,sp_FOG_COL_VERT.rgb,vtx_offs.a); \n\
		} \n\
		#endif\n\
	} \n\
	#endif\n\
	#if pp_FogCtrl==0 \n\
	{ \n\
		color.rgb=mix(color.rgb,sp_FOG_COL_RAM.rgb,fog_mode2(gl_FragCoord.w));  \n\
	} \n\
	#endif\n\
	#if cp_AlphaTest == 1 \n\
      color.a=1.0; \n\
	#endif  \n"
#ifndef GLES
   "\
	float w = gl_FragCoord.w * 100000.0; \n\
	gl_FragDepth = log2(1.0 + w) / 34; \n"
#endif
	FRAGCOL "=color; \n\
}";

const char* ModifierVolumeShader =
" \
uniform " LOWP " float sp_ShaderColor; \n\
/* Vertex input*/ \n\
void main() \n\
{ \n"
#ifndef GLES
	"\
	float w = gl_FragCoord.w * 100000.0; \n\
	gl_FragDepth = log2(1.0 + w) / 34; \n"
#endif
	FRAGCOL "=vec4(0.0, 0.0, 0.0, sp_ShaderColor); \n\
}";


static int gles_screen_width  = 640;
static int gles_screen_height = 480;
GLuint fogTextureId;

static s32 SetTileClip(u32 val, bool set)
{
	float csx=0,csy=0,cex=0,cey=0;
	u32 clipmode=val>>28;
	s32 clip_mode;
	if (clipmode<2)
		clip_mode=0;    //always passes
	else if (clipmode&1)
		clip_mode=-1;   //render stuff outside the region
	else
		clip_mode=1;    //render stuff inside the region

	csx=(float)(val&63);
	cex=(float)((val>>6)&63);
	csy=(float)((val>>12)&31);
	cey=(float)((val>>17)&31);
	csx=csx*32;
	cex=cex*32 +32;
	csy=csy*32;
	cey=cey*32 +32;

	if (csx <= 0 && csy <= 0 && cex >= 640 && cey >= 480)
		return 0;
	
	if (set && clip_mode)
   {
    	csy = 480 - csy;
		cey = 480 - cey;
		float dc2s_scale_h = gles_screen_height / 480.0f;
		float ds2s_offs_x = (gles_screen_width - dc2s_scale_h * 640) / 2;
		csx = csx * dc2s_scale_h + ds2s_offs_x;
		cex = cex * dc2s_scale_h + ds2s_offs_x;
		csy = csy * dc2s_scale_h;
		cey = cey * dc2s_scale_h;
		glUniform4f(CurrentShader->pp_ClipTest, csx, cey, cex, csy);		
   }

	return clip_mode;
}

static void SetCull(u32 CulliMode)
{
	if (CullMode[CulliMode] == GL_NONE)
		glcache.Disable(GL_CULL_FACE);
	else
	{
		glcache.Enable(GL_CULL_FACE);
		glcache.CullFace(CullMode[CulliMode]); //GL_FRONT/GL_BACK, ...
	}
}

int GetProgramID(
      u32 cp_AlphaTest,
      u32 pp_ClipTestMode,
      u32 pp_Texture,
      u32 pp_UseAlpha,
      u32 pp_IgnoreTexA,
      u32 pp_ShadInstr,
      u32 pp_Offset,
      u32 pp_FogCtrl)
{
	u32 rv=0;

	rv|=pp_ClipTestMode;
	rv<<=1; rv|=cp_AlphaTest;
	rv<<=1; rv|=pp_Texture;
	rv<<=1; rv|=pp_UseAlpha;
	rv<<=1; rv|=pp_IgnoreTexA;
	rv<<=2; rv|=pp_ShadInstr;
	rv<<=1; rv|=pp_Offset;
	rv<<=2; rv|=pp_FogCtrl;

	return rv;
}

static GLuint gl_CompileShader(const char* shader,GLuint type)
{
	GLint result;
	GLint compile_log_len;
	GLuint rv=glCreateShader(type);
	glShaderSource(rv, 1,&shader, NULL);
	glCompileShader(rv);

	//lets see if it compiled ...
	glGetShaderiv(rv, GL_COMPILE_STATUS, &result);
	glGetShaderiv(rv, GL_INFO_LOG_LENGTH, &compile_log_len);

	if (!result && compile_log_len>0)
	{
		if (compile_log_len==0)
			compile_log_len=1;
		char* compile_log=(char*)malloc(compile_log_len);
		*compile_log=0;

		glGetShaderInfoLog(rv, compile_log_len, &compile_log_len, compile_log);
		printf("Shader: %s \n%s\n",result?"compiled!":"failed to compile",compile_log);

		free(compile_log);
	}

	return rv;
}

static GLuint gl_CompileAndLink(const char* VertexShader, const char* FragmentShader)
{
	GLint compile_log_len;
	GLint result;
	/* Create vertex/fragment shaders */
	GLuint vs      = gl_CompileShader(VertexShader ,GL_VERTEX_SHADER);
	GLuint ps      = gl_CompileShader(FragmentShader ,GL_FRAGMENT_SHADER);
	GLuint program = glCreateProgram();

	glAttachShader(program, vs);
	glAttachShader(program, ps);

	/* Bind vertex attribute to VBO inputs */
	glBindAttribLocation(program, VERTEX_POS_ARRAY,      "in_pos");
	glBindAttribLocation(program, VERTEX_COL_BASE_ARRAY, "in_base");
	glBindAttribLocation(program, VERTEX_COL_OFFS_ARRAY, "in_offs");
	glBindAttribLocation(program, VERTEX_UV_ARRAY,       "in_uv");

#ifndef HAVE_OPENGLES
	glBindFragDataLocation(program, 0, "FragColor");
#endif

	glLinkProgram(program);
	glGetProgramiv(program, GL_LINK_STATUS, &result);
	glGetProgramiv(program, GL_INFO_LOG_LENGTH, &compile_log_len);

	if (!result && compile_log_len>0)
	{
      char *compile_log = NULL;

		if (compile_log_len==0)
			compile_log_len = 1;
		compile_log_len   += 1024;
		compile_log        = (char*)malloc(compile_log_len);
		*compile_log       = 0;

		glGetProgramInfoLog(program, compile_log_len, &compile_log_len, compile_log);
		printf("Shader linking: %s \n (%d bytes), - %s -\n",result?"linked":"failed to link", compile_log_len,compile_log);

		free(compile_log);
		die("shader compile fail\n");
	}

	glDeleteShader(vs);
	glDeleteShader(ps);

	glcache.UseProgram(program);

	verify(glIsProgram(program));

	return program;
}

static void set_shader_uniforms(struct ShaderUniforms_t *uni, PipelineShader* s)
{
   if (s->cp_AlphaTestValue!=-1)
      glUniform1f(s->cp_AlphaTestValue, uni->PT_ALPHA);

   if (s->scale!=-1)
      glUniform4fv( s->scale, 1, uni->scale_coefs);

   if (s->depth_scale!=-1)
      glUniform4fv( s->depth_scale, 1, uni->depth_coefs);

   if (s->sp_FOG_DENSITY!=-1)
      glUniform1f( s->sp_FOG_DENSITY, uni->fog_den_float);

   if (s->sp_FOG_COL_RAM!=-1)
      glUniform3fv( s->sp_FOG_COL_RAM, 1, uni->ps_FOG_COL_RAM);

   if (s->sp_FOG_COL_VERT!=-1)
      glUniform3fv( s->sp_FOG_COL_VERT, 1, uni->ps_FOG_COL_VERT);
}

bool CompilePipelineShader(PipelineShader *s)
{
	char pshader[8192];

	sprintf(pshader,PixelPipelineShader,
                s->cp_AlphaTest,s->pp_ClipTestMode,s->pp_UseAlpha,
                s->pp_Texture,s->pp_IgnoreTexA,s->pp_ShadInstr,s->pp_Offset,s->pp_FogCtrl);

	s->program            = gl_CompileAndLink(VertexShaderSource,pshader);


	//setup texture 0 as the input for the shader
	GLuint gu=glGetUniformLocation(s->program, "tex");
	if (s->pp_Texture==1)
		glUniform1i(gu,0);

	//get the uniform locations
	s->scale	             = glGetUniformLocation(s->program, "scale");
	s->depth_scale        = glGetUniformLocation(s->program, "depth_scale");


	s->pp_ClipTest        = glGetUniformLocation(s->program, "pp_ClipTest");

	s->sp_FOG_DENSITY     = glGetUniformLocation(s->program, "sp_FOG_DENSITY");

	s->cp_AlphaTestValue  = glGetUniformLocation(s->program, "cp_AlphaTestValue");

	//FOG_COL_RAM,FOG_COL_VERT,FOG_DENSITY;
	if (s->pp_FogCtrl==1 && s->pp_Texture==1)
		s->sp_FOG_COL_VERT = glGetUniformLocation(s->program, "sp_FOG_COL_VERT");
	else
		s->sp_FOG_COL_VERT = -1;
	if (s->pp_FogCtrl==0 || s->pp_FogCtrl==3)
	{
		s->sp_FOG_COL_RAM=glGetUniformLocation(s->program, "sp_FOG_COL_RAM");
	}
	else
	{
		s->sp_FOG_COL_RAM=-1;
	}
   // Setup texture 1 as the fog table
   gu = glGetUniformLocation(s->program, "fog_table");
   if (gu != -1)
      glUniform1i(gu, 1);

	set_shader_uniforms(&ShaderUniforms, s);

	return glIsProgram(s->program)==GL_TRUE;
}

static void SetTextureRepeatMode(GLuint dir, u32 clamp, u32 mirror)
{
	if (clamp)
		glcache.TexParameteri(GL_TEXTURE_2D, dir, GL_CLAMP_TO_EDGE);
	else
		glcache.TexParameteri(GL_TEXTURE_2D, dir, mirror ? GL_MIRRORED_REPEAT : GL_REPEAT);
}

template <u32 Type, bool SortingEnabled>
static __forceinline void SetGPState(const PolyParam* gp, u32 cflip)
{
   CurrentShader = &program_table[
									 GetProgramID(Type == TA_LIST_PUNCH_THROUGH ? 1 : 0,
											 	  SetTileClip(gp->tileclip, false) + 1,
												  gp->pcw.Texture,
												  gp->tsp.UseAlpha,
												  gp->tsp.IgnoreTexA,
												  gp->tsp.ShadInstr,
												  gp->pcw.Offset,
												  gp->tsp.FogCtrl)];

   if (CurrentShader->program == -1)
      CompilePipelineShader(CurrentShader);
   glcache.UseProgram(CurrentShader->program);
   SetTileClip(gp->tileclip,true);

   // This bit controls which pixels are affected
   // by modvols
   const u32 stencil = (gp->pcw.Shadow!=0)?0x80:0;
   glcache.StencilFunc(GL_ALWAYS, stencil, stencil);

   glcache.BindTexture(GL_TEXTURE_2D, gp->texid == -1 ? 0 : gp->texid);
   SetTextureRepeatMode(GL_TEXTURE_WRAP_S, gp->tsp.ClampU, gp->tsp.FlipU);
   SetTextureRepeatMode(GL_TEXTURE_WRAP_T, gp->tsp.ClampV, gp->tsp.FlipV);

   //set texture filter mode
	if (gp->tsp.FilterMode == 0)
	{
		//disable filtering, mipmaps
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	else
	{
		//bilinear filtering
		//PowerVR supports also trilinear via two passes, but we ignore that for now
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (gp->tcw.MipMapped && settings.rend.UseMipmaps) ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

   if (Type==TA_LIST_TRANSLUCENT)
   {
      glcache.Enable(GL_BLEND);
      glcache.BlendFunc(SrcBlendGL[gp->tsp.SrcInstr], DstBlendGL[gp->tsp.DstInstr]);
   }
   else
      glcache.Disable(GL_BLEND);

   //set cull mode !
   //cflip is required when exploding triangles for triangle sorting
   //gcflip is global clip flip, needed for when rendering to texture due to mirrored Y direction
   SetCull(gp->isp.CullMode ^ cflip ^ gcflip);

   /* Set Z mode, only if required */
   if (Type == TA_LIST_PUNCH_THROUGH || (Type == TA_LIST_TRANSLUCENT && SortingEnabled))
      glcache.DepthFunc(GL_GEQUAL);
   else
      glcache.DepthFunc(Zfunction[gp->isp.DepthMode]);

   if (SortingEnabled && settings.pvr.Emulation.AlphaSortMode == 0)
      glcache.DepthMask(GL_FALSE);
   else
      glcache.DepthMask(!gp->isp.ZWriteDis);
}

template <u32 Type, bool SortingEnabled>
static void DrawList(const List<PolyParam>& gply)
{
   PolyParam* params=gply.head();
   int count=gply.used();

   /* We want at least 1 PParam */
   if (count==0)
      return;

   /* set some 'global' modes for all primitives */
   glcache.Enable(GL_STENCIL_TEST);
   glcache.StencilFunc(GL_ALWAYS,0,0);
   glcache.StencilOp(GL_KEEP,GL_KEEP,GL_REPLACE);

   while(count-->0)
   {
      if (params->count>2) /* this actually happens for some games. No idea why .. */
      {
         SetGPState<Type,SortingEnabled>(params, 0);
         glDrawElements(GL_TRIANGLE_STRIP, params->count, GL_UNSIGNED_SHORT, (GLvoid*)(2*params->first));
      }

      params++;
   }

   glDisable(GL_STENCIL_TEST);
   glStencilFunc(
         GL_ALWAYS,
         0,
         1);
   glStencilOp(GL_KEEP,
         GL_KEEP,
         GL_KEEP);
}

bool operator<(const PolyParam &left, const PolyParam &right)
{
   /* put any condition you want to sort on here */
	return left.zvZ  < right.zvZ;
#if 0
	return left.zMin < right.zMax;
#endif
}

//Sort based on min-z of each strip
static void SortPParams(void)
{
   u16 *idx_base      = NULL;
   Vertex *vtx_base   = NULL;
   PolyParam *pp      = NULL;
   PolyParam *pp_end  = NULL;

   if (pvrrc.verts.used()==0 || pvrrc.global_param_tr.used()<=1)
      return;

   vtx_base          = pvrrc.verts.head();
   idx_base          = pvrrc.idx.head();
   pp                = pvrrc.global_param_tr.head();
   pp_end            = pp + pvrrc.global_param_tr.used();

   while(pp!=pp_end)
   {
      if (pp->count<2)
         pp->zvZ=0;
      else
      {
         u16*      idx   = idx_base+pp->first;
         Vertex*   vtx   = vtx_base+idx[0];
         Vertex* vtx_end = vtx_base + idx[pp->count-1]+1;
         u32 zv          = 0xFFFFFFFF;

         while(vtx!=vtx_end)
         {
            zv = min(zv,(u32&)vtx->z);
            vtx++;
         }

         pp->zvZ=(f32&)zv;
      }
      pp++;
   }

   unsigned first = 0;
   unsigned count = pvrrc.global_param_tr.used();
   std::stable_sort(pvrrc.global_param_tr.head() + first,
         pvrrc.global_param_tr.head() + first + count);
}

static inline float min3(float v0,float v1,float v2)
{
	return min(min(v0,v1),v2);
}

static inline float max3(float v0,float v1,float v2)
{
	return max(max(v0,v1),v2);
}

static inline float minZ(Vertex* v,u16* mod)
{
	return min(min(v[mod[0]].z,v[mod[1]].z),v[mod[2]].z);
}

bool operator<(const IndexTrig &left, const IndexTrig &right)
{
	return left.z<right.z;
}

//are two poly params the same?
static inline bool PP_EQ(PolyParam* pp0, PolyParam* pp1)
{
   return 
      (pp0->pcw.full&PCW_DRAW_MASK)==(pp1->pcw.full&PCW_DRAW_MASK) 
      && pp0->isp.full==pp1->isp.full 
      && pp0->tcw.full==pp1->tcw.full
      && pp0->tsp.full==pp1->tsp.full
      && pp0->tileclip==pp1->tileclip;
}

static void GenSorted(void)
{
   static vector<IndexTrig> lst;
   static vector<u16> vidx_sort;

   static u32 vtx_cnt;
   int idx            = -1;
   int pfsti          =  0;

   pidx_sort.clear();

   if (pvrrc.verts.used()==0 || pvrrc.global_param_tr.used()<=1)
      return;

   Vertex* vtx_base=pvrrc.verts.head();
   u16* idx_base=pvrrc.idx.head();

   PolyParam* pp_base=pvrrc.global_param_tr.head();
   PolyParam* pp=pp_base;
   PolyParam* pp_end= pp + pvrrc.global_param_tr.used();

   Vertex* vtx_arr=vtx_base+idx_base[pp->first];
   vtx_sort_base=vtx_base;
   int vtx_count=idx_base[pp_end[-1].first+pp_end[-1].count-1]-idx_base[pp->first];
   if (vtx_count>vtx_cnt)
      vtx_cnt=vtx_count;

#if PRINT_SORT_STATS
   printf("TVTX: %d || %d\n",vtx_cnt,vtx_count);
#endif

   if (vtx_count<=0)
      return;

   /* Make lists of all triangles, with their PID and VID */

   lst.resize(vtx_count*4);

   while(pp != pp_end)
   {
      Vertex *vtx     = NULL;
      Vertex *vtx_end = NULL;
      u16 *idx        = NULL;
      u32 flip        = 0;
      u32 ppid        = (pp-pp_base);

      if (pp->count <= 2)
      {
         pp++;
         continue;
      }

      idx             = idx_base + pp->first;
      vtx             = vtx_base+idx[0];
      vtx_end         = vtx_base + idx[pp->count-1]-1;

      while(vtx != vtx_end)
      {
         Vertex *v0   = &vtx[0];
         Vertex *v1   = &vtx[1];
         Vertex *v2   = &vtx[2];

         if (flip)
         {
            v0=&vtx[2];
            v1=&vtx[1];
            v2=&vtx[0];
         }

         u16* d =lst[pfsti].id;
         Vertex *vb = vtx_base;
         d[0]=v0-vb;
         d[1]=v1-vb;
         d[2]=v2-vb;

         lst[pfsti].pid= ppid ;
         lst[pfsti].z = minZ(vtx_base,lst[pfsti].id);
         pfsti++;

         flip ^= 1;
         vtx++;
      }
      pp++;
   }

   u32 aused=pfsti;

   lst.resize(aused);

   /* sort them */
   std::stable_sort(lst.begin(),lst.end());

   /* Merge PIDs/draw commands if two different PIDs are actually equal */

   for (u32 k = 1; k < aused; k++)
   {
      if (lst[k].pid == lst[k-1].pid)
         continue;

      if (PP_EQ(&pp_base[lst[k].pid],&pp_base[lst[k-1].pid]))
         lst[k].pid=lst[k-1].pid;
   }

   /* Reassemble vertex indices into drawing commands */

   vidx_sort.resize(aused*3);

   for (u32 i=0; i<aused; i++)
   {
      SortTrigDrawParam stdp;
      int   pid          = lst[i].pid;
      u16* midx          = lst[i].id;

      vidx_sort[i*3 + 0] = midx[0];
      vidx_sort[i*3 + 1] = midx[1];
      vidx_sort[i*3 + 2] = midx[2];

      if (idx == pid)
         continue;

      stdp.ppid  = pp_base + pid;
      stdp.first = (u16)(i*3);
      stdp.count = 0;

      if (idx!=-1)
      {
         SortTrigDrawParam *last = &pidx_sort[pidx_sort.size()-1];

         if (last)
            last->count=stdp.first-last->first;
      }

      pidx_sort.push_back(stdp);
      idx=pid;
   }

   SortTrigDrawParam *stdp = &pidx_sort[pidx_sort.size()-1];

   if (stdp)
      stdp->count=aused*3-stdp->first;

#if PRINT_SORT_STATS
   printf("Reassembled into %d from %d\n",pidx_sort.size(),pp_end-pp_base);
#endif

   /* Upload to GPU if needed, otherwise return */
   if (pidx_sort.size())
   {
      /* Bind and upload sorted index buffer */
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.idxs2);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER,vidx_sort.size()*2,&vidx_sort[0],GL_STREAM_DRAW);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
   }
}

static void DrawSorted(u32 count)
{
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.idxs2);

   //set some 'global' modes for all primitives

   glcache.Enable(GL_STENCIL_TEST);
   glcache.StencilFunc(GL_ALWAYS, 0, 0);
   glcache.StencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

   for (u32 p=0; p<count; p++)
   {
      PolyParam* params = pidx_sort[p].ppid;
      if (pidx_sort[p].count>2) //this actually happens for some games. No idea why ..
      {
         SetGPState<TA_LIST_TRANSLUCENT, true>(params, 0);
         glDrawElements(GL_TRIANGLES, pidx_sort[p].count, GL_UNSIGNED_SHORT, (GLvoid*)(2*pidx_sort[p].first));
      }
      params++;
   }
}

//All pixels are in area 0 by default.
//If inside an 'in' volume, they are in area 1
//if inside an 'out' volume, they are in area 0
/*
	Stencil bits:
		bit 7: mv affected (must be preserved)
		bit 1: current volume state
		but 0: summary result (starts off as 0)

	Lower 2 bits:

	IN volume (logical OR):
	00 -> 00
	01 -> 01
	10 -> 01
	11 -> 01

	Out volume (logical AND):
	00 -> 00
	01 -> 00
	10 -> 00
	11 -> 01
*/
static void SetMVS_Mode(u32 mv_mode,ISP_Modvol ispc)
{
	if (mv_mode==0)	//normal trigs
	{
		//set states
		glcache.Enable(GL_DEPTH_TEST);
		//write only bit 1
      glcache.StencilMask(2);
      //no stencil testing
      glcache.StencilFunc(GL_ALWAYS, 0, 2);
		//count the number of pixels in front of the Z buffer (and only keep the lower bit of the count)
      glcache.StencilOp(GL_KEEP, GL_KEEP, GL_INVERT);

		//Cull mode needs to be set
		SetCull(ispc.CullMode);
	}
	else
	{
		//1 (last in) or 2 (last out)
		//each triangle forms the last of a volume

		//common states

		//no depth test
		glcache.Disable(GL_DEPTH_TEST);

      //write bits 1:0
      glcache.StencilMask(3);

		if (mv_mode==1)
		{
         // Inclusion volume
			//res : old : final 
			//0   : 0      : 00
			//0   : 1      : 01
			//1   : 0      : 01
			//1   : 1      : 01
			
			//if (1<=st) st=1; else st=0;
         glcache.StencilFunc(GL_LEQUAL, 1, 3);

         glcache.StencilOp(GL_ZERO, GL_ZERO, GL_REPLACE);

			/*
			//if !=0 -> set to 10
			verifyc(dev->SetRenderState(D3DRS_STENCILFUNC,D3DCMP_LESSEQUAL));
			verifyc(dev->SetRenderState(D3DRS_STENCILREF,1));					
			verifyc(dev->SetRenderState(D3DRS_STENCILPASS,D3DSTENCILOP_REPLACE));
			verifyc(dev->SetRenderState(D3DRS_STENCILFAIL,D3DSTENCILOP_ZERO));
			*/
		}
		else
		{
         // Exclusion volume
			/*
				I've only seen a single game use it, so i guess it doesn't matter ? (Zombie revenge)
				(actually, i think there was also another, racing game)
			*/

         // The initial value for exclusion volumes is 1 so we need to invert the result before and'ing.
			//res : old : final 
			//0   : 0   : 00
			//0   : 1   : 01
			//1   : 0   : 00
			//1   : 1   : 00

			//if (1 == st) st = 1; else st = 0;
         glcache.StencilFunc(GL_EQUAL, 1, 3);
         glcache.StencilOp(GL_ZERO, GL_ZERO, GL_REPLACE);
		}
	}
}

static void SetupMainVBO(void)
{
	glBindBuffer(GL_ARRAY_BUFFER, vbo.geometry);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.idxs);

	//setup vertex buffers attrib pointers
	glEnableVertexAttribArray(VERTEX_POS_ARRAY);
	glVertexAttribPointer(VERTEX_POS_ARRAY, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex,x));

	glEnableVertexAttribArray(VERTEX_COL_BASE_ARRAY);
	glVertexAttribPointer(VERTEX_COL_BASE_ARRAY, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex,col));

	glEnableVertexAttribArray(VERTEX_COL_OFFS_ARRAY);
	glVertexAttribPointer(VERTEX_COL_OFFS_ARRAY, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex,vtx_spc));

	glEnableVertexAttribArray(VERTEX_UV_ARRAY);
	glVertexAttribPointer(VERTEX_UV_ARRAY, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex,u));

}

static void DrawModVols(void)
{
   /* A bit of explanation:
     * In theory it works like this: generate a 1-bit stencil for each polygon
     * volume, and then AND or OR it against the overall 1-bit tile stencil at 
     * the end of the volume. */

	if (pvrrc.modtrig.used()==0 || settings.pvr.Emulation.ModVolMode == 0)
		return;

	glBindBuffer(GL_ARRAY_BUFFER, vbo.modvols);

	//setup vertex buffers attrib pointers
	glEnableVertexAttribArray(VERTEX_POS_ARRAY);
	glVertexAttribPointer(VERTEX_POS_ARRAY, 3, GL_FLOAT, GL_FALSE, sizeof(float)*3, (void*)0);

	glDisableVertexAttribArray(VERTEX_UV_ARRAY);
	glDisableVertexAttribArray(VERTEX_COL_OFFS_ARRAY);
	glDisableVertexAttribArray(VERTEX_COL_BASE_ARRAY);

	glcache.Enable(GL_BLEND);
   glcache.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glcache.UseProgram(modvol_shader.program);
	glUniform1f(modvol_shader.sp_ShaderColor,0.5f);

   glcache.DepthMask(GL_FALSE);
	glcache.DepthFunc(GL_GREATER);

	if(settings.pvr.Emulation.ModVolMode == 1)
	{
		//simply draw the volumes -- for debugging
		SetCull(0);
		glDrawArrays(GL_TRIANGLES,0,pvrrc.modtrig.used()*3);
		SetupMainVBO();
	}
	else
	{
		/*
		mode :
		normal trig : flip
		last *in*   : flip, merge*in* &clear from last merge
		last *out*  : flip, merge*out* &clear from last merge
		*/

		/*

			Do not write to color
			Do not write to depth

			read from stencil bits 1:0
			write to stencil bits 1:0
		*/

		glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);

		if (settings.pvr.Emulation.ModVolMode == 2)
		{
			//simple single level stencil
			glcache.Enable(GL_STENCIL_TEST);
         glcache.StencilFunc(GL_ALWAYS, 0x1, 0x1);
         glcache.StencilOp(GL_KEEP, GL_KEEP, GL_INVERT);
         glcache.StencilMask(0x1);
			SetCull(0);
			glDrawArrays(GL_TRIANGLES,0,pvrrc.modtrig.used()*3);
		}
		else if (settings.pvr.Emulation.ModVolMode == 3)
		{
         glEnable(GL_STENCIL_TEST);
			//Full emulation
			//the *out* mode is buggy

			u32 mod_base=0; //cur start triangle
			u32 mod_last=0; //last merge

			u32 cmv_count= (pvrrc.global_param_mvo.used()-1);
			ISP_Modvol* params=pvrrc.global_param_mvo.head();

			//ISP_Modvol
			for (u32 cmv=0;cmv<cmv_count;cmv++)
			{

				ISP_Modvol ispc=params[cmv];
				mod_base=ispc.id;
				u32 sz=params[cmv+1].id-mod_base;

            if (sz == 0)
               continue;

				u32 mv_mode = ispc.DepthMode;

				if (mv_mode==0)	//normal trigs
				{
					SetMVS_Mode(0,ispc);
					//Render em (counts intersections)
					//verifyc(dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST,sz,pvrrc.modtrig.data+mod_base,3*4));
					glDrawArrays(GL_TRIANGLES,mod_base*3,sz*3);
				}
				else if (mv_mode<3)
				{
					while(sz)
					{
						//merge and clear all the prev. stencil bits

						//Count Intersections (last poly)
						SetMVS_Mode(0,ispc);
						glDrawArrays(GL_TRIANGLES,mod_base*3,3);

						//Sum the area
						SetMVS_Mode(mv_mode,ispc);
						glDrawArrays(GL_TRIANGLES,mod_last*3,(mod_base-mod_last+1)*3);

						//update pointers
						mod_last=mod_base+1;
						sz--;
						mod_base++;
					}
				}
			}
		}
		//disable culling
		SetCull(0);
		//enable color writes
		glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);

		//black out any stencil with '1'
		glcache.Enable(GL_BLEND);
      glcache.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		
		glcache.Enable(GL_STENCIL_TEST);
      //only pixels that are Modvol enabled, and in area 1
      glcache.StencilFunc(GL_EQUAL, 0x81, 0x81);
		
		//clear the stencil result bit
      glcache.StencilMask(0x3); /* write to LSB */
      glcache.StencilOp(GL_ZERO, GL_ZERO, GL_ZERO);

		//don't do depth testing
		glcache.Disable(GL_DEPTH_TEST);

		SetupMainVBO();
		glDrawArrays(GL_TRIANGLE_STRIP,0,4);

		//Draw and blend
		//glDrawArrays(GL_TRIANGLES,pvrrc.modtrig.used(),2);
	}

	//restore states
   glcache.Enable(GL_DEPTH_TEST);
}

/*
GL|ES 2
Slower, smaller subset of gl2

*Optimisation notes*
Keep stuff in packed ints
Keep data as small as possible
Keep vertex programs as small as possible
The drivers more or less suck. Don't depend on dynamic allocation, or any 'complex' feature
as it is likely to be problematic/slow
Do we really want to enable striping joins?

*Design notes*
Follow same architecture as the d3d renderer for now
Render to texture, keep track of textures in GL memory
Direct flip to screen (no vlbank/fb emulation)
Do we really need a combining shader? it is needlessly expensive for openGL | ES
Render contexts
Free over time? we actually care about ram usage here?
Limit max resource size? for psp 48k verts worked just fine

FB:
Pixel clip, mapping

SPG/VO:
mapping

TA:
Tile clip

*/

static bool gl_create_resources(void)
{
   u32 i;
   u32 cp_AlphaTest;
   u32 pp_ClipTestMode;
   u32 pp_UseAlpha;
   u32 pp_Texture;
   u32 pp_FogCtrl;
   u32 pp_IgnoreTexA;
   u32 pp_Offset;
   u32 pp_ShadInstr;
	PipelineShader* dshader  = 0;
   u32 compile              = 0;

	/* create VBOs */
	glGenBuffers(1, &vbo.geometry);
	glGenBuffers(1, &vbo.modvols);
	glGenBuffers(1, &vbo.idxs);
	glGenBuffers(1, &vbo.idxs2);

	memset(program_table,0,sizeof(program_table));

   for(cp_AlphaTest = 0; cp_AlphaTest <= 1; cp_AlphaTest++)
	{
      for (pp_ClipTestMode = 0; pp_ClipTestMode <= 2; pp_ClipTestMode++)
		{
			for (pp_UseAlpha = 0; pp_UseAlpha <= 1; pp_UseAlpha++)
			{
				for (pp_Texture = 0; pp_Texture <= 1; pp_Texture++)
				{
					for (pp_FogCtrl = 0; pp_FogCtrl <= 3; pp_FogCtrl++)
					{
						for (pp_IgnoreTexA = 0; pp_IgnoreTexA <= 1; pp_IgnoreTexA++)
						{
							for (pp_ShadInstr = 0; pp_ShadInstr <= 3; pp_ShadInstr++)
							{
								for (pp_Offset = 0; pp_Offset <= 1; pp_Offset++)
								{
                           int prog_id              = GetProgramID(
                                 cp_AlphaTest,
                                 pp_ClipTestMode,
                                 pp_Texture,
                                 pp_UseAlpha,
                                 pp_IgnoreTexA,
                                 pp_ShadInstr,
                                 pp_Offset,pp_FogCtrl);
									dshader                  = &program_table[prog_id];

									dshader->cp_AlphaTest    = cp_AlphaTest;
									dshader->pp_ClipTestMode = pp_ClipTestMode-1;
									dshader->pp_Texture      = pp_Texture;
									dshader->pp_UseAlpha     = pp_UseAlpha;
									dshader->pp_IgnoreTexA   = pp_IgnoreTexA;
									dshader->pp_ShadInstr    = pp_ShadInstr;
									dshader->pp_Offset       = pp_Offset;
									dshader->pp_FogCtrl      = pp_FogCtrl;
									dshader->program         = -1;
								}
							}
						}
					}
				}
			}
		}
	}

	modvol_shader.program        = gl_CompileAndLink(VertexShaderSource,ModifierVolumeShader);
	modvol_shader.scale          = glGetUniformLocation(modvol_shader.program, "scale");
	modvol_shader.sp_ShaderColor = glGetUniformLocation(modvol_shader.program, "sp_ShaderColor");
	modvol_shader.depth_scale    = glGetUniformLocation(modvol_shader.program, "depth_scale");

   if (settings.pvr.Emulation.precompile_shaders)
   {
      for (i=0;i<sizeof(program_table)/sizeof(program_table[0]);i++)
      {
         if (!CompilePipelineShader(	&program_table[i] ))
            return false;
      }
   }

	return true;
}

void UpdateFogTexture(u8 *fog_table)
{
	glActiveTexture(GL_TEXTURE1);
	if (fogTextureId == 0)
	{
		fogTextureId = glcache.GenTexture();
		glcache.BindTexture(GL_TEXTURE_2D, fogTextureId);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	else
		glcache.BindTexture(GL_TEXTURE_2D, fogTextureId);

	u8 temp_tex_buffer[256];
	for (int i = 0; i < 128; i++)
	{
		temp_tex_buffer[i] = fog_table[i * 4];
		temp_tex_buffer[i + 128] = fog_table[i * 4 + 1];
	}
	glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, 128, 2, 0, GL_ALPHA, GL_UNSIGNED_BYTE, temp_tex_buffer);

	glActiveTexture(GL_TEXTURE0);
}

static void vertex_buffer_unmap(void)
{
   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

#ifdef MSB_FIRST
#define INDEX_GET(a) (a^3)
#else
#define INDEX_GET(a) (a)
#endif

static void DrawStrips(void)
{
	SetupMainVBO();
	//Draw the strips !

	//initial state
	glcache.Enable(GL_DEPTH_TEST);

	//We use sampler 0
   glActiveTexture(GL_TEXTURE0);

	//Opaque
	DrawList<TA_LIST_OPAQUE, false>(pvrrc.global_param_op);

	//Alpha tested
	DrawList<TA_LIST_PUNCH_THROUGH, false>(pvrrc.global_param_pt);

   // Modifier volumes
	DrawModVols();

	//Alpha blended
   if (settings.pvr.Emulation.AlphaSortMode == 0)
   {
      u32 count = pidx_sort.size();
      //if any drawing commands, draw them
      if (pvrrc.isAutoSort && count)
         DrawSorted(count);
      else
         DrawList<TA_LIST_TRANSLUCENT, false>(pvrrc.global_param_tr);
   }
   else if (settings.pvr.Emulation.AlphaSortMode == 1)
   {
      if (pvrrc.isAutoSort)
         SortPParams();
      DrawList<TA_LIST_TRANSLUCENT, true>(pvrrc.global_param_tr);
   }

   vertex_buffer_unmap();
}

void DoCleanup() {
}

static bool RenderFrame(void)
{
   DoCleanup();

	bool is_rtt=pvrrc.isRTT;

	//if (FrameCount&7) return;

	//Setup the matrix
   float vtx_min_fZ = 0.f;
	float vtx_max_fZ = pvrrc.fZ_max;

	//sanitise the values, now with NaN detection (for omap)
	//0x49800000 is 1024*1024. Using integer math to avoid issues w/ infs and nans
	if ((s32&)vtx_max_fZ<0 || (u32&)vtx_max_fZ>0x49800000)
		vtx_max_fZ=10*1024;


	//add some extra range to avoid clipping border cases
	vtx_min_fZ*=0.98f;
	vtx_max_fZ*=1.001f;

	//calculate a projection so that it matches the pvr x,y setup, and
	//a) Z is linearly scaled between 0 ... 1
	//b) W is passed though for proper perspective calculations

	/*
	PowerVR coords:
	fx, fy (pixel coordinates)
	fz=1/w

	(as a note, fx=x*fz;fy=y*fz)

	Clip space
	-Wc .. Wc, xyz
	x: left-right, y: bottom-top
	NDC space
	-1 .. 1, xyz
	Window space:
	translated NDC (viewport, glDepth)

	Attributes:
	//this needs to be cleared up, been some time since I wrote my rasteriser and i'm starting
	//to forget/mixup stuff
	vaX         -> VS output
	iaX=vaX*W   -> value to be interpolated
	iaX',W'     -> interpolated values
	paX=iaX'/W' -> Per pixel interpolated value for attribute


	Proper mappings:
	Output from shader:
	W=1/fz
	x=fx*W -> maps to fx after perspective divide
	y=fy*W ->         fy   -//-
	z=-W for min, W for max. Needs to be linear.



	umodified W, perfect mapping:
	Z mapping:
	pz=z/W
	pz=z/(1/fz)
	pz=z*fz
	z=zt_s+zt_o
	pz=(zt_s+zt_o)*fz
	pz=zt_s*fz+zt_o*fz
	zt_s=scale
	zt_s=2/(max_fz-min_fz)
	zt_o*fz=-min_fz-1
	zt_o=(-min_fz-1)/fz == (-min_fz-1)*W


	x=fx/(fx_range/2)-1		//0 to max -> -1 to 1
	y=fy/(-fy_range/2)+1	//0 to max -> 1 to -1
	z=-min_fz*W + (zt_s-1)  //0 to +inf -> -1 to 1

	o=a*z+c
	1=a*z_max+c
	-1=a*z_min+c

	c=-a*z_min-1
	1=a*z_max-a*z_min-1
	2=a*(z_max-z_min)
	a=2/(z_max-z_min)
	*/

	//float B=2/(min_invW-max_invW);
	//float A=-B*max_invW+vnear;

	//these should be adjusted based on the current PVR scaling etc params
	float dc_width=640;
	float dc_height=480;

	if (!is_rtt)
	{
		gcflip=0;
	}
	else
	{
		gcflip=1;

		//For some reason this produces wrong results
		//so for now its hacked based like on the d3d code
		/*
		u32 pvr_stride=(FB_W_LINESTRIDE.stride)*8;
		*/

		dc_width  = FB_X_CLIP.max-FB_X_CLIP.min+1;
		dc_height = FB_Y_CLIP.max-FB_Y_CLIP.min+1;
	}

	scale_x = 1;
   scale_y = 1;

	float scissoring_scale_x = 1;

	if (!is_rtt)
	{
		scale_x=fb_scale_x;
		scale_y=fb_scale_y;

		//work out scaling parameters !
		//Pixel doubling is on VO, so it does not affect any pixel operations
		//A second scaling is used here for scissoring
		if (VO_CONTROL.pixel_double)
		{
			scissoring_scale_x  = 0.5f;
			scale_x            *= 0.5f;
		}
	}

	if (SCALER_CTL.hscale)
	{
      /* If the horizontal scaler is in use, we're (in principle) supposed to
    	 * divide everything by 2. However in the interests of display quality,
    	 * instead we want to render to the unscaled resolution and downsample
    	 * only if/when required.
    	 */
		scale_x*=2;
	}

	dc_width  *= scale_x;
	dc_height *= scale_y;

	/*

	float vnear=0;
	float vfar =1;

	float max_invW=1/vtx_min_fZ;
	float min_invW=1/vtx_max_fZ;

	float B=vfar/(min_invW-max_invW);
	float A=-B*max_invW+vnear;


	GLfloat dmatrix[16] =
	{
		(2.f/dc_width)  ,0                ,-(640/dc_width)              ,0  ,
		0               ,-(2.f/dc_height) ,(480/dc_height)              ,0  ,
		0               ,0                ,A                            ,B  ,
		0               ,0                ,1                            ,0
	};

	glUniformMatrix4fv(matrix, 1, GL_FALSE, dmatrix);

	*/

	/*
		Handle Dc to screen scaling
	*/
	float dc2s_scale_h = is_rtt ? (gles_screen_width / dc_width) : (gles_screen_height/480.0);
	float ds2s_offs_x  = is_rtt ? 0 : ((gles_screen_width-dc2s_scale_h*640)/2);

	//-1 -> too much to left
	ShaderUniforms.scale_coefs[0]=2.0f/(gles_screen_width/dc2s_scale_h*scale_x);
	ShaderUniforms.scale_coefs[1]= (is_rtt?2:-2) / dc_height;
   // FIXME CT2 needs 480 here instead of dc_height=512
	ShaderUniforms.scale_coefs[2]=1-2*ds2s_offs_x/(gles_screen_width);
	ShaderUniforms.scale_coefs[3]=(is_rtt?1:-1);


	ShaderUniforms.depth_coefs[0]=2/(vtx_max_fZ-vtx_min_fZ);
	ShaderUniforms.depth_coefs[1]=-vtx_min_fZ-1;
	ShaderUniforms.depth_coefs[2]=0;
	ShaderUniforms.depth_coefs[3]=0;

	//printf("scale: %f, %f, %f, %f\n", ShaderUniforms.scale_coefs[0],scale_coefs[1], ShaderUniforms.scale_coefs[2], ShaderUniforms.scale_coefs[3]);


	//VERT and RAM fog color constants
	u8* fog_colvert_bgra=(u8*)&FOG_COL_VERT;
	u8* fog_colram_bgra=(u8*)&FOG_COL_RAM;
	ShaderUniforms.ps_FOG_COL_VERT[0]=fog_colvert_bgra[INDEX_GET(2)]/255.0f;
	ShaderUniforms.ps_FOG_COL_VERT[1]=fog_colvert_bgra[INDEX_GET(1)]/255.0f;
	ShaderUniforms.ps_FOG_COL_VERT[2]=fog_colvert_bgra[INDEX_GET(0)]/255.0f;

	ShaderUniforms.ps_FOG_COL_RAM[0]=fog_colram_bgra [INDEX_GET(2)]/255.0f;
	ShaderUniforms.ps_FOG_COL_RAM[1]=fog_colram_bgra [INDEX_GET(1)]/255.0f;
	ShaderUniforms.ps_FOG_COL_RAM[2]=fog_colram_bgra [INDEX_GET(0)]/255.0f;


	//Fog density constant
	u8* fog_density=(u8*)&FOG_DENSITY;
	float fog_den_mant=fog_density[INDEX_GET(1)]/128.0f;  //bit 7 -> x. bit, so [6:0] -> fraction -> /128
	s32 fog_den_exp=(s8)fog_density[INDEX_GET(0)];
#ifndef MSB_FIRST
   float fog_den_float = fog_den_mant * powf(2.0f,fog_den_exp);
#endif
	ShaderUniforms.fog_den_float= fog_den_float;


	if (fog_needs_update)
	{
		fog_needs_update=false;
      UpdateFogTexture((u8 *)FOG_TABLE);
	}

	glUseProgram(modvol_shader.program);

	glUniform4fv(modvol_shader.scale, 1, ShaderUniforms.scale_coefs);
	glUniform4fv(modvol_shader.depth_scale, 1, ShaderUniforms.depth_coefs);


	GLfloat td[4]={0.5,0,0,0};

	ShaderUniforms.PT_ALPHA=(PT_ALPHA_REF&0xFF)/255.0f;

	for (u32 i=0;i<sizeof(program_table)/sizeof(program_table[0]);i++)
	{
		PipelineShader* s=&program_table[i];
		if (s->program == -1)
			continue;

		glcache.UseProgram(s->program);

      set_shader_uniforms(&ShaderUniforms, s);
	}

	//setup render target first
	if (is_rtt)
	{
		GLuint channels,format;
		switch(FB_W_CTRL.fb_packmode)
		{
		case 0: //0x0   0555 KRGB 16 bit  (default)	Bit 15 is the value of fb_kval[7].
			channels=GL_RGBA;
			format=GL_UNSIGNED_BYTE;
			break;

		case 1: //0x1   565 RGB 16 bit
			channels=GL_RGB;
			format=GL_UNSIGNED_SHORT_5_6_5;
			break;

		case 2: //0x2   4444 ARGB 16 bit
			channels=GL_RGBA;
			format=GL_UNSIGNED_BYTE;
			break;

		case 3://0x3    1555 ARGB 16 bit    The alpha value is determined by comparison with the value of fb_alpha_threshold.
			channels=GL_RGBA;
			format=GL_UNSIGNED_BYTE;
			break;

		case 4: //0x4   888 RGB 24 bit packed
		case 5: //0x5   0888 KRGB 32 bit    K is the value of fk_kval.
		case 6: //0x6   8888 ARGB 32 bit
         fprintf(stderr, "Unsupported render to texture format: %d\n", FB_W_CTRL.fb_packmode);
         return false;
		case 7: //7     invalid
			die("7 is not valid");
			break;
		}
      //printf("RTT packmode=%d stride=%d - %d,%d -> %d,%d\n", FB_W_CTRL.fb_packmode, FB_W_LINESTRIDE.stride * 8,
 		//		FB_X_CLIP.min, FB_Y_CLIP.min, FB_X_CLIP.max, FB_Y_CLIP.max);	 		//		FB_X_CLIP.min, FB_Y_CLIP.min, FB_X_CLIP.max, FB_Y_CLIP.max);
		BindRTT(FB_W_SOF1 & VRAM_MASK, dc_width, dc_height, channels,format);
	}
   else
   {
      glViewport(0, 0, gles_screen_width, gles_screen_height);
   }

   bool wide_screen_on = !is_rtt && settings.rend.WideScreen
			&& pvrrc.fb_X_CLIP.min == 0
			&& (pvrrc.fb_X_CLIP.max + 1) / scale_x == 640
			&& pvrrc.fb_Y_CLIP.min == 0
			&& (pvrrc.fb_Y_CLIP.max + 1) / scale_y == 480;

   // Color is cleared by the bgp
   if (wide_screen_on)
      glcache.ClearColor(pvrrc.verts.head()->col[2]/255.0f,pvrrc.verts.head()->col[1]/255.0f,pvrrc.verts.head()->col[0]/255.0f,1.0f);
   else
      glcache.ClearColor(0,0,0,1.0f);

   glcache.Disable(GL_SCISSOR_TEST);

   glcache.DepthMask(GL_TRUE);
   glcache.StencilMask(0xFF);
   glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	if (UsingAutoSort())
		GenSorted();

	//move vertex to gpu

	//Main VBO
	glBindBuffer(GL_ARRAY_BUFFER, vbo.geometry);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.idxs);

	glBufferData(GL_ARRAY_BUFFER,pvrrc.verts.bytes(),pvrrc.verts.head(),GL_STREAM_DRAW);

	glBufferData(GL_ELEMENT_ARRAY_BUFFER,pvrrc.idx.bytes(),pvrrc.idx.head(),GL_STREAM_DRAW);

	//Modvol VBO
	if (pvrrc.modtrig.used())
	{
		glBindBuffer(GL_ARRAY_BUFFER, vbo.modvols);
		glBufferData(GL_ARRAY_BUFFER,pvrrc.modtrig.bytes(),pvrrc.modtrig.head(),GL_STREAM_DRAW);
	}

	int offs_x=ds2s_offs_x+0.5f;
	//this needs to be scaled

	//not all scaling affects pixel operations, scale to adjust for that
	scale_x *= scissoring_scale_x;

#if 0
   //handy to debug really stupid render-not-working issues ...
   printf("SS: %dx%d\n", gles_screen_width, gles_screen_height);
   printf("SCI: %d, %f\n", pvrrc.fb_X_CLIP.max, dc2s_scale_h);
   printf("SCI: %f, %f, %f, %f\n", offs_x+pvrrc.fb_X_CLIP.min/scale_x,(pvrrc.fb_Y_CLIP.min/scale_y)*dc2s_scale_h,(pvrrc.fb_X_CLIP.max-pvrrc.fb_X_CLIP.min+1)/scale_x*dc2s_scale_h,(pvrrc.fb_Y_CLIP.max-pvrrc.fb_Y_CLIP.min+1)/scale_y*dc2s_scale_h);
#endif

   if (!wide_screen_on)
   {
      glScissor(
            offs_x + pvrrc.fb_X_CLIP.min / scale_x,
            (pvrrc.fb_Y_CLIP.min / scale_y) * dc2s_scale_h,
            (pvrrc.fb_X_CLIP.max-pvrrc.fb_X_CLIP.min+1)/scale_x*dc2s_scale_h,
            (pvrrc.fb_Y_CLIP.max-pvrrc.fb_Y_CLIP.min+1)/scale_y*dc2s_scale_h
            );
      glcache.Enable(GL_SCISSOR_TEST);
   }

	//restore scale_x
	scale_x /= scissoring_scale_x;

   DrawStrips();

	KillTex = false;
   
   if (is_rtt)
      ReadRTTBuffer();

	return !is_rtt;
}

void rend_set_fb_scale(float x,float y)
{
	fb_scale_x=x;
	fb_scale_y=y;
}

void co_dc_yield(void);

bool ProcessFrame(TA_context* ctx)
{
#ifndef TARGET_NO_THREADS
   slock_lock(ctx->rend_inuse);
#endif
   ctx->MarkRend();

   if (KillTex)
   {
      void killtex();
      killtex();
      printf("Texture cache cleared\n");
   }

   if (!ta_parse_vdrc(ctx))
      return false;

   CollectCleanup();

   return true;
}

struct glesrend : Renderer
{
	bool Init()
   {
      libCore_vramlock_Init();

      glsm_ctl(GLSM_CTL_STATE_SETUP, NULL);

      if (!gl_create_resources())
         return false;

#ifdef GLES
      glHint(GL_GENERATE_MIPMAP_HINT, GL_FASTEST);
#endif

      return true;
   }
	void Resize(int w, int h) { gles_screen_width=w; gles_screen_height=h; }
	void Term() { libCore_vramlock_Free(); }

	bool Process(TA_context* ctx)
   {
      return ProcessFrame(ctx);
   }
	bool Render()
   {
      glsm_ctl(GLSM_CTL_STATE_BIND, NULL);
      return RenderFrame();
   }

	void Present()
   {
      glsm_ctl(GLSM_CTL_STATE_UNBIND, NULL);
      co_dc_yield();
   }

	virtual u32 GetTexture(TSP tsp, TCW tcw) {
		return gl_GetTexture(tsp, tcw);
	}
};

Renderer* rend_GLES2() { return new glesrend(); }
