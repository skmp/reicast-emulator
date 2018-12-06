#include <math.h>

#include <memalign.h>

#ifdef __SSE4_1__
#include <xmmintrin.h>
#endif

#include <libretro.h>

#include "gl4.h"
#include "../rend.h"
#include "../TexCache.h"

#include "../../hw/pvr/pvr_mem.h"
#include "../../hw/mem/_vmem.h"

extern "C" struct retro_hw_render_callback hw_render;
gl4_ctx gl4;

gl4ShaderUniforms_t gl4ShaderUniforms;

#define FOG_CHANNEL "r"
#define FOG_IMG_TYPE GL_RED

//Fragment and vertex shaders code
//
static const char* VertexShaderSource =
"#version 130 \n"
"\
#define pp_Gouraud %d \n\
 \n\
#if pp_Gouraud == 0 \n\
#define INTERPOLATION flat \n\
#else \n\
#define INTERPOLATION smooth \n\
#endif \n\
 \n\
/* Vertex constants*/  \n\
uniform highp vec4      scale; \n\
uniform highp float     extra_depth_scale; \n\
/* Vertex input */ \n\
" "in highp vec4    in_pos; \n\
" "in lowp vec4     in_base; \n\
" "in lowp vec4     in_offs; \n\
" "in mediump vec2  in_uv; \n\
" "in lowp vec4     in_base1; \n\
" "in lowp vec4     in_offs1; \n\
" "in mediump vec2  in_uv1; \n\
/* output */ \n\
INTERPOLATION out lowp vec4 vtx_base; \n\
INTERPOLATION out lowp vec4 vtx_offs; \n\
			  out mediump vec2 vtx_uv; \n\
INTERPOLATION out lowp vec4 vtx_base1; \n\
INTERPOLATION out lowp vec4 vtx_offs1; \n\
			  out mediump vec2 vtx_uv1; \n\
void main() \n\
{ \n\
	vtx_base=in_base; \n\
	vtx_offs=in_offs; \n\
	vtx_uv=in_uv; \n\
   vtx_base1 = in_base1; \n\
	vtx_offs1 = in_offs1; \n\
	vtx_uv1 = in_uv1; \n\
	vec4 vpos=in_pos; \n\
	if (vpos.z < 0.0 || vpos.z > 3.4e37) \n\
	{ \n\
	   gl_Position = vec4(0.0, 0.0, 1.0, 1.0 / vpos.z); \n\
	   return; \n\
	} \n\
	\n\
	vpos.w = extra_depth_scale / vpos.z; \n\
	vpos.z = vpos.w; \n\
	vpos.xy=vpos.xy*scale.xy-scale.zw;  \n\
	vpos.xy*=vpos.w;  \n\
	gl_Position = vpos; \n\
}";

