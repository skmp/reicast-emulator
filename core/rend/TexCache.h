#pragma once
#include "../types.h"

extern u8* vq_codebook;
extern u32 palette_index;
extern u32 palette_ram[1024];
extern bool pal_needs_update,fog_needs_update;
extern u32 pal_rev_256[4];
extern u32 pal_rev_16[64];
extern u32 _pal_rev_256[4];
extern u32 _pal_rev_16[64];

extern u32 detwiddle[2][8][1024];

struct PixelBuffer
{
	u16* p_buffer_start;
	u16* p_current_line;
	u16* p_current_pixel;

	u32 pixels_per_line;
};

void libCore_vramlock_Free(void);
void libCore_vramlock_Init(void);
void palette_update(void);

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


__forceinline u32 YUV422(s32 Y,s32 Yu,s32 Yv)
{
	Yu-=128;
	Yv-=128;

	//s32 B = (76283*(Y - 16) + 132252*(Yu - 128))>>16;
	//s32 G = (76283*(Y - 16) - 53281 *(Yv - 128) - 25624*(Yu - 128))>>16;
	//s32 R = (76283*(Y - 16) + 104595*(Yv - 128))>>16;
	
	s32 R = clamp(0, 255, Y + Yv*11/8);            // Y + (Yv-128) * (11/8) ?
	s32 G = clamp(0, 255, Y - (Yu*11 + Yv*22)/32); // Y - (Yu-128) * (11/8) * 0.25 - (Yv-128) * (11/8) * 0.5 ?
	s32 B = clamp(0, 255, Y + Yu*110/64);          // Y + (Yu-128) * (11/8) * 1.25 ?

   return ((R >> 3) << 11) | ((G >> 2) << 5) | ((B >> 3) << 0);
}

#define twop(x,y,bcx,bcy) (detwiddle[0][bcy][x]+detwiddle[1][bcx][y])

