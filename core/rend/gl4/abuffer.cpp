/*
 * abuffer.cpp
 *
 *  Created on: May 26, 2018
 *      Author: raph
 */
#include "gles.h"
#include "glcache.h"

GLuint abufferTexID = 0;
GLuint abufferBlendingTexID = 0;
GLuint abufferCounterTexID = 0;
PipelineShader g_abuffer_final_shader;
PipelineShader g_abuffer_final_nosort_shader;
PipelineShader g_abuffer_clear_shader;
PipelineShader g_abuffer_pass2_shader;
static GLuint g_quadBuffer = 0;
static GLuint g_quadVertexArray = 0;

static int g_imageWidth = 0;
static int g_imageHeight = 0;

static const char *final_shader_source = "\
#version 140 \n\
#extension GL_EXT_shader_image_load_store : enable \n\
#define ABUFFER_SIZE " ABUFFER_SIZE_STR " \n\
#define DEPTH_SORTED %d \n\
out vec4 FragColor; \n\
uniform layout(size1x32) uimage2D abufferCounterImg; \n\
uniform layout(size4x32) image2DArray abufferImg; \n\
uniform layout(size4x32) image2DArray abufferBlendingImg; \n\
uniform lowp vec2 screen_size; \n\
 \n\
vec4 colorList[ABUFFER_SIZE]; \n\
vec4 depthBlendList[ABUFFER_SIZE]; \n\
 \n\
int resolveClosest(ivec2 coords, int num_frag) { \n\
 \n\
	// Search smallest z \n\
	float minZ = 1000000.0f; \n\
	int minIdx; \n\
	for (int i = 0; i < num_frag; i++) { \n\
		vec4 val = imageLoad(abufferBlendingImg, ivec3(coords, i)); \n\
		if (val.x < minZ) { \n\
			minZ = val.x; \n\
			minIdx = i; \n\
		} \n\
	} \n\
 \n\
	// Return the index of the closest fragment \n\
	return minIdx; \n\
} \n\
 \n\
 \n\
int findOpaque(ivec2 coords, int num_frag) { \n\
 \n\
	for (int i = 0; i < num_frag; i++) { \n\
		vec4 val = imageLoad(abufferBlendingImg, ivec3(coords, i)); \n\
		if (round(val.y) == 8) { \n\
			return i; \n\
		} \n\
	} \n\
 \n\
	return 0; \n\
} \n\
 \n\
void fillFragmentArray(ivec2 coords, int num_frag) { \n\
	// Load fragments into a local memory array for sorting \n\
	for (int i = 0; i < num_frag; i++) { \n\
		colorList[i] = imageLoad(abufferImg, ivec3(coords, i)); \n\
		depthBlendList[i] = imageLoad(abufferBlendingImg, ivec3(coords, i)); \n\
	} \n\
} \n\
 \n\
// Bubble sort used to sort fragments \n\
void bubbleSort(int array_size) { \n\
	for (int i = array_size - 2; i >= 0; i--) { \n\
		for (int j = 0; j <= i; j++) { \n\
#if DEPTH_SORTED == 1 \n\
			// depth only \n\
			//if (depthBlendList[j].x < depthBlendList[j + 1].x) { \n\
			// depth then poly number \n\
			if (depthBlendList[j].x < depthBlendList[j + 1].x || (depthBlendList[j].x == depthBlendList[j + 1].x && depthBlendList[j].z > depthBlendList[j + 1].z)) { \n\
#else \n\
			// poly number only \n\
			if (depthBlendList[j].z > depthBlendList[j + 1].z) { \n\
#endif \n\
				vec4 depthBlend = depthBlendList[j + 1]; \n\
				depthBlendList[j + 1] = depthBlendList[j]; \n\
				depthBlendList[j] = depthBlend; \n\
				vec4 color = colorList[j + 1]; \n\
				colorList[j + 1] = colorList[j]; \n\
				colorList[j] = color; \n\
			} \n\
		} \n\
	} \n\
} \n\
 \n\
 \n\
// Insertion sort used to sort fragments \n\
void insertionSort(int array_size) { \n\
	for (int i = 1; i < array_size; i++) { \n\
		vec4 aDepth = depthBlendList[i]; \n\
		vec4 aColor = colorList[i]; \n\
		int j = i - 1; \n\
#if DEPTH_SORTED == 1 \n\
		for (; j >= 0 && (depthBlendList[j].x < aDepth.x || (depthBlendList[j].x == aDepth.x && depthBlendList[j].z > aDepth.z)); j--) { \n\
#else \n\
		for (; j >= 0 && depthBlendList[j].z < aDepth.z; j--) { \n\
#endif \n\
			depthBlendList[j + 1] = depthBlendList[j]; \n\
			colorList[j + 1] = colorList[j]; \n\
		} \n\
		depthBlendList[j + 1] = aDepth; \n\
		colorList[j + 1] = aColor; \n\
	} \n\
} \n\
 \n\
vec4 returnNthLayer(ivec2 coords, int num_frag, int layer) { \n\
	 \n\
	// Copy fragments in local array \n\
	fillFragmentArray(coords, num_frag); \n\
	 \n\
	// Sort fragments in local memory array \n\
	bubbleSort(num_frag); \n\
	 \n\
	return vec4(colorList[min(layer, num_frag - 1)].rgb, 1); \n\
} \n\
 \n\
// Blend fragments back-to-front \n\
vec4 resolveAlphaBlend(ivec2 coords, int num_frag){ \n\
	 \n\
	// Copy fragments in local array \n\
	fillFragmentArray(coords, num_frag); \n\
	 \n\
	// Sort fragments in local memory array \n\
	bubbleSort(num_frag); \n\
	 \n\
	vec4 finalColor = colorList[0]; \n\
	for (int i = 1; i < num_frag; i++) { \n\
		vec4 srcColor = colorList[i]; \n\
		float srcAlpha = srcColor.a; \n\
		float dstAlpha = finalColor.a; \n\
		 \n\
		int srcBlend = int(depthBlendList[i].y) / 8; \n\
		switch (srcBlend) \n\
		{ \n\
			case 0: // zero \n\
				srcColor = vec4(0); \n\
				break; \n\
			case 1: // one \n\
				break; \n\
			case 2: // other color \n\
				srcColor *= finalColor; \n\
				break; \n\
			case 3: // inverse other color \n\
				srcColor *= vec4(1) - finalColor; \n\
				break; \n\
			case 4: // src alpha \n\
				srcColor *= srcAlpha; \n\
				break; \n\
			case 5: // inverse src alpha \n\
				srcColor *= 1 - srcAlpha; \n\
				break; \n\
			case 6: // dst alpha \n\
				srcColor *= dstAlpha; \n\
				break; \n\
			case 7: // inverse dst alpha \n\
				srcColor *= 1 - dstAlpha; \n\
				break; \n\
		} \n\
		int dstBlend = int(depthBlendList[i].y) % 8; \n\
		switch (dstBlend) \n\
		{ \n\
			case 0: // zero \n\
				finalColor = vec4(0); \n\
				break; \n\
			case 1: // one \n\
				break; \n\
			case 2: // other color \n\
				finalColor *= colorList[i]; \n\
				break; \n\
			case 3: // inverse other color \n\
				finalColor *= vec4(1) - colorList[i]; \n\
				break; \n\
			case 4: // src alpha \n\
				finalColor *= srcAlpha; \n\
				break; \n\
			case 5: // inverse src alpha \n\
				finalColor *= 1 - srcAlpha; \n\
				break; \n\
			case 6: // dst alpha \n\
				finalColor *= dstAlpha; \n\
				break; \n\
			case 7: // inverse dst alpha \n\
				finalColor *= 1 - dstAlpha; \n\
				break; \n\
		} \n\
		finalColor = clamp(finalColor + srcColor, 0, 1); \n\
	} \n\
	finalColor.a = 1; \n\
	return finalColor; \n\
	 \n\
} \n\
 \n\
void main(void) \n\
{ \n\
	ivec2 coords = ivec2(gl_FragCoord.xy); \n\
	int num_frag = int(imageLoad(abufferCounterImg, coords).r); \n\
	// Crash without this (WTF ?) \n\
	if (num_frag < 0) \n\
		num_frag = 0; \n\
	if (num_frag > ABUFFER_SIZE) \n\
		num_frag = ABUFFER_SIZE; \n\
	if (num_frag > 0) { \n\
		// Compute and output final color for the frame buffer \n\
		//If we only want the closest fragment \n\
		//int minIdx = resolveClosest(coords, num_frag); \n\
		//FragColor = imageLoad(abufferImg, ivec3(coords, minIdx)); \n\
		// Visualize the number of layers in use \n\
		//FragColor = vec4(float(num_frag) / ABUFFER_SIZE, 0, 0, 1); \n\
		//FragColor = imageLoad(abufferImg, ivec3(coords, 0)); \n\
		//FragColor = returnNthLayer(coords, num_frag, 1); \n\
		FragColor = resolveAlphaBlend(coords, num_frag); \n\
	} \n\
	else \n\
		// If no fragment, write nothing \n\
		discard; \n\
	 \n\
} \n\
";

