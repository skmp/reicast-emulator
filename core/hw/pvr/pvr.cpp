#include "types.h"
#include "pvr.h"
#include "ta.h"
#include "hw/mem/_vmem.h"

#include "ta.h"
#include "hw/mem/_vmem.h"
#include "TexCache.h"

//TODO : move code later to a plugin
//TODO : Fix registers arrays , they must be smaller now doe to the way SB registers are handled
#include "hw/holly/holly.h"
#include "hw/sh4/sh4_mmr.h"
#include "hw/sh4/modules/dmac.h"
#include "hw/sh4/sh4_sched.h"
#include "hw/sh4/sh4_mem.h"

#include <rthreads/rthreads.h>

using namespace std;

vector<vram_block*> VramLocks[VRAM_SIZE/PAGE_SIZE];
//vram 32-64b
VArray2 vram;

extern bool pal_needs_update;
extern u32 _pal_rev_256[4];
extern u32 _pal_rev_16[64];
extern u32 pal_rev_256[4];
extern u32 pal_rev_16[64];
extern u32 palette_ram[1024];

bool fog_needs_update=true;

u8 pvr_regs[pvr_RegSize];

u32 in_vblank=0;
u32 clc_pvr_scanline;
u32 pvr_numscanlines=512;
u32 prv_cur_scanline=-1;
u32 vblk_cnt=0;

u32 Line_Cycles=0;
u32 Frame_Cycles=0;
static int render_end_sched;
static int vblank_sched;
int time_sync;

double speed_load_mspdf;

int mips_counter;

double full_rps;

u32 VertexCount=0;
u32 FrameCount=1;

Renderer* renderer;

#if !defined(TARGET_NO_THREADS)
cResetEvent rs;
cResetEvent re;
sthread_t *rthd;
#endif

bool pend_rend = false;

int max_idx,max_mvo,max_op,max_pt,max_tr,max_vtx,max_modt, ovrn;

TA_context* _pvrrc;

//SPG emulation; Scanline/Raster beam registers & interrupts
//Time to emulate that stuff correctly ;)

static void pvr_reconfigure_spg(void)
{
   /*                          00=VGA    01=NTSC   10=PAL,   11=illegal/undocumented */
   const int spg_clks[4]   = { 26944080, 13458568, 13462800, 26944080 };
	float scale_x           = 1;
   float scale_y           = 1;
	u32 pixel_clock         = spg_clks[(SPG_CONTROL.full >> 6) & 3];
	pvr_numscanlines        = SPG_LOAD.vcount+1;
	Line_Cycles             = (u32)((u64)SH4_MAIN_CLOCK*(u64)(SPG_LOAD.hcount+1)/(u64)pixel_clock);
	
	if (SPG_CONTROL.interlace)
	{
		//this is a temp hack
		Line_Cycles         /= 2;
		u32 interl_mode      = VO_CONTROL.field_mode;
		
		//if (interl_mode==2)//3 will be funny =P
		//  scale_y=0.5f;//single interlace
		//else
			scale_y=1;
	}
	else
	{
		if (FB_R_CTRL.vclk_div)
			scale_y           = 1.0f;//non interlaced VGA mode has full resolution :)
		else
			scale_y           = 0.5f;//non interlaced modes have half resolution
	}

	rend_set_fb_scale(scale_x,scale_y);
	
	Frame_Cycles            = pvr_numscanlines*Line_Cycles;
	prv_cur_scanline        = 0;

	sh4_sched_request(vblank_sched, Line_Cycles);
}

static int elapse_time(int tag, int cycl, int jit)
{
	return min(max(Frame_Cycles,(u32)1*1000*1000),(u32)8*1000*1000);
}

