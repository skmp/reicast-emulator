/*
 * abuffer.cpp
 *
 *  Created on: May 26, 2018
 *      Author: raph
 */
#include "glcache.h"

GLuint pixels_buffer;
GLuint pixels_pointers;
GLuint atomic_buffer;
PipelineShader g_abuffer_final_shader;
PipelineShader g_abuffer_final_nosort_shader;
PipelineShader g_abuffer_clear_shader;
PipelineShader g_abuffer_pass2_shader;
PipelineShader g_abuffer_tr_modvol_shader;
PipelineShader g_abuffer_tr_modvol_final_shader;
static GLuint g_quadBuffer = 0;
static GLuint g_quadVertexArray = 0;

static int g_imageWidth = 0;
static int g_imageHeight = 0;

#define MAX_PIXELS_PER_FRAGMENT "32"

static const char *final_shader_source = SHADER_HEADER "\
#define DEPTH_SORTED %d \n\
#define MAX_PIXELS_PER_FRAGMENT " MAX_PIXELS_PER_FRAGMENT " \n\
 \n\
layout(binding = 0) uniform sampler2D tex; \n\
uniform lowp vec2 screen_size; \n\
 \n\
out vec4 FragColor; \n\
 \n\
Pixel pixel_list[MAX_PIXELS_PER_FRAGMENT]; \n\
 \n\
int fillFragmentArray(ivec2 coords) { \n\
	// Load fragments into a local memory array for sorting \n\
	uint idx = imageLoad(abufferPointerImg, coords).x; \n\
	int i = 0; \n\
	for (; idx != EOL && i < MAX_PIXELS_PER_FRAGMENT; i++) { \n\
		pixel_list[i] = pixels[idx]; \n\
		idx = pixel_list[i].next; \n\
	} \n\
	return i; \n\
} \n\
 \n\
// Bubble sort used to sort fragments \n\
void bubbleSort(int array_size) { \n\
	for (int i = array_size - 2; i >= 0; i--) { \n\
		for (int j = 0; j <= i; j++) { \n\
#if DEPTH_SORTED == 1 \n\
			// depth then poly number \n\
			if (pixel_list[j].depth < pixel_list[j + 1].depth \n\
				|| (pixel_list[j].depth == pixel_list[j + 1].depth && pixel_list[j].seq_num > pixel_list[j + 1].seq_num)) { \n\
#else \n\
			// poly number only \n\
			if (pixel_list[j].seq_num > pixel_list[j + 1].seq_num) { \n\
#endif \n\
				Pixel p = pixel_list[j + 1]; \n\
				pixel_list[j + 1] = pixel_list[j]; \n\
				pixel_list[j] = p; \n\
			} \n\
		} \n\
	} \n\
} \n\
 \n\
 \n\
// Insertion sort used to sort fragments \n\
void insertionSort(int array_size) { \n\
	for (int i = 1; i < array_size; i++) { \n\
		Pixel p = pixel_list[i]; \n\
		int j = i - 1; \n\
#if DEPTH_SORTED == 1 \n\
		for (; j >= 0 && (pixel_list[j].depth < p.depth || (pixel_list[j].depth == p.depth && pixel_list[j].seq_num > p.seq_num)); j--) { \n\
#else \n\
		for (; j >= 0 && pixel_list[j].seq_num > p.seq_num; j--) { \n\
#endif \n\
			pixel_list[j + 1] = pixel_list[j]; \n\
		} \n\
		pixel_list[j + 1] = p; \n\
	} \n\
} \n\
 \n\
// Blend fragments back-to-front \n\
vec4 resolveAlphaBlend(ivec2 coords) { \n\
	 \n\
	// Copy fragments in local array \n\
	int num_frag = fillFragmentArray(coords); \n\
	 \n\
	// Sort fragments in local memory array \n\
	bubbleSort(num_frag); \n\
	 \n\
	vec4 finalColor = texture(tex, gl_FragCoord.xy / screen_size); \n\
	for (int i = 0; i < num_frag; i++) { \n\
		vec4 srcColor = pixel_list[i].color; \n\
		float srcAlpha = srcColor.a; \n\
		float dstAlpha = finalColor.a; \n\
		 \n\
		int srcBlend = int(pixel_list[i].blend_stencil) / 256 / 8; \n\
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
		int dstBlend = (int(pixel_list[i].blend_stencil) / 256) % 8; \n\
		switch (dstBlend) \n\
		{ \n\
			case 0: // zero \n\
				finalColor = vec4(0); \n\
				break; \n\
			case 1: // one \n\
				break; \n\
			case 2: // other color \n\
				finalColor *= pixel_list[i].color; \n\
				break; \n\
			case 3: // inverse other color \n\
				finalColor *= vec4(1) - pixel_list[i].color; \n\
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
	 \n\
	return finalColor; \n\
	 \n\
} \n\
 \n\