const char* gl4PixelPipelineShader = SHADER_HEADER
"\
#define cp_AlphaTest %d \n\
#define pp_ClipTestMode %d \n\
#define pp_UseAlpha %d \n\
#define pp_Texture %d \n\
#define pp_IgnoreTexA %d \n\
#define pp_ShadInstr %d \n\
#define pp_Offset %d \n\
#define pp_FogCtrl %d \n\
#define pp_TwoVolumes %d \n\
#define pp_DepthFunc %d \n\
#define pp_Gouraud %d \n\
#define pp_BumpMap %d \n\
#define FogClamping %d \n\
#define PASS %d \n\
#define PI 3.1415926 \n\
 \n\
   #if PASS <= 1 \n\
	out vec4 FragColor; \n\
#endif \n\
 \n\
#if pp_TwoVolumes == 1 \n\
#define IF(x) if (x) \n\
#else \n\
#define IF(x) \n\
#endif \n\
 \n\
#if pp_Gouraud == 0 \n\
#define INTERPOLATION flat \n\
#else \n\
#define INTERPOLATION smooth \n\
#endif \n\
 \n\
/* Shader program params*/ \n\
/* gles has no alpha test stage, so its emulated on the shader */ \n\
uniform lowp float cp_AlphaTestValue; \n\
uniform lowp vec4 pp_ClipTest; \n\
uniform lowp vec3 sp_FOG_COL_RAM,sp_FOG_COL_VERT; \n\
uniform highp float sp_FOG_DENSITY; \n\
uniform highp float shade_scale_factor; \n\
uniform sampler2D tex0, tex1; \n\
layout(binding = 5) uniform sampler2D fog_table; \n\
uniform int pp_Number; \n\
uniform usampler2D shadow_stencil; \n\
uniform sampler2D DepthTex; \n\
uniform lowp float trilinear_alpha; \n\
uniform lowp vec4 fog_clamp_min; \n\
uniform lowp vec4 fog_clamp_max; \n\
uniform highp float extra_depth_scale; \n\
\n\
uniform ivec2 blend_mode[2]; \n\
#if pp_TwoVolumes == 1 \n\
uniform bool use_alpha[2]; \n\
uniform bool ignore_tex_alpha[2]; \n\
uniform int shading_instr[2]; \n\
uniform int fog_control[2]; \n\
#endif \n\
 \n\
/* Vertex input*/ \n\
INTERPOLATION in lowp vec4 vtx_base; \n\
INTERPOLATION in lowp vec4 vtx_offs; \n\
			  in mediump vec2 vtx_uv; \n\
INTERPOLATION in lowp vec4 vtx_base1; \n\
INTERPOLATION in lowp vec4 vtx_offs1; \n\
			  in mediump vec2 vtx_uv1; \n\
 \n\
lowp float fog_mode2(highp float w) \n\
{ \n\
   highp float z = clamp(w * extra_depth_scale * sp_FOG_DENSITY, 1.0, 255.9999); \n\
   uint i = uint(floor(log2(z))); \n\
   highp float m = z * 16 / pow(2, i) - 16; \n\
   float idx = floor(m) + i * 16 + 0.5; \n\
   vec4 fog_coef = texture(fog_table, vec2(idx / 128, 0.75 - (m - floor(m)) / 2)); \n\
   return fog_coef." FOG_CHANNEL "; \n\
} \n\
 \n\
highp vec4 fog_clamp(highp vec4 col) \n\
{ \n\
#if FogClamping == 1 \n\
	return clamp(col, fog_clamp_min, fog_clamp_max); \n\
#else \n\
	return col; \n\
#endif \n\
} \n\
 \n\
void main() \n\
{ \n\
   setFragDepth(); \n\
   \n\
   #if PASS == 3 \n\
   // Manual depth testing \n\
		highp float frontDepth = texture(DepthTex, gl_FragCoord.xy / textureSize(DepthTex, 0)).r; \n\
		#if pp_DepthFunc == 0		// Never \n\
			discard; \n\
		#elif pp_DepthFunc == 1		// Greater \n\
			if (gl_FragDepth <= frontDepth) \n\
				discard; \n\
		#elif pp_DepthFunc == 2		// Equal \n\
			if (gl_FragDepth != frontDepth) \n\
				discard; \n\
		#elif pp_DepthFunc == 3		// Greater or equal \n\
			if (gl_FragDepth < frontDepth) \n\
				discard; \n\
		#elif pp_DepthFunc == 4		// Less \n\
			if (gl_FragDepth >= frontDepth) \n\
				discard; \n\
		#elif pp_DepthFunc == 5		// Not equal \n\
			if (gl_FragDepth == frontDepth) \n\
				discard; \n\
		#elif pp_DepthFunc == 6		// Less or equal \n\
			if (gl_FragDepth > frontDepth) \n\
				discard; \n\
		#endif \n\
	#endif \n\
   \n\
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
   highp vec4 color = vtx_base; \n\
   lowp vec4 offset = vtx_offs; \n\
   mediump vec2 uv = vtx_uv; \n\
   bool area1 = false; \n\
   ivec2 cur_blend_mode = blend_mode[0]; \n\
	\n\
	#if pp_TwoVolumes == 1 \n\
      bool cur_use_alpha = use_alpha[0]; \n\
      bool cur_ignore_tex_alpha = ignore_tex_alpha[0]; \n\
      int cur_shading_instr = shading_instr[0]; \n\
      int cur_fog_control = fog_control[0]; \n\
		#if PASS == 1 \n\
			uvec4 stencil = texture(shadow_stencil, gl_FragCoord.xy / textureSize(shadow_stencil, 0)); \n\
			if (stencil.r == 0x81u) { \n\
				color = vtx_base1; \n\
				offset = vtx_offs1; \n\
				uv = vtx_uv1; \n\
				area1 = true; \n\
            cur_blend_mode = blend_mode[1]; \n\
            cur_use_alpha = use_alpha[1]; \n\
            cur_ignore_tex_alpha = ignore_tex_alpha[1]; \n\
            cur_shading_instr = shading_instr[1]; \n\
            cur_fog_control = fog_control[1]; \n\
			} \n\
		#endif\n\
	#endif\n\
	\n\
	#if pp_UseAlpha==0 || pp_TwoVolumes == 1 \n\
      IF(!cur_use_alpha) \n\
			color.a=1.0; \n\
	#endif\n\
   #if pp_FogCtrl==3 || pp_TwoVolumes == 1 // LUT Mode 2 \n\
      IF(cur_fog_control == 3) \n\
         color=vec4(sp_FOG_COL_RAM.rgb,fog_mode2(gl_FragCoord.w)); \n\
	#endif\n\
	#if pp_Texture==1 \n\
	{ \n\
      highp vec4 texcol; \n\
		if (area1) \n\
			texcol = texture(tex1, uv); \n\
		else \n\
			texcol = texture(tex0, uv); \n\
      #if pp_BumpMap == 1 \n\
			float s = PI / 2.0 * (texcol.a * 15.0 * 16.0 + texcol.r * 15.0) / 255.0; \n\
			float r = 2.0 * PI * (texcol.g * 15.0 * 16.0 + texcol.b * 15.0) / 255.0; \n\
			texcol.a = clamp(vtx_offs.a + vtx_offs.r * sin(s) + vtx_offs.g * cos(s) * cos(r - 2.0 * PI * vtx_offs.b), 0.0, 1.0); \n\
			texcol.rgb = vec3(1.0, 1.0, 1.0);	 \n\
		#else\n\
			#if pp_IgnoreTexA==1 || pp_TwoVolumes == 1 \n\
				IF(cur_ignore_tex_alpha) \n\
					texcol.a=1.0;	 \n\
			#endif\n\
			\n\
			#if cp_AlphaTest == 1 \n\
				if (cp_AlphaTestValue>texcol.a) discard;\n\
			#endif  \n\
		#endif\n\
      #if pp_ShadInstr==0 || pp_TwoVolumes == 1 // DECAL \n\
      IF(cur_shading_instr == 0) \n\
		{ \n\
         color=texcol; \n\
		} \n\
		#endif\n\
      #if pp_ShadInstr==1 || pp_TwoVolumes == 1 // MODULATE \n\
      IF(cur_shading_instr == 1) \n\
		{ \n\
			color.rgb*=texcol.rgb; \n\
			color.a=texcol.a; \n\
		} \n\
		#endif\n\
      #if pp_ShadInstr==2 || pp_TwoVolumes == 1 // DECAL ALPHA \n\
      IF(cur_shading_instr == 2) \n\
		{ \n\
			color.rgb=mix(color.rgb,texcol.rgb,texcol.a); \n\
		} \n\
		#endif\n\
      #if  pp_ShadInstr==3 || pp_TwoVolumes == 1 // MODULATE ALPHA \n\
      IF(cur_shading_instr == 3) \n\
		{ \n\
			color*=texcol; \n\
		} \n\
		#endif\n\
		\n\
		#if pp_Offset==1 && pp_BumpMap == 0 \n\
		{ \n\
         color.rgb += offset.rgb; \n\
		} \n\
		#endif\n\
	} \n\
	#endif\n\
   #if PASS == 1 && pp_TwoVolumes == 0 \n\
      uvec4 stencil = texture(shadow_stencil, gl_FragCoord.xy / textureSize(shadow_stencil, 0)); \n\
	   if (stencil.r == 0x81u) \n\
			color.rgb *= shade_scale_factor; \n\
	#endif\n\
    \n\
	color = fog_clamp(color); \n\
	 \n\
   #if pp_FogCtrl==0 || pp_TwoVolumes == 1 // LUT \n\
   	IF(cur_fog_control == 0) \n\
		{ \n\
			color.rgb=mix(color.rgb,sp_FOG_COL_RAM.rgb,fog_mode2(gl_FragCoord.w));  \n\
		} \n\
	#endif\n\
	#if pp_Offset==1 && pp_BumpMap == 0 && (pp_FogCtrl == 1 || pp_TwoVolumes == 1)  // Per vertex \n\
		IF(cur_fog_control == 1) \n\
		{ \n\
			color.rgb=mix(color.rgb, sp_FOG_COL_VERT.rgb, offset.a); \n\
		} \n\
	#endif\n\
    \n\
   color *= trilinear_alpha; \n\
	 \n\
	#if cp_AlphaTest == 1 \n\
      color.a=1.0; \n\
	#endif  \n\
   \n\
   //color.rgb=vec3(gl_FragCoord.w * sp_FOG_DENSITY / 128.0); \n\
	\n\
	#if PASS == 1 \n\
      FragColor = color; \n\
   #elif PASS > 1 \n\
      // Discard as many pixels as possible \n\
      switch (cur_blend_mode.y) // DST \n\
		{ \n\
		case ONE: \n\
         switch (cur_blend_mode.x) // SRC \n\
			{ \n\
				case ZERO: \n\
               discard; \n\
				case ONE: \n\
				case OTHER_COLOR: \n\
				case INVERSE_OTHER_COLOR: \n\
               if (color == vec4(0.0)) \n\
                  discard; \n\
					break; \n\
				case SRC_ALPHA: \n\
               if (color.a == 0.0 || color.rgb == vec3(0.0)) \n\
                  discard; \n\
               break; \n\
				case INVERSE_SRC_ALPHA: \n\
               if (color.a == 1.0 || color.rgb == vec3(0.0)) \n\
                  discard; \n\
					break; \n\
			} \n\
			break; \n\
		case OTHER_COLOR: \n\
         if (cur_blend_mode.x == ZERO && color == vec4(1.0)) \n\
            discard; \n\
			break; \n\
		case INVERSE_OTHER_COLOR: \n\
         if (cur_blend_mode.x <= SRC_ALPHA && color == vec4(0.0)) \n\
            discard; \n\
			break; \n\
		case SRC_ALPHA: \n\
         if ((cur_blend_mode.x == ZERO || cur_blend_mode.x == INVERSE_SRC_ALPHA) && color.a == 1.0) \n\
            discard; \n\
			break; \n\
		case INVERSE_SRC_ALPHA: \n\
         switch (cur_blend_mode.x) // SRC \n\
			{ \n\
				case ZERO: \n\
				case SRC_ALPHA: \n\
               if (color.a == 0.0) \n\
                  discard; \n\
					break; \n\
				case ONE: \n\
				case OTHER_COLOR: \n\
				case INVERSE_OTHER_COLOR: \n\
               if (color == vec4(0.0)) \n\
                  discard; \n\
					break; \n\
			} \n\
			break; \n\
		} \n\
		\n\
      ivec2 coords = ivec2(gl_FragCoord.xy); \n\
      uint idx =  getNextPixelIndex(); \n\
      Pixel pixel; \n\
      pixel.color = color; \n\
      pixel.depth = gl_FragDepth; \n\
      pixel.seq_num = uint(pp_Number); \n\
      pixel.next = imageAtomicExchange(abufferPointerImg, coords, idx); \n\
      pixels[idx] = pixel; \n\
      \n\
      discard; \n\
		\n\
	#endif \n\
}";

