#include <math.h>

#include <libretro.h>

#include "gl4.h"
#include "rend/gles/glcache.h"
#include "rend/rend.h"
#include "rend/TexCache.h"

extern "C" struct retro_hw_render_callback hw_render;
gl4_ctx gl4;

gl4ShaderUniforms_t gl4ShaderUniforms;

#define FOG_CHANNEL "r"
#define FOG_IMG_TYPE GL_RED

//Fragment and vertex shaders code

static const char* VertexShaderSource = R"(#version 130
#define pp_Gouraud %d

#if pp_Gouraud == 0
#define INTERPOLATION flat
#else
#define INTERPOLATION smooth
#endif

/* Vertex constants*/ 
uniform highp vec4      scale;
uniform highp float     extra_depth_scale;
/* Vertex input */
in highp vec4    in_pos;
in lowp vec4     in_base;
in lowp vec4     in_offs;
in mediump vec2  in_uv;
in lowp vec4     in_base1;
in lowp vec4     in_offs1;
in mediump vec2  in_uv1;
/* output */
INTERPOLATION out lowp vec4 vtx_base;
INTERPOLATION out lowp vec4 vtx_offs;
			  out mediump vec2 vtx_uv;
INTERPOLATION out lowp vec4 vtx_base1;
INTERPOLATION out lowp vec4 vtx_offs1;
			  out mediump vec2 vtx_uv1;
void main()
{
	vtx_base=in_base;
	vtx_offs=in_offs;
	vtx_uv=in_uv;
   vtx_base1 = in_base1;
	vtx_offs1 = in_offs1;
	vtx_uv1 = in_uv1;
	vec4 vpos=in_pos;
	if (vpos.z < 0.0 || vpos.z > 3.4e37)
	{
	   gl_Position = vec4(0.0, 0.0, 1.0, 1.0 / vpos.z);
	   return;
	}
	
	vpos.w = extra_depth_scale / vpos.z;
	vpos.z = vpos.w;
	vpos.xy=vpos.xy*scale.xy-scale.zw; 
	vpos.xy*=vpos.w; 
	gl_Position = vpos;
}
)";

const char* gl4PixelPipelineShader = SHADER_HEADER
R"(
#define cp_AlphaTest %d
#define pp_ClipTestMode %d
#define pp_UseAlpha %d
#define pp_Texture %d
#define pp_IgnoreTexA %d
#define pp_ShadInstr %d
#define pp_Offset %d
#define pp_FogCtrl %d
#define pp_TwoVolumes %d
#define pp_Gouraud %d
#define pp_BumpMap %d
#define FogClamping %d
#define PASS %d
#define PI 3.1415926

#define PASS_DEPTH 0
#define PASS_COLOR 1
#define PASS_OIT 2

#if PASS == PASS_DEPTH || PASS == PASS_COLOR
	out vec4 FragColor;
#endif

#if pp_TwoVolumes == 1
#define IF(x) if (x)
#else
#define IF(x)
#endif

#if pp_Gouraud == 0
#define INTERPOLATION flat
#else
#define INTERPOLATION smooth
#endif

/* Shader program params*/
uniform lowp float cp_AlphaTestValue;
uniform lowp vec4 pp_ClipTest;
uniform lowp vec3 sp_FOG_COL_RAM,sp_FOG_COL_VERT;
uniform highp float sp_FOG_DENSITY;
uniform highp float shade_scale_factor;
uniform sampler2D tex0, tex1;
layout(binding = 5) uniform sampler2D fog_table;
uniform int pp_Number;
uniform usampler2D shadow_stencil;
uniform sampler2D DepthTex;
uniform lowp float trilinear_alpha;
uniform lowp vec4 fog_clamp_min;
uniform lowp vec4 fog_clamp_max;

uniform ivec2 blend_mode[2];
#if pp_TwoVolumes == 1
uniform bool use_alpha[2];
uniform bool ignore_tex_alpha[2];
uniform int shading_instr[2];
uniform int fog_control[2];
#endif

/* Vertex input*/
INTERPOLATION in lowp vec4 vtx_base;
INTERPOLATION in lowp vec4 vtx_offs;
			  in mediump vec2 vtx_uv;
INTERPOLATION in lowp vec4 vtx_base1;
INTERPOLATION in lowp vec4 vtx_offs1;
			  in mediump vec2 vtx_uv1;

