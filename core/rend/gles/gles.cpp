#include <math.h>

#include <memalign.h>

#ifdef __SSE4_1__
#include <xmmintrin.h>
#endif

#include <libretro.h>

#include "gles.h"
#include "rend/rend.h"
#include "../TexCache.h"

#include "hw/pvr/Renderer_if.h"
#include "../../hw/mem/_vmem.h"

#ifndef GL_RED
#define GL_RED                            0x1903
#endif
#ifndef GL_MAJOR_VERSION
#define GL_MAJOR_VERSION                  0x821B
#endif

GLCache glcache;
gl_ctx gl;

struct ShaderUniforms_t ShaderUniforms;

u32 gcflip;

float fb_scale_x = 0.0f;
float fb_scale_y = 0.0f;
float scale_x, scale_y;

//Fragment and vertex shaders code
//pretty much 1:1 copy of the d3d ones for now
static const char* VertexShaderSource =
"\
%s \n\
#define TARGET_GL %s \n\
#define pp_Gouraud %d \n\
 \n\
#define GLES2 0 \n\
#define GLES3 1 \n\
#define GL2 2 \n\
#define GL3 3 \n\
 \n\
#if TARGET_GL == GL2 \n\
#define highp \n\
#define lowp \n\
#define mediump \n\
#endif \n\
#if TARGET_GL == GLES2 || TARGET_GL == GL2 \n\
#define in attribute \n\
#define out varying \n\
#endif \n\
 \n\
 \n\
#if TARGET_GL == GL3 || TARGET_GL == GLES3 \n\
#if pp_Gouraud == 0 \n\
#define INTERPOLATION flat \n\
#else \n\
#define INTERPOLATION smooth \n\
#endif \n\
#else \n\
#define INTERPOLATION \n\
#endif \n\
\n\
/* Vertex constants*/  \n\
uniform highp vec4      scale; \n\
uniform highp vec4      depth_scale; \n\
uniform highp float     extra_depth_scale; \n\
uniform highp float sp_FOG_DENSITY; \n\
 \n\
/* Vertex input */ \n\
in highp vec4    in_pos; \n\
in lowp  vec4     in_base; \n\
in lowp vec4     in_offs; \n\
in mediump vec2  in_uv; \n\
/* output */ \n\
INTERPOLATION out lowp vec4 vtx_base; \n\
INTERPOLATION out lowp vec4 vtx_offs; \n\
              out mediump vec2 vtx_uv; \n\
#if TARGET_GL == GLES2 \n\
              out highp float fog_depth; \n\
#endif  \n\
\n\
void main() \n\
{ \n\
	vtx_base=in_base; \n\
	vtx_offs=in_offs; \n\
	vtx_uv=in_uv; \n\
	highp vec4 vpos = in_pos; \n\
	if (vpos.z < 0.0 || vpos.z > 3.4e37) \n\
	{ \n\
	   gl_Position = vec4(0.0, 0.0, 1.0, 1.0 / vpos.z); \n\
	   return; \n\
	} \n\
	\n\
	vpos.w = extra_depth_scale / vpos.z; \n\
#if TARGET_GL != GLES2 \n\
   vpos.z = vpos.w; \n\
#else \n\
   fog_depth = vpos.z * sp_FOG_DENSITY; \n\
   vpos.z=depth_scale.x+depth_scale.y*vpos.w;  \n\
#endif \n\
	vpos.xy=vpos.xy*scale.xy-scale.zw;  \n\
	vpos.xy*=vpos.w;  \n\
	gl_Position = vpos; \n\
}";

