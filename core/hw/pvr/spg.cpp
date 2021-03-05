#include "spg.h"
#include "Renderer_if.h"
#include "pvr_regs.h"
#include "hw/holly/holly_intc.h"
#include "hw/holly/sb.h"
#include "hw/sh4/sh4_sched.h"

u32 in_vblank;
u32 clc_pvr_scanline;
static u32 pvr_numscanlines = 512;
static u32 pvr_cur_scanline = -1;
static u32 vblk_cnt;

#define PIXEL_CLOCK (54*1000*1000/2)

static u32 Line_Cycles;
int render_end_schid;
int vblank_schid;

static u32 lightgun_line = 0xffff;
static u32 lightgun_hpos;
static bool maple_int_pending;

void CalculateSync(void)
{
	const u32 pixel_clock = PIXEL_CLOCK / (FB_R_CTRL.vclk_div ? 1 : 2);

	//We need to calculate the pixel clock
	pvr_numscanlines = SPG_LOAD.vcount + 1;

	Line_Cycles = (u32)((u64)SH4_MAIN_CLOCK * (u64)(SPG_LOAD.hcount + 1) / (u64)pixel_clock);

	float scale_x = 1.f, scale_y = 1.f;

	if (SPG_CONTROL.interlace)
	{
		//this is a temp hack
		Line_Cycles /= 2;

		//u32 interl_mode      = VO_CONTROL.field_mode;
		
		//if (interl_mode==2)//3 will be funny =P
		//  scale_y=0.5f;//single interlace
		//else
			scale_y = 1.f;
	}
	else
	{
		if (FB_R_CTRL.vclk_div)
			scale_y           = 1.0f;//non interlaced VGA mode has full resolution :)
		else
			scale_y           = 0.5f;//non interlaced modes have half resolution
	}

	rend_set_fb_scale(scale_x, scale_y);

	pvr_cur_scanline = 0;

	sh4_sched_request(vblank_schid, Line_Cycles);
}


//called from sh4 context , should update pvr/ta state and everything else
int spg_line_sched(int tag, int cycl, int jit)
{
	clc_pvr_scanline       += cycl;

	while (clc_pvr_scanline >=  Line_Cycles)//60 ~hertz = 200 mhz / 60=3333333.333 cycles per screen refresh
	{
		//ok .. here , after much effort , we did one line
		//now , we must check for raster beam interrupts and vblank
		pvr_cur_scanline = (pvr_cur_scanline + 1) % pvr_numscanlines;
		clc_pvr_scanline -= Line_Cycles;
		//Check for scanline interrupts -- really need to test the scanline values
		
      /* Vblank in */
		if (SPG_VBLANK_INT.vblank_in_interrupt_line_number == pvr_cur_scanline)
      {
         if (maple_int_pending)
         {
            maple_int_pending = false;
            SB_MDST = 0;
            asic_RaiseInterrupt(holly_MAPLE_DMA);
         }

         asic_RaiseInterrupt(holly_SCANINT1);
      }

      /* Vblank Out */
		if (SPG_VBLANK_INT.vblank_out_interrupt_line_number == pvr_cur_scanline)
			asic_RaiseInterrupt(holly_SCANINT2);

		if (SPG_VBLANK.vstart == pvr_cur_scanline)
			in_vblank = 1;

		if (SPG_VBLANK.vbend == pvr_cur_scanline)
			in_vblank = 0;

		SPG_STATUS.vsync    = in_vblank;
		SPG_STATUS.scanline = pvr_cur_scanline;
		
		switch (SPG_HBLANK_INT.hblank_int_mode)
		{
		case 0x0:
			if (pvr_cur_scanline == SPG_HBLANK_INT.line_comp_val)
				asic_RaiseInterrupt(holly_HBLank);
			break;
		case 0x2:
			asic_RaiseInterrupt(holly_HBLank);
			break;
		default:
			die("Unimplemented HBLANK INT mode");
			break;
		}

		//Vblank start -- really need to test the scanline values
		if (pvr_cur_scanline==0)
		{
			if (SPG_CONTROL.interlace)
				SPG_STATUS.fieldnum = ~SPG_STATUS.fieldnum;
			else
				SPG_STATUS.fieldnum=0;

			/* Vblank counter */
			vblk_cnt++;

			rend_vblank(); // notify for vblank
		}
		if (lightgun_line != 0xffff && lightgun_line == pvr_cur_scanline)
		{
         maple_int_pending = false;
			SPG_TRIGGER_POS = ((lightgun_line & 0x3FF) << 16) | (lightgun_hpos & 0x3FF);
         SB_MDST = 0;
			asic_RaiseInterrupt(holly_MAPLE_DMA);
			lightgun_line = 0xffff;
		}
	}

	//interrupts
	//0
	//vblank_in_interrupt_line_number
	//vblank_out_interrupt_line_number
	//vstart
	//vbend
	//pvr_numscanlines
	u32 min_scanline=pvr_cur_scanline+1;
	u32 min_active = pvr_numscanlines;

	if (min_scanline < SPG_VBLANK_INT.vblank_in_interrupt_line_number)
		min_active = min(min_active, SPG_VBLANK_INT.vblank_in_interrupt_line_number);

	if (min_scanline < SPG_VBLANK_INT.vblank_out_interrupt_line_number)
		min_active = min(min_active, SPG_VBLANK_INT.vblank_out_interrupt_line_number);

	if (min_scanline < SPG_VBLANK.vstart)
		min_active = min(min_active, SPG_VBLANK.vstart);

	if (min_scanline < SPG_VBLANK.vbend)
		min_active = min(min_active, SPG_VBLANK.vbend);

	if (min_scanline < pvr_numscanlines)
		min_active = min(min_active, pvr_numscanlines);

	if (lightgun_line != 0xffff && min_scanline < lightgun_line)
		min_active = min(min_active, lightgun_line);

	min_active = max(min_active,min_scanline);

	return (min_active - pvr_cur_scanline)*Line_Cycles;
}

void read_lightgun_position(int x, int y)
{
   static u8 flip;
   maple_int_pending = true;
	if (y < 0 || y >= 480 || x < 0 || x >= 640)
   {
		// Off screen
		lightgun_line = 0xffff;
   }
	else
	{
		lightgun_line = y / (SPG_CONTROL.interlace ? 2 : 1) + SPG_VBLANK_INT.vblank_out_interrupt_line_number;
      // For some reason returning the same position twice makes it register off screen
		lightgun_hpos = (x + 286) ^ flip;
		flip ^= 1;
	}
}

int rend_end_sch(int tag, int cycl, int jitt)
{
	asic_RaiseInterrupt(holly_RENDER_DONE);
	asic_RaiseInterrupt(holly_RENDER_DONE_isp);
	asic_RaiseInterrupt(holly_RENDER_DONE_vd);

#ifdef TARGET_NO_THREADS
   if (!settings.UpdateMode && !settings.UpdateModeForced)
#endif
      rend_end_render();

	return 0;
}

bool spg_Init()
{
   render_end_schid = sh4_sched_register(0,&rend_end_sch);
   vblank_schid     = sh4_sched_register(0,&spg_line_sched);

   return true;
}

void spg_Term()
{
}

void spg_Reset(bool hard)
{
   CalculateSync();
}

void SetREP(TA_context* cntx)
{
	if (cntx)
		// TODO depend on TA data size?
		sh4_sched_request(render_end_schid, 1500000);
	else
		sh4_sched_request(render_end_schid, 4096);
}
