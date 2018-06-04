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
   TA_LIST_OPAQUE                  = 0,
   TA_LIST_OPAQUE_MODVOL,
   TA_LIST_TRANSLUCENT,
   TA_LIST_TRANSLUCENT_MODVOL,
   TA_LIST_PUNCH_THROUGH,
   TA_NUM_LISTS
};

enum
{
   /* Control Parameter */
   TA_PARAM_END_OF_LIST                     = 0,
   TA_PARAM_USER_TILE_CLIP,
   TA_PARAM_OBJ_LIST_SET,
   /* Reserved */
   TA_PARAM_RESERVED0,
   /* Global Parameter */
   TA_PARAM_POLY_OR_VOL,
   TA_PARAM_SPRITE,
   /* Reserved */
   TA_PARAM_RESERVED1,
   /* Vertex , Sprite or ModVolume Parameter */
   TA_PARAM_VERTEX,
   TA_NUM_PARAMS
};

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

#define STRIPS_AS_PPARAMS 1

#include "ta_ctx.h"

void ta_ctx_init(void);
void ta_ctx_free(void);
