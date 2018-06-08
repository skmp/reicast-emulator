#include "gles.h"
#include "glcache.h"

static int g_imageWidth = 0;
static int g_imageHeight = 0;
static GLuint g_quadBuffer = 0;
static GLuint g_quadVertexArray = 0;

GLenum g_drawBuffers[] = {GL_COLOR_ATTACHMENT0,
						   GL_COLOR_ATTACHMENT1,
						   GL_COLOR_ATTACHMENT2,
						   GL_COLOR_ATTACHMENT3,
						   GL_COLOR_ATTACHMENT4,
						   GL_COLOR_ATTACHMENT5,
						   GL_COLOR_ATTACHMENT6
};

//
// Weighted Average
//
static GLuint g_accumulationTexId[2];
static GLuint g_accumulationFboId;
PipelineShader g_wavg_final_shader;

void InitAccumulationRenderTargets()
{
	glGenTextures(2, g_accumulationTexId);

	glcache.BindTexture(GL_TEXTURE_RECTANGLE, g_accumulationTexId[0]);
	glcache.TexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glcache.TexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glcache.TexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glcache.TexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGBA16F,
				 g_imageWidth, g_imageHeight, 0, GL_RGBA, GL_FLOAT, NULL);

	glcache.BindTexture(GL_TEXTURE_RECTANGLE, g_accumulationTexId[1]);
	glcache.TexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glcache.TexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glcache.TexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glcache.TexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RG32F,
				 g_imageWidth, g_imageHeight, 0, GL_RGBA, GL_FLOAT, NULL);

	glGenFramebuffers(1, &g_accumulationFboId);
	glBindFramebuffer(GL_FRAMEBUFFER, g_accumulationFboId);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
							  GL_TEXTURE_RECTANGLE, g_accumulationTexId[0], 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
							  GL_TEXTURE_RECTANGLE, g_accumulationTexId[1], 0);
#if 0
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, stencilTexId, 0);
#endif
}

void DeleteAccumulationRenderTargets()
{
	glDeleteFramebuffers(1, &g_accumulationFboId);
	glDeleteTextures(2, g_accumulationTexId);
}

const char *wavg_final_fragment = "\
#version 140 \n\
out vec4 FragColor; \n\
uniform sampler2DRect ColorTex0; \n\
uniform sampler2DRect ColorTex1; \n\
 \n\
void main(void) \n\
{ \n\
	highp vec4 SumColor = texture2DRect(ColorTex0, gl_FragCoord.xy); \n\
	highp float n = texture2DRect(ColorTex1, gl_FragCoord.xy).r; \n\
 \n\
	// Average Color \n\
	highp vec3 AvgColor = SumColor.rgb / SumColor.a; \n\
	if (n == 0.0 || isinf(AvgColor.r) || isinf(AvgColor.g) || isinf(AvgColor.b) || isnan(AvgColor.r) || isnan(AvgColor.g) || isnan(AvgColor.b)) { \n\
		FragColor.rgba = vec4(0, 0, 0, 0); \n\
		return; \n\
	} \n\
 \n\
	highp float AvgAlpha = SumColor.a / n; \n\
 \n\
	highp float T = pow(1.0 - AvgAlpha, n); \n\
	if (isnan(T)) T = 0; \n\
	FragColor.rgb = AvgColor; \n\
	FragColor.a = 1 - T; \n\
	// Weighted Blended \n\
//	vec3 AvgColor = SumColor.rgb / max(SumColor.a, 0.00001); \n\
//	FragColor.rgb = AvgColor; \n\
//	FragColor.a = 1 - n; \n\
	//FragColor.rgb = vec3(AvgAlpha, 0, 0); FragColor.a = 1.0; \n\
	//FragColor.rgb = AvgColor; FragColor.a = 1.0; \n\
} \n\
";

