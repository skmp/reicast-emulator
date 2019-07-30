#pragma once

#include "types.h"
#include "gles.h"

void gles_dump_texture(u32 hash, int w, int h, GLuint textype, void* temp_tex_buffer);