static const char* ModifierVolumeShader = SHADER_HEADER
" \
/* Vertex input*/ \n\
void main() \n\
{ \n\
   setFragDepth(); \n\
      \n\
}";

bool gl4CompilePipelineShader(gl4PipelineShader *s, const char *source /* = PixelPipelineShader */)
{
   char vshader[16384];

	sprintf(vshader, VertexShaderSource, s->pp_Gouraud);
	char pshader[16384];

	sprintf(pshader, source,
                s->cp_AlphaTest,s->pp_ClipTestMode,s->pp_UseAlpha,
                s->pp_Texture,s->pp_IgnoreTexA,s->pp_ShadInstr,s->pp_Offset,s->pp_FogCtrl, s->pp_TwoVolumes, s->pp_DepthFunc, s->pp_Gouraud, s->pp_BumpMap, s->fog_clamping, s->pass);


   s->program = gl_CompileAndLink(vshader, pshader);


	//setup texture 0 as the input for the shader
   GLint gu = glGetUniformLocation(s->program, "tex0");
   if (s->pp_Texture == 1 && gu != -1)
		glUniform1i(gu,0);
   // Setup texture 1 as the input for area 1 in two volume mode
   gu = glGetUniformLocation(s->program, "tex1");
   if (s->pp_Texture == 1 && gu != -1)
      glUniform1i(gu, 1);

	//get the uniform locations
	s->scale	             = glGetUniformLocation(s->program, "scale");

	s->extra_depth_scale = glGetUniformLocation(s->program, "extra_depth_scale");

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
	s->shade_scale_factor = glGetUniformLocation(s->program, "shade_scale_factor");

   // Use texture 1 for depth texture
	gu = glGetUniformLocation(s->program, "DepthTex");
	if (gu != -1)
		glUniform1i(gu, 2);     // GL_TEXTURE2

   s->trilinear_alpha = glGetUniformLocation(s->program, "trilinear_alpha");

   if (s->fog_clamping)
   {
      s->fog_clamp_min = glGetUniformLocation(s->program, "fog_clamp_min");
      s->fog_clamp_max = glGetUniformLocation(s->program, "fog_clamp_max");
   }
   else
   {
      s->fog_clamp_min = -1;
      s->fog_clamp_max = -1;
   }


   // Shadow stencil for OP/PT rendering pass
   gu = glGetUniformLocation(s->program, "shadow_stencil");
   if (gu != -1)
   	glUniform1i(gu, 3);		// GL_TEXTURE3

   s->pp_Number = glGetUniformLocation(s->program, "pp_Number");
   s->pp_DepthFunc = glGetUniformLocation(s->program, "pp_DepthFunc");

   s->blend_mode = glGetUniformLocation(s->program, "blend_mode");
   s->use_alpha = glGetUniformLocation(s->program, "use_alpha");
   s->ignore_tex_alpha = glGetUniformLocation(s->program, "ignore_tex_alpha");
   s->shading_instr = glGetUniformLocation(s->program, "shading_instr");
   s->fog_control = glGetUniformLocation(s->program, "fog_control");

	return glIsProgram(s->program)==GL_TRUE;
}