static const char* PixelPipelineShader =
"\
%s \n\
#define TARGET_GL %s \n\
\n\
#define cp_AlphaTest %d \n\
#define pp_ClipTestMode %d \n\
#define pp_UseAlpha %d \n\
#define pp_Texture %d \n\
#define pp_IgnoreTexA %d \n\
#define pp_ShadInstr %d \n\
#define pp_Offset %d \n\
#define pp_FogCtrl %d \n\
#define pp_Gouraud %d \n\
#define pp_BumpMap %d \n\
#define FogClamping %d \n\
#define pp_TriLinear %d \n\
#define PI 3.1415926 \n\
\n\
#define GLES2 0 \n\
#define GLES3 1 \n\
#define GL2 2 \n\
#define GL3 3 \n\
 \n\
#if TARGET_GL == GL2 \n\
#define highp \n\
#define lowp \n\
#define mediump \n\
#endif \n\
#if TARGET_GL == GLES3 \n\
out highp vec4 FragColor; \n\
#define gl_FragColor FragColor \n\
#define FOG_CHANNEL a \n\
#elif TARGET_GL == GL3 \n\
out highp vec4 FragColor; \n\
#define gl_FragColor FragColor \n\
#define FOG_CHANNEL r \n\
#else \n\
#define in varying \n\
#define texture texture2D \n\
#define FOG_CHANNEL a \n\
#endif \n\
 \n\
#if TARGET_GL == GL3 || TARGET_GL == GLES3 \n\
#if pp_Gouraud == 0 \n\
#define INTERPOLATION flat \n\
#else \n\
#define INTERPOLATION smooth \n\
#endif \n\
#else \n\
#define INTERPOLATION \n\
#endif \n\
\n\
/* Shader program params*/ \n\
/* gles has no alpha test stage, so its emulated on the shader */ \n\
uniform lowp float cp_AlphaTestValue; \n\
uniform lowp vec4 pp_ClipTest; \n\
uniform lowp vec3 sp_FOG_COL_RAM,sp_FOG_COL_VERT; \n\
uniform highp float sp_FOG_DENSITY; \n\
uniform sampler2D tex,fog_table; \n\
uniform lowp float trilinear_alpha; \n\
uniform lowp vec4 fog_clamp_min; \n\
uniform lowp vec4 fog_clamp_max; \n\
/* Vertex input*/ \n\
INTERPOLATION in lowp vec4 vtx_base; \n\
INTERPOLATION in lowp vec4 vtx_offs; \n\
in mediump vec2 vtx_uv; \n\
#if TARGET_GL == GLES2 \n\
in highp float fog_depth; \n\
#endif \n\
 \n\
lowp float fog_mode2(highp float w) \n\
{ \n\
#if TARGET_GL == GLES2 \n\
	highp float z = clamp(fog_depth, 1.0, 255.9999); \n\
#else \n\
	highp float z = clamp(w * sp_FOG_DENSITY, 1.0, 255.9999); \n\
#endif \n\
	mediump float exp = floor(log2(z)); \n\
	highp float m = z * 16.0 / pow(2.0, exp) - 16.0; \n\
	mediump float idx = floor(m) + exp * 16.0 + 0.5; \n\
	highp vec4 fog_coef = texture(fog_table, vec2(idx / 128.0, 0.75 - (m - floor(m)) / 2.0)); \n\
	return fog_coef.FOG_CHANNEL; \n\
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
	lowp vec4 color=vtx_base; \n\
	#if pp_UseAlpha==0 \n\
		color.a=1.0; \n\
	#endif\n\
	#if pp_FogCtrl==3 \n\
		color=vec4(sp_FOG_COL_RAM.rgb,fog_mode2(gl_FragCoord.w)); \n\
	#endif\n\
	#if pp_Texture==1 \n\
	{ \n\
		lowp vec4 texcol=texture(tex, vtx_uv); \n\
		 \n\
		#if pp_BumpMap == 1 \n\
			highp float s = PI / 2.0 * (texcol.a * 15.0 * 16.0 + texcol.r * 15.0) / 255.0; \n\
			highp float r = 2.0 * PI * (texcol.g * 15.0 * 16.0 + texcol.b * 15.0) / 255.0; \n\
			texcol.a = clamp(vtx_offs.a + vtx_offs.r * sin(s) + vtx_offs.g * cos(s) * cos(r - 2.0 * PI * vtx_offs.b), 0.0, 1.0); \n\
			texcol.rgb = vec3(1.0, 1.0, 1.0);	 \n\
		#else\n\
			#if pp_IgnoreTexA==1 \n\
				texcol.a=1.0;	 \n\
			#endif\n\
			\n\
			#if cp_AlphaTest == 1 \n\
				if (cp_AlphaTestValue>texcol.a) discard;\n\
			#endif  \n\
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
		#if pp_Offset==1 && pp_BumpMap == 0 \n\
		{ \n\
			color.rgb+=vtx_offs.rgb; \n\
		} \n\
		#endif\n\
	} \n\
	#endif\n\
	 \n\
	color = fog_clamp(color); \n\
	 \n\
	#if pp_FogCtrl == 0 \n\
	{ \n\
		color.rgb=mix(color.rgb,sp_FOG_COL_RAM.rgb,fog_mode2(gl_FragCoord.w));  \n\
	} \n\
	#endif\n\
	#if pp_FogCtrl == 1 && pp_Offset==1 && pp_BumpMap == 0 \n\
	{ \n\
		color.rgb=mix(color.rgb,sp_FOG_COL_VERT.rgb,vtx_offs.a); \n\
	} \n\
	#endif\n\
	 \n\
	#if pp_TriLinear == 1 \n\
	color *= trilinear_alpha; \n\
	#endif \n\
	 \n\
	#if cp_AlphaTest == 1 \n\
		color.a=1.0; \n\
	#endif  \n\
	//color.rgb=vec3(gl_FragCoord.w * sp_FOG_DENSITY / 128.0);\n\
