/*
	PowerVR interface to plugins
	Handles YUV conversion (slow and ugly -- but hey it works ...)

	Most of this was hacked together when i needed support for YUV-dma for thps2 ;)
*/
#include "types.h"
#include "pvr_mem.h"
#include "ta.h"
#include "pvr_regs.h"
#include "Renderer_if.h"
#include "hw/mem/_vmem.h"

//TODO : move code later to a plugin
//TODO : Fix registers arrays , they must be smaller now doe to the way SB registers are handled
#include "types.h"
#include "hw/holly/holly.h"
#include "hw/sh4/sh4_mmr.h"
#include "hw/pvr/ta.h"

#define VRAM_BANK_BIT 0x400000

#define TA_YUV420_MACROBLOCK_SIZE 384
#define TA_YUV422_MACROBLOCK_SIZE 512

//YUV converter code :)
//inits the YUV converter
u32 YUV_tempdata[512/4];//512 bytes

u32 YUV_dest=0;

u32 YUV_blockcount;

u32 YUV_x_curr;
u32 YUV_y_curr;

u32 YUV_x_size;
u32 YUV_y_size;

void YUV_init(void)
{
   YUV_x_curr     = 0;
   YUV_y_curr     = 0;
   YUV_dest       = TA_YUV_TEX_BASE&VRAM_MASK;//TODO : add the masking needed
   TA_YUV_TEX_CNT = 0;
   YUV_blockcount = (((TA_YUV_TEX_CTRL>>0)&0x3F)+1)*(((TA_YUV_TEX_CTRL>>8)&0x3F)+1);
   YUV_x_size     = 16;
   YUV_y_size     = 16;

   if ((TA_YUV_TEX_CTRL>>16 )&1)
   {
      die ("YUV: Not supported configuration\n");
   }
   else // yesh!!!
   {
      YUV_x_size  = (((TA_YUV_TEX_CTRL>>0)&0x3F)+1)*16;
      YUV_y_size  = (((TA_YUV_TEX_CTRL>>8)&0x3F)+1)*16;
   }
}

static void ta_yuv_process_block(u8* in_uv,u8* in_y, u8* out_uyvy)
{
   unsigned y, x;
   u8* out_row0   = out_uyvy + 0;
   u8* out_row1   = out_uyvy + YUV_x_size * 2;

   /* reencode 8x8 subblock of YUV420 data as UYVY422 */
   for (y = 0;y < 8;y += 2)
   {
      for (x = 0;x < 8;x += 2)
      {
         u8 u          = in_uv[0];
         u8 v          = in_uv[64];

         out_row0[0]   = u;
         out_row0[1]   = in_y[0];
         out_row0[2]   = v;
         out_row0[3]   = in_y[1];

         out_row1[0]   = u;
         out_row1[1]   = in_y[8+0];
         out_row1[2]   = v;
         out_row1[3]   = in_y[8+1];

         in_uv        += 1;
         in_y         += 2;

         out_row0     += 4;
         out_row1     += 4;
      }
      in_y            += 8;
      in_uv           += 4;

      out_row0        += YUV_x_size*4-8*2;
      out_row1        += YUV_x_size*4-8*2;
   }
}

static INLINE void YUV_ConvertMacroBlock(u8* datap)
{
   /* YUV420 data comes in as a series of 16x16 
    * macroblocks that need to be converted into a single
    * UYVY422 texture */
   TA_YUV_TEX_CNT++;

   u8 *in     = (u8*)datap;
   u8 *out    = vram.data + YUV_dest;
   u8* inuv   = in;
   u8* iny    = in + 128;
   u8* p_out  = out;

   /* Process each 8x8 subblock individually */
   ta_yuv_process_block(inuv+ 0,iny+  0,p_out);                    /* (0,0) */
   ta_yuv_process_block(inuv+ 4,iny+64,p_out+8*2);                 /* (8,0) */
   ta_yuv_process_block(inuv+32,iny+128,p_out+YUV_x_size*8*2);     /* (0,8) */
   ta_yuv_process_block(inuv+36,iny+192,p_out+YUV_x_size*8*2+8*2); /* (8,8) */

   YUV_dest   += 32;
   YUV_x_curr += 16;

   if (YUV_x_curr == YUV_x_size)
   {
      YUV_dest     += YUV_x_size * 2 * 15;
      YUV_x_curr    = 0;
      YUV_y_curr   += 16;

      if (YUV_y_curr == YUV_y_size)
         YUV_y_curr = 0;
   }

   /* reset state once all macroblocks have been preprocessed */
   if (YUV_blockcount == TA_YUV_TEX_CNT)
   {
      YUV_init();

      /* raise DMA end interrupt */
      asic_RaiseInterruptWait(holly_YUV_DMA);
   }
}