static void gl_term(void)
{
   glDeleteProgram(gl4.modvol_shader.program);
   glDeleteBuffers(1, &gl4.vbo.geometry);
   glDeleteBuffers(1, &gl4.vbo.modvols);
   glDeleteBuffers(1, &gl4.vbo.idxs);
   glDeleteBuffers(1, &gl4.vbo.idxs2);
   glDeleteBuffers(1, &gl4.vbo.tr_poly_params);
   for (auto it = gl4.shaders.begin(); it != gl4.shaders.end(); it++)
   {
	  if (it->second->program != -1)
		 glDeleteProgram(it->second->program);
	  delete it->second;
   }
   gl4.shaders.clear();
   glDeleteTextures(1, &fbTextureId);
   fbTextureId = 0;
}

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

	/* create VBOs */
	glGenBuffers(1, &gl4.vbo.geometry);
	glGenBuffers(1, &gl4.vbo.modvols);
	glGenBuffers(1, &gl4.vbo.idxs);
	glGenBuffers(1, &gl4.vbo.idxs2);

   char vshader[16384];
	sprintf(vshader, VertexShaderSource, 1);

   gl4.modvol_shader.program=gl_CompileAndLink(vshader, ModifierVolumeShader);
	gl4.modvol_shader.scale          = glGetUniformLocation(gl4.modvol_shader.program, "scale");
   gl4.modvol_shader.extra_depth_scale = glGetUniformLocation(gl4.modvol_shader.program, "extra_depth_scale");

   // Create the buffer for Translucent poly params
	glGenBuffers(1, &gl4.vbo.tr_poly_params);
	// Bind it
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, gl4.vbo.tr_poly_params);
	// Declare storage
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, gl4.vbo.tr_poly_params);
	glCheck();

	return true;
}