//called from sh4 context , should update pvr/ta state and everything else
static int spg_line_sched(int tag, int cycl, int jit)
{
	clc_pvr_scanline       += cycl;

	while (clc_pvr_scanline >=  Line_Cycles)//60 ~hertz = 200 mhz / 60=3333333.333 cycles per screen refresh
	{
		//ok .. here , after much effort , we did one line
		//now , we must check for raster beam interrupts and vblank
		prv_cur_scanline     = (prv_cur_scanline+1) % pvr_numscanlines;
		clc_pvr_scanline    -= Line_Cycles;
		//Check for scanline interrupts -- really need to test the scanline values
		
      /* Vblank in */
		if (SPG_VBLANK_INT.vblank_in_interrupt_line_number == prv_cur_scanline)
			asic_RaiseInterrupt(holly_SCANINT1);

      /* Vblank Out */
		if (SPG_VBLANK_INT.vblank_out_interrupt_line_number == prv_cur_scanline)
			asic_RaiseInterrupt(holly_SCANINT2);

		if (SPG_VBLANK.vstart == prv_cur_scanline)
			in_vblank = 1;

		if (SPG_VBLANK.vbend == prv_cur_scanline)
			in_vblank = 0;

		SPG_STATUS.vsync    = in_vblank;
		SPG_STATUS.scanline = prv_cur_scanline;
		
		//Vblank start -- really need to test the scanline values
		if (prv_cur_scanline==0)
		{
         SPG_STATUS.fieldnum = 0;
			if (SPG_CONTROL.interlace)
				SPG_STATUS.fieldnum = ~SPG_STATUS.fieldnum;

			/* Vblank counter */
			vblk_cnt++;

         /* HBlank in */
			asic_RaiseInterrupt(holly_HBLank);
         os_DoEvents();
		}
	}

	//interrupts
	//0
	//vblank_in_interrupt_line_number
	//vblank_out_interrupt_line_number
	//vstart
	//vbend
	//pvr_numscanlines
	u32 min_scanline=prv_cur_scanline+1;
	u32 min_active=pvr_numscanlines;

	if (min_scanline < SPG_VBLANK_INT.vblank_in_interrupt_line_number)
		min_active=min(min_active,SPG_VBLANK_INT.vblank_in_interrupt_line_number);

	if (min_scanline < SPG_VBLANK_INT.vblank_out_interrupt_line_number)
		min_active=min(min_active,SPG_VBLANK_INT.vblank_out_interrupt_line_number);

	if (min_scanline < SPG_VBLANK.vstart)
		min_active=min(min_active,SPG_VBLANK.vstart);

	if (min_scanline < SPG_VBLANK.vbend)
		min_active=min(min_active,SPG_VBLANK.vbend);

	if (min_scanline < pvr_numscanlines)
		min_active=min(min_active,pvr_numscanlines);

	min_active=max(min_active,min_scanline);

	return (min_active-prv_cur_scanline)*Line_Cycles;
}

static int rend_end_sch(int tag, int cycl, int jitt)
{
	asic_RaiseInterrupt(holly_RENDER_DONE);
	asic_RaiseInterrupt(holly_RENDER_DONE_isp);
	asic_RaiseInterrupt(holly_RENDER_DONE_vd);

   if (!settings.UpdateMode && !settings.UpdateModeForced)
      rend_end_render();

	return 0;
}

static void SetREP(TA_context* cntx)
{
   unsigned pending_cycles = 4096;
	if (cntx && !cntx->rend.Overrun)
	{
		pending_cycles  = cntx->rend.verts.used()*60;
		pending_cycles += 500000*3;
		VertexCount    += cntx->rend.verts.used();
	}

   sh4_sched_request(render_end_sched, pending_cycles);
}

void libPvr_LockedBlockWrite (vram_block* block,u32 addr)
{
	rend_text_invl(block);
}