static const char *clear_shader_source = "\
#version 140 \n\
#extension GL_EXT_shader_image_load_store : enable \n\
coherent uniform layout(size1x32) uimage2D abufferCounterImg; \n\
 \n\
void main(void) \n\
{ \n\
	ivec2 coords = ivec2(gl_FragCoord.xy); \n\
 \n\
	// Reset counter \n\
	imageStore(abufferCounterImg, coords, uvec4(0)); \n\
 \n\
	// Discard fragment so nothing is writen to the framebuffer \n\
	discard; \n\
} \n\
";

// Renders the opaque and pt rendered texture into a-buffers
static const char *pass2_shader_source = "\
#version 140 \n\
#extension GL_EXT_shader_image_load_store : enable \n\
coherent uniform layout(size1x32) uimage2D abufferCounterImg; \n\
coherent uniform layout(size4x32) image2DArray abufferImg; \n\
coherent uniform layout(size4x32) image2DArray abufferBlendingImg; \n\
uniform lowp vec2 screen_size; \n\
uniform sampler2D DepthTex; \n\
uniform sampler2D tex; \n\
 \n\
void main(void) \n\
{ \n\
	ivec2 coords = ivec2(gl_FragCoord.xy); \n\
 \n\
	int abidx = int(imageAtomicAdd(abufferCounterImg, coords, uint(1))); \n\
	ivec3 coords3 = ivec3(coords, abidx); \n\
 \n\
	highp float depth = texture(DepthTex, gl_FragCoord.xy / screen_size).r; \n\
	vec4 blend_val = vec4(depth, 8, 0, 0); \n\
	imageStore(abufferBlendingImg, coords3, blend_val); \n\
	vec4 color = texture(tex, gl_FragCoord.xy / screen_size); \n\
	imageStore(abufferImg, coords3, color); \n\
 \n\
	// Discard fragment so nothing is writen to the framebuffer \n\
	discard; \n\
} \n\
";

