#ifndef PVR_TYPES_H
#define PVR_TYPES_H

#include <stdint.h>

union fb_r_ctrl
{
	uint32_t full;
	struct
	{
#ifdef MSB_FIRST
		uint32_t reserved1           : 8; //31-24
		uint32_t vclk_div            : 1; //23
		uint32_t fb_strip_buf_en     : 1; //22
		uint32_t fb_stripsize        : 6; //21-16
		uint32_t fb_chroma_threshold : 8; //15-8
		uint32_t R                   : 1; //7
		uint32_t fb_concat           : 3; //6-4
		uint32_t fb_depth            : 2; //3-2
		uint32_t fb_line_double      : 1; //1
		uint32_t fb_enable           : 1; //0
#else
		uint32_t fb_enable           : 1; //0
		uint32_t fb_line_double      : 1; //1
		uint32_t fb_depth            : 2; //3-2
		uint32_t fb_concat           : 3; //6-4
		uint32_t R                   : 1; //7
		uint32_t fb_chroma_threshold : 8; //15-8
		uint32_t fb_stripsize        : 6; //21-16
		uint32_t fb_strip_buf_en     : 1; //22
		uint32_t vclk_div            : 1; //23
		uint32_t reserved1           : 8; //31-24
#endif
	};
};

union fb_w_ctrl
{
	uint32_t full;
	struct
	{
#ifdef MSB_FIRST
		uint32_t reserved1          : 8;
		uint32_t fb_alpha_threshold : 8;
		uint32_t fb_kval            : 8;
		uint32_t pad0               : 4;
		uint32_t fb_dither          : 1;
		uint32_t fb_packmode        : 3;
#else
		uint32_t fb_packmode        : 3;
		uint32_t fb_dither          : 1;
		uint32_t pad0               : 4;
		uint32_t fb_kval            : 8;
		uint32_t fb_alpha_threshold : 8;
		uint32_t reserved1          : 8;
#endif
	};
};

union isp_backgnd_t
{
	uint32_t full;
	struct
	{
#ifdef MSB_FIRST
		uint32_t cache_bypass : 1;
		uint32_t shadow       : 1;
		uint32_t skip         : 3;
		uint32_t tag_address  : 21;
		uint32_t tag_offset   : 3;
#else
		uint32_t tag_offset   : 3;
		uint32_t tag_address  : 21;
		uint32_t skip         : 3;
		uint32_t shadow       : 1;
		uint32_t cache_bypass : 1;
#endif
	};
};

#endif