void libPvr_Reset(bool Manual)
{
	ID                  = 0x17FD11DB;
	REVISION            = 0x00000011;
	SOFTRESET           = 0x00000007;
	SPG_HBLANK_INT.full = 0x031D0000;
	SPG_VBLANK_INT.full = 0x01500104;
	FPU_PARAM_CFG       = 0x0007DF77;
	HALF_OFFSET         = 0x00000007;
	ISP_FEED_CFG        = 0x00402000;
	SDRAM_REFRESH       = 0x00000020;
	SDRAM_ARB_CFG       = 0x0000001F;
	SDRAM_CFG           = 0x15F28997;
	SPG_HBLANK.full     = 0x007E0345;
	SPG_LOAD.full       = 0x01060359;
	SPG_VBLANK.full     = 0x01500104;
	SPG_WIDTH.full      = 0x07F1933F;
	VO_CONTROL.full     = 0x00000108;
	VO_STARTX.full      = 0x0000009D;
	VO_STARTY.full      = 0x00000015;
	SCALER_CTL.full     = 0x00000400;
	FB_BURSTCTRL        = 0x00090639;
	PT_ALPHA_REF        = 0x000000FF;

	pvr_reconfigure_spg();
	//rend_reset(); //*TODO* wtf ?
}

s32 libPvr_Init(void)
{
   ta_ctx_init();
   
	render_end_sched = sh4_sched_register(0,&rend_end_sch);
	vblank_sched     = sh4_sched_register(0,&spg_line_sched);
	time_sync        = sh4_sched_register(0,&elapse_time);

	sh4_sched_request(time_sync,8*1000*1000);

   //failed
	if (!rend_init())
		return rv_error;

	return rv_ok;
}

//called when exiting from sh4 thread , from the new thread context (for any thread specific de init) :P
void libPvr_Term(void)
{
	rend_term();
   ta_ctx_free();
}

//List functions
//
static void vramlock_list_remove(vram_block* block)
{
	u32 base = block->start/PAGE_SIZE;
	u32 end  = block->end/PAGE_SIZE;

	for (u32 i=base;i<=end;i++)
	{
		vector<vram_block*>* list=&VramLocks[i];
		for (size_t j=0;j<list->size();j++)
		{
			if ((*list)[j]==block)
				(*list)[j]=0;
		}
	}
}
 
static void vramlock_list_add(vram_block* block)
{
	u32 base = block->start/PAGE_SIZE;
	u32 end = block->end/PAGE_SIZE;


	for (u32 i=base;i<=end;i++)
	{
		vector<vram_block*>* list=&VramLocks[i];
		for (u32 j=0;j<list->size();j++)
		{
			if ((*list)[j]==0)
			{
				(*list)[j]=block;
				goto added_it;
			}
		}

		list->push_back(block);
added_it:
		i=i;
	}
}
 
#ifndef TARGET_NO_THREADS
slock_t *vramlist_lock;
#endif

//simple IsInRange test
static inline bool IsInRange(vram_block* block,u32 offset)
{
	return (block->start<=offset) && (block->end>=offset);
}


vram_block* libCore_vramlock_Lock(u32 start_offset64,u32 end_offset64,void* userdata)
{
	vram_block* block=(vram_block* )malloc(sizeof(vram_block));

	if (end_offset64>(VRAM_SIZE-1))
	{
		msgboxf("vramlock_Lock_64: end_offset64>(VRAM_SIZE-1) \n Tried to lock area out of vram , possibly bug on the pvr plugin",MBX_OK);
		end_offset64=(VRAM_SIZE-1);
	}

	if (start_offset64>end_offset64)
	{
		msgboxf("vramlock_Lock_64: start_offset64>end_offset64 \n Tried to lock negative block , possibly bug on the pvr plugin",MBX_OK);
		start_offset64=0;
	}

	

	block->end=end_offset64;
	block->start=start_offset64;
	block->len=end_offset64-start_offset64+1;
	block->userdata=userdata;
	block->type=64;

#ifndef TARGET_NO_THREADS
   slock_lock(vramlist_lock);
#endif

   VArray2_LockRegion(&vram, block->start,block->len);

   //TODO: Fix this for 32M wrap as well
   if (_nvmem_enabled() && VRAM_SIZE == 0x800000)
      VArray2_LockRegion(&vram, block->start + VRAM_SIZE, block->len);

   vramlock_list_add(block);

#ifndef TARGET_NO_THREADS
   slock_unlock(vramlist_lock);
#endif

	return block;
}


