#pragma once
#include "types.h"
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

void ta_vtx_ListCont();
void ta_vtx_ListInit();
void ta_vtx_SoftReset();

void DYNACALL ta_vtx_data32(void* data);
void ta_vtx_data(u32* data, u32 size);

bool ta_parse_vdrc(TA_context* ctx);

class TaTypeLut
{
public:
	static const TaTypeLut& instance() {
		static TaTypeLut _instance;
		return _instance;
	}
	u32 table[256];

private:
	TaTypeLut();
};

#define STRIPS_AS_PPARAMS 1

#include "ta_ctx.h"
