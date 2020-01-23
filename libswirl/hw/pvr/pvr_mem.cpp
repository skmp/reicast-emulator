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
#include "pvr_yuv.h"

//TODO : move code later to a plugin
//TODO : Fix registers arrays , they must be smaller now doe to the way SB registers are handled
#include "hw/holly/holly_intc.h"
#include "hw/holly/sb.h"

static SystemBus* sb;


//Regs

//vram 32-64b

u32 fb1_watch_addr_start;
u32 fb1_watch_addr_end;
u32 fb2_watch_addr_start;
u32 fb2_watch_addr_end;
bool fb_dirty;

void pvr_update_framebuffer_watches()
{
    u32 fb_size = (FB_R_SIZE.fb_y_size + 1) * (FB_R_SIZE.fb_x_size + FB_R_SIZE.fb_modulus) * 4;
    fb1_watch_addr_start = FB_R_SOF1 & VRAM_MASK;
    fb1_watch_addr_end = fb1_watch_addr_start + fb_size;
    fb2_watch_addr_start = FB_R_SOF2 & VRAM_MASK;
    fb2_watch_addr_end = fb2_watch_addr_start + fb_size;
}


//read
u8 DYNACALL pvr_read_area1_8(SuperH4* sh4, u32 addr)
{
	printf("8-bit VRAM reads are not possible\n");
	return 0;
}

u16 DYNACALL pvr_read_area1_16(SuperH4* sh4, u32 addr)
{
	return *(u16*)&sh4->vram[pvr_map32(addr)];
}
u32 DYNACALL pvr_read_area1_32(SuperH4* sh4, u32 addr)
{
	return *(u32*)&sh4->vram[pvr_map32(addr)];
}

//write
void DYNACALL pvr_write_area1_8(SuperH4* sh4, u32 addr,u8 data)
{
	printf("8-bit VRAM writes are not possible\n");
}
void DYNACALL pvr_write_area1_16(SuperH4* sh4, u32 addr,u16 data)
{
    u32 vaddr = addr & VRAM_MASK;
    if (!fb_dirty
        && ((vaddr >= fb1_watch_addr_start && vaddr < fb1_watch_addr_end)
            || (vaddr >= fb2_watch_addr_start && vaddr < fb2_watch_addr_end)))
    {
        fb_dirty = true;
    }
	*(u16*)&sh4->vram[pvr_map32(addr)]=data;
}
void DYNACALL pvr_write_area1_32(SuperH4* sh4, u32 addr,u32 data)
{
    u32 vaddr = addr & VRAM_MASK;
    if (!fb_dirty
        && ((vaddr >= fb1_watch_addr_start && vaddr < fb1_watch_addr_end)
            || (vaddr >= fb2_watch_addr_start && vaddr < fb2_watch_addr_end)))
    {
        fb_dirty = true;
    }
	*(u32*)&sh4->vram[pvr_map32(addr)] = data;
}

void TAWrite(u32 address,u32* data,u32 count, u8* vram)
{
	u32 address_w=address&0x1FFFFFF;//correct ?
	if (address_w<0x800000)//TA poly
	{
		ta_vtx_data(data, count);
	}
	else if(address_w<0x1000000) //Yuv Converter
	{
		YUV_data(data, count, vram);
	}
	else //Vram Writef
	{
		//shouldn't really get here (?) -> works on dc :D need to handle lmmodes
		//printf("Vram TAWrite 0x%X , bkls %d\n",address,count);
		verify(SB_LMMODE0 == 0);
		memcpy(&vram[address & VRAM_MASK], data, count * 32);
	}
}

#include "hw/sh4/sh4_mmr.h"

void NOINLINE MemWrite32(void* dst, void* src)
{
	memcpy((u64*)dst,(u64*)src,32);
}

#if HOST_CPU!=CPU_ARM
extern "C" void DYNACALL TAWriteSQ(u32 address,u8* sqb)
{
	u32 address_w=address&0x1FFFFFF;//correct ?
	u8* sq=&sqb[address&0x20];

	u8* vram = sqb + 512 + 0x04000000;

	if (likely(address_w<0x800000))//TA poly
	{
		ta_vtx_data32(sq);
	}
	else if(likely(address_w<0x1000000)) //Yuv Converter
	{
		YUV_data((u32*)sq, 1, vram);
	}
	else //Vram Writef
	{
		// Used by WinCE
		//printf("Vram TAWriteSQ 0x%X SB_LMMODE0 %d\n",address, SB_LMMODE0);
		if (SB_LMMODE0 == 0)
		{
			// 64b path
			MemWrite32(&vram[address_w&(VRAM_MASK-0x1F)],sq);
		}
		else
		{
			// 32b path
			for (int i = 0; i < 8; i++, address_w += 4)
			{
				pvr_write_area1_32(sh4_cpu, address_w, ((u32 *)sq)[i]);
			}
		}
	}
}
#endif

//Misc interface

void pvr_mem_Init(SystemBus* sb) {
	::sb = sb;
}

#define VRAM_BANK_BIT 0x400000

u32 pvr_map32(u32 offset32)
{
	//64b wide bus is achieved by interleaving the banks every 32 bits
	const u32 bank_bit = VRAM_BANK_BIT;
	const u32 static_bits = (VRAM_MASK - (VRAM_BANK_BIT * 2 - 1)) | 3;
	const u32 offset_bits = (VRAM_BANK_BIT - 1) & ~3;

	u32 bank = (offset32 & VRAM_BANK_BIT) / VRAM_BANK_BIT;

	u32 rv = offset32 & static_bits;

	rv |= (offset32 & offset_bits) * 2;

	rv |= bank * 4;
	
	return rv;
}


f32 vrf(u8* vram, u32 addr)
{
	return *(f32*)&vram[pvr_map32(addr)];
}
u32 vri(u8* vram, u32 addr)
{
	return *(u32*)&vram[pvr_map32(addr)];
}
