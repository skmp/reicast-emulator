/*
	This file is part of libswirl
*/
#include "license/bsd"


#include "pvr_yuv.h"
#include "pvr_regs.h"
#include "hw/holly/holly_intc.h"

//YUV converter code :)
//inits the YUV converter
u32 YUV_tempdata[512 / 4];//512 bytes

u32 YUV_dest = 0;

u32 YUV_blockcount;

u32 YUV_x_curr;
u32 YUV_y_curr;

u32 YUV_x_size;
u32 YUV_y_size;

static u32 YUV_index = 0;

static ASIC* asic;

void YUV_init(ASIC* asic)
{
	::asic = asic;
	YUV_x_curr = 0;
	YUV_y_curr = 0;

	YUV_dest = TA_YUV_TEX_BASE & VRAM_MASK;//TODO : add the masking needed
	TA_YUV_TEX_CNT = 0;
	YUV_blockcount = (TA_YUV_TEX_CTRL.yuv_u_size + 1) * (TA_YUV_TEX_CTRL.yuv_v_size + 1);

	if (TA_YUV_TEX_CTRL.yuv_tex != 0)
	{
		die("YUV: Not supported configuration\n");
		YUV_x_size = 16;
		YUV_y_size = 16;
	}
	else // yesh!!!
	{
		YUV_x_size = (TA_YUV_TEX_CTRL.yuv_u_size + 1) * 16;
		YUV_y_size = (TA_YUV_TEX_CTRL.yuv_v_size + 1) * 16;
	}
	YUV_index = 0;
}


INLINE u8 GetY420(int x, int y, u8* base)
{
	//u32 base=0;
	if (x > 7)
	{
		x -= 8;
		base += 64;
	}

	if (y > 7)
	{
		y -= 8;
		base += 128;
	}

	return base[x + y * 8];
}

INLINE u8 GetUV420(int x, int y, u8* base)
{
	int realx = x >> 1;
	int realy = y >> 1;

	return base[realx + realy * 8];
}

void YUV_Block8x8(u8* inuv, u8* iny, u8* out)
{
	u8* line_out_0 = out + 0;
	u8* line_out_1 = out + YUV_x_size * 2;

	for (int y = 0; y < 8; y += 2)
	{
		for (int x = 0; x < 8; x += 2)
		{
			u8 u = inuv[0];
			u8 v = inuv[64];

			line_out_0[0] = u;
			line_out_0[1] = iny[0];
			line_out_0[2] = v;
			line_out_0[3] = iny[1];

			line_out_1[0] = u;
			line_out_1[1] = iny[8 + 0];
			line_out_1[2] = v;
			line_out_1[3] = iny[8 + 1];

			inuv += 1;
			iny += 2;

			line_out_0 += 4;
			line_out_1 += 4;
		}
		iny += 8;
		inuv += 4;

		line_out_0 += YUV_x_size * 4 - 8 * 2;
		line_out_1 += YUV_x_size * 4 - 8 * 2;
	}
}

INLINE void YUV_Block384(u8* in, u8* out)
{
	u8* inuv = in;
	u8* iny = in + 128;
	u8* p_out = out;

	YUV_Block8x8(inuv + 0, iny + 0, p_out);                    //(0,0)
	YUV_Block8x8(inuv + 4, iny + 64, p_out + 8 * 2);                 //(8,0)
	YUV_Block8x8(inuv + 32, iny + 128, p_out + YUV_x_size * 8 * 2);     //(0,8)
	YUV_Block8x8(inuv + 36, iny + 192, p_out + YUV_x_size * 8 * 2 + 8 * 2); //(8,8)
}

INLINE void YUV_ConvertMacroBlock(u8* vram, u8* datap)
{
	//do shit
	TA_YUV_TEX_CNT++;

	YUV_Block384((u8*)datap, vram + YUV_dest);

	YUV_dest += 32;

	YUV_x_curr += 16;
	if (YUV_x_curr == YUV_x_size)
	{
		YUV_dest += 15 * YUV_x_size * 2;
		YUV_x_curr = 0;
		YUV_y_curr += 16;
		if (YUV_y_curr == YUV_y_size)
		{
			YUV_y_curr = 0;
		}
	}

	if (YUV_blockcount == TA_YUV_TEX_CNT)
	{
		YUV_init(asic);

		asic->RaiseInterrupt(holly_YUV_DMA);
	}
}

void YUV_data(u32* data, u32 count, u8* vram)
{
	if (YUV_blockcount == 0)
	{
		die("YUV_data : YUV decoder not inited , *WATCH*\n");
		//wtf ? not inited
		YUV_init(asic);
	}

	u32 block_size = TA_YUV_TEX_CTRL.yuv_form == 0 ? 384 : 512;

	verify(block_size == 384); //no support for 512

	count *= 32;

	while (count > 0)
	{
		if (YUV_index + count >= block_size)
		{
			//more or exactly one block remaining
			u32 dr = block_size - YUV_index;				//remaining bytes til block end
			if (YUV_index == 0)
			{
				// Avoid copy
				YUV_ConvertMacroBlock(vram, (u8*)data);				//convert block
			}
			else
			{
				memcpy(&YUV_tempdata[YUV_index >> 2], data, dr);//copy em
				YUV_ConvertMacroBlock(vram, (u8*)&YUV_tempdata[0]);	//convert block
				YUV_index = 0;
			}
			data += dr >> 2;									//count em
			count -= dr;
		}
		else
		{	//less that a whole block remaining
			memcpy(&YUV_tempdata[YUV_index >> 2], data, count);	//append it
			YUV_index += count;
			count = 0;
		}
	}

	verify(count == 0);
}