void DrawQuad()
{
	glBindVertexArray(g_quadVertexArray);

	struct Vertex vertices[] = {
			{ 0, screen_height, 1, { 255, 255, 255, 255 }, { 0, 0, 0, 0 }, 0, 1 },
			{ 0, 0, 1, { 255, 255, 255, 255 }, { 0, 0, 0, 0 }, 0, 0 },
			{ screen_width, screen_height, 1, { 255, 255, 255, 255 }, { 0, 0, 0, 0 }, 1, 1 },
			{ screen_width, 0, 1, { 255, 255, 255, 255 }, { 0, 0, 0, 0 }, 1, 0 },
	};
	GLushort indices[] = { 0, 1, 2, 1, 3 };

	glBindBuffer(GL_ARRAY_BUFFER, g_quadBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STREAM_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glEnableVertexAttribArray(VERTEX_POS_ARRAY);
	glVertexAttribPointer(VERTEX_POS_ARRAY, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex,x));

	glEnableVertexAttribArray(VERTEX_COL_BASE_ARRAY);
	glVertexAttribPointer(VERTEX_COL_BASE_ARRAY, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex,col));

	glEnableVertexAttribArray(VERTEX_COL_OFFS_ARRAY);
	glVertexAttribPointer(VERTEX_COL_OFFS_ARRAY, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex,vtx_spc));

	glEnableVertexAttribArray(VERTEX_UV_ARRAY);
	glVertexAttribPointer(VERTEX_UV_ARRAY, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex,u));

	glDrawElements(GL_TRIANGLE_STRIP, 5, GL_UNSIGNED_SHORT, indices);
}

//--------------------------------------------------------------------------
void RenderAverageColors()
{
	glcache.Enable(GL_DEPTH_TEST);
	glcache.DepthMask(false);

	// ---------------------------------------------------------------------
	// 1. Accumulate Colors and Depth Complexity
	// ---------------------------------------------------------------------

	glBindFramebuffer(GL_FRAMEBUFFER, g_accumulationFboId);
	glDrawBuffers(2, g_drawBuffers);

	glcache.ClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);

	glBlendEquation(GL_FUNC_ADD);
	glcache.BlendFunc(GL_ONE, GL_ONE);
	glcache.Enable(GL_BLEND);

	DrawListTranslucentAutoSorted(pvrrc.global_param_tr, 0, pvrrc.global_param_tr.used(), true);

	// ---------------------------------------------------------------------
	// 2. Approximate Blending
	// ---------------------------------------------------------------------

   glBindFramebuffer(RARCH_GL_FRAMEBUFFER, hw_render.get_current_framebuffer());

	glcache.Enable(GL_BLEND);
	glcache.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glcache.Disable(GL_DEPTH_TEST);

	glcache.UseProgram(g_wavg_final_shader.program);
	ShaderUniforms.Set(&g_wavg_final_shader);
	glActiveTexture(GL_TEXTURE0);
	glcache.BindTexture(GL_TEXTURE_RECTANGLE, g_accumulationTexId[0]);
	glUniform1i(glGetUniformLocation(g_wavg_final_shader.program, "ColorTex0"), 0);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_RECTANGLE, g_accumulationTexId[1]);
	glUniform1i(glGetUniformLocation(g_wavg_final_shader.program, "ColorTex1"), 1);
	glActiveTexture(GL_TEXTURE0);

	DrawQuad();
}

extern bool hack_on;

void RenderWeightedBlended()
{
//	if (hack_on)
//		glcache.Disable(GL_DEPTH_TEST);
//	else
		glcache.Enable(GL_DEPTH_TEST);
	glcache.DepthMask(false);

	// ---------------------------------------------------------------------
	// 1. Accumulate Colors and Depth Complexity
	// ---------------------------------------------------------------------

	glBindFramebuffer(GL_FRAMEBUFFER, g_accumulationFboId);
	glDrawBuffers(2, g_drawBuffers);

	glcache.ClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);

	glcache.Enable(GL_BLEND);
	glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);

	DrawListTranslucentAutoSorted(pvrrc.global_param_tr, 0, pvrrc.global_param_tr.used(), true);

	// ---------------------------------------------------------------------
	// 2. Approximate Blending
	// ---------------------------------------------------------------------

	glBindFramebuffer(RARCH_GL_FRAMEBUFFER, hw_render.get_current_framebuffer());

	glcache.Enable(GL_BLEND);
	glBlendFunc(GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA);

	glcache.UseProgram(g_wavg_final_shader.program);
	ShaderUniforms.Set(&g_wavg_final_shader);
	glActiveTexture(GL_TEXTURE0);
	glcache.BindTexture(GL_TEXTURE_RECTANGLE, g_accumulationTexId[0]);
	glUniform1i(glGetUniformLocation(g_wavg_final_shader.program, "ColorTex0"), 0);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_RECTANGLE, g_accumulationTexId[1]);
	glUniform1i(glGetUniformLocation(g_wavg_final_shader.program, "ColorTex1"), 1);
	glActiveTexture(GL_TEXTURE0);

	DrawQuad();
}