#if TARGET_GL != GLES2 \n\
	highp float w = gl_FragCoord.w * 100000.0; \n\
	gl_FragDepth = log2(1.0 + w) / 34.0; \n\
#endif \n\
	gl_FragColor =color; \n\
}";

static const char* ModifierVolumeShader =
"\
%s \n\
#define TARGET_GL %s \n\
 \n\
#define GLES2 0 \n\
#define GLES3 1 \n\
#define GL2 2 \n\
#define GL3 3 \n\
 \n\
#if TARGET_GL == GL2 \n\
#define highp \n\
#define lowp \n\
#define mediump \n\
#endif \n\
#if TARGET_GL != GLES2 && TARGET_GL != GL2 \n\
out highp vec4 FragColor; \n\
#define gl_FragColor FragColor \n\
#endif \n\
 \n\
uniform lowp float sp_ShaderColor; \n\
/* Vertex input*/ \n\
void main() \n\
{ \n\
#if TARGET_GL != GLES2 \n\
   highp float w = gl_FragCoord.w * 100000.0; \n\
   gl_FragDepth = log2(1.0 + w) / 34.0; \n\
#endif \n\
   gl_FragColor=vec4(0.0, 0.0, 0.0, sp_ShaderColor); \n\
}";


int screen_width  = 640;
int screen_height = 480;
GLuint fogTextureId;

PipelineShader *GetProgram(
      u32 cp_AlphaTest,
      u32 pp_ClipTestMode,
      u32 pp_Texture,
      u32 pp_UseAlpha,
      u32 pp_IgnoreTexA,
      u32 pp_ShadInstr,
      u32 pp_Offset,
      u32 pp_FogCtrl,
      bool pp_Gouraud,
      bool pp_BumpMap,
      bool fog_clamping,
      bool trilinear)
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
   rv<<=1; rv|=pp_Gouraud;
   rv<<=1; rv|=pp_BumpMap;
   rv<<=1; rv|=fog_clamping;
   rv<<=1; rv|=trilinear;

   PipelineShader *shader = &gl.shaders[rv];
   if (shader->program == 0)
   {
   	shader->cp_AlphaTest = cp_AlphaTest;
   	shader->pp_ClipTestMode = pp_ClipTestMode - 1;
   	shader->pp_Texture = pp_Texture;
   	shader->pp_UseAlpha = pp_UseAlpha;
   	shader->pp_IgnoreTexA = pp_IgnoreTexA;
   	shader->pp_ShadInstr = pp_ShadInstr;
   	shader->pp_Offset = pp_Offset;
   	shader->pp_FogCtrl = pp_FogCtrl;
   	shader->pp_Gouraud = pp_Gouraud;
   	shader->pp_BumpMap = pp_BumpMap;
   	shader->fog_clamping = fog_clamping;
   	shader->trilinear = trilinear;
   	CompilePipelineShader(shader);
   }
#ifdef RPI4_SET_UNIFORM_ATTRIBUTES_BUG
    // rpi 4 has a bug where it does not save uniform and attribute state with
    // the program, so they have to be reinit each time you reuse the program
    glcache.UseProgram(shader->program);
    //setup texture 0 as the input for the shader
    GLuint gu=glGetUniformLocation(shader->program, "tex");
    if (shader->pp_Texture==1)
        glUniform1i(gu,0);
    // Setup texture 1 as the fog table
    gu = glGetUniformLocation(shader->program, "fog_table");
    if (gu != -1)
        glUniform1i(gu, 1);
    ShaderUniforms.Set(shader);
