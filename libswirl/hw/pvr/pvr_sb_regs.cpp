/*
    PVR-SB handling
    DMA hacks are here
*/

#include "types.h"
#include "hw/holly/holly_intc.h"
#include "hw/holly/sb.h"
#include "hw/sh4/modules/dmac.h"
#include "hw/sh4/sh4_mem.h"
#include "pvr_sb_regs.h"
#include "hw/sh4/sh4_mmr.h"
#include "ta.h"
#include "pvr_regs.h"
#include "pvr_mem.h"
#include "Renderer_if.h"
#include "ta.h"
#include "spg.h"
#include "pvr_yuv.h"

#include "hw/sh4/sh4_mmio.h"

bool pal_needs_update = true;
bool fog_needs_update = true;

u8 pvr_regs[pvr_RegSize];

struct PVRDevice : MMIODevice {
    void DMAC_Ch2St(u8* vram)
    {
        u32 chcr = DMAC_CHCR(2).full;
        u32 dmaor = DMAC_DMAOR.full;
        u32 dmatcr = DMAC_DMATCR(2);

        u32 src = DMAC_SAR(2);
        u32 dst = SB_C2DSTAT;
        u32 len = SB_C2DLEN;

        if (0x8201 != (dmaor & DMAOR_MASK))
        {
            printf("\n!\tDMAC: DMAOR has invalid settings (%X) !\n", dmaor);
            return;
        }

        if (len & 0x1F)
        {
            printf("\n!\tDMAC: SB_C2DLEN has invalid size (%X) !\n", len);
            return;
        }

        //	printf(">>\tDMAC: Ch2 DMA SRC=%X DST=%X LEN=%X\n", src, dst, len );

            // Direct DList DMA (Ch2)

            // Texture DMA 
        if ((dst >= 0x10000000) && (dst <= 0x10FFFFFF))
        {
            u32 p_addr = src & RAM_MASK;
            //GetMemPtr perhaps ? it's not good to use the mem arrays directly 
            while (len)
            {
                if ((p_addr + len) > RAM_SIZE)
                {
                    u32* sys_buf = (u32*)GetMemPtr(src, len);//(&mem_b[src&RAM_MASK]);
                    u32 new_len = RAM_SIZE - p_addr;
                    TAWrite(dst, sys_buf, (new_len / 32), vram);
                    len -= new_len;
                    src += new_len;
                    //dst+=new_len;
                }
                else
                {
                    u32* sys_buf = (u32*)GetMemPtr(src, len);//(&mem_b[src&RAM_MASK]);
                    TAWrite(dst, sys_buf, (len / 32), vram);
                    src += len;
                    break;
                }
            }
            //libPvr_TADma(dst,sys_buf,(len/32));
        }
        // If SB_C2DSTAT reg is inrange from 0x11000000 to 0x11FFFFE0,	 set 1 in SB_LMMODE0 reg.
        else if ((dst >= 0x11000000) && (dst <= 0x11FFFFE0))
        {
            //printf(">>\tDMAC: TEX LNMODE0 Ch2 DMA SRC=%X DST=%X LEN=%X SB_LMMODE0 %d\n", src, dst, len, SB_LMMODE0);
            SB_C2DSTAT += len;

            if (SB_LMMODE0 == 0)
            {
                // 64-bit path
                dst = (dst & 0xFFFFFF) | 0xa4000000;
                u32 p_addr = src & RAM_MASK;
                while (len)
                {
                    if ((p_addr + len) > RAM_SIZE)
                    {
                        u32 new_len = RAM_SIZE - p_addr;
                        WriteMemBlock_nommu_dma(dst, src, new_len);
                        len -= new_len;
                        src += new_len;
                        dst += new_len;
                    }
                    else
                    {
                        WriteMemBlock_nommu_dma(dst, src, len);
                        src += len;
                        break;
                    }
                }
            }
            else
            {
                // 32-bit path
                dst = (dst & 0xFFFFFF) | 0xa5000000;
                while (len > 0)
                {
                    u32 v = ReadMem32(src);
                    pvr_write_area1_32(vram, dst, v);
                    len -= 4;
                    src += 4;
                    dst += 4;
                }
            }
        }
        // If SB_C2DSTAT reg is in range from 0x13000000 to 0x13FFFFE0, set 1 in SB_LMMODE1 reg.
        else if ((dst >= 0x13000000) && (dst <= 0x13FFFFE0))
        {
            die(".\tPVR DList DMA LNMODE1\n\n");
            src += len;
        }
        else
        {
            printf("\n!\tDMAC: SB_C2DSTAT has invalid address (%X) !\n", dst);
            src += len;
        }


        // Setup some of the regs so it thinks we've finished DMA

        DMAC_SAR(2) = (src);
        DMAC_CHCR(2).full &= 0xFFFFFFFE;
        DMAC_DMATCR(2) = 0x00000000;

        SB_C2DST = 0x00000000;
        SB_C2DLEN = 0x00000000;

        // The DMA end interrupt flag (SB_ISTNRM - bit 19: DTDE2INT) is set to "1."
        //-> fixed , holly_PVR_DMA is for different use now (fixed the interrupts enum too)
        asic->RaiseInterrupt(holly_CH2_DMA);
    }

