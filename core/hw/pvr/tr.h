#pragma once

extern u8* vq_codebook;
extern u32 palette_index;
extern u32 palette_ram[1024];
extern bool pal_needs_update,fog_needs_update;
extern u32 pal_rev_256[4];
extern u32 pal_rev_16[64];
extern u32 _pal_rev_256[4];
extern u32 _pal_rev_16[64];

extern u32 detwiddle[2][8][1024];

#define clamp(minv,maxv,x) min(maxv,max(minv,x))

#define ARGB1555( word )	( ((word>>15)&1) | (((word>>10) & 0x1F)<<11)  | (((word>>5) & 0x1F)<<6)  | (((word>>0) & 0x1F)<<1) )
//	ARGB8888(unpack_1_to_8[(word>>15)&1],unpack_5_to_8[(word>>10) & 0x1F],	
//unpack_5_to_8[(word>>5) & 0x1F],unpack_5_to_8[word&0x1F])

#define ARGB565( word )	( (((word>>0)&0x1F)<<0) | (((word>>5)&0x3F)<<5) | (((word>>11)&0x1F)<<11) )
	
//ARGB8888(0xFF,unpack_5_to_8[(word>>11) & 0x1F],	unpack_6_to_8[(word>>5) & 0x3F],unpack_5_to_8[word&0x1F])
//( 0xFF000000 | unpack_5_to_8[(word>>11) & 0x1F] | unpack_5_to_8[(word>>5) & 0x3F]<<8 | unpack_5_to_8[word&0x1F]<<16 )

#define ARGB4444( word ) ( (((word>>0)&0xF)<<4) | (((word>>4)&0xF)<<8) | (((word>>8)&0xF)<<12) | (((word>>12)&0xF)<<0) )
//ARGB8888( (word&0xF000)>>(12-4),(word&0xF00)>>(8-4),(word&0xF0)>>(4-4),(word&0xF)<<4 )

#define ARGB8888( word ) ( (((word>>4)&0xF)<<4) | (((word>>12)&0xF)<<8) | (((word>>20)&0xF)<<12) | (((word>>28)&0xF)<<0) )

void palette_update(void);