//
// Front depth peeling
//
static float g_opacity = 0.6;
static int g_numPasses = 20;		// SoA opening sequence needs at least 12 passes!!!
GLuint g_frontFboId[2];
GLuint g_frontDepthTexId[2];
GLuint g_frontColorTexId[2];
GLuint g_frontColorBlenderTexId;
GLuint g_frontColorBlenderFboId;
//GLuint g_frontDepthInitTexId;
GLuint g_samples_query;
PipelineShader g_front_blend_shader;
PipelineShader g_front_final_shader;

const char *front_blend_fragment_source = "\
#version 140 \n\
out vec4 FragColor; \n\
uniform sampler2D TempTex; \n\
uniform lowp vec2 screen_size; \n\
 \n\
void main(void) \n\
{ \n\
   FragColor = texture(TempTex, gl_FragCoord.xy / screen_size); \n\
} \n\
";


const char *front_final_fragment_source = "\
#version 140 \n\
out vec4 FragColor; \n\
uniform sampler2D ColorTex; \n\
uniform lowp vec2 screen_size; \n\
 \n\
void main(void) \n\
{ \n\
	vec4 frontColor = texture(ColorTex, gl_FragCoord.xy / screen_size); \n\
//	if (frontColor.a >= 0.99) { \n\
//		FragColor = vec4(0, 0, 0, 0); \n\
//		return; \n\
//	} \n\
	FragColor.rgb = frontColor.rgb / (1 - frontColor.a); \n\
	FragColor.a = 1 - frontColor.a; \n\
} \n\
";

void InitFrontPeelingRenderTargets()
{
	glGenTextures(2, g_frontDepthTexId);
	glGenTextures(2, g_frontColorTexId);
	glGenFramebuffers(2, g_frontFboId);

	for (int i = 0; i < 2; i++)
	{
		glcache.BindTexture(GL_TEXTURE_2D, g_frontDepthTexId[i]);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glcache.TexParameteri(GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_DEPTH_COMPONENT);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH32F_STENCIL8,
					 g_imageWidth, g_imageHeight, 0, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV, NULL);

		glcache.BindTexture(GL_TEXTURE_2D, g_frontColorTexId[i]);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g_imageWidth, g_imageHeight,
					 0, GL_RGBA, GL_FLOAT, 0);

		glBindFramebuffer(GL_FRAMEBUFFER, g_frontFboId[i]);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
								  GL_TEXTURE_2D, g_frontDepthTexId[i], 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
								  GL_TEXTURE_2D, g_frontColorTexId[i], 0);
	}

	g_frontColorBlenderTexId = glcache.GenTexture();
	glcache.BindTexture(GL_TEXTURE_2D, g_frontColorBlenderTexId);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g_imageWidth, g_imageHeight,
				 0, GL_RGBA, GL_FLOAT, 0);

	//g_frontDepthInitTexId = glcache.GenTexture();
	//glcache.BindTexture(GL_TEXTURE_RECTANGLE, g_frontDepthInitTexId);
	//glcache.TexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	//glcache.TexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	//glcache.TexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	//glcache.TexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	//glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_DEPTH_COMPONENT,
				 //g_imageWidth, g_imageHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

	glGenFramebuffers(1, &g_frontColorBlenderFboId);
	glBindFramebuffer(GL_FRAMEBUFFER, g_frontColorBlenderFboId);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
							  GL_TEXTURE_2D, g_frontDepthTexId[0], 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
							  GL_TEXTURE_2D, g_frontColorBlenderTexId, 0);

	glGenQueries(1, &g_samples_query);
}