bool VramLockedWrite(u8* address)
{
   size_t offset=address-vram.data;

   if (offset<VRAM_SIZE)
   {

      size_t addr_hash = offset/PAGE_SIZE;
      vector<vram_block*>* list=&VramLocks[addr_hash];

#ifndef TARGET_NO_THREADS
      slock_lock(vramlist_lock);
#endif

      for (size_t i=0;i<list->size();i++)
      {
         if ((*list)[i])
         {
            libPvr_LockedBlockWrite((*list)[i],(u32)offset);

            if ((*list)[i])
            {
               msgboxf("Error : pvr is supposed to remove lock",MBX_OK);
               dbgbreak;
            }

         }
      }
      list->clear();

      VArray2_UnLockRegion(&vram, (u32)offset&(~(PAGE_SIZE-1)),PAGE_SIZE);

      //TODO: Fix this for 32M wrap as well
      if (_nvmem_enabled() && VRAM_SIZE == 0x800000)
         VArray2_UnLockRegion(&vram, (u32)offset&(~(PAGE_SIZE-1)) + VRAM_SIZE,PAGE_SIZE);

#ifndef TARGET_NO_THREADS
      slock_unlock(vramlist_lock);
#endif

      return true;
   }

   return false;
}

#ifdef TARGET_NO_THREADS
void libCore_vramlock_Free(void) { }
void libCore_vramlock_Init(void) { }

//unlocks mem
//also frees the handle
void libCore_vramlock_Unlock_block(vram_block* block)
{
	libCore_vramlock_Unlock_block_wb(block);
}
#else
void libCore_vramlock_Free(void)
{
   slock_free(vramlist_lock);
   vramlist_lock = NULL;
}

void libCore_vramlock_Init(void)
{
   vramlist_lock = slock_new();
}

//unlocks mem
//also frees the handle
void libCore_vramlock_Unlock_block(vram_block* block)
{
	slock_lock(vramlist_lock);
	libCore_vramlock_Unlock_block_wb(block);
	slock_unlock(vramlist_lock);
}
#endif

void libCore_vramlock_Unlock_block_wb(vram_block* block)
{
	if (block->end <= VRAM_SIZE)
	{
		vramlock_list_remove(block);
		//more work needed
		free(block);
	}
}

/*
	PowerVR interface to plugins
	Handles YUV conversion (slow and ugly -- but hey it works ...)

	Most of this was hacked together when i needed support for YUV-dma for thps2 ;)
*/

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

static void YUV_init(void)
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
      YUV_init();

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
      u8* vram  = sqb + TA_YUV422_MACROBLOCK_SIZE + 0x04000000;
      void *dst = &vram[address_w&(VRAM_MASK-0x1F)];
      void *src = sq;

      memcpy((u64*)dst,(u64*)src,32);
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

#if defined(TARGET_NO_THREADS)
static bool rend_frame(TA_context* ctx, bool draw_osd)
{
   return renderer->Process(ctx) && renderer->Render();
}

void rend_end_render(void)
{
   if (pend_rend)
      renderer->Present();
}

void rend_term(void) { }
#else
static bool rend_frame(TA_context* ctx, bool draw_osd)
{
   bool proc = renderer->Process(ctx);
   
   slock_lock(re.mutx);
   re.state = true;
   scond_signal(re.cond);
   slock_unlock(re.mutx);

   return proc && renderer->Render();
}

void rend_end_render(void)
{
   if (pend_rend)
   {
      slock_lock(re.mutx);
      if (!re.state)
         scond_wait( re.cond, re.mutx );
      re.state=false;
      slock_unlock(re.mutx);
   }
}

