/*
	DMAC is not really emulated on nullDC. We just fake the dmas ;p
		Dreamcast uses sh4's dmac in ddt mode to multiplex ch0 and ch2 for dma access.
		nullDC just 'fakes' each dma as if it was a full channel, never bothering properly
		updating the dmac regs -- it works just fine really :|
*/
#include "types.h"
#include "hw/sh4/sh4_mmr.h"
#include "hw/holly/sb.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/pvr/pvr_mem.h"
#include "dmac.h"
#include "hw/sh4/sh4_interrupts.h"
#include "hw/holly/holly_intc.h"
#include "types.h"

/*
u32 DMAC_SAR[4];
u32 DMAC_DAR[4];
u32 DMAC_DMATCR[4];
DMAC_CHCR_type DMAC_CHCR[4];

DMAC_DMAOR_type DMAC_DMAOR;

*/

//on demand data transfer
//ch0/on demand data transfer request
void dmac_ddt_ch0_ddt(u32 src,u32 dst,u32 count)
{
	
}

//ch2/direct data transfer request
void dmac_ddt_ch2_direct(u32 dst,u32 count)
{
}

//transfer 22kb chunks (or less) [704 x 32] (22528)
void UpdateDMA()
{
	/*if (DMAC_DMAOR.AE==1 || DMAC_DMAOR.DME==0)
		return;//DMA disabled

	//DMAC _must_ be on DDT mode
	verify(DMAC_DMAOR.DDT==1);

	for (int ch=0;ch<4;ch++)
	{
		if (DMAC_CHCR[ch].DE==1 && DMAC_CHCR[ch].TE==0)
		{
			verify(DMAC_CHCR[ch].RS<0x8);
			verify(DMAC_CHCR[ch].RS<0x4);
		}
	}*/
}

static SuperH4Mmr* sh4mmr;

template<u32 ch>
void WriteCHCR(void* that, u32 addr, u32 data)
{
	DMAC_CHCR(ch).full=data;
	//printf("Write to CHCR%d = 0x%X\n",ch,data);
}

void WriteDMAOR(void* that, u32 addr, u32 data)
{
	DMAC_DMAOR.full=data;
	//printf("Write to DMAOR = 0x%X\n",data);
}

