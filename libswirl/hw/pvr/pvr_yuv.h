/*
	This file is part of libswirl
*/
#include "license/bsd"


#pragma once
#include "types.h"

struct ASIC;
void YUV_init(ASIC* asic);
void YUV_data(u32* data, u32 count, u8* vram);