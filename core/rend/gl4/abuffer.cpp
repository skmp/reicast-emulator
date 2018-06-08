/*
 * abuffer.cpp
 *
 *  Created on: May 26, 2018
 *      Author: raph
 */
#include "gles.h"
#include "glcache.h"

#define ABUFFER_SIZE 32

GLuint abufferTexID = 0;
GLuint abufferBlendingTexID = 0;
GLuint abufferCounterTexID = 0;
PipelineShader g_abuffer_final_shader;
PipelineShader g_abuffer_clear_shader;

static int g_imageWidth = 0;
static int g_imageHeight = 0;

extern void DrawQuad();

static const char *final_shader_source = "\
#version 140 \n\
#extension GL_EXT_shader_image_load_store : enable \n\
#define ABUFFER_SIZE 32 \n\
out vec4 FragColor; \n\
uniform layout(size1x32) uimage2D abufferCounterImg; \n\
uniform layout(size4x32) image2DArray abufferImg; \n\
uniform layout(size2x32) image2DArray abufferBlendingImg; \n\
uniform lowp vec2 screen_size; \n\
 \n\
vec4 colorList[ABUFFER_SIZE]; \n\
vec4 depthBlendList[ABUFFER_SIZE]; \n\
 \n\
int resolveClosest(ivec2 coords, int abNumFrag) { \n\
 \n\
	// Search smallest z \n\
	float minZ = 1000000.0f; \n\
	int minIdx; \n\
	for (int i = 0; i < abNumFrag; i++) { \n\
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
int findOpaque(ivec2 coords, int abNumFrag) { \n\
 \n\
	for (int i = 0; i < abNumFrag; i++) { \n\
		vec4 val = imageLoad(abufferBlendingImg, ivec3(coords, i)); \n\
		if (round(val.y) == 8) { \n\
			return i; \n\
		} \n\
	} \n\
 \n\
	return 0; \n\
} \n\
 \n\
void fillFragmentArray(ivec2 coords, int abNumFrag) { \n\
	// Load fragments into a local memory array for sorting \n\
	for (int i = 0; i < abNumFrag; i++) { \n\
		colorList[i] = imageLoad(abufferImg, ivec3(coords, i)); \n\
		depthBlendList[i] = imageLoad(abufferBlendingImg, ivec3(coords, i)); \n\
	} \n\
} \n\
 \n\
// Bubble sort used to sort fragments \n\
void bubbleSort(int array_size) { \n\
  for (int i = (array_size - 2); i >= 0; --i) { \n\
    for (int j = 0; j <= i; ++j) { \n\
      if (depthBlendList[j].x > depthBlendList[j+1].x) { \n\
		vec4 temp = depthBlendList[j + 1]; \n\
		depthBlendList[j + 1] = depthBlendList[j]; \n\
		depthBlendList[j] = temp; \n\
		temp = colorList[j + 1]; \n\
		colorList[j + 1] = colorList[j]; \n\
		colorList[j] = temp; \n\
      } \n\
    } \n\
  } \n\
} \n\
 \n\
// Blend fragments back-to-front \n\
vec4 resolveAlphaBlend(ivec2 coords, int abNumFrag){ \n\
	 \n\
	// Copy fragments in local array \n\
	fillFragmentArray(coords, abNumFrag); \n\
	 \n\
	// Sort fragments in local memory array \n\
	bubbleSort(abNumFrag); \n\
	 \n\
	vec4 finalColor = colorList[abNumFrag - 1]; \n\
	for (int i = abNumFrag - 2; i >= 0; i--) { \n\
		vec4 col = colorList[i]; \n\
		 \n\
		int srcBlend = int(depthBlendList[i].y) / 8; \n\
		int dstBlend = int(depthBlendList[i].y) % 8; \n\
		if (srcBlend == 1 && dstBlend == 0) \n\
			finalColor = col; \n\
		else if (srcBlend == 4 && dstBlend == 5) \n\
			finalColor = finalColor * (1 - col.a) + col * (col.a); \n\
		else if (srcBlend == 4 && dstBlend == 1) \n\
			finalColor = finalColor + col * (col.a); \n\
		else if (srcBlend == 1 && dstBlend == 1) \n\
			finalColor = finalColor + col; \n\
	} \n\
	finalColor.a = 1; \n\
	return finalColor; \n\
	 \n\
} \n\
 \n\