void DeleteFrontPeelingRenderTargets()
{
	glDeleteFramebuffers(2, g_frontFboId);
	glDeleteFramebuffers(1, &g_frontColorBlenderFboId);
	glcache.DeleteTextures(2, g_frontDepthTexId);
	glcache.DeleteTextures(2, g_frontColorTexId);
	glcache.DeleteTextures(1, &g_frontColorBlenderTexId);
	//glcache.DeleteTextures(1, &g_frontDepthInitTexId);
	glDeleteQueries(1, &g_samples_query);
}

extern GLuint geom_fbo;
//--------------------------------------------------------------------------
void RenderFrontToBackPeeling(int first, int count)
{
	// ---------------------------------------------------------------------
	// 1. Initialize Min Depth Buffer
	// ---------------------------------------------------------------------
   glBindFramebuffer(GL_FRAMEBUFFER, geom_fbo);
	glBindTexture(GL_TEXTURE_2D, g_frontDepthTexId[0]);
	// FIXME:
	// GL_DEPTH_STENCIL is super slow
	// GL_DEPTH32F_STENCIL8 is fast but doesn't seem to work (depth values are wrong?!?!?)
	// GL_DEPTH24_STENCIL8 seems to work but is super slow
	// GL_DEPTH_COMPONENT32F fails
	// GL_FLOAT_32_UNSIGNED_INT_24_8_REV fails
	glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH32F_STENCIL8, 0, 0, g_imageWidth, g_imageHeight, 0);
	glBindTexture(GL_TEXTURE_2D, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, g_frontColorBlenderFboId);
	glDrawBuffer(g_drawBuffers[0]);

	glcache.DepthMask(true);
	glcache.Enable(GL_DEPTH_TEST);

	glcache.ClearColor(0, 0, 0, 1);
   glClear(GL_COLOR_BUFFER_BIT /* | GL_DEPTH_BUFFER_BIT */);

	glcache.Disable(GL_BLEND);

   // Hack on
	glClear(GL_DEPTH_BUFFER_BIT);
	// TODO Hack to get the depth from OP and PT passes.
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

	DrawListOpaque(pvrrc.global_param_op, 0, pvrrc.global_param_op.used(), false, 1);
	DrawListPunchThrough(pvrrc.global_param_pt, 0, pvrrc.global_param_pt.used(), false, 1);

	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
   // Hack off


	DrawListTranslucentAutoSorted(pvrrc.global_param_tr, first, count, false, 1, 4, 5);		// FIXME initial pass for other blend modes

	// ---------------------------------------------------------------------
	// 2. Depth Peeling + Blending
	// ---------------------------------------------------------------------
bool hack_on;
if (hack_on)
	g_numPasses = 4;
