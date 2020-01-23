/*
	SH4/mod/intc

	Implements the register interface of the sh4 interrupt controller.
	For the actual implementation of interrupt caching/handling logic, look at sh4_interrupts.cpp

	--
*/

#include "types.h"
#include "../sh4_interrupts.h"
#include "../sh4_mmr.h"
#include "modules.h"

struct Sh4ModIntc_impl : Sh4ModIntc {

	SuperH4Mmr* sh4mmr;

	//Register writes need interrupt re-testing !

	void write_INTC_IPRA(u32 addr, u32 data)
	{
		if (INTC_IPRA.reg_data != (u16)data)
		{
			INTC_IPRA.reg_data = (u16)data;
			SIIDRebuild();	//we need to rebuild the table
		}
	}

	void write_INTC_IPRB(u32 addr, u32 data)
	{
		if (INTC_IPRB.reg_data != (u16)data)
		{
			INTC_IPRB.reg_data = (u16)data;
			SIIDRebuild(); //we need to rebuild the table
		}
	}

	void write_INTC_IPRC(u32 addr, u32 data)
	{
		if (INTC_IPRC.reg_data != (u16)data)
		{
			INTC_IPRC.reg_data = (u16)data;
			SIIDRebuild(); //we need to rebuild the table
		}
	}

	u32 read_INTC_IPRD(u32 addr)
	{
		return 0;
	}

	//Init/Res/Term
	Sh4ModIntc_impl(SuperH4Mmr* sh4mmr) : sh4mmr(sh4mmr)
	{
		//INTC ICR 0xFFD00000 0x1FD00000 16 0x0000 0x0000 Held Held Pclk
		sh4_rio_reg(this, INTC, INTC_ICR_addr, RIO_DATA, 16);

		//INTC IPRA 0xFFD00004 0x1FD00004 16 0x0000 0x0000 Held Held Pclk
		sh4_rio_reg(this, INTC, INTC_IPRA_addr, RIO_WF, 16, 0, STATIC_FORWARD(Sh4ModIntc_impl, write_INTC_IPRA));

		//INTC IPRB 0xFFD00008 0x1FD00008 16 0x0000 0x0000 Held Held Pclk
		sh4_rio_reg(this, INTC, INTC_IPRB_addr, RIO_WF, 16, 0, STATIC_FORWARD(Sh4ModIntc_impl, write_INTC_IPRB));

		//INTC IPRC 0xFFD0000C 0x1FD0000C 16 0x0000 0x0000 Held Held Pclk
		sh4_rio_reg(this, INTC, INTC_IPRC_addr, RIO_WF, 16, 0, STATIC_FORWARD(Sh4ModIntc_impl, write_INTC_IPRC));

		//INTC IPRD 0xFFD00010 0x1FD00010 16 0xDA74 0xDA74 Held Held Pclk	(SH7750S, SH7750R only)
		sh4_rio_reg(this, INTC, INTC_IPRD_addr, RIO_RO_FUNC, 16, STATIC_FORWARD(Sh4ModIntc_impl, read_INTC_IPRD));

		interrupts_init();
	}

	void Reset()
	{
		INTC_ICR.reg_data = 0x0;
		INTC_IPRA.reg_data = 0x0;
		INTC_IPRB.reg_data = 0x0;
		INTC_IPRC.reg_data = 0x0;

		SIIDRebuild(); //rebuild the interrupts table

		interrupts_reset();
	}

	~Sh4ModIntc_impl()
	{
		interrupts_term();
	}

};

Sh4ModIntc* Sh4ModIntc::Create(SuperH4Mmr* sh4mmr) {
	return new Sh4ModIntc_impl(sh4mmr);
}