void main(void) \n\
{ \n\
	ivec2 coords = ivec2(gl_FragCoord.xy); \n\
	int abNumFrag = int(imageLoad(abufferCounterImg, coords).r); \n\
	// Crash without this (WTF ?) \n\
	if (abNumFrag < 0) \n\
		abNumFrag = 0; \n\
	if (abNumFrag > ABUFFER_SIZE) \n\
		abNumFrag = ABUFFER_SIZE; \n\
	if (abNumFrag > 0) { \n\
		// Compute and output final color for the frame buffer \n\
		//If we only want the closest fragment \n\
		//int minIdx = resolveClosest(coords, abNumFrag); \n\
		//FragColor = vec4(float(abNumFrag) / ABUFFER_SIZE, 0, 0, 1); \n\
		//FragColor = imageLoad(abufferImg, ivec3(coords, 0)); \n\
		FragColor = resolveAlphaBlend(coords, abNumFrag); \n\
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
coherent uniform layout(size4x32) image2DArray abufferImg; \n\
coherent uniform layout(size2x32) image2DArray abufferBlendingImg; \n\
 \n\
void main(void) \n\
{ \n\
	ivec2 coords = ivec2(gl_FragCoord.xy); \n\
 \n\
	// Reset counter \n\
	imageStore(abufferCounterImg, coords, uvec4(0)); \n\
 \n\
	// FIXME should not be necessary \n\
	// Put black in first layer \n\
	//imageStore(abufferImg, ivec3(coords, 0), vec4(0)); \n\
	// Reset depth \n\
	//imageStore(abufferBlendingImg, ivec3(coords, 0), vec4(0)); \n\
 \n\
	// Discard fragment so nothing is writen to the framebuffer \n\
	discard; \n\
} \n\
";

void initABuffer()
{
	g_imageWidth = screen_width;
	g_imageHeight = screen_height;

	if (abufferTexID == 0)
		abufferTexID = glcache.GenTexture();
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D_ARRAY, abufferTexID);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA32F, g_imageWidth, g_imageHeight, ABUFFER_SIZE, 0,  GL_RGBA, GL_FLOAT, 0);
	glBindImageTexture(3, abufferTexID, 0, true, 0,  GL_READ_WRITE, GL_RGBA32F);

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

	if (abufferBlendingTexID == 0)
		abufferBlendingTexID = glcache.GenTexture();
	glActiveTexture(GL_TEXTURE5);
	glBindTexture(GL_TEXTURE_2D_ARRAY, abufferBlendingTexID);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RG32F, g_imageWidth, g_imageHeight, ABUFFER_SIZE, 0,  GL_RG, GL_FLOAT, 0);
	glBindImageTexture(5, abufferBlendingTexID, 0, true, 0,  GL_READ_WRITE, GL_RG32F);

	if (g_abuffer_final_shader.program == 0)
		CompilePipelineShader(&g_abuffer_final_shader, final_shader_source);
	if (g_abuffer_clear_shader.program == 0)
		CompilePipelineShader(&g_abuffer_clear_shader, clear_shader_source);
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

void renderABuffer() {
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D_ARRAY, abufferTexID);
	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D, abufferCounterTexID);
	glActiveTexture(GL_TEXTURE5);
	glBindTexture(GL_TEXTURE_2D_ARRAY, abufferBlendingTexID);

	glcache.UseProgram(g_abuffer_final_shader.program);
	ShaderUniforms.Set(&g_abuffer_final_shader);

	glcache.Disable(GL_BLEND);
	glcache.Disable(GL_DEPTH_TEST);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	DrawQuad();


	glcache.UseProgram(g_abuffer_clear_shader.program);
	ShaderUniforms.Set(&g_abuffer_clear_shader);

	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	DrawQuad();

}
