/*
	This file is part of libswirl
*/
#include "license/bsd"


#pragma once
#include "types.h"

struct SuperH4;

void map_area0_init(SuperH4* sh4);
void map_area0(SuperH4* sh4, u32 base);