lowp float fog_mode2(highp float w)
{
   highp float z = clamp(w * sp_FOG_DENSITY, 1.0, 255.9999);
   uint i = uint(floor(log2(z)));
   highp float m = z * 16 / pow(2, i) - 16;
   float idx = floor(m) + i * 16 + 0.5;
   vec4 fog_coef = texture(fog_table, vec2(idx / 128, 0.75 - (m - floor(m)) / 2));
   return fog_coef.r;
}

highp vec4 fog_clamp(highp vec4 col)
{
#if FogClamping == 1
	return clamp(col, fog_clamp_min, fog_clamp_max);
#else
	return col;
#endif
}

void main()
{
   setFragDepth();
  
   #if PASS == PASS_OIT
      // Manual depth testing
      highp float frontDepth = texture(DepthTex, gl_FragCoord.xy / textureSize(DepthTex, 0)).r;
      if (gl_FragDepth < frontDepth)
         discard;
   #endif
  
	// Clip outside the box
	#if pp_ClipTestMode==1
		if (gl_FragCoord.x < pp_ClipTest.x || gl_FragCoord.x > pp_ClipTest.z
				|| gl_FragCoord.y < pp_ClipTest.y || gl_FragCoord.y > pp_ClipTest.w)
			discard;
	#endif
	// Clip inside the box
	#if pp_ClipTestMode==-1
		if (gl_FragCoord.x >= pp_ClipTest.x && gl_FragCoord.x <= pp_ClipTest.z
				&& gl_FragCoord.y >= pp_ClipTest.y && gl_FragCoord.y <= pp_ClipTest.w)
			discard;
	#endif
	
   highp vec4 color = vtx_base;
   lowp vec4 offset = vtx_offs;
   mediump vec2 uv = vtx_uv;
   bool area1 = false;
   ivec2 cur_blend_mode = blend_mode[0];
	
	#if pp_TwoVolumes == 1
      bool cur_use_alpha = use_alpha[0];
      bool cur_ignore_tex_alpha = ignore_tex_alpha[0];
      int cur_shading_instr = shading_instr[0];
      int cur_fog_control = fog_control[0];
      #if PASS == PASS_COLOR
			uvec4 stencil = texture(shadow_stencil, gl_FragCoord.xy / textureSize(shadow_stencil, 0));
			if (stencil.r == 0x81u) {
				color = vtx_base1;
				offset = vtx_offs1;
				uv = vtx_uv1;
				area1 = true;
            cur_blend_mode = blend_mode[1];
            cur_use_alpha = use_alpha[1];
            cur_ignore_tex_alpha = ignore_tex_alpha[1];
            cur_shading_instr = shading_instr[1];
            cur_fog_control = fog_control[1];
			}
      #endif
	#endif
	
	#if pp_UseAlpha==0 || pp_TwoVolumes == 1
      IF(!cur_use_alpha)
			color.a=1.0;
	#endif
   #if pp_FogCtrl==3 || pp_TwoVolumes == 1 // LUT Mode 2
      IF(cur_fog_control == 3)
         color=vec4(sp_FOG_COL_RAM.rgb,fog_mode2(gl_FragCoord.w));
	#endif
	#if pp_Texture==1
	{
      highp vec4 texcol;
		if (area1)
			texcol = texture(tex1, uv);
		else
			texcol = texture(tex0, uv);
      #if pp_BumpMap == 1
			float s = PI / 2.0 * (texcol.a * 15.0 * 16.0 + texcol.r * 15.0) / 255.0;
			float r = 2.0 * PI * (texcol.g * 15.0 * 16.0 + texcol.b * 15.0) / 255.0;
			texcol.a = clamp(vtx_offs.a + vtx_offs.r * sin(s) + vtx_offs.g * cos(s) * cos(r - 2.0 * PI * vtx_offs.b), 0.0, 1.0);
			texcol.rgb = vec3(1.0, 1.0, 1.0);	
		#else
			#if pp_IgnoreTexA==1 || pp_TwoVolumes == 1
				IF(cur_ignore_tex_alpha)
					texcol.a=1.0;	
			#endif
			
			#if cp_AlphaTest == 1
				if (cp_AlphaTestValue>texcol.a) discard;
			#endif 
		#endif
      #if pp_ShadInstr==0 || pp_TwoVolumes == 1 // DECAL
      IF(cur_shading_instr == 0)
		{
         color=texcol;
		}
		#endif
      #if pp_ShadInstr==1 || pp_TwoVolumes == 1 // MODULATE
      IF(cur_shading_instr == 1)
		{
			color.rgb*=texcol.rgb;
			color.a=texcol.a;
		}
		#endif
      #if pp_ShadInstr==2 || pp_TwoVolumes == 1 // DECAL ALPHA
      IF(cur_shading_instr == 2)
		{
			color.rgb=mix(color.rgb,texcol.rgb,texcol.a);
		}
		#endif
      #if  pp_ShadInstr==3 || pp_TwoVolumes == 1 // MODULATE ALPHA
      IF(cur_shading_instr == 3)
		{
			color*=texcol;
		}
		#endif
		
		#if pp_Offset==1 && pp_BumpMap == 0
		{
         color.rgb += offset.rgb;
		}
		#endif
	}
	#endif
	#if PASS == PASS_COLOR && pp_TwoVolumes == 0
      uvec4 stencil = texture(shadow_stencil, gl_FragCoord.xy / textureSize(shadow_stencil, 0));
	   if (stencil.r == 0x81u)
			color.rgb *= shade_scale_factor;
	#endif
   
	color = fog_clamp(color);
	
   #if pp_FogCtrl==0 || pp_TwoVolumes == 1 // LUT
   	IF(cur_fog_control == 0)
		{
			color.rgb=mix(color.rgb,sp_FOG_COL_RAM.rgb,fog_mode2(gl_FragCoord.w)); 
		}
	#endif
	#if pp_Offset==1 && pp_BumpMap == 0 && (pp_FogCtrl == 1 || pp_TwoVolumes == 1)  // Per vertex
		IF(cur_fog_control == 1)
		{
			color.rgb=mix(color.rgb, sp_FOG_COL_VERT.rgb, offset.a);
		}
	#endif
   
   color *= trilinear_alpha;
	
	#if cp_AlphaTest == 1
      color.a=1.0;
	#endif 
  
   //color.rgb=vec3(gl_FragCoord.w * sp_FOG_DENSITY / 128.0);
	
	#if PASS == PASS_COLOR 
      FragColor = color;
	#elif PASS == PASS_OIT
      // Discard as many pixels as possible
      switch (cur_blend_mode.y) // DST
		{
		case ONE:
         switch (cur_blend_mode.x) // SRC
			{
				case ZERO:
               discard;
				case ONE:
				case OTHER_COLOR:
				case INVERSE_OTHER_COLOR:
               if (color == vec4(0.0))
                  discard;
					break;
				case SRC_ALPHA:
               if (color.a == 0.0 || color.rgb == vec3(0.0))
                  discard;
               break;
				case INVERSE_SRC_ALPHA:
               if (color.a == 1.0 || color.rgb == vec3(0.0))
                  discard;
					break;
			}
			break;
		case OTHER_COLOR:
         if (cur_blend_mode.x == ZERO && color == vec4(1.0))
            discard;
			break;
		case INVERSE_OTHER_COLOR:
         if (cur_blend_mode.x <= SRC_ALPHA && color == vec4(0.0))
            discard;
			break;
		case SRC_ALPHA:
         if ((cur_blend_mode.x == ZERO || cur_blend_mode.x == INVERSE_SRC_ALPHA) && color.a == 1.0)
            discard;
			break;
		case INVERSE_SRC_ALPHA:
         switch (cur_blend_mode.x) // SRC
			{
				case ZERO:
				case SRC_ALPHA:
               if (color.a == 0.0)
                  discard;
					break;
				case ONE:
				case OTHER_COLOR:
				case INVERSE_OTHER_COLOR:
               if (color == vec4(0.0))
                  discard;
					break;
			}
			break;
		}
		
      ivec2 coords = ivec2(gl_FragCoord.xy);
      uint idx =  getNextPixelIndex();
      
      Pixel pixel;
      pixel.color = color;
      pixel.depth = gl_FragDepth;
      pixel.seq_num = uint(pp_Number);
      pixel.next = imageAtomicExchange(abufferPointerImg, coords, idx);
      pixels[idx] = pixel;
     
      discard;
		
	#endif
}
)";