    void SB_C2DST_write(u32 addr, u32 data)
    {
        if (1 & data)
        {
            SB_C2DST = 1;
            DMAC_Ch2St(vram);
        }
    }
    //PVR-DMA
    void do_pvr_dma()
    {
        u32 chcr = DMAC_CHCR(0).full;
        u32 dmaor = DMAC_DMAOR.full;
        u32 dmatcr = DMAC_DMATCR(0);

        u32 src = SB_PDSTAR;	// System RAM address
        u32 dst = SB_PDSTAP;	// VRAM address
        u32 len = SB_PDLEN;

        if (0x8201 != (dmaor & DMAOR_MASK))
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
            WriteMemBlock_nommu_dma(src, dst, len);
        }
        else
        {
            //System -> PVR
            WriteMemBlock_nommu_dma(dst, src, len);
        }

        DMAC_SAR(0) = (src + len);
        DMAC_CHCR(0).full &= 0xFFFFFFFE;
        DMAC_DMATCR(0) = 0x00000000;

        SB_PDST = 0x00000000;

        //TODO : *CHECKME* is that ok here ? the docs don't say here it's used [PVR-DMA , bit 11]
        asic->RaiseInterrupt(holly_PVR_DMA);
    }
    void SB_PDST_write(u32 addr, u32 data)
    {
        if (1 & data)
        {
            SB_PDST = 1;
            do_pvr_dma();
        }
    }
    u32 calculate_start_link_addr()
    {
        u8* base = &mram[SB_SDSTAW & RAM_MASK];
        u32 rv;
        if (SB_SDWLT == 0)
        {
            //16b width
            rv = ((u16*)base)[SB_SDDIV];
        }
        else
        {
            //32b width
            rv = ((u32*)base)[SB_SDDIV];
        }
        SB_SDDIV++; //next index

        return rv;
    }
    void pvr_do_sort_dma()
    {

        SB_SDDIV = 0;//index is 0 now :)
        u32 link_addr = calculate_start_link_addr();
        u32 link_base_addr = SB_SDBAAW;

        while (link_addr != 1)
        {
            if (SB_SDLAS == 1)
                link_addr *= 32;

            u32 ea = (link_base_addr + link_addr) & RAM_MASK;
            u32* ea_ptr = (u32*)&mram[ea];

            link_addr = ea_ptr[0x1C >> 2];//Next link
            //transfer global param
            ta_vtx_data(ea_ptr, ea_ptr[0x18 >> 2]);
            if (link_addr == 2)
            {
                link_addr = calculate_start_link_addr();
            }
        }

        // End of DMA :)
        SB_SDST = 0;
        asic->RaiseInterrupt(holly_PVR_SortDMA);
    }
    // Auto sort DMA :|
    void SB_SDST_write(u32 addr, u32 data)
    {
        if (1 & data)
        {
            pvr_do_sort_dma();
        }
    }




    SystemBus* sb;
    ASIC* asic;
    SPG* spg;
    SuperH4Mmr* sh4mmr;
    u8* mram;
    u8* vram;

    PVRDevice(SuperH4Mmr* sh4mmr, SystemBus* sb, ASIC* asic, SPG* spg, u8* mram, u8* vram) : sh4mmr(sh4mmr), sb(sb), asic(asic), spg(spg), mram(mram), vram(vram) { }

    u32 Read(u32 addr, u32 sz)
    {
        verify(sz == 4);
        return PvrReg(addr, u32);
    }

    void Write(u32 paddr, u32 data, u32 sz)
    {
        verify(sz == 4);

        u32 addr = paddr & pvr_RegMask;

        if (addr == ID_addr)
            return;//read only
        if (addr == REVISION_addr)
            return;//read only
        if (addr == TA_YUV_TEX_CNT_addr)
            return;//read only

        if (addr == STARTRENDER_addr)
        {
            //start render
            rend_start_render(vram);
            return;
        }

        if (addr == TA_LIST_INIT_addr)
        {
            if (data >> 31)
            {
                ta_vtx_ListInit();
                data = 0;
            }
        }

        if (addr == SOFTRESET_addr)
        {
            if (data != 0)
            {
                if (data & 1)
                    ta_vtx_SoftReset();
                data = 0;
            }
        }

        if (addr == TA_LIST_CONT_addr)
        {
            //a write of anything works ?
            ta_vtx_ListCont();
        }

        if (addr == SPG_CONTROL_addr || addr == SPG_LOAD_addr)
        {
            if (PvrReg(addr, u32) != data)
            {
                PvrReg(addr, u32) = data;
                spg->CalculateSync();
            }
            return;
        }
        if (addr == FB_R_CTRL_addr)
        {
            bool vclk_div_changed = (PvrReg(addr, u32) ^ data) & (1 << 23);
            PvrReg(addr, u32) = data;
            if (vclk_div_changed)
                spg->CalculateSync();
            return;
        }
        if (addr == FB_R_SIZE_addr)
        {
            if (PvrReg(addr, u32) != data)
            {
                PvrReg(addr, u32) = data;
                fb_dirty = false;
                pvr_update_framebuffer_watches();
            }
            return;
        }
        if (addr == TA_YUV_TEX_BASE_addr || addr == TA_YUV_TEX_CTRL_addr)
        {
            PvrReg(addr, u32) = data;
            YUV_init(asic);
            return;
        }

        if (addr >= PALETTE_RAM_START_addr && PvrReg(addr, u32) != data)
        {
            pal_needs_update = true;
        }

        if (addr >= FOG_TABLE_START_addr && addr <= FOG_TABLE_END_addr && PvrReg(addr, u32) != data)
        {
            fog_needs_update = true;
        }
        PvrReg(addr, u32) = data;
    }

    //Init/Term , global
    bool Init()
    {
        //0x005F7C18    SB_PDST RW  PVR-DMA start
        sb->RegisterRIO(this, SB_PDST_addr, RIO_WF, 0, STATIC_FORWARD(PVRDevice, SB_PDST_write));

        //0x005F6808    SB_C2DST RW  ch2-DMA start 
        sb->RegisterRIO(this, SB_C2DST_addr, RIO_WF, 0, STATIC_FORWARD(PVRDevice, SB_C2DST_write));

        //0x005F6820    SB_SDST RW  Sort-DMA start
        sb->RegisterRIO(this, SB_SDST_addr, RIO_WF, 0, STATIC_FORWARD(PVRDevice, SB_SDST_write));

        pvr_mem_Init(sb);
        return true;
    }

    void Term()
    {
    }

    //Reset -> Reset - Initialise
    void Reset(bool Manual)
    {
        ID = 0x17FD11DB;
        REVISION = 0x00000011;
        SOFTRESET = 0x00000007;
        SPG_HBLANK_INT.full = 0x031D0000;
        SPG_VBLANK_INT.full = 0x01500104;
        FPU_PARAM_CFG = 0x0007DF77;
        HALF_OFFSET = 0x00000007;
        ISP_FEED_CFG = 0x00402000;
        SDRAM_REFRESH = 0x00000020;
        SDRAM_ARB_CFG = 0x0000001F;
        SDRAM_CFG = 0x15F28997;
        SPG_HBLANK.full = 0x007E0345;
        SPG_LOAD.full = 0x01060359;
        SPG_VBLANK.full = 0x01500104;
        SPG_WIDTH.full = 0x07F1933F;
        VO_CONTROL.full = 0x00000108;
        VO_STARTX.full = 0x0000009D;
        VO_STARTY.full = 0x00000015;
        SCALER_CTL.full = 0x00000400;
        FB_BURSTCTRL = 0x00090639;
        PT_ALPHA_REF = 0x000000FF;
    }

};

MMIODevice* Create_PVRDevice(SuperH4Mmr* sh4mmr, SystemBus* sb, ASIC* asic, SPG* spg, u8* mram, u8* vram) {
    return new PVRDevice(sh4mmr, sb, asic, spg, mram, vram);
}