void rend_term(void)
{
   sthread_join(rthd);
   rthd = NULL;

   slock_free(re.mutx);
   slock_free(rs.mutx);
   scond_free(re.cond);
   scond_free(rs.cond);
   re.mutx = NULL;
   rs.mutx = NULL;
   re.cond = NULL;
   rs.cond = NULL;
}
#endif

static bool rend_single_frame(void)
{
   //wait render start only if no frame pending
   do
   {
#if !defined(TARGET_NO_THREADS)
      slock_lock(rs.mutx);
      if (!rs.state)
         scond_wait( rs.cond, rs.mutx );
      rs.state=false;
      slock_unlock(rs.mutx);
#endif
      _pvrrc = DequeueRender();
   }
   while (!_pvrrc);
   bool do_swp = rend_frame(_pvrrc, true);

   //clear up & free data ..
   FinishRender(_pvrrc);
   _pvrrc=0;

   return do_swp;
}

static void rend_start_render(void)
{
   pend_rend = false;
   TA_context* ctx = tactx_Pop(CORE_CURRENT_CTX);

   SetREP(ctx);

   if (!ctx)
      return;

   if (ctx->rend.Overrun)
   {
      ovrn++;
      printf("WARNING: Rendering context is overrun (%d), aborting frame\n",ovrn);
      tactx_Recycle(ctx);
      return;
   }

   //printf("REP: %.2f ms\n",render_end_pending_cycles/200000.0);
   FillBGP(ctx);

   ctx->rend.isRTT      = (FB_W_SOF1& 0x1000000)!=0;
   ctx->rend.isAutoSort = UsingAutoSort();

   ctx->rend.fb_X_CLIP  = FB_X_CLIP;
   ctx->rend.fb_Y_CLIP  = FB_Y_CLIP;

   max_idx              = max(max_idx,  ctx->rend.idx.used());
   max_vtx              = max(max_vtx,  ctx->rend.verts.used());
   max_op               = max(max_op,   ctx->rend.global_param_op.used());
   max_pt               = max(max_pt,   ctx->rend.global_param_pt.used());
   max_tr               = max(max_tr,   ctx->rend.global_param_tr.used());

   max_mvo              = max(max_mvo,  ctx->rend.global_param_mvo.used());
   max_modt             = max(max_modt, ctx->rend.modtrig.used());

   if (QueueRender(ctx) || !settings.QueueRender)
   {
      palette_update();
#if !defined(TARGET_NO_THREADS)
      slock_lock(rs.mutx);
      rs.state=true;
      scond_signal(rs.cond);
      slock_unlock(rs.mutx);
#else
      rend_single_frame();
#endif
      pend_rend = true;
   }
}

void pvr_WriteReg(u32 paddr,u32 data)
{
	u32 addr=paddr&pvr_RegMask;

   switch (addr)
   {
      case ID_addr: /* read-only */
      case REVISION_addr:
      case TA_YUV_TEX_CNT_addr:
         return;
      case STARTRENDER_addr:
         //start render
         rend_start_render();
         return;
      case TA_LIST_INIT_addr:
         if (data>>31)
         {
            ta_vtx_ListInit();
            data=0;
         }
         break;
      case SOFTRESET_addr:
         if (data!=0)
         {
            if (data & 1)
               ta_vtx_SoftReset();
            data=0;
         }
         break;
      case TA_LIST_CONT_addr:
         //a write of anything works ?
         ta_vtx_ListCont();
         break;
      case FB_R_CTRL_addr:
      case SPG_CONTROL_addr:
      case SPG_LOAD_addr:
         PvrReg(addr,u32)=data;
         pvr_reconfigure_spg();
         return;
      case TA_YUV_TEX_BASE_addr:
         YUV_init();
         break;
	}

	if (addr>=PALETTE_RAM_START_addr)
	{
		if (PvrReg(addr,u32)!=data)
		{
			u32 pal=(addr/4)&1023;

			pal_needs_update=true;
			_pal_rev_256[pal>>8]++;
			_pal_rev_16[pal>>4]++;
		}
	}

	if (  addr >= FOG_TABLE_START_addr && 
         addr <= FOG_TABLE_END_addr   &&
         PvrReg(addr,u32) != data)
		fog_needs_update=true;

	PvrReg(addr,u32)=data;
}