void main(void) \n\
{ \n\
	ivec2 coords = ivec2(gl_FragCoord.xy); \n\
	// Compute and output final color for the frame buffer \n\
	// Visualize the number of layers in use \n\
	//FragColor = vec4(float(fillFragmentArray(coords)) / MAX_PIXELS_PER_FRAGMENT, 0, 0, 1); \n\
	FragColor = resolveAlphaBlend(coords); \n\
} \n\
";

static const char *clear_shader_source = SHADER_HEADER "\
 \n\
void main(void) \n\
{ \n\
	ivec2 coords = ivec2(gl_FragCoord.xy); \n\
 \n\
	// Reset pointers \n\
	imageStore(abufferPointerImg, coords, uvec4(EOL)); \n\
 \n\
	// Discard fragment so nothing is written to the framebuffer \n\
	discard; \n\
} \n\
";

// Renders the opaque and pt rendered texture into a-buffers
static const char *pass2_shader_source = SHADER_HEADER "\
uniform lowp vec2 screen_size; \n\
uniform sampler2D DepthTex; \n\
uniform sampler2D tex; \n\
 \n\
void main(void) \n\
{ \n\
	ivec2 coords = ivec2(gl_FragCoord.xy); \n\
 \n\
	uint idx = atomicCounterIncrement(buffer_index); \n\
	if ((idx + 1u) * 32u - 1u >= ABUFFER_SIZE) \n\
		discard; \n\
	Pixel pixel; \n\
	pixel.color = texture(tex, gl_FragCoord.xy / screen_size); \n\
	pixel.depth = texture(DepthTex, gl_FragCoord.xy / screen_size).r; \n\
	pixel.seq_num = 0; \n\
	pixel.blend_stencil = 0x800u; \n\
	pixel.next = imageAtomicExchange(abufferPointerImg, coords, idx); \n\
	pixels[idx] = pixel; \n\
 \n\
	// Discard fragment so nothing is written to the framebuffer \n\
	discard; \n\
} \n\
";

static const char *tr_modvol_shader_source = SHADER_HEADER "\
#define LAST_PASS %d \n\
void main(void) \n\
{ \n\
#if LAST_PASS == 0 \n\
	setFragDepth(); \n\
#endif \n\
	ivec2 coords = ivec2(gl_FragCoord.xy); \n\
	if (all(greaterThanEqual(coords, ivec2(0))) && all(lessThan(coords, imageSize(abufferPointerImg)))) { \n\
		 \n\
		uint idx = imageLoad(abufferPointerImg, coords).x; \n\
		if ((idx + 1u) * 32u - 1u >= ABUFFER_SIZE) \n\
			discard; \n\
		while (idx != EOL) { \n\
			if (pixels[idx].seq_num > 0) { \n\
#if LAST_PASS == 0 \n\
//				if (gl_FragDepth <= pixels[idx].depth) \n\
//					atomicXor(pixels[idx].blend_stencil, 2u); \n\
#else \n\
				if (mod(pixels[idx].blend_stencil, 256u) != 0u) \n\
					pixels[idx].color.a = 1.0; // FIXME \n\
#endif \n\
			} \n\
			idx = pixels[idx].next; \n\
		} \n\
	} \n\
	 \n\
	discard; \n\
} \n\
";

void initABuffer()
{
	g_imageWidth = screen_width;
	g_imageHeight = screen_height;

	if (pixels_pointers == 0)
		pixels_pointers = glcache.GenTexture();
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, pixels_pointers);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	//Uses GL_R32F instead of GL_R32I that is not working in R257.15
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, g_imageWidth, g_imageHeight, 0, GL_RED, GL_FLOAT, 0);
	glBindImageTexture(3, pixels_pointers, 0, false, 0,  GL_READ_WRITE, GL_R32UI);
	glCheck();

	if (pixels_buffer == 0 )
	{
		// Create the buffer
		glGenBuffers(1, &pixels_buffer);
		// Bind it
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, pixels_buffer);
		// Declare storage
		glBufferData(GL_SHADER_STORAGE_BUFFER, ABUFFER_SIZE, NULL, GL_DYNAMIC_COPY);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, pixels_buffer);
		glCheck();
	}

	if (atomic_buffer == 0 )
	{
		// Create the buffer
		glGenBuffers(1, &atomic_buffer);
		// Bind it
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, atomic_buffer);
		// Declare storage
		glBufferData(GL_ATOMIC_COUNTER_BUFFER, 4, NULL, GL_DYNAMIC_COPY);
		glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, atomic_buffer);
		glCheck();
	}

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
	if (g_abuffer_tr_modvol_shader.program == 0)
	{
		char source[8192];
		sprintf(source, tr_modvol_shader_source, 0);
		CompilePipelineShader(&g_abuffer_tr_modvol_shader, source);
	}
	if (g_abuffer_tr_modvol_final_shader.program == 0)
	{
		char source[8192];
		sprintf(source, tr_modvol_shader_source, 1);
		CompilePipelineShader(&g_abuffer_tr_modvol_final_shader, source);
	}

	if (g_quadVertexArray == 0)
		glGenVertexArrays(1, &g_quadVertexArray);
	if (g_quadBuffer == 0)
		glGenBuffers(1, &g_quadBuffer);

	glCheck();

}