static void YUV_data(u32* data , u32 count)
{
   if (YUV_blockcount==0)
   {
      die("YUV_data : YUV decoder not inited , *WATCH*\n");
      //wtf ? not inited
      YUV_init();
   }

   u32 block_size=(TA_YUV_TEX_CTRL & (1<<24))==0 ?
      TA_YUV420_MACROBLOCK_SIZE : TA_YUV422_MACROBLOCK_SIZE;

   count *= 32;

   while(count>=block_size)
   {
      YUV_ConvertMacroBlock((u8*)data); //convert block
      data  += block_size>>2;
      count -= block_size;
   }
}

//Regs

//vram 32-64b

//read
u8 DYNACALL pvr_read_area1_8(u32 addr)
{
   printf("8-bit VRAM reads are not possible\n");
   return 0;
}

u16 DYNACALL pvr_read_area1_16(u32 addr)
{
   return *(u16*)&vram.data[pvr_map32(addr) & VRAM_MASK];
}
u32 DYNACALL pvr_read_area1_32(u32 addr)
{
   return *(u32*)&vram.data[pvr_map32(addr) & VRAM_MASK];
}

//write
void DYNACALL pvr_write_area1_8(u32 addr,u8 data)
{
   printf("8-bit VRAM writes are not possible\n");
}

void DYNACALL pvr_write_area1_16(u32 addr,u16 data)
{
   *(u16*)&vram.data[pvr_map32(addr) & VRAM_MASK]=data;
}

void DYNACALL pvr_write_area1_32(u32 addr,u32 data)
{
   *(u32*)&vram.data[pvr_map32(addr) & VRAM_MASK] = data;
}

void TAWrite(u32 address,u32* data,u32 count)
{
   //printf("TAWrite 0x%08X %d\n",address,count);
   u32 address_w=address&0x1FFFFFF;//correct ?
   if (address_w<0x800000)//TA poly
   {
      ta_vtx_data(data,count);
   }
   else if(address_w<0x1000000) //Yuv Converter
   {
      YUV_data(data,count);
   }
   else //Vram Writef
   {
      //shouldn't really get here (?) -> works on dc :D need to handle lmmodes
      //printf("Vram Write 0x%X , size %d\n",address,count*32);
      memcpy(&vram.data[address & VRAM_MASK],data,count*32);
   }
}

static void NOINLINE MemWrite32(void* dst, void* src)
{
   memcpy((u64*)dst,(u64*)src,32);
}

#if HOST_CPU!=CPU_ARM
extern "C" void DYNACALL TAWriteSQ(u32 address,u8* sqb)
{
   u32 address_w=address&0x1FFFFFF;//correct ?
   u8* sq=&sqb[address&0x20];

   if (likely(address_w<0x800000))//TA poly
   {
      ta_vtx_data((u32*)sq, 1);
   }
   else if(likely(address_w<0x1000000)) //Yuv Converter
   {
      YUV_data((u32*)sq,1);
   }
   else //Vram Writef
   {
      //shouldn't really get here (?)
      //printf("Vram Write 0x%X , size %d\n",address,count*32);
      u8* vram=sqb + TA_YUV422_MACROBLOCK_SIZE + 0x04000000;
      MemWrite32(&vram[address_w&(VRAM_MASK-0x1F)],sq);
   }
}
#endif

//Misc interface

//Reset -> Reset - Initialise to default values
void pvr_Reset(bool Manual)
{
   if (!Manual)
      VArray2_Zero(&vram);
}

u32 pvr_map32(u32 offset32)
{
   //64b wide bus is achieved by interleaving the banks every 32 bits
   const u32 bank_bit    = VRAM_BANK_BIT;
   const u32 static_bits = VRAM_MASK - (VRAM_BANK_BIT * 2 - 1);
   const u32 offset_bits = VRAM_BANK_BIT - 1;
   u32 bank              = (offset32 & VRAM_BANK_BIT) / VRAM_BANK_BIT;
   u32 rv                = offset32 & static_bits;

   rv |= (offset32 & offset_bits) * 8;

   rv |= bank * 4;

   return rv;
}