/*
	PVR-SB handling
	DMA hacks are here
*/

/* PVR-DMA */
static void do_pvr_dma(void)
{
	u32 chcr   = DMAC_CHCR(0).full;
	u32 dmaor  = DMAC_DMAOR.full;
	u32 dmatcr = DMAC_DMATCR(0);

	u32 src    = SB_PDSTAR;
	u32 dst    = SB_PDSTAP;
	u32 len    = SB_PDLEN;

	if((dmaor &DMAOR_MASK) != 0x8201)
	{
		printf("\n!\tDMAC: DMAOR has invalid settings (%X) !\n", dmaor);
		return;
	}

	if (len & 0x1F)
	{
		printf("\n!\tDMAC: SB_C2DLEN has invalid size (%X) !\n", len);
		return;
	}

	if (SB_PDDIR)
	{
		//PVR -> System
		WriteMemBlock_nommu_dma(dst,src,len);
	}
	else
	{
		//System -> PVR
		//TODO : FIX THAT , to warp around on dmas :)
		WriteMemBlock_nommu_dma(dst,src,len);
	}

	DMAC_SAR(0)        = (src + len);
	DMAC_CHCR(0).full &= 0xFFFFFFFE;
	DMAC_DMATCR(0)     = 0x00000000;

	SB_PDST            = 0x00000000;

	//TODO : *CHECKME* is that ok here ? the docs don't say here it's used [PVR-DMA , bit 11]
	asic_RaiseInterruptWait(holly_PVR_DMA);
}

static u32 calculate_start_link_addr(void)
{
	u32 rv;
	u8* base=&mem_b.data[SB_SDSTAW & RAM_MASK];

	if (SB_SDWLT==0) /* 16b width */
		rv=((u16*)base)[SB_SDDIV];
	else /* 32b width */
		rv=((u32*)base)[SB_SDDIV];

	SB_SDDIV++; //next index

	return rv;
}

static void pvr_do_sort_dma(void)
{
	SB_SDDIV           = 0; //index is 0 now :)
	u32 link_addr      = calculate_start_link_addr();
	u32 link_base_addr = SB_SDBAAW;

	while (link_addr!=1)
	{
		if (SB_SDLAS==1)
			link_addr   *= 32;

		u32 ea          = (link_base_addr+link_addr) & RAM_MASK;
		u32* ea_ptr     = (u32*)&mem_b.data[ea];
		link_addr       = ea_ptr[0x1C>>2];//Next link

		/* transfer global param */
		ta_vtx_data(ea_ptr,ea_ptr[0x18>>2]);
		if (link_addr==2)
			link_addr    = calculate_start_link_addr();
	}

	// End of DMA :)
	SB_SDST            = 0;
	asic_RaiseInterruptWait(holly_PVR_SortDMA);
}

static void RegWrite_SB_PDST(u32 addr, u32 data)
{
	if (data & 1)
	{
		SB_PDST=1;
		do_pvr_dma();
	}
}

static void RegWrite_SB_C2DST(u32 addr, u32 data)
{
	if(data & 1)
	{
		SB_C2DST=1;
		DMAC_Ch2St();
	}
}

// Auto sort DMA :|
static void RegWrite_SB_SDST(u32 addr, u32 data)
{
	if(data & 1)
		pvr_do_sort_dma();
}


