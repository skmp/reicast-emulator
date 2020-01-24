#include "types.h"
#include "modules.h"
#include "hw/sh4/sh4_mmr.h"

struct CPG_impl : Sh4ModCpg
{
	SuperH4Mmr* sh4mmr;
	CPG_impl(SuperH4Mmr* sh4mmr) : sh4mmr(sh4mmr)
	{
		//CPG FRQCR H'FFC0 0000 H'1FC0 0000 16 *2 Held Held Held Pclk
		sh4_rio_reg(this, CPG, CPG_FRQCR_addr, RIO_DATA, 16);

		//CPG STBCR H'FFC0 0004 H'1FC0 0004 8 H'00 Held Held Held Pclk
		sh4_rio_reg(this, CPG, CPG_STBCR_addr, RIO_DATA, 8);

		//CPG WTCNT H'FFC0 0008 H'1FC0 0008 8/16*3 H'00 Held Held Held Pclk
		sh4_rio_reg(this, CPG, CPG_WTCNT_addr, RIO_DATA, 16);

		//CPG WTCSR H'FFC0 000C H'1FC0 000C 8/16*3 H'00 Held Held Held Pclk
		sh4_rio_reg(this, CPG, CPG_WTCSR_addr, RIO_DATA, 16);

		//CPG STBCR2 H'FFC0 0010 H'1FC0 0010 8 H'00 Held Held Held Pclk
		sh4_rio_reg(this, CPG, CPG_STBCR2_addr, RIO_DATA, 8);
	}

	void Reset()
	{
		/*
		CPG FRQCR H'FFC0 0000 H'1FC0 0000 16 *2 Held Held Held Pclk
		CPG STBCR H'FFC0 0004 H'1FC0 0004 8 H'00 Held Held Held Pclk
		CPG WTCNT H'FFC0 0008 H'1FC0 0008 8/16*3 H'00 Held Held Held Pclk
		CPG WTCSR H'FFC0 000C H'1FC0 000C 8/16*3 H'00 Held Held Held Pclk
		CPG STBCR2 H'FFC0 0010 H'1FC0 0010 8 H'00 Held Held Held Pclk
		*/
		CPG_FRQCR = 0;
		CPG_STBCR = 0;
		CPG_WTCNT = 0;
		CPG_WTCSR = 0;
		CPG_STBCR2 = 0;
	}
};

Sh4ModCpg* Sh4ModCpg::Create(SuperH4Mmr* sh4mmr)
{
	return new CPG_impl(sh4mmr);
}