#ifdef MSB_FIRST
#define INDEX_GET(a) (a^3)
#else
#define INDEX_GET(a) (a)
#endif

static bool RenderFrame(void)
{
   int vmu_screen_number = 0 ;

   static int old_screen_width, old_screen_height;
	if (screen_width != old_screen_width || screen_height != old_screen_height) {
		rend_resize(screen_width, screen_height);
		old_screen_width = screen_width;
		old_screen_height = screen_height;
	}
   DoCleanup();

	bool is_rtt=pvrrc.isRTT;

	//if (FrameCount&7) return;

	//Setup the matrix
   float vtx_min_fZ = 0.f;
	float vtx_max_fZ = pvrrc.fZ_max;
   //printf("Zmin %g Zmax %g\n", pvrrc.fZ_min, pvrrc.fZ_max);
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

      dc_width = pvrrc.fb_X_CLIP.max - pvrrc.fb_X_CLIP.min + 1;
		dc_height = pvrrc.fb_Y_CLIP.max - pvrrc.fb_Y_CLIP.min + 1;
	}

	scale_x = 1;
   scale_y = 1;

	float scissoring_scale_x = 1;

	if (!is_rtt && !pvrrc.isRenderFramebuffer)
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

      if (SCALER_CTL.hscale)
      {
         scissoring_scale_x /= 2;
         scale_x*=2;
      }
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
	float dc2s_scale_h = is_rtt ? (screen_width / dc_width) : (screen_height/480.0);
	float ds2s_offs_x  = is_rtt ? 0 : ((screen_width-dc2s_scale_h*640)/2);

	//-1 -> too much to left
	gl4ShaderUniforms.scale_coefs[0]=2.0f/(screen_width/dc2s_scale_h*scale_x);
	gl4ShaderUniforms.scale_coefs[1]= (is_rtt?2:-2) / dc_height;
   // FIXME CT2 needs 480 here instead of dc_height=512
	gl4ShaderUniforms.scale_coefs[2]=1-2*ds2s_offs_x/(screen_width);
	gl4ShaderUniforms.scale_coefs[3]=(is_rtt?1:-1);

	gl4ShaderUniforms.extra_depth_scale = settings.rend.ExtraDepthScale;

	//printf("scale: %f, %f, %f, %f\n", ShaderUniforms.scale_coefs[0],scale_coefs[1], ShaderUniforms.scale_coefs[2], ShaderUniforms.scale_coefs[3]);


	//VERT and RAM fog color constants
	u8* fog_colvert_bgra=(u8*)&FOG_COL_VERT;
	u8* fog_colram_bgra=(u8*)&FOG_COL_RAM;
	gl4ShaderUniforms.ps_FOG_COL_VERT[0]=fog_colvert_bgra[INDEX_GET(2)]/255.0f;
	gl4ShaderUniforms.ps_FOG_COL_VERT[1]=fog_colvert_bgra[INDEX_GET(1)]/255.0f;
	gl4ShaderUniforms.ps_FOG_COL_VERT[2]=fog_colvert_bgra[INDEX_GET(0)]/255.0f;

	gl4ShaderUniforms.ps_FOG_COL_RAM[0]=fog_colram_bgra [INDEX_GET(2)]/255.0f;
	gl4ShaderUniforms.ps_FOG_COL_RAM[1]=fog_colram_bgra [INDEX_GET(1)]/255.0f;
	gl4ShaderUniforms.ps_FOG_COL_RAM[2]=fog_colram_bgra [INDEX_GET(0)]/255.0f;


	//Fog density constant
	u8* fog_density=(u8*)&FOG_DENSITY;
	float fog_den_mant=fog_density[INDEX_GET(1)]/128.0f;  //bit 7 -> x. bit, so [6:0] -> fraction -> /128
	s32 fog_den_exp=(s8)fog_density[INDEX_GET(0)];
