/*
	This file is part of libswirl
*/
#include "license/bsd"


#pragma once

#include "hw/holly/holly_intc.h"
#include "hw/sh4/sh4_if.h"
#include "oslib/oslib.h"
#include "ta_structs.h"

struct TA_context;
#if FEAT_TA == TA_HLE
void ta_init();
void ta_reset();
void ta_term();


void ta_vtx_ListCont();
void ta_vtx_ListInit();
void ta_vtx_SoftReset();

void DYNACALL ta_vtx_data32(void* data);
void ta_vtx_data(u32* data, u32 size);
#endif

struct Renderer;
bool ta_parse_vdrc(Renderer* renderer, u8* vram, TA_context* ctx);

#define TRIG_SORT 1