//Init term res
void dmac_init(SuperH4Mmr* sh4mmr)
{
	::sh4mmr = sh4mmr;

	//DMAC SAR0 0xFFA00000 0x1FA00000 32 Undefined Undefined Held Held Bclk
	sh4_rio_reg(sh4_cpu, DMAC,DMAC_SAR0_addr,RIO_DATA,32);

	//DMAC DAR0 0xFFA00004 0x1FA00004 32 Undefined Undefined Held Held Bclk
	sh4_rio_reg(sh4_cpu, DMAC,DMAC_DAR0_addr,RIO_DATA,32);

	//DMAC DMATCR0 0xFFA00008 0x1FA00008 32 Undefined Undefined Held Held Bclk
	sh4_rio_reg(sh4_cpu, DMAC,DMAC_DMATCR0_addr,RIO_DATA,32);

	//DMAC CHCR0 0xFFA0000C 0x1FA0000C 32 0x00000000 0x00000000 Held Held Bclk
	sh4_rio_reg(sh4_cpu, DMAC,DMAC_CHCR0_addr,RIO_WF,32,0,&WriteCHCR<0>);

	//DMAC SAR1 0xFFA00010 0x1FA00010 32 Undefined Undefined Held Held Bclk
	sh4_rio_reg(sh4_cpu, DMAC,DMAC_SAR1_addr,RIO_DATA,32);

	//DMAC DAR1 0xFFA00014 0x1FA00014 32 Undefined Undefined Held Held Bclk
	sh4_rio_reg(sh4_cpu, DMAC,DMAC_DAR1_addr,RIO_DATA,32);

	//DMAC DMATCR1 0xFFA00018 0x1FA00018 32 Undefined Undefined Held Held Bclk
	sh4_rio_reg(sh4_cpu, DMAC,DMAC_DMATCR1_addr,RIO_DATA,32);

	//DMAC CHCR1 0xFFA0001C 0x1FA0001C 32 0x00000000 0x00000000 Held Held Bclk
	sh4_rio_reg(sh4_cpu, DMAC,DMAC_CHCR1_addr,RIO_WF,32,0,&WriteCHCR<1>);

	//DMAC SAR2 0xFFA00020 0x1FA00020 32 Undefined Undefined Held Held Bclk
	sh4_rio_reg(sh4_cpu, DMAC,DMAC_SAR2_addr,RIO_DATA,32);

	//DMAC DAR2 0xFFA00024 0x1FA00024 32 Undefined Undefined Held Held Bclk
	sh4_rio_reg(sh4_cpu, DMAC,DMAC_DAR2_addr,RIO_DATA,32);

	//DMAC DMATCR2 0xFFA00028 0x1FA00028 32 Undefined Undefined Held Held Bclk
	sh4_rio_reg(sh4_cpu, DMAC,DMAC_DMATCR2_addr,RIO_DATA,32);

	//DMAC CHCR2 0xFFA0002C 0x1FA0002C 32 0x00000000 0x00000000 Held Held Bclk
	sh4_rio_reg(sh4_cpu, DMAC,DMAC_CHCR2_addr,RIO_WF,32,0,&WriteCHCR<2>);

	//DMAC SAR3 0xFFA00030 0x1FA00030 32 Undefined Undefined Held Held Bclk
	sh4_rio_reg(sh4_cpu, DMAC,DMAC_SAR3_addr,RIO_DATA,32);

	//DMAC DAR3 0xFFA00034 0x1FA00034 32 Undefined Undefined Held Held Bclk
	sh4_rio_reg(sh4_cpu, DMAC,DMAC_DAR3_addr,RIO_DATA,32);

	//DMAC DMATCR3 0xFFA00038 0x1FA00038 32 Undefined Undefined Held Held Bclk
	sh4_rio_reg(sh4_cpu, DMAC,DMAC_DMATCR3_addr,RIO_DATA,32);

	//DMAC CHCR3 0xFFA0003C 0x1FA0003C 32 0x00000000 0x00000000 Held Held Bclk
	sh4_rio_reg(sh4_cpu, DMAC,DMAC_CHCR3_addr,RIO_WF,32,0,&WriteCHCR<3>);

	//DMAC DMAOR 0xFFA00040 0x1FA00040 32 0x00000000 0x00000000 Held Held Bclk
	sh4_rio_reg(sh4_cpu, DMAC,DMAC_DMAOR_addr,RIO_WF,32,0,&WriteDMAOR);
}
void dmac_reset()
{
	/*
	DMAC SAR0 H'FFA0 0000 H'1FA0 0000 32 Undefined Undefined Held Held Bclk
	DMAC DAR0 H'FFA0 0004 H'1FA0 0004 32 Undefined Undefined Held Held Bclk
	DMAC DMATCR0 H'FFA0 0008 H'1FA0 0008 32 Undefined Undefined Held Held Bclk
	DMAC CHCR0 H'FFA0 000C H'1FA0 000C 32 H'0000 0000 H'0000 0000 Held Held Bclk
	DMAC SAR1 H'FFA0 0010 H'1FA0 0010 32 Undefined Undefined Held Held Bclk
	DMAC DAR1 H'FFA0 0014 H'1FA0 0014 32 Undefined Undefined Held Held Bclk
	DMAC DMATCR1 H'FFA0 0018 H'1FA0 0018 32 Undefined Undefined Held Held Bclk
	DMAC CHCR1 H'FFA0 001C H'1FA0 001C 32 H'0000 0000 H'0000 0000 Held Held Bclk
	DMAC SAR2 H'FFA0 0020 H'1FA0 0020 32 Undefined Undefined Held Held Bclk
	DMAC DAR2 H'FFA0 0024 H'1FA0 0024 32 Undefined Undefined Held Held Bclk
	DMAC DMATCR2 H'FFA0 0028 H'1FA0 0028 32 Undefined Undefined Held Held Bclk
	DMAC CHCR2 H'FFA0 002C H'1FA0 002C 32 H'0000 0000 H'0000 0000 Held Held Bclk
	DMAC SAR3 H'FFA0 0030 H'1FA0 0030 32 Undefined Undefined Held Held Bclk
	DMAC DAR3 H'FFA0 0034 H'1FA0 0034 32 Undefined Undefined Held Held Bclk
	DMAC DMATCR3 H'FFA0 0038 H'1FA0 0038 32 Undefined Undefined Held Held Bclk
	DMAC CHCR3 H'FFA0 003C H'1FA0 003C 32 H'0000 0000 H'0000 0000 Held Held Bclk
	DMAC DMAOR H'FFA0 0040 H'1FA0 0040 32 H'0000 0000 H'0000 0000 Held Held Bclk
	*/
	DMAC_CHCR(0).full = 0x0;
	DMAC_CHCR(1).full = 0x0;
	DMAC_CHCR(2).full = 0x0;
	DMAC_CHCR(3).full = 0x0;
	DMAC_DMAOR.full = 0x0;
}
void dmac_term()
{
}