#endif

   return shader;
}

void findGLVersion()
{
   gl.stencil_present = true;
   gl.index_type = GL_UNSIGNED_INT;

   while (true)
      if (glGetError() == GL_NO_ERROR)
         break;
   glGetIntegerv(GL_MAJOR_VERSION, &gl.gl_major);
   if (glGetError() == GL_INVALID_ENUM)
      gl.gl_major = 2;
   const char *version = (const char *)glGetString(GL_VERSION);
   NOTICE_LOG(RENDERER, "OpenGL version: %s", version);
   if (!strncmp(version, "OpenGL ES", 9))
   {
      gl.is_gles = true;
      if (gl.gl_major >= 3)
      {
         gl.gl_version = "GLES3";
         gl.glsl_version_header = "#version 300 es";
      }
      else
      {
         gl.gl_version = "GLES2";
         gl.glsl_version_header = "";
         gl.index_type = GL_UNSIGNED_SHORT;
      }
      gl.fog_image_format = GL_ALPHA;

      GLint stencilBits = 0;
      glGetIntegerv(GL_STENCIL_BITS, &stencilBits);
      if (stencilBits == 0)
    	 gl.stencil_present = false;
   }
   else
   {
      gl.is_gles = false;
      if (gl.gl_major >= 3)
      {
         gl.gl_version = "GL3";
         gl.glsl_version_header = "#version 130";
         gl.fog_image_format = GL_RED;
      }
      else
      {
         gl.gl_version = "GL2";
         gl.glsl_version_header = "#version 120";
         gl.fog_image_format = GL_ALPHA;
      }
   }
}

GLuint gl_CompileShader(const char* shader,GLuint type)
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
		WARN_LOG(RENDERER, "Shader: %s \n%s\n", result ? "compiled!" : "failed to compile", compile_log);

		free(compile_log);
	}

	return rv;
}

GLuint gl_CompileAndLink(const char* VertexShader, const char* FragmentShader)
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
	glBindAttribLocation(program, VERTEX_COL_BASE1_ARRAY, "in_base1");
	glBindAttribLocation(program, VERTEX_COL_OFFS1_ARRAY, "in_offs1");
	glBindAttribLocation(program, VERTEX_UV1_ARRAY,       "in_uv1");

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
		WARN_LOG(RENDERER, "Shader linking: %s \n (%d bytes), - %s -\n", result ? "linked" : "failed to link", compile_log_len, compile_log);
		WARN_LOG(RENDERER, "VERTEX:\n%s\nFRAGMENT:\n%s\n", VertexShader, FragmentShader);

		free(compile_log);
		die("shader compile fail\n");
	}

	glDeleteShader(vs);
	glDeleteShader(ps);

	glcache.UseProgram(program);

	verify(glIsProgram(program));

	return program;
}


