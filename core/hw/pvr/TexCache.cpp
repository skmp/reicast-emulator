#include "../../types.h"
#include "TexCache.h"
#include "pvr_regs.h"

u8* vq_codebook;
u32 palette_index;
bool KillTex=false;

u32 decoded_colors[3][65536];
u32 detwiddle[2][8][1024];
//input : address in the yyyyyxxxxx format
//output : address in the xyxyxyxy format
//U : x resolution , V : y resolution
//twiddle works on 64b words

static u32 twiddle_slow(u32 x,u32 y,u32 x_sz,u32 y_sz)
{
	u32 rv=0;//low 2 bits are directly passed  -> needs some misc stuff to work.However
			 //Pvr internally maps the 64b banks "as if" they were twiddled :p

	u32 sh=0;
	x_sz>>=1;
	y_sz>>=1;
	while(x_sz!=0 || y_sz!=0)
	{
		if (y_sz)
		{
			u32 temp=y&1;
			rv|=temp<<sh;

			y_sz>>=1;
			y>>=1;
			sh++;
		}
		if (x_sz)
		{
			u32 temp=x&1;
			rv|=temp<<sh;

			x_sz>>=1;
			x>>=1;
			sh++;
		}
	}	
	return rv;
}

static void BuildTwiddleTables(void)
{
	for (u32 s=0;s<8;s++)
	{
		u32 x_sz=1024;
		u32 y_sz=8<<s;
		for (u32 i=0;i<x_sz;i++)
		{
			detwiddle[0][s][i]=twiddle_slow(i,0,x_sz,y_sz);
			detwiddle[1][s][i]=twiddle_slow(0,i,y_sz,x_sz);
		}
	}
}

static OnLoad btt(&BuildTwiddleTables);

void palette_update(void)
{
   if (pal_needs_update==false)
      return;
   memcpy(pal_rev_256,_pal_rev_256,sizeof(pal_rev_256));
   memcpy(pal_rev_16,_pal_rev_16,sizeof(pal_rev_16));

   pal_needs_update=false;
   switch(PAL_RAM_CTRL&3)
   {
      case 0:
         for (int i=0;i<1024;i++)
            palette_ram[i]=ARGB1555(PALETTE_RAM[i]);
         break;

      case 1:
         for (int i=0;i<1024;i++)
            palette_ram[i]=ARGB565(PALETTE_RAM[i]);
         break;

      case 2:
         for (int i=0;i<1024;i++)
            palette_ram[i]=ARGB4444(PALETTE_RAM[i]);
         break;

      case 3:
         for (int i=0;i<1024;i++)
            palette_ram[i]=ARGB8888(PALETTE_RAM[i]);
         break;
   }

}