else
	g_numPasses = 20;

	int numLayers = (g_numPasses - 1) * 2;
	for (int layer = 1; layer < numLayers; layer++) {
		int currId = layer % 2;
		int prevId = 1 - currId;

      glBindFramebuffer(GL_FRAMEBUFFER, geom_fbo);
		glBindTexture(GL_TEXTURE_2D, g_frontDepthTexId[currId]);
		glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH32F_STENCIL8, 0, 0, g_imageWidth, g_imageHeight, 0);
		glBindTexture(GL_TEXTURE_2D, 0);

		glBindFramebuffer(GL_FRAMEBUFFER, g_frontFboId[currId]);
		glDrawBuffer(g_drawBuffers[0]);
      //		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, g_frontDepthTexId[currId], 0);

		glcache.ClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT);

		glcache.Enable(GL_DEPTH_TEST);

		glActiveTexture(GL_TEXTURE2);
      glBindTexture(GL_TEXTURE_2D, g_frontDepthTexId[prevId]);		// DepthTex
		glActiveTexture(GL_TEXTURE0);

		glBeginQuery(GL_SAMPLES_PASSED, g_samples_query);
		// Peeling shader
		DrawListTranslucentAutoSorted(pvrrc.global_param_tr, first, count, false, 2, 4, 5);	// Only 4,5 blending
		//DrawListTranslucentAutoSorted(pvrrc.global_param_tr, first, count, false, 2, 1, 1); // Only 1,1 blending
		//DrawListTranslucentAutoSorted(pvrrc.global_param_tr, first, count, false, 2, 4, 1); // Only 4,1 blending
		glEndQuery(GL_SAMPLES_PASSED);

		GLuint sample_count;
		glGetQueryObjectuiv(g_samples_query, GL_QUERY_RESULT, &sample_count);
		if (sample_count == 0) {
			printf("Aborting depth peeling after %d layers\n", layer);
			break;
		}

		glBindFramebuffer(GL_FRAMEBUFFER, g_frontColorBlenderFboId);
		glDrawBuffer(g_drawBuffers[0]);

		glcache.Disable(GL_DEPTH_TEST);
		glcache.Enable(GL_BLEND);

		glBlendEquation(GL_FUNC_ADD);
		glBlendFuncSeparate(GL_DST_ALPHA, GL_ONE,
							GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
		// Let's do it again for 1,1 blending
		//glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_REVERSE_SUBTRACT);
		//glBlendFunc(GL_ONE, GL_ONE);
		// Let's do it again for 4,1 blending
		//glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_REVERSE_SUBTRACT);
		//glBlendFunc(GL_SRC_ALPHA, GL_ONE);

		glcache.UseProgram(g_front_blend_shader.program);
		ShaderUniforms.Set(&g_front_blend_shader);
		glActiveTexture(GL_TEXTURE2);
      glBindTexture(GL_TEXTURE_2D, g_frontColorTexId[currId]);		// TempTex
		glActiveTexture(GL_TEXTURE0);

		// Blending shader
		DrawQuad();

		SetupMainVBO();

		glcache.Disable(GL_BLEND);
	}

	// ---------------------------------------------------------------------
	// 3. Final Pass
	// ---------------------------------------------------------------------

	glBindFramebuffer(RARCH_GL_FRAMEBUFFER, hw_render.get_current_framebuffer());
	glDrawBuffer(GL_BACK);
	glcache.Disable(GL_DEPTH_TEST);

	glcache.Enable(GL_BLEND);

	// FIXME dst=GL_ONE blending don't reduce the dst alpha, so this blending function cannot work.
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);	// No cache! b/c of glBlendFuncSeparate
	// kinda works for GL_ONE blending
	//glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glcache.UseProgram(g_front_final_shader.program);
	ShaderUniforms.Set(&g_front_final_shader);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, g_frontColorBlenderTexId);			// ColorTex
	glActiveTexture(GL_TEXTURE0);

	// Final blending
	DrawQuad();
}


void InitDualPeeling()
{
	if (g_accumulationTexId[0] != 0 || g_frontFboId[0] != 0)
		return;

	g_imageWidth = screen_width;
	g_imageHeight = screen_height;

	glGenVertexArrays(1, &g_quadVertexArray);
	glGenBuffers(1, &g_quadBuffer);

	// Allocate render targets first
	InitFrontPeelingRenderTargets();
	InitAccumulationRenderTargets();
	glBindFramebuffer(RARCH_GL_FRAMEBUFFER, hw_render.get_current_framebuffer());

	// Build shaders

	CompilePipelineShader(&g_wavg_final_shader, wavg_final_fragment);

	CompilePipelineShader(&g_front_blend_shader, front_blend_fragment_source);
	glUniform1i(glGetUniformLocation(g_front_blend_shader.program, "TempTex"), 2);

	CompilePipelineShader(&g_front_final_shader, front_final_fragment_source);
	glUniform1i(glGetUniformLocation(g_front_final_shader.program, "ColorTex"), 2);
}

void DualPeelingReshape(int w, int h)
{
	if (g_imageWidth != w || g_imageHeight != h)
	{
		g_imageWidth = w;
		g_imageHeight = h;

		DeleteFrontPeelingRenderTargets();
		InitFrontPeelingRenderTargets();

		DeleteAccumulationRenderTargets();
		InitAccumulationRenderTargets();
	}
}