#ifndef MSB_FIRST
   float fog_den_float = fog_den_mant * powf(2.0f,fog_den_exp);
#endif
   gl4ShaderUniforms.fog_den_float= fog_den_float;

   gl4ShaderUniforms.fog_clamp_min[0] = ((pvrrc.fog_clamp_min >> 16) & 0xFF) / 255.0f;
   gl4ShaderUniforms.fog_clamp_min[1] = ((pvrrc.fog_clamp_min >> 8) & 0xFF) / 255.0f;
   gl4ShaderUniforms.fog_clamp_min[2] = ((pvrrc.fog_clamp_min >> 0) & 0xFF) / 255.0f;
   gl4ShaderUniforms.fog_clamp_min[3] = ((pvrrc.fog_clamp_min >> 24) & 0xFF) / 255.0f;
	
   gl4ShaderUniforms.fog_clamp_max[0] = ((pvrrc.fog_clamp_max >> 16) & 0xFF) / 255.0f;
   gl4ShaderUniforms.fog_clamp_max[1] = ((pvrrc.fog_clamp_max >> 8) & 0xFF) / 255.0f;
   gl4ShaderUniforms.fog_clamp_max[2] = ((pvrrc.fog_clamp_max >> 0) & 0xFF) / 255.0f;
   gl4ShaderUniforms.fog_clamp_max[3] = ((pvrrc.fog_clamp_max >> 24) & 0xFF) / 255.0f;

   if (fog_needs_update)
   {
	  fog_needs_update=false;
      UpdateFogTexture((u8 *)FOG_TABLE, GL_TEXTURE5, GL_RED);
   }

	glUseProgram(gl4.modvol_shader.program);

	glUniform4fv(gl4.modvol_shader.scale, 1, gl4ShaderUniforms.scale_coefs);
	glUniform1f(gl4.modvol_shader.extra_depth_scale, gl4ShaderUniforms.extra_depth_scale);

	GLfloat td[4]={0.5,0,0,0};

	gl4ShaderUniforms.PT_ALPHA=(PT_ALPHA_REF&0xFF)/255.0f;

	GLuint output_fbo;

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
      output_fbo = gl4BindRTT(FB_W_SOF1 & VRAM_MASK, dc_width, dc_height, channels, format);
	}
   else
   {
      glViewport(0, 0, screen_width, screen_height);
      output_fbo = hw_render.get_current_framebuffer();
   }

   bool wide_screen_on = !is_rtt && settings.rend.WideScreen
			&& pvrrc.fb_X_CLIP.min == 0
			&& (pvrrc.fb_X_CLIP.max + 1) / scale_x == 640
			&& pvrrc.fb_Y_CLIP.min == 0
			&& (pvrrc.fb_Y_CLIP.max + 1) / scale_y == 480;

   // Color is cleared by the background plane

   glcache.Disable(GL_SCISSOR_TEST);

	//move vertex to gpu
   if (!pvrrc.isRenderFramebuffer)
   {
      //Main VBO
	   glBindBuffer(GL_ARRAY_BUFFER, gl4.vbo.geometry); glCheck();
	   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl4.vbo.idxs); glCheck();

      glBufferData(GL_ARRAY_BUFFER,pvrrc.verts.bytes(),pvrrc.verts.head(),GL_STREAM_DRAW); glCheck();

      glBufferData(GL_ELEMENT_ARRAY_BUFFER,pvrrc.idx.bytes(),pvrrc.idx.head(),GL_STREAM_DRAW); glCheck();

      //Modvol VBO
      if (pvrrc.modtrig.used())
	   {
         glBindBuffer(GL_ARRAY_BUFFER, gl4.vbo.modvols); glCheck();
         glBufferData(GL_ARRAY_BUFFER,pvrrc.modtrig.bytes(),pvrrc.modtrig.head(),GL_STREAM_DRAW); glCheck();
      }

      // TR PolyParam data
      glBindBuffer(GL_SHADER_STORAGE_BUFFER, gl4.vbo.tr_poly_params);
      glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(struct PolyParam) * pvrrc.global_param_tr.used(), pvrrc.global_param_tr.head(), GL_STATIC_DRAW);
      glCheck();

      //not all scaling affects pixel operations, scale to adjust for that
      scale_x *= scissoring_scale_x;