static const char *tr_modvol_shader_source = "\
#version 140 \n\
#extension GL_EXT_shader_image_load_store : enable \n\
coherent uniform layout(size1x32) uimage2D abufferCounterImg; \n\
coherent uniform layout(size4x32) image2DArray abufferBlendingImg; \n\
uniform lowp vec2 screen_size; \n\
uniform sampler2D DepthTex; \n\
 \n\
void main(void) \n\
{ \n\
	ivec2 coords = ivec2(gl_FragCoord.xy); \n\
 \n\
	int num_frag = int(imageLoad(abufferCounterImg, coords).r); \n\
 \n\
	highp float w = 100000.0 * gl_FragCoord.w; \n\
	highp float depth = 1 - log2(1.0 + w) / 34; \n\
	for (int i = 0; i < num_frag; i++) \n\
	{ \n\
		vec4 pixel_info = imageLoad(abufferBlendingImg, ivec3(coords, i)); \n\
		highp float pixel_depth = info.x; \n\
		if (depth > pixel_depth) \n\
			continue; \n\
		// FIXME Need int or uint pixel format, not vec4 \n\
		imageAtomicXor(abufferBlendingImg, ivec3(coords, i), 1); \n\
	} \n\
 \n\
	discard; \n\
} \n\
";

void initABuffer()
{
	g_imageWidth = screen_width;
	g_imageHeight = screen_height;

	if (abufferTexID == 0)
		abufferTexID = glcache.GenTexture();
	glActiveTexture(GL_TEXTURE3); glCheck();
	glBindTexture(GL_TEXTURE_2D_ARRAY, abufferTexID); glCheck();
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST); glCheck();
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST); glCheck();
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA32F, g_imageWidth, g_imageHeight, ABUFFER_SIZE, 0,  GL_RGBA, GL_FLOAT, 0); glCheck();
	glBindImageTexture(3, abufferTexID, 0, true, 0,  GL_READ_WRITE, GL_RGBA32F);
	glCheck();

	if (abufferCounterTexID == 0)
		abufferCounterTexID = glcache.GenTexture();
	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D, abufferCounterTexID);

	// Set filter
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	//Texture creation
	//Uses GL_R32F instead of GL_R32I that is not working in R257.15
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, g_imageWidth, g_imageHeight, 0,  GL_RED, GL_FLOAT, 0);
	glBindImageTexture(4, abufferCounterTexID, 0, false, 0,  GL_READ_WRITE, GL_R32UI);
	glCheck();

	if (abufferBlendingTexID == 0)
		abufferBlendingTexID = glcache.GenTexture();
	glActiveTexture(GL_TEXTURE5);
	glBindTexture(GL_TEXTURE_2D_ARRAY, abufferBlendingTexID);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA32F, g_imageWidth, g_imageHeight, ABUFFER_SIZE, 0,  GL_RGBA, GL_FLOAT, 0);
	glBindImageTexture(5, abufferBlendingTexID, 0, true, 0,  GL_READ_WRITE, GL_RGBA32F);

	if (g_abuffer_final_shader.program == 0)
	{
		char source[8192];
		sprintf(source, final_shader_source, 1);
		CompilePipelineShader(&g_abuffer_final_shader, source);
	}
	if (g_abuffer_final_nosort_shader.program == 0)
	{
		char source[8192];
		sprintf(source, final_shader_source, 0);
		CompilePipelineShader(&g_abuffer_final_nosort_shader, source);
	}
	if (g_abuffer_clear_shader.program == 0)
		CompilePipelineShader(&g_abuffer_clear_shader, clear_shader_source);
	if (g_abuffer_pass2_shader.program == 0)
		CompilePipelineShader(&g_abuffer_pass2_shader, pass2_shader_source);

	glGenVertexArrays(1, &g_quadVertexArray);
	glGenBuffers(1, &g_quadBuffer);

	glCheck();
}