//Init/Term , global
void pvr_sb_Init(void)
{
	//0x005F7C18    SB_PDST RW  PVR-DMA start
	sb_rio_register(SB_PDST_addr,RIO_WF,0,&RegWrite_SB_PDST);

	//0x005F6808    SB_C2DST RW  ch2-DMA start 
	sb_rio_register(SB_C2DST_addr,RIO_WF,0,&RegWrite_SB_C2DST);

	//0x005F6820    SB_SDST RW  Sort-DMA start
	sb_rio_register(SB_SDST_addr,RIO_WF,0,&RegWrite_SB_SDST);
}

void pvr_sb_Term(void)
{
}

//Reset -> Reset - Initialise
void pvr_sb_Reset(bool Manual)
{
}


/*

	rendv3 ideas
	- multiple backends
	  - ESish
	    - OpenGL ES2.0
	    - OpenGL ES3.0
	    - OpenGL 3.1
	  - OpenGL 4.x
	  - Direct3D 10+ ?
	- correct memory ordering model
	- resource pools
	- threaded TA
	- threaded rendering
	- RTTs
	- framebuffers
	- overlays


	PHASES
	- TA submission (memops, dma)

	- TA parsing (defered, rend thread)

	- CORE render (in-order, defered, rend thread)


	submission is done in-order
	- Partial handling of TA values
	- Gotchas with TA contexts

	parsing is done on demand and out-of-order, and might be skipped
	- output is only consumed by renderer

	render is queued on RENDER_START, and won't stall the emulation or might be skipped
	- VRAM integrity is an issue with out-of-order or delayed rendering.
	- selective vram snapshots require TA parsing to complete in order with REND_START / REND_END


	Complications
	- For some apis (gles2, maybe gl31) texture allocation needs to happen on the gpu thread
	- multiple versions of different time snapshots of the same texture are required
	- TA parsing vs frameskip logic


	Texture versioning and staging
	 A memory copy of the texture can be used to temporary store the texture before upload to vram
	 This can be moved to another thread
	 If the api supports async resource creation, we don't need the extra copy
	 Texcache lookups need to be versioned


	rendv2x hacks
	- Only a single pending render. Any renders while still pending are dropped (before parsing)
	- wait and block for parse/texcache. Render is async
*/

static void *rend_thread(void* p)
{
#if SET_AFNT
   cpu_set_t mask;

   /* CPU_ZERO initializes all the bits in the mask to zero. */
   CPU_ZERO( &mask );
   /* CPU_SET sets only the bit corresponding to cpu. */
   CPU_SET( 1, &mask );

   /* sched_setaffinity returns 0 in success */

   if( sched_setaffinity( 0, sizeof(mask), &mask ) == -1 )
      printf("WARNING: Could not set CPU Affinity, continuing...\n");
#endif

   if (!renderer->Init())
      die("rend->init() failed\n");

   //we don't know if this is true, so let's not speculate here
   //renderer->Resize(640, 480);

   for(;;)
   {
      if (rend_single_frame())
         renderer->Present();
   }
}


void rend_resize(int width, int height)
{
	renderer->Resize(width, height);
}



extern int screen_width;
extern int screen_height;

bool rend_init(void)
{
#ifdef NO_REND
	renderer	 = rend_norend();
#else
	renderer = rend_GLES2();
#endif

#if !defined(TARGET_NO_THREADS)
   rthd = (sthread_t*)sthread_create(rend_thread, 0);

   rs.mutx = slock_new();
   rs.cond = scond_new();
   re.mutx = slock_new();
   re.cond = scond_new();
#else
   if (!renderer->Init()) die("rend->init() failed\n");

   renderer->Resize(screen_width, screen_height);
#endif

#if SET_AFNT
	cpu_set_t mask;

	/* CPU_ZERO initializes all the bits in the mask to zero. */
	CPU_ZERO( &mask );
	/* CPU_SET sets only the bit corresponding to cpu. */
	CPU_SET( 0, &mask );

	/* sched_setaffinity returns 0 in success */

	if( sched_setaffinity( 0, sizeof(mask), &mask ) == -1 )
		printf("WARNING: Could not set CPU Affinity, continuing...\n");
#endif

	return true;
}