void reshapeABuffer(int w, int h)
{
	if (w != g_imageWidth || h != g_imageHeight) {
		glcache.DeleteTextures(1, &pixels_pointers);
		pixels_pointers = 0;

		// FIXME We might need to resize the pixels_buffer accordingly

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
	glBindTexture(GL_TEXTURE_2D, pixels_pointers);
	glActiveTexture(GL_TEXTURE0);

	glcache.UseProgram(g_abuffer_pass2_shader.program);
	ShaderUniforms.Set(&g_abuffer_pass2_shader);

	glcache.Disable(GL_BLEND);
	glcache.Disable(GL_DEPTH_TEST);
	glcache.Disable(GL_CULL_FACE);

	DrawQuad();
}

void DrawTranslucentModVols(int first, int count)
{
	if (count == 0 || pvrrc.modtrig.used() == 0)
		return;
	printf("Drawing %d translucent modvols %d triangles\n", count, pvrrc.modtrig.used());
	SetupModvolVBO();

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, pixels_pointers);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);

	glcache.Disable(GL_BLEND);
	glcache.Disable(GL_DEPTH_TEST);
	glcache.Disable(GL_STENCIL_TEST);

	glcache.UseProgram(g_abuffer_tr_modvol_final_shader.program);
	ShaderUniforms.Set(&g_abuffer_tr_modvol_final_shader);
	glcache.UseProgram(g_abuffer_tr_modvol_shader.program);
	ShaderUniforms.Set(&g_abuffer_tr_modvol_shader);
	glCheck();

	u32 mod_base = 0; //cur start triangle
	u32 mod_last = 0; //last merge

	u32 cmv_count = count - 1;
	ISP_Modvol* params = &pvrrc.global_param_mvo_tr.head()[first];

	//ISP_Modvol
	for (u32 cmv = 0; cmv < 3 /* FIXME cmv_count */; cmv++)
	{

		ISP_Modvol ispc = params[cmv];
		mod_base = ispc.id;
		if (mod_last == 0)
			// FIXME Will this work if no OP modvols are drawn?
			mod_last = mod_base;

		u32 sz = params[cmv + 1].id - mod_base;
		if (sz == 0)
			continue;

		u32 mv_mode = ispc.DepthMode;

		verify(mod_base > 0 && mod_base + sz <= pvrrc.modtrig.used());

		if (mv_mode == 0)	//normal trigs
		{
			glcache.UseProgram(g_abuffer_tr_modvol_shader.program); glCheck();
			SetCull(ispc.CullMode); glCheck();
			glDrawArrays(GL_TRIANGLES, mod_base * 3, sz * 3); glCheck();
		}
		else if (mv_mode < 3)
		{
			while(sz)
			{
				//merge and clear all the prev. stencil bits

				//Count Intersections (last poly)
				glcache.UseProgram(g_abuffer_tr_modvol_shader.program); glCheck();
				SetCull(ispc.CullMode); glCheck();
				glDrawArrays(GL_TRIANGLES, mod_base * 3, 3); glCheck();

				//Sum the area
				glcache.UseProgram(g_abuffer_tr_modvol_final_shader.program); glCheck();

				glDrawArrays(GL_TRIANGLES, mod_last * 3, (mod_base - mod_last + 1) * 3); glCheck();

				//update pointers
				mod_last = mod_base + 1;
				sz--;
				mod_base++;
			}
		}
	}
}

void renderABuffer(bool sortFragments)
{
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, pixels_pointers);
	glCheck();

	glcache.UseProgram(sortFragments ? g_abuffer_final_shader.program : g_abuffer_final_nosort_shader.program);
	ShaderUniforms.Set(&g_abuffer_final_shader);

	glcache.Disable(GL_BLEND);
	glcache.Disable(GL_DEPTH_TEST);
	glcache.Disable(GL_CULL_FACE);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT|GL_BUFFER_UPDATE_BARRIER_BIT);
	DrawQuad();

	glCheck();

	glcache.UseProgram(g_abuffer_clear_shader.program);
	ShaderUniforms.Set(&g_abuffer_clear_shader);

	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT|GL_BUFFER_UPDATE_BARRIER_BIT);

//	GLuint size = 0;
//	glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, 4, &size);
//	printf("ABUFFER %d pixels used\n", size);
//	if ((size + 1) * 32 - 1 >= ABUFFER_SIZE)
//		printf("ABUFFER OVERRUN %d pixels\n", size);

	DrawQuad();

	glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, atomic_buffer);
	GLuint zero = 0;
 	glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0 , sizeof(GLuint), &zero);

	glActiveTexture(GL_TEXTURE0);
	glCheck();
}