static const char* ModifierVolumeShader = SHADER_HEADER
R"(
void main()
{
   setFragDepth();
}
)";

int max_image_width;
int max_image_height;
extern GLuint fogTextureId;

bool gl4CompilePipelineShader(gl4PipelineShader *s, const char *pixel_source /* = PixelPipelineShader */, const char *vertex_source /* = NULL */)
{
   char vshader[16384];

	sprintf(vshader, vertex_source == NULL ? VertexShaderSource : vertex_source, s->pp_Gouraud);
	char pshader[16384];

	sprintf(pshader, pixel_source,
                s->cp_AlphaTest,s->pp_ClipTestMode,s->pp_UseAlpha,
                s->pp_Texture,s->pp_IgnoreTexA,s->pp_ShadInstr,s->pp_Offset,s->pp_FogCtrl, s->pp_TwoVolumes, s->pp_Gouraud, s->pp_BumpMap, s->fog_clamping, (int)s->pass);


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
   glDeleteVertexArrays(1, &gl4.vbo.main_vao);
   glDeleteVertexArrays(1, &gl4.vbo.modvol_vao);
   glDeleteBuffers(1, &gl4.vbo.geometry);
   glDeleteBuffers(1, &gl4.vbo.modvols);
   glDeleteBuffers(1, &gl4.vbo.idxs);
   glDeleteBuffers(1, &gl4.vbo.idxs2);
   glDeleteBuffers(1, &gl4.vbo.tr_poly_params);
   for (auto it = gl4.shaders.begin(); it != gl4.shaders.end(); it++)
   {
   	if (it->second.program != 0)
   		glDeleteProgram(it->second.program);
   }
   gl4.shaders.clear();
   glDeleteTextures(1, &fbTextureId);
   fbTextureId = 0;
   glDeleteTextures(1, &fogTextureId);
   fogTextureId = 0;
}