bool CompilePipelineShader(PipelineShader *s)
{
   char vshader[8192];

   sprintf(vshader, VertexShaderSource, gl.glsl_version_header, gl.gl_version, s->pp_Gouraud);

	char pshader[8192];

   sprintf(pshader,PixelPipelineShader, gl.glsl_version_header, gl.gl_version,
                s->cp_AlphaTest,s->pp_ClipTestMode,s->pp_UseAlpha,
                s->pp_Texture,s->pp_IgnoreTexA,s->pp_ShadInstr,s->pp_Offset,s->pp_FogCtrl, s->pp_Gouraud, s->pp_BumpMap, s->fog_clamping, s->trilinear);

	s->program            = gl_CompileAndLink(vshader, pshader);


	//setup texture 0 as the input for the shader
	GLuint gu=glGetUniformLocation(s->program, "tex");
	if (s->pp_Texture==1)
		glUniform1i(gu,0);

	//get the uniform locations
	s->scale	             = glGetUniformLocation(s->program, "scale");
	s->depth_scale      = glGetUniformLocation(s->program, "depth_scale");

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
   // Setup texture 1 as the fog table
   gu = glGetUniformLocation(s->program, "fog_table");
   if (gu != -1)
      glUniform1i(gu, 1);
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

   ShaderUniforms.Set(s);
	return glIsProgram(s->program)==GL_TRUE;
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

static void gl_delete_shaders()
{
	for (const auto& it : gl.shaders)
	{
		if (it.second.program != 0)
			glDeleteProgram(it.second.program);
	}
	gl.shaders.clear();
	glDeleteProgram(gl.modvol_shader.program);
	gl.modvol_shader.program = 0;
}

static void gl_term(void)
{
	glDeleteBuffers(1, &gl.vbo.geometry);
	glDeleteBuffers(1, &gl.vbo.modvols);
	glDeleteBuffers(1, &gl.vbo.idxs);
	glDeleteBuffers(1, &gl.vbo.idxs2);
	glDeleteTextures(1, &fbTextureId);
	fbTextureId = 0;
	glDeleteTextures(1, &fogTextureId);
	fogTextureId = 0;

	gl_delete_shaders();
}

static bool gl_create_resources(void)
{
	/* create VBOs */
	glGenBuffers(1, &gl.vbo.geometry);
	glGenBuffers(1, &gl.vbo.modvols);
	glGenBuffers(1, &gl.vbo.idxs);
	glGenBuffers(1, &gl.vbo.idxs2);

   findGLVersion();

   char vshader[8192];
   sprintf(vshader, VertexShaderSource, gl.glsl_version_header, gl.gl_version, 1);
   char fshader[8192];
	sprintf(fshader, ModifierVolumeShader, gl.glsl_version_header, gl.gl_version);

   gl.modvol_shader.program=gl_CompileAndLink(vshader, fshader);
	gl.modvol_shader.scale          = glGetUniformLocation(gl.modvol_shader.program, "scale");
	gl.modvol_shader.depth_scale          = glGetUniformLocation(gl.modvol_shader.program, "depth_scale");
   gl.modvol_shader.extra_depth_scale = glGetUniformLocation(gl.modvol_shader.program, "extra_depth_scale");
	gl.modvol_shader.sp_ShaderColor = glGetUniformLocation(gl.modvol_shader.program, "sp_ShaderColor");

	return true;
}

void UpdateFogTexture(u8 *fog_table, GLenum texture_slot, GLint fog_image_format)
{
	glActiveTexture(texture_slot);
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
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, fog_image_format, 128, 2, 0, fog_image_format, GL_UNSIGNED_BYTE, temp_tex_buffer);

	glActiveTexture(GL_TEXTURE0);
}

void DrawVmuTexture(u8 vmu_screen_number, bool draw_additional_primitives);
void DrawGunCrosshair(u8 port, bool draw_additional_primitives);

void vertex_buffer_unmap(void)
{
   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

#ifdef MSB_FIRST
#define INDEX_GET(a) (a^3)
#else
#define INDEX_GET(a) (a)
#endif

void DoCleanup() {
}

static void upload_vertex_indices()
{
	if (gl.index_type == GL_UNSIGNED_SHORT)
	{
		static bool overrun;
		static List<u16> short_idx;
		if (short_idx.daty != NULL)
			short_idx.Free();
		short_idx.Init(pvrrc.idx.used(), &overrun, NULL);
		for (u32 *p = pvrrc.idx.head(); p < pvrrc.idx.LastPtr(0); p++)
			*(short_idx.Append()) = *p;
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, short_idx.bytes(), short_idx.head(), GL_STREAM_DRAW);
	}
	else
		glBufferData(GL_ELEMENT_ARRAY_BUFFER,pvrrc.idx.bytes(),pvrrc.idx.head(),GL_STREAM_DRAW);
}