#if 0
      //handy to debug really stupid render-not-working issues ...
      printf("SS: %dx%d\n", screen_width, screen_height);
      printf("SCI: %d, %f\n", pvrrc.fb_X_CLIP.max, dc2s_scale_h);
      printf("SCI: %f, %f, %f, %f\n", offs_x+pvrrc.fb_X_CLIP.min/scale_x,(pvrrc.fb_Y_CLIP.min/scale_y)*dc2s_scale_h,(pvrrc.fb_X_CLIP.max-pvrrc.fb_X_CLIP.min+1)/scale_x*dc2s_scale_h,(pvrrc.fb_Y_CLIP.max-pvrrc.fb_Y_CLIP.min+1)/scale_y*dc2s_scale_h);
#endif

      if (!wide_screen_on)
      {
         float width  = (pvrrc.fb_X_CLIP.max - pvrrc.fb_X_CLIP.min + 1) / scale_x;
         float height = (pvrrc.fb_Y_CLIP.max - pvrrc.fb_Y_CLIP.min + 1) / scale_y;
         float min_x  = pvrrc.fb_X_CLIP.min / scale_x;
         float min_y  = pvrrc.fb_Y_CLIP.min / scale_y;
         if (!is_rtt)
         {
            // Add x offset for aspect ratio > 4/3
            min_x = min_x * dc2s_scale_h + ds2s_offs_x;
            // Invert y coordinates when rendering to screen
            min_y = screen_height - (min_y + height) * dc2s_scale_h;
            width *= dc2s_scale_h;
            height *= dc2s_scale_h;

			if (ds2s_offs_x > 0)
			{
			   float rounded_offs_x = ds2s_offs_x + 0.5f;

			   glcache.ClearColor(0.f, 0.f, 0.f, 0.f);
			   glcache.Enable(GL_SCISSOR_TEST);
			   glScissor(0, 0, rounded_offs_x, screen_height);
			   glClear(GL_COLOR_BUFFER_BIT);
			   glScissor(screen_width - rounded_offs_x, 0, rounded_offs_x, screen_height);
			   glClear(GL_COLOR_BUFFER_BIT);
			}
         }
         else if (settings.rend.RenderToTextureUpscale > 1 && !settings.rend.RenderToTextureBuffer)
         {
            min_x *= settings.rend.RenderToTextureUpscale;
            min_y *= settings.rend.RenderToTextureUpscale;
            width *= settings.rend.RenderToTextureUpscale;
            height *= settings.rend.RenderToTextureUpscale;
         }

         glScissor(min_x + 0.5f, min_y + 0.5f, width + 0.5f, height + 0.5f);
         glcache.Enable(GL_SCISSOR_TEST);
      }

      //restore scale_x
      scale_x /= scissoring_scale_x;
      gl4DrawStrips(output_fbo);
   }
   else
   {
	  glBindFramebuffer(GL_FRAMEBUFFER, output_fbo);
      glcache.ClearColor(0.f, 0.f, 0.f, 0.f);
      glClear(GL_COLOR_BUFFER_BIT);
      gl4DrawFramebuffer(dc_width, dc_height);
   }


   for ( vmu_screen_number = 0 ; vmu_screen_number < 4 ; vmu_screen_number++)
      if ( vmu_screen_params[vmu_screen_number].vmu_screen_display )
         gl4DrawVmuTexture(vmu_screen_number, true) ;

	KillTex = false;
   
   if (is_rtt)
      ReadRTTBuffer();

	return !is_rtt;
}
extern void initABuffer();
void termABuffer();