static bool gl_create_resources(void)
{
	// Create VAOs
	glGenVertexArrays(1, &gl4.vbo.main_vao);
	glGenVertexArrays(1, &gl4.vbo.modvol_vao);

	// Create VBOs
	glGenBuffers(1, &gl4.vbo.geometry);
	glGenBuffers(1, &gl4.vbo.modvols);
	glGenBuffers(1, &gl4.vbo.idxs);
	glGenBuffers(1, &gl4.vbo.idxs2);

	gl4SetupMainVBO();
	gl4SetupModvolVBO();

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

extern void initABuffer();
void reshapeABuffer(int width, int height);
extern void gl4CreateTextures(int width, int height);

static void resize(int w, int h)
{
	if (w > max_image_width || h > max_image_height || stencilTexId == 0)
	{
		if (w > max_image_width)
			max_image_width = w;
		if (h > max_image_height)
			max_image_height = h;

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
		gl4CreateTextures(max_image_width, max_image_height);
		reshapeABuffer(max_image_width, max_image_height);
	}
}

static bool RenderFrame(void)
{
   int vmu_screen_number = 0 ;
   int lightgun_port = 0 ;

   DoCleanup();

	bool is_rtt=pvrrc.isRTT;

	//if (FrameCount&7) return;

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
		if (SCALER_CTL.interlace == 0 && SCALER_CTL.vscalefactor > 0x400)
			scale_y *= roundf((float)SCALER_CTL.vscalefactor / 0x400);

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
		Handle Dc to screen scaling
	*/
	float dc2s_scale_h = is_rtt ? (screen_width / dc_width) : (screen_height/480.0);
	float ds2s_offs_x  = is_rtt ? 0 : ((screen_width-dc2s_scale_h*640)/2);

	//-1 -> too much to left
	gl4ShaderUniforms.scale_coefs[0] = 2.0f / (screen_width / dc2s_scale_h * scale_x);
	gl4ShaderUniforms.scale_coefs[1] = (is_rtt ? 2 : -2) / dc_height;
	gl4ShaderUniforms.scale_coefs[2] = 1 - 2 * ds2s_offs_x / screen_width;
	gl4ShaderUniforms.scale_coefs[3] = is_rtt ? 1 : -1;

	int rendering_width;
	int rendering_height;
	if (is_rtt)
	{
		int scaling = settings.rend.RenderToTextureBuffer ? 1 : settings.rend.RenderToTextureUpscale;
		resize(dc_width * scaling, dc_height * scaling);
		rendering_width = dc_width * scaling;
		rendering_height = dc_height * scaling;
	}
	else
	{
		rendering_width = screen_width;
		rendering_height = screen_height;
	}
	resize(rendering_width, rendering_height);
	
	gl4ShaderUniforms.extra_depth_scale = settings.rend.ExtraDepthScale;

	DEBUG_LOG(RENDERER, "scale: %f, %f, %f, %f", ShaderUniforms.scale_coefs[0], ShaderUniforms.scale_coefs[1], ShaderUniforms.scale_coefs[2], ShaderUniforms.scale_coefs[3]);


	//VERT and RAM fog color constants
	u8* fog_colvert_bgra=(u8*)&FOG_COL_VERT;
	u8* fog_colram_bgra=(u8*)&FOG_COL_RAM;
	gl4ShaderUniforms.ps_FOG_COL_VERT[0]=fog_colvert_bgra[2]/255.0f;
	gl4ShaderUniforms.ps_FOG_COL_VERT[1]=fog_colvert_bgra[1]/255.0f;
	gl4ShaderUniforms.ps_FOG_COL_VERT[2]=fog_colvert_bgra[0]/255.0f;

	gl4ShaderUniforms.ps_FOG_COL_RAM[0]=fog_colram_bgra [2]/255.0f;
	gl4ShaderUniforms.ps_FOG_COL_RAM[1]=fog_colram_bgra [1]/255.0f;
	gl4ShaderUniforms.ps_FOG_COL_RAM[2]=fog_colram_bgra [0]/255.0f;


	//Fog density constant
	u8* fog_density=(u8*)&FOG_DENSITY;
	float fog_den_mant=fog_density[1]/128.0f;  //bit 7 -> x. bit, so [6:0] -> fraction -> /128
	s32 fog_den_exp=(s8)fog_density[0];
   gl4ShaderUniforms.fog_den_float = fog_den_mant * powf(2.0f,fog_den_exp) * settings.rend.ExtraDepthScale;

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

	glcache.UseProgram(gl4.modvol_shader.program);

	glUniform4fv(gl4.modvol_shader.scale, 1, gl4ShaderUniforms.scale_coefs);
	glUniform1f(gl4.modvol_shader.extra_depth_scale, gl4ShaderUniforms.extra_depth_scale);

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
			WARN_LOG(RENDERER, "Unsupported render to texture format: %d", FB_W_CTRL.fb_packmode);
         return false;
		case 7: //7     invalid
			die("7 is not valid");
			break;
		}
		DEBUG_LOG(RENDERER, "RTT packmode=%d stride=%d - %d,%d -> %d,%d", FB_W_CTRL.fb_packmode, FB_W_LINESTRIDE.stride * 8,
 				FB_X_CLIP.min, FB_Y_CLIP.min, FB_X_CLIP.max, FB_Y_CLIP.max);
      output_fbo = gl4BindRTT(FB_W_SOF1 & VRAM_MASK, dc_width, dc_height, channels, format);
	}
   else
   {
      glViewport(0, 0, screen_width, screen_height);
      output_fbo = hw_render.get_current_framebuffer();
   }

   glcache.Disable(GL_SCISSOR_TEST);

	//move vertex to gpu
   if (!pvrrc.isRenderFramebuffer)
   {
      //Main VBO
	   glBindVertexArray(gl4.vbo.main_vao);
	   glBindBuffer(GL_ARRAY_BUFFER, gl4.vbo.geometry); glCheck();
	   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl4.vbo.idxs); glCheck();

      glBufferData(GL_ARRAY_BUFFER,pvrrc.verts.bytes(),pvrrc.verts.head(),GL_STREAM_DRAW); glCheck();

      glBufferData(GL_ELEMENT_ARRAY_BUFFER,pvrrc.idx.bytes(),pvrrc.idx.head(),GL_STREAM_DRAW); glCheck();

      //Modvol VBO
      if (pvrrc.modtrig.used())
	   {
         glBindVertexArray(gl4.vbo.modvol_vao);
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
      DEBUG_LOG(RENDERER, "SS: %dx%d", screen_width, screen_height);
      DEBUG_LOG(RENDERER, "SCI: %d, %f", pvrrc.fb_X_CLIP.max, dc2s_scale_h);
      DEBUG_LOG(RENDERER, "SCI: %f, %f, %f, %f", offs_x+pvrrc.fb_X_CLIP.min/scale_x,(pvrrc.fb_Y_CLIP.min/scale_y)*dc2s_scale_h,(pvrrc.fb_X_CLIP.max-pvrrc.fb_X_CLIP.min+1)/scale_x*dc2s_scale_h,(pvrrc.fb_Y_CLIP.max-pvrrc.fb_Y_CLIP.min+1)/scale_y*dc2s_scale_h);
#endif

      bool wide_screen_on = !is_rtt && settings.rend.WideScreen
   			&& pvrrc.fb_X_CLIP.min == 0
   			&& lroundf((pvrrc.fb_X_CLIP.max + 1) / scale_x) == 640L
   			&& pvrrc.fb_Y_CLIP.min == 0
   			&& lroundf((pvrrc.fb_Y_CLIP.max + 1) / scale_y) == 480L;

      if (!wide_screen_on)
      {
         float width  = (pvrrc.fb_X_CLIP.max - pvrrc.fb_X_CLIP.min + 1) / scale_x;
         float height = (pvrrc.fb_Y_CLIP.max - pvrrc.fb_Y_CLIP.min + 1) / scale_y;
         float min_x  = pvrrc.fb_X_CLIP.min / scale_x;
         float min_y  = pvrrc.fb_Y_CLIP.min / scale_y;
         if (!is_rtt)
         {
				if (SCALER_CTL.interlace && SCALER_CTL.vscalefactor > 0x400)
				{
					// Clipping is done after scaling/filtering so account for that if enabled
					height *= (float)SCALER_CTL.vscalefactor / 0x400;
					min_y *= (float)SCALER_CTL.vscalefactor / 0x400;
				}
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

         glScissor((GLint)lroundf(min_x), (GLint)lroundf(min_y), (GLsizei)lroundf(width), (GLsizei)lroundf(height));
         glcache.Enable(GL_SCISSOR_TEST);
      }

      //restore scale_x
      scale_x /= scissoring_scale_x;
		gl4DrawStrips(output_fbo, rendering_width, rendering_height);
   }
   else
   {
   	glBindFramebuffer(GL_FRAMEBUFFER, output_fbo);
   	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      glcache.ClearColor(0.f, 0.f, 0.f, 0.f);
      glClear(GL_COLOR_BUFFER_BIT);
      gl4DrawFramebuffer(dc_width, dc_height);
   }


   if (!is_rtt)
   {
		for ( vmu_screen_number = 0 ; vmu_screen_number < 4 ; vmu_screen_number++)
			if ( vmu_screen_params[vmu_screen_number].vmu_screen_display )
				gl4DrawVmuTexture(vmu_screen_number);

		for ( lightgun_port = 0 ; lightgun_port < 4 ; lightgun_port++)
				gl4DrawGunCrosshair(lightgun_port);
   }

	KillTex = false;
   
   if (is_rtt)
      ReadRTTBuffer();

   glBindVertexArray(0);

	return !is_rtt;
}

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
   bool Init() override
   {
	  int major = 0;
	  int minor = 0;
	  glGetIntegerv(GL_MAJOR_VERSION, &major);
	  glGetIntegerv(GL_MINOR_VERSION, &minor);
	  if (major < 4 || (major == 4 && minor < 3))
	  {
		  WARN_LOG(RENDERER, "Warning: OpenGL %d.%d doesn't support per-pixel sorting. 4.3 required", major, minor);
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
      TexCache.Clear();

      return true;
   }
   void Resize(int w, int h) override
	{
		screen_width=w;
		screen_height=h;
		resize(w, h);
	}
	void Term() override
	{
		glcache.DeleteTextures(ARRAY_SIZE(vmuTextureId), vmuTextureId);
		memset(vmuTextureId, 0, sizeof(vmuTextureId));
		glcache.DeleteTextures(ARRAY_SIZE(lightgunTextureId), lightgunTextureId);
		memset(lightgunTextureId, 0, sizeof(lightgunTextureId));

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
	   if (geom_fbo != 0)
	   {
	   	glDeleteFramebuffers(1, &geom_fbo);
	   	geom_fbo = 0;
	   }
		if (texSamplers[0] != 0)
		{
			glDeleteSamplers(2, texSamplers);
			texSamplers[0] = texSamplers[1] = 0;
		}
	   if (depth_fbo != 0)
	   {
	   	glDeleteFramebuffers(1, &depth_fbo);
	   	depth_fbo = 0;
	   }
		TexCache.Clear();

	   gl_term();
   }

	bool Process(TA_context* ctx) override
   {
#if !defined(TARGET_NO_THREADS)
      if (!settings.rend.ThreadedRendering)
#endif
         glsm_ctl(GLSM_CTL_STATE_BIND, NULL);
      return ProcessFrame(ctx);
   }
	bool Render() override
   {
      bool ret = RenderFrame();
#if !defined(TARGET_NO_THREADS)
      if (!settings.rend.ThreadedRendering)
#endif
    	  glsm_ctl(GLSM_CTL_STATE_UNBIND, NULL);
      return ret;
   }

	void Present() override
   {
      co_dc_yield();
   }

	virtual u64 GetTexture(TSP tsp, TCW tcw) override
	{
		return gl_GetTexture(tsp, tcw);
	}
};

Renderer* rend_GL4() { return new gl4rend(); }