void reshapeABuffer(int w, int h)
{
	if (w != g_imageWidth || h != g_imageHeight) {
		glcache.DeleteTextures(1, &abufferTexID);
		abufferTexID = 0;
		glcache.DeleteTextures(1, &abufferCounterTexID);
		abufferCounterTexID = 0;
		glcache.DeleteTextures(1, &abufferBlendingTexID);
		abufferBlendingTexID = 0;

		initABuffer();
	}
}


void DrawQuad()
{
	glBindVertexArray(g_quadVertexArray);

	struct Vertex vertices[] = {
			{ 0, screen_height, 0.001, { 255, 255, 255, 255 }, { 0, 0, 0, 0 }, 0, 1 },
			{ 0, 0, 0.001, { 255, 255, 255, 255 }, { 0, 0, 0, 0 }, 0, 0 },
			{ screen_width, screen_height, 0.001, { 255, 255, 255, 255 }, { 0, 0, 0, 0 }, 1, 1 },
			{ screen_width, 0, 0.001, { 255, 255, 255, 255 }, { 0, 0, 0, 0 }, 1, 0 },
	};
	GLushort indices[] = { 0, 1, 2, 1, 3 };

	glBindBuffer(GL_ARRAY_BUFFER, g_quadBuffer); glCheck();
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STREAM_DRAW); glCheck();
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); glCheck();

	glEnableVertexAttribArray(VERTEX_POS_ARRAY); glCheck();
	glVertexAttribPointer(VERTEX_POS_ARRAY, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex,x)); glCheck();

	glEnableVertexAttribArray(VERTEX_COL_BASE_ARRAY); glCheck();
	glVertexAttribPointer(VERTEX_COL_BASE_ARRAY, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex,col)); glCheck();

	glEnableVertexAttribArray(VERTEX_COL_OFFS_ARRAY); glCheck();
	glVertexAttribPointer(VERTEX_COL_OFFS_ARRAY, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex,vtx_spc)); glCheck();

	glEnableVertexAttribArray(VERTEX_UV_ARRAY); glCheck();
	glVertexAttribPointer(VERTEX_UV_ARRAY, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex,u)); glCheck();

	glDrawElements(GL_TRIANGLE_STRIP, 5, GL_UNSIGNED_SHORT, indices); glCheck();
}

void renderPass2(GLuint textureId, GLuint depthTexId)
{
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, textureId);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, depthTexId);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D_ARRAY, abufferTexID);
	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D, abufferCounterTexID);
	glActiveTexture(GL_TEXTURE5);
	glBindTexture(GL_TEXTURE_2D_ARRAY, abufferBlendingTexID);
	glActiveTexture(GL_TEXTURE0);

	glcache.UseProgram(g_abuffer_pass2_shader.program);
	ShaderUniforms.Set(&g_abuffer_pass2_shader);

	glcache.Disable(GL_BLEND);
	glcache.Disable(GL_DEPTH_TEST);
	glcache.Disable(GL_CULL_FACE);
glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	DrawQuad();
glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

void renderABuffer(bool sortFragments)
{
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D_ARRAY, abufferTexID);
	glCheck();
	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D, abufferCounterTexID);
	glCheck();
	glActiveTexture(GL_TEXTURE5);
	glBindTexture(GL_TEXTURE_2D_ARRAY, abufferBlendingTexID);
	glCheck();

	glcache.UseProgram(sortFragments ? g_abuffer_final_shader.program : g_abuffer_final_nosort_shader.program);
	ShaderUniforms.Set(&g_abuffer_final_shader);

	glcache.Disable(GL_BLEND);
	glcache.Disable(GL_DEPTH_TEST);
	glcache.Disable(GL_CULL_FACE);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	DrawQuad();

	glCheck();

	glcache.UseProgram(g_abuffer_clear_shader.program);
	ShaderUniforms.Set(&g_abuffer_clear_shader);

	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	DrawQuad();

	glActiveTexture(GL_TEXTURE0);
	glCheck();
}