static bool RenderFrame(void)
{
   int vmu_screen_number = 0 ;
   int lightgun_port = 0 ;

   DoCleanup();

	bool is_rtt=pvrrc.isRTT;

	//if (FrameCount&7) return;

	float vtx_min_fZ=0.f;   //pvrrc.fZ_min;
	float vtx_max_fZ=pvrrc.fZ_max;

	//sanitise the values, now with NaN detection (for omap)
	//0x49800000 is 1024*1024. Using integer math to avoid issues w/ infs and nans
	if ((s32&)vtx_max_fZ<0 || (u32&)vtx_max_fZ>0x49800000)
		vtx_max_fZ=10*1024;


	//add some extra range to avoid clipping border cases
	vtx_min_fZ*=0.98f;
	vtx_max_fZ*=1.001f;

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
	ShaderUniforms.scale_coefs[0]=2.0f/(screen_width/dc2s_scale_h*scale_x);
	ShaderUniforms.scale_coefs[1]= (is_rtt?2:-2) / dc_height;
   // FIXME CT2 needs 480 here instead of dc_height=512
	ShaderUniforms.scale_coefs[2]=1-2*ds2s_offs_x/(screen_width);
	ShaderUniforms.scale_coefs[3]=(is_rtt?1:-1);

	ShaderUniforms.depth_coefs[0]=2/(vtx_max_fZ-vtx_min_fZ);
	ShaderUniforms.depth_coefs[1]=-vtx_min_fZ-1;
	ShaderUniforms.depth_coefs[2]=0;
	ShaderUniforms.depth_coefs[3]=0;

   ShaderUniforms.extra_depth_scale = settings.rend.ExtraDepthScale;

   DEBUG_LOG(RENDERER, "scale: %f, %f, %f, %f", ShaderUniforms.scale_coefs[0], ShaderUniforms.scale_coefs[1], ShaderUniforms.scale_coefs[2], ShaderUniforms.scale_coefs[3]);


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

   ShaderUniforms.fog_clamp_min[0] = ((pvrrc.fog_clamp_min >> 16) & 0xFF) / 255.0f;
	ShaderUniforms.fog_clamp_min[1] = ((pvrrc.fog_clamp_min >> 8) & 0xFF) / 255.0f;
	ShaderUniforms.fog_clamp_min[2] = ((pvrrc.fog_clamp_min >> 0) & 0xFF) / 255.0f;
	ShaderUniforms.fog_clamp_min[3] = ((pvrrc.fog_clamp_min >> 24) & 0xFF) / 255.0f;
	
	ShaderUniforms.fog_clamp_max[0] = ((pvrrc.fog_clamp_max >> 16) & 0xFF) / 255.0f;
	ShaderUniforms.fog_clamp_max[1] = ((pvrrc.fog_clamp_max >> 8) & 0xFF) / 255.0f;
	ShaderUniforms.fog_clamp_max[2] = ((pvrrc.fog_clamp_max >> 0) & 0xFF) / 255.0f;
	ShaderUniforms.fog_clamp_max[3] = ((pvrrc.fog_clamp_max >> 24) & 0xFF) / 255.0f;


	if (fog_needs_update)
	{
		fog_needs_update=false;
      UpdateFogTexture((u8 *)FOG_TABLE, GL_TEXTURE1, gl.fog_image_format);
	}

	glcache.UseProgram(gl.modvol_shader.program);

	glUniform4fv(gl.modvol_shader.scale, 1, ShaderUniforms.scale_coefs);
	glUniform4fv(gl.modvol_shader.depth_scale, 1, ShaderUniforms.depth_coefs);

   glUniform1f(gl.modvol_shader.extra_depth_scale, ShaderUniforms.extra_depth_scale);

	ShaderUniforms.PT_ALPHA=(PT_ALPHA_REF&0xFF)/255.0f;

	for (auto& it : gl.shaders)
	{
		glcache.UseProgram(it.second.program);
		ShaderUniforms.Set(&it.second);
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
			WARN_LOG(RENDERER, "Unsupported render to texture format: %d", FB_W_CTRL.fb_packmode);
         return false;
		case 7: //7     invalid
			die("7 is not valid");
			break;
		}
		DEBUG_LOG(RENDERER, "RTT packmode=%d stride=%d - %d,%d -> %d,%d\n", FB_W_CTRL.fb_packmode, FB_W_LINESTRIDE.stride * 8,
 				FB_X_CLIP.min, FB_Y_CLIP.min, FB_X_CLIP.max, FB_Y_CLIP.max);
		BindRTT(FB_W_SOF1 & VRAM_MASK, dc_width, dc_height, channels,format);
	}
   else
   {
      glViewport(0, 0, screen_width, screen_height);
   }

   bool wide_screen_on = !is_rtt && settings.rend.WideScreen
			&& pvrrc.fb_X_CLIP.min == 0
			&& (pvrrc.fb_X_CLIP.max + 1) / scale_x == 640
			&& pvrrc.fb_Y_CLIP.min == 0
			&& (pvrrc.fb_Y_CLIP.max + 1) / scale_y == 480;

   // Color is cleared by the background plane

   glcache.Disable(GL_SCISSOR_TEST);
   glClearDepth(0.f);
   glcache.DepthMask(GL_TRUE);
   glStencilMask(0xFF);
   glClear(GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//move vertex to gpu

   if (!pvrrc.isRenderFramebuffer)
   {
      //Main VBO
      glBindBuffer(GL_ARRAY_BUFFER, gl.vbo.geometry); glCheck();
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl.vbo.idxs); glCheck();

      glBufferData(GL_ARRAY_BUFFER,pvrrc.verts.bytes(),pvrrc.verts.head(),GL_STREAM_DRAW); glCheck();

      upload_vertex_indices();

      //Modvol VBO
      if (pvrrc.modtrig.used())
      {
         glBindBuffer(GL_ARRAY_BUFFER, gl.vbo.modvols); glCheck();
         glBufferData(GL_ARRAY_BUFFER,pvrrc.modtrig.bytes(),pvrrc.modtrig.head(),GL_STREAM_DRAW); glCheck();
      }

      //not all scaling affects pixel operations, scale to adjust for that
      scale_x *= scissoring_scale_x;

#if 0
      //handy to debug really stupid render-not-working issues ...
      DEBUG_LOG(RENDERER, "SS: %dx%d", screen_width, screen_height);
      DEBUG_LOG(RENDERER, "SCI: %d, %f", pvrrc.fb_X_CLIP.max, dc2s_scale_h);
      DEBUG_LOG(RENDERER, "SCI: %f, %f, %f, %f", offs_x+pvrrc.fb_X_CLIP.min/scale_x,(pvrrc.fb_Y_CLIP.min/scale_y)*dc2s_scale_h,(pvrrc.fb_X_CLIP.max-pvrrc.fb_X_CLIP.min+1)/scale_x*dc2s_scale_h,(pvrrc.fb_Y_CLIP.max-pvrrc.fb_Y_CLIP.min+1)/scale_y*dc2s_scale_h);
#endif

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
            min_x   = min_x * dc2s_scale_h + ds2s_offs_x;
            // Invert y coordinates when rendering to screen
            min_y   = screen_height - (min_y + height) * dc2s_scale_h;
            width  *= dc2s_scale_h;
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

      DrawStrips();
   }
   else
   {
      glcache.ClearColor(0.f, 0.f, 0.f, 0.f);
      glClear(GL_COLOR_BUFFER_BIT);
      glBindBuffer(GL_ARRAY_BUFFER, gl.vbo.geometry); glCheck();
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl.vbo.idxs); glCheck();
      DrawFramebuffer(dc_width, dc_height);
   }

   for ( vmu_screen_number = 0 ; vmu_screen_number < 4 ; vmu_screen_number++)
      if ( vmu_screen_params[vmu_screen_number].vmu_screen_display )
         DrawVmuTexture(vmu_screen_number, true) ;
         
   for ( lightgun_port = 0 ; lightgun_port < 4 ; lightgun_port++)
         DrawGunCrosshair(lightgun_port, true) ;

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
   ctx->rend_inuse.Lock();

   if (KillTex)
   {
      void killtex();
      killtex();
      INFO_LOG(RENDERER, "Texture cache cleared");
   }

   if (ctx->rend.isRenderFramebuffer)
	{
		RenderFramebuffer();
		ctx->rend_inuse.Unlock();
	}
	else
	{
		if (!ta_parse_vdrc(ctx))
			return false;
	}
   CollectCleanup();

   return !ctx->rend.Overrun;
}

struct glesrend : Renderer
{
   bool Init()
   {
      if (!gl_create_resources())
         return false;

      glcache.EnableCache();

#ifdef HAVE_OPENGLES
      glHint(GL_GENERATE_MIPMAP_HINT, GL_FASTEST);
#endif

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
      killtex();

      return true;
   }
	void Resize(int w, int h) { screen_width=w; screen_height=h; }
	void Term()
   {
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

Renderer* rend_GLES2() { return new glesrend(); }
