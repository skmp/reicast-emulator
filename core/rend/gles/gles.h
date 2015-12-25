#pragma once
#include "rend/rend.h"

#ifdef DEBUG
#define glCheck() do { if (unlikely(settings.validate.OpenGlChecks)) { verify(glGetError()==GL_NO_ERROR); } } while(0)
#else
#define glCheck()
#endif

#define VERTEX_POS_ARRAY 0
#define VERTEX_COL_BASE_ARRAY 1
#define VERTEX_COL_OFFS_ARRAY 2
#define VERTEX_UV_ARRAY 3

//vertex types
extern u32 gcflip;

void DrawStrips();

GLuint gl_GetTexture(TSP tsp,TCW tcw);
struct text_info {
	u16* pdata;
	u32 width;
	u32 height;
	u32 textype; // 0 565, 1 1555, 2 4444
};

text_info raw_GetTexture(TSP tsp, TCW tcw);
void CollectCleanup();
void DoCleanup();
void SortPParams();

void BindRTT(u32 addy, u32 fbw, u32 fbh, u32 channels, u32 fmt);

bool CompilePipelineShader(void *data);
#define TEXTURE_LOAD_ERROR 0
GLuint loadPNG(const string& subpath, int &width, int &height);