//pixel convertors !
#define pixelcvt_start(name,x,y) \
struct name \
{ \
	static const u32 xpp=x;\
	static const u32 ypp=y;	\
	__forceinline static void Convert(PixelBuffer* pb,u8* data) \
{

#define pixelcvt_end } }
#define pixelcvt_next(name,x,y) pixelcvt_end;  pixelcvt_start(name,x,y)
//Non twiddled
pixelcvt_start(conv565_PL,4,1)
{
	/* convert 4x1 */
	u16* p_in=(u16*)data;
   pb->p_current_pixel[0] = ARGB565(p_in[0]); /* 0,0 */
   pb->p_current_pixel[1] = ARGB565(p_in[1]); /* 1,0 */
   pb->p_current_pixel[2] = ARGB565(p_in[2]); /* 2,0 */
   pb->p_current_pixel[3] = ARGB565(p_in[3]); /* 3,0 */
}

pixelcvt_next(conv1555_PL,4,1)
{
	/* convert 4x1 */
	u16* p_in=(u16*)data;
   pb->p_current_pixel[0] = ARGB1555(p_in[0]); /* 0,0 */
   pb->p_current_pixel[1] = ARGB1555(p_in[1]); /* 1,0 */
   pb->p_current_pixel[2] = ARGB1555(p_in[2]); /* 2,0 */
   pb->p_current_pixel[3] = ARGB1555(p_in[3]); /* 3,0 */
}

pixelcvt_next(conv4444_PL,4,1)
{
	/* convert 4x1 */
	u16* p_in=(u16*)data;
	pb->p_current_pixel[0] = ARGB4444(p_in[0]); /* 0,0 */
	pb->p_current_pixel[1] = ARGB4444(p_in[1]); /* 1,0 */
	pb->p_current_pixel[2] = ARGB4444(p_in[2]); /* 2,0 */
	pb->p_current_pixel[3] = ARGB4444(p_in[3]); /* 3,0 */
}

pixelcvt_next(convYUV_PL,4,1)
{
	/* convert 4x1 4444 to 4x1 8888 */
	u32* p_in=(u32*)data;

	s32 Y0 = (p_in[0]>>8) &255; //
	s32 Yu = (p_in[0]>>0) &255; //p_in[0]
	s32 Y1 = (p_in[0]>>24) &255; //p_in[3]
	s32 Yv = (p_in[0]>>16) &255; //p_in[2]

   pb->p_current_pixel[0] = YUV422(Y0,Yu,Yv); /* 0,0 */
   pb->p_current_pixel[1] = YUV422(Y1,Yu,Yv); /* 1,0 */

	//next 4 bytes
	p_in+=1;

	Y0 = (p_in[0]>>8) &255; //
	Yu = (p_in[0]>>0) &255; //p_in[0]
	Y1 = (p_in[0]>>24) &255; //p_in[3]
	Yv = (p_in[0]>>16) &255; //p_in[2]

   pb->p_current_pixel[2] = YUV422(Y0,Yu,Yv); /* 0,0 */
   pb->p_current_pixel[3] = YUV422(Y1,Yu,Yv); /* 1,0 */
}

pixelcvt_next(convBMP_PL,4,1)
{
	u16* p_in=(u16*)data;
   pb->p_current_pixel[0] = ARGB8888(p_in[0]);
   pb->p_current_pixel[1] = ARGB8888(p_in[1]);
   pb->p_current_pixel[2] = ARGB8888(p_in[2]);
   pb->p_current_pixel[3] = ARGB8888(p_in[3]);
}

pixelcvt_end;
//twiddled 
pixelcvt_start(conv565_TW,2,2)
{
	/*convert 4x1 565 to 4x1 8888 */
	u16* p_in=(u16*)data;
   pb->p_current_pixel[0]                       = ARGB565(p_in[0]); /* 0,0 */
   pb->p_current_pixel[1*pb->pixels_per_line]   = ARGB565(p_in[1]); /* 0,1 */
   pb->p_current_pixel[1]                       = ARGB565(p_in[2]); /* 1,0 */
   pb->p_current_pixel[1*pb->pixels_per_line+1] = ARGB565(p_in[3]); /* 1,1 */
}
pixelcvt_next(conv1555_TW,2,2)
{
	/* convert 4x1 565 to 4x1 8888 */
	u16* p_in=(u16*)data;
   pb->p_current_pixel[0]                       = ARGB1555(p_in[0]); /* 0,0 */
   pb->p_current_pixel[1*pb->pixels_per_line]   = ARGB1555(p_in[1]); /* 0,1 */
   pb->p_current_pixel[1]                       = ARGB1555(p_in[2]); /* 1,0 */
   pb->p_current_pixel[1*pb->pixels_per_line+1] = ARGB1555(p_in[3]); /* 1,1 */
}
pixelcvt_next(conv4444_TW,2,2)
{
	/* convert 4x1 565 to 4x1 8888 */
	u16* p_in=(u16*)data;
   pb->p_current_pixel[0]                       = ARGB4444(p_in[0]); /* 0,0 */
   pb->p_current_pixel[1*pb->pixels_per_line]   = ARGB4444(p_in[1]); /* 0,1 */
   pb->p_current_pixel[1]                       = ARGB4444(p_in[2]); /* 1,0 */
   pb->p_current_pixel[1*pb->pixels_per_line+1] = ARGB4444(p_in[3]); /* 1,1 */
}

pixelcvt_next(convYUV_TW,2,2)
{
	//convert 4x1 4444 to 4x1 8888
	u16* p_in=(u16*)data;

	s32 Y0 = (p_in[0]>>8) &255; //
	s32 Yu = (p_in[0]>>0) &255; //p_in[0]
	s32 Y1 = (p_in[2]>>8) &255; //p_in[3]
	s32 Yv = (p_in[2]>>0) &255; //p_in[2]

   pb->p_current_pixel[0]= YUV422(Y0,Yu,Yv); /* 0,0 */
   pb->p_current_pixel[1]=YUV422(Y1,Yu,Yv);  /* 1,0 */

	//next 4 bytes
	//p_in+=2;

	Y0 = (p_in[1]>>8) &255; //
	Yu = (p_in[1]>>0) &255; //p_in[0]
	Y1 = (p_in[3]>>8) &255; //p_in[3]
	Yv = (p_in[3]>>0) &255; //p_in[2]

   pb->p_current_pixel[1*pb->pixels_per_line]=YUV422(Y0,Yu,Yv);   /* 0,1 */
   pb->p_current_pixel[1*pb->pixels_per_line+1]=YUV422(Y1,Yu,Yv); /* 1,1 */
}

pixelcvt_next(convBMP_TW,2,2)
{
	u16* p_in=(u16*)data;
   pb->p_current_pixel[0]                  =ARGB8888(p_in[0]);
   pb->p_current_pixel[1*pb->pixels_per_line]  =ARGB8888(p_in[1]);
   pb->p_current_pixel[1]                  =ARGB8888(p_in[2]);
   pb->p_current_pixel[1*pb->pixels_per_line+1]=ARGB8888(p_in[3]);
}

pixelcvt_end;

pixelcvt_start(convPAL4_TW,4,4)
{
	u8* p_in=(u8*)data;
	u32* pal=&palette_ram[palette_index];

   pb->p_current_pixel[0]=pal[p_in[0]&0xF];
   pb->p_current_pixel[1*pb->pixels_per_line]=pal[(p_in[0]>>4)&0xF];
   p_in++;
   pb->p_current_pixel[1]=pal[p_in[0]&0xF];
   pb->p_current_pixel[1*pb->pixels_per_line+1]=pal[(p_in[0]>>4)&0xF];
   p_in++;

   pb->p_current_pixel[2*pb->pixels_per_line]=pal[p_in[0]&0xF];
   pb->p_current_pixel[3*pb->pixels_per_line]=pal[(p_in[0]>>4)&0xF];
   p_in++;
   pb->p_current_pixel[2*pb->pixels_per_line+1]=pal[p_in[0]&0xF];
   pb->p_current_pixel[3*pb->pixels_per_line+1]=pal[(p_in[0]>>4)&0xF];
   p_in++;

   pb->p_current_pixel[2]=pal[p_in[0]&0xF];
   pb->p_current_pixel[1*pb->pixels_per_line+2]=pal[(p_in[0]>>4)&0xF];
   p_in++;
   pb->p_current_pixel[3]=pal[p_in[0]&0xF];
   pb->p_current_pixel[1*pb->pixels_per_line+3]=pal[(p_in[0]>>4)&0xF];
   p_in++;

   pb->p_current_pixel[2*pb->pixels_per_line+2]=pal[p_in[0]&0xF];
   pb->p_current_pixel[3*pb->pixels_per_line+2]=pal[(p_in[0]>>4)&0xF];
   p_in++;
   pb->p_current_pixel[2*pb->pixels_per_line+3]=pal[p_in[0]&0xF];
   pb->p_current_pixel[3*pb->pixels_per_line+3]=pal[(p_in[0]>>4)&0xF];
   p_in++;
}
pixelcvt_next(convPAL8_TW,2,4)
{
	u8* p_in=(u8*)data;
	u32* pal=&palette_ram[palette_index];

   pb->p_current_pixel[0]=pal[p_in[0]];
   p_in++;
   pb->p_current_pixel[1*pb->pixels_per_line]=pal[p_in[0]];
   p_in++;
   pb->p_current_pixel[1]=pal[p_in[0]];
   p_in++;
   pb->p_current_pixel[1*pb->pixels_per_line+1]=pal[p_in[0]];
   p_in++;

   pb->p_current_pixel[2*pb->pixels_per_line]=pal[p_in[0]];
   p_in++;
   pb->p_current_pixel[3*pb->pixels_per_line]=pal[p_in[0]];
   p_in++;
   pb->p_current_pixel[2*pb->pixels_per_line+1]=pal[p_in[0]];
   p_in++;
   pb->p_current_pixel[3*pb->pixels_per_line+1]=pal[p_in[0]];
   p_in++;
}
pixelcvt_end;
//handler functions
template<class PixelConvertor>
void texture_PL(PixelBuffer* pb,u8* p_in,u32 Width,u32 Height)
{
   pb->p_current_line  = pb->p_buffer_start;
   pb->p_current_pixel = pb->p_current_line;

	Width/=PixelConvertor::xpp;

	for (u32 y=0;y<Height;y++)
	{
		for (u32 x=0;x<Width;x++)
		{
			u8* p = p_in;
			PixelConvertor::Convert(pb,p);
			p_in+=8;

         pb->p_current_pixel += PixelConvertor::xpp;
		}
      pb->p_current_line += pb->pixels_per_line;
		pb->p_current_pixel = pb->p_current_line;
	}
}

template<class PixelConvertor>
void texture_TW(PixelBuffer* pb,u8* p_in,u32 Width,u32 Height)
{
   u32 bcx, bcy;
	unsigned long bcx_,bcy_;
	const u32 divider=PixelConvertor::xpp*PixelConvertor::ypp;

   pb->p_current_line  = pb->p_buffer_start;
   pb->p_current_pixel = pb->p_current_line;

	bcx_=bitscanrev(Width);
	bcy_=bitscanrev(Height);
	bcx=bcx_-3;
	bcy=bcy_-3;

	for (u32 y=0;y<Height;y+=PixelConvertor::ypp)
	{
		for (u32 x=0;x<Width;x+=PixelConvertor::xpp)
		{
			u8* p = &p_in[(twop(x,y,bcx,bcy)/divider)<<3];
			PixelConvertor::Convert(pb,p);

         pb->p_current_pixel += PixelConvertor::xpp;
		}

      pb->p_current_line += pb->pixels_per_line * PixelConvertor::ypp;
		pb->p_current_pixel = pb->p_current_line;
	}
}

template<class PixelConvertor>
void texture_VQ(PixelBuffer* pb,u8* p_in,u32 Width,u32 Height)
{
   u32 bcx, bcy;
   unsigned long bcx_,bcy_;
	const u32 divider=PixelConvertor::xpp*PixelConvertor::ypp;
	p_in+=256*4*2;

   pb->p_current_line  = pb->p_buffer_start;
   pb->p_current_pixel = pb->p_current_line;

	bcx_= bitscanrev(Width);
	bcy_= bitscanrev(Height);
	bcx = bcx_-3;
	bcy = bcy_-3;

	for (u32 y=0;y<Height;y+=PixelConvertor::ypp)
	{
		for (u32 x=0;x<Width;x+=PixelConvertor::xpp)
		{
#ifdef MSB_FIRST
			u8 p = p_in[(twop(x,y,bcx,bcy)/divider)^3];
#else
			u8 p = p_in[(twop(x,y,bcx,bcy)/divider)];
#endif
			PixelConvertor::Convert(pb,&vq_codebook[p*8]);

         pb->p_current_pixel += PixelConvertor::xpp;
		}

      pb->p_current_line += pb->pixels_per_line * PixelConvertor::ypp;
		pb->p_current_pixel = pb->p_current_line;
	}
}

//We ask the compiler to generate the templates here
//;)
//planar formats !
template void texture_PL<conv565_PL >(PixelBuffer* pb,u8* p_in,u32 Width,u32 Height);
template void texture_PL<conv1555_PL >(PixelBuffer* pb,u8* p_in,u32 Width,u32 Height);
template void texture_PL<conv4444_PL >(PixelBuffer* pb,u8* p_in,u32 Width,u32 Height);
template void texture_PL<convYUV_PL >(PixelBuffer* pb,u8* p_in,u32 Width,u32 Height);
template void texture_PL<convBMP_PL >(PixelBuffer* pb,u8* p_in,u32 Width,u32 Height);

//twiddled formats !
template void texture_TW<conv565_TW >(PixelBuffer* pb,u8* p_in,u32 Width,u32 Height);
template void texture_TW<conv1555_TW >(PixelBuffer* pb,u8* p_in,u32 Width,u32 Height);
template void texture_TW<conv4444_TW >(PixelBuffer* pb,u8* p_in,u32 Width,u32 Height);
template void texture_TW<convYUV_TW >(PixelBuffer* pb,u8* p_in,u32 Width,u32 Height);
template void texture_TW<convBMP_TW >(PixelBuffer* pb,u8* p_in,u32 Width,u32 Height);

template void texture_TW<convPAL4_TW >(PixelBuffer* pb,u8* p_in,u32 Width,u32 Height);
template void texture_TW<convPAL8_TW >(PixelBuffer* pb,u8* p_in,u32 Width,u32 Height);

//VQ formats !
template void texture_VQ<conv565_TW >(PixelBuffer* pb,u8* p_in,u32 Width,u32 Height);
template void texture_VQ<conv1555_TW >(PixelBuffer* pb,u8* p_in,u32 Width,u32 Height);
template void texture_VQ<conv4444_TW >(PixelBuffer* pb,u8* p_in,u32 Width,u32 Height);
template void texture_VQ<convYUV_TW >(PixelBuffer* pb,u8* p_in,u32 Width,u32 Height);
template void texture_VQ<convBMP_TW >(PixelBuffer* pb,u8* p_in,u32 Width,u32 Height);

//Planar
#define tex565_PL texture_PL<conv565_PL >
#define tex1555_PL texture_PL<conv1555_PL >
#define tex4444_PL texture_PL<conv4444_PL >
#define texYUV422_PL texture_PL<convYUV_PL >
#define texBMP_PL texture_PL<convBMP_PL >

//Twiddle
#define tex565_TW texture_TW<conv565_TW >
#define tex1555_TW texture_TW<conv1555_TW >
#define tex4444_TW texture_TW<conv4444_TW >
#define texYUV422_TW texture_TW<convYUV_TW >
#define texBMP_TW texture_TW<convBMP_TW >
#define texPAL4_TW texture_TW<convPAL4_TW >
#define texPAL8_TW  texture_TW<convPAL8_TW >

//VQ
#define tex565_VQ texture_VQ<conv565_TW >
#define tex1555_VQ texture_VQ<conv1555_TW >
#define tex4444_VQ texture_VQ<conv4444_TW >
#define texYUV422_VQ texture_VQ<convYUV_TW >
#define texBMP_VQ texture_VQ<convBMP_TW >
