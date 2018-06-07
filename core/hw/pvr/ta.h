#pragma once
#include "pvr.h"
#include "ta.h"

#include "helper_classes.h"

#include "hw/holly/holly.h"
#include "hw/sh4/sh4_if.h"

#ifndef TARGET_NO_THREADS
#include <rthreads/rthreads.h>
#endif

#include "ta_structs.h"

enum
{
   TA_PIXEL_1555 = 0,
   TA_PIXEL_565,
   TA_PIXEL_4444,
   TA_PIXEL_YUV422,
   TA_PIXEL_BUMPMAP,
   TA_PIXEL_4BPP,
   TA_PIXEL_8BPP,
   TA_PIXEL_RESERVED
};

enum
{
   TA_PAL_ARGB1555 = 0,
   TA_PAL_RGB565,
   TA_PAL_ARGB4444,
   TA_PAL_ARGB8888
};

struct TA_context;

void ta_init();
void ta_reset();
void ta_term();


void ta_vtx_ListCont();
void ta_vtx_ListInit();
void ta_vtx_SoftReset();

void DYNACALL ta_vtx_data32(void* data);
void ta_vtx_data(u32* data, u32 size);

bool ta_parse_vdrc(TA_context* ctx);

#ifdef HAVE_OIT
#define STRIPS_AS_PPARAMS 0
#else
#define STRIPS_AS_PPARAMS 1
#endif

#include "ta_ctx.h"

void ta_ctx_init(void);
void ta_ctx_free(void);
