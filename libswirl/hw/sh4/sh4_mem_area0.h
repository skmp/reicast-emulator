#pragma once
#include "types.h"

#include "SuperH4_impl.h"

void map_area0_init();
void map_area0(u32 base);

//Init/Res/Term
bool sh4_area0_Init(SuperH4_impl* sh4);
void sh4_area0_Reset(SuperH4_impl* sh4, bool Manual);
void sh4_area0_Term(SuperH4_impl* sh4);