void gl_DebugOutput(GLenum source,
                            GLenum type,
                            GLuint id,
                            GLenum severity,
                            GLsizei length,
                            const GLchar *message,
                            void *userParam)
{
    // ignore non-significant error/warning codes
    if(id == 131169 || id == 131185 || id == 131218 || id == 131204) return;
    if (id == 131186)
    	// Warning when fetching the atomic_uint pixel count
    	return;

    printf("OpenGL Debug message (%d): %s\n", id, message);

    switch (source)
    {
        case GL_DEBUG_SOURCE_API:             printf("Source: API"); break;
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   printf("Source: Window System"); break;
        case GL_DEBUG_SOURCE_SHADER_COMPILER: printf("Source: Shader Compiler"); break;
        case GL_DEBUG_SOURCE_THIRD_PARTY:     printf("Source: Third Party"); break;
        case GL_DEBUG_SOURCE_APPLICATION:     printf("Source: Application"); break;
        case GL_DEBUG_SOURCE_OTHER:           printf("Source: Other"); break;
    }
    printf(" ");

    switch (type)
    {
        case GL_DEBUG_TYPE_ERROR:               printf("Type: Error"); break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: printf("Type: Deprecated Behaviour"); break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  printf("Type: Undefined Behaviour"); break;
        case GL_DEBUG_TYPE_PORTABILITY:         printf("Type: Portability"); break;
        case GL_DEBUG_TYPE_PERFORMANCE:         printf("Type: Performance"); break;
        case GL_DEBUG_TYPE_MARKER:              printf("Type: Marker"); break;
        case GL_DEBUG_TYPE_PUSH_GROUP:          printf("Type: Push Group"); break;
        case GL_DEBUG_TYPE_POP_GROUP:           printf("Type: Pop Group"); break;
        case GL_DEBUG_TYPE_OTHER:               printf("Type: Other"); break;
    }
    printf(" ");

    switch (severity)
    {
        case GL_DEBUG_SEVERITY_HIGH:         printf("Severity: high"); break;
        case GL_DEBUG_SEVERITY_MEDIUM:       printf("Severity: medium"); break;
        case GL_DEBUG_SEVERITY_LOW:          printf("Severity: low"); break;
        case GL_DEBUG_SEVERITY_NOTIFICATION: printf("Severity: notification"); break;
    };
    printf("\n");
}

void reshapeABuffer(int w, int h);

struct gl4rend : Renderer
{
   bool Init()
   {
	  int major = 0;
	  int minor = 0;
	  glGetIntegerv(GL_MAJOR_VERSION, &major);
	  glGetIntegerv(GL_MINOR_VERSION, &minor);
	  if (major < 4 || (major == 4 && minor < 3))
	  {
		 printf("Warning: OpenGL %d.%d doesn't support per-pixel sorting. 4.3 required\n", major, minor);
		 return false;
	  }

	  if (!gl_create_resources())
         return false;

      glcache.DisableCache();

//    glEnable(GL_DEBUG_OUTPUT);
//    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
//    glDebugMessageCallback(gl_DebugOutput, NULL);
//    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);

      initABuffer();

#ifdef HAVE_TEXUPSCALE
      if (settings.rend.TextureUpscale > 1)
      {
         // Trick to preload the tables used by xBRZ
         u32 src[] { 0x11111111, 0x22222222, 0x33333333, 0x44444444 };
         u32 dst[16];
         UpscalexBRZ(2, src, dst, 2, 2, false);
      }
#endif
      fog_needs_update = true;

      return true;
   }
   void Resize(int w, int h)
	{
		screen_width=w;
		screen_height=h;
		if (stencilTexId != 0)
		{
			glcache.DeleteTextures(1, &stencilTexId);
			stencilTexId = 0;
		}
      if (depthTexId != 0)
		{
			glcache.DeleteTextures(1, &depthTexId);
			depthTexId = 0;
		}
		if (opaqueTexId != 0)
		{
			glcache.DeleteTextures(1, &opaqueTexId);
			opaqueTexId = 0;
		}
      if (depthSaveTexId != 0)
		{
			glcache.DeleteTextures(1, &depthSaveTexId);
			depthSaveTexId = 0;
		}
      reshapeABuffer(w, h);
	}
	void Term()
	{
	   termABuffer();
	   if (stencilTexId != 0)
	   {
		  glcache.DeleteTextures(1, &stencilTexId);
		  stencilTexId = 0;
	   }
	   if (depthTexId != 0)
	   {
		  glcache.DeleteTextures(1, &depthTexId);
		  depthTexId = 0;
	   }
	   if (opaqueTexId != 0)
	   {
		  glcache.DeleteTextures(1, &opaqueTexId);
		  opaqueTexId = 0;
	   }
	   if (depthSaveTexId != 0)
	   {
		  glcache.DeleteTextures(1, &depthSaveTexId);
		  depthSaveTexId = 0;
	   }
	   killtex();

	   gl_term();
   }

	bool Process(TA_context* ctx)
   {
#if !defined(TARGET_NO_THREADS)
      if (!settings.rend.ThreadedRendering)
#endif
         glsm_ctl(GLSM_CTL_STATE_BIND, NULL);
      return ProcessFrame(ctx);
   }
	bool Render()
   {
      bool ret = RenderFrame();
#if !defined(TARGET_NO_THREADS)
      if (!settings.rend.ThreadedRendering)
#endif
    	  glsm_ctl(GLSM_CTL_STATE_UNBIND, NULL);
      return ret;
   }

	void Present()
   {
      co_dc_yield();
   }

	virtual u32 GetTexture(TSP tsp, TCW tcw) {
		return gl_GetTexture(tsp, tcw);
	}
};

Renderer* rend_GL4() { return new gl4rend(); }
