/*
	This file is part of libswirl
*/
#include "license/bsd"


#include "aica_mmio.h"
#include "dsp.h"
#include "dsp_backend.h"
#include "aica_mem.h"
#include "oslib/oslib.h"
#include <memory>

DECL_ALIGN(4096) dsp_context_t dsp;

struct DSP_impl final : DSP {
	u8* aica_ram;
	u32 aram_size;
	DSPData_struct* DSPData;

	unique_ptr<DSPBackend> backend;

	DSP_impl(u8* aica_reg, u8* aica_ram, u32 aram_size) : aica_ram(aica_ram), aram_size(aram_size) {
		
		DSPData = (DSPData_struct*)&aica_reg[0x3000];
		
		setBackend(DSPBE_INTERPRETER);

		
	}

	bool Init() {
		// XX is this the right place for this?
		memset(DSPData, 0, sizeof(*DSPData));

		memset(&dsp, 0, sizeof(dsp));
		dsp.RBL = 0x8000 - 1;
		dsp.Stopped = 1;
		dsp.regs.MDEC_CT = 1;
		dsp.dyndirty = true;


		return true;
	}

	void WritenMem(u32 addr)
	{
		if (addr >= 0x3400 && addr < 0x3C00)
		{
			dsp.dyndirty = true;
		}
		else if (addr >= 0x4000 && addr < 0x4400)
		{
			// TODO proper sharing of memory with sh4 through DSPData
			memset(dsp.TEMP, 0, sizeof(dsp.TEMP));
		}
		else if (addr >= 0x4400 && addr < 0x4500)
		{
			// TODO proper sharing of memory with sh4 through DSPData
			memset(dsp.MEMS, 0, sizeof(dsp.MEMS));
		}
	}

	void Step() {
		if (dsp.dyndirty) {
			backend->Recompile();
			dsp.dyndirty = false;
		}

		backend->Step();
	}

	bool setBackend(DspBackends type) {
		dsp.dyndirty = true;

		if (type == DSPBE_INTERPRETER) {
			backend.reset(DSPBackend::CreateInterpreter(DSPData, &dsp, aica_ram, aram_size));
			return true;
		}
#if FEAT_DSPREC == DYNAREC_JIT
		else if (type == DSPBE_DYNAREC) {
			backend.reset(DSPBackend::CreateJIT(DSPData, &dsp, aica_ram, aram_size));
			return true;
		}
#endif

		return false;
	}

	dsp_context_t* GetDspContext() {
		return &dsp;
	}

	void serialize(void** data, unsigned int* total_size) {
		REICAST_S(dsp);
	}

	void unserialize(void** data, unsigned int* total_size) {
		REICAST_US(dsp);
	}
};

DSP* DSP::Create(AicaContext* aica_ctx, u8* aica_ram, u32 aram_size) {
	return new DSP_impl(aica_ctx->regs, aica_ram, aram_size);
}