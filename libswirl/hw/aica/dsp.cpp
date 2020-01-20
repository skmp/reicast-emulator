#include "dsp.h"
#include "dsp_backend.h"
#include "aica_mem.h"
#include "oslib/oslib.h"
#include <memory>

DECL_ALIGN(4096) dsp_context_t dsp;

//float format is ?
u16 DYNACALL DSPBackend::PACK(s32 val)
{
	u32 temp;
	int sign, exponent, k;

	sign = (val >> 23) & 0x1;
	temp = (val ^ (val << 1)) & 0xFFFFFF;
	exponent = 0;
	for (k = 0; k < 12; k++)
	{
		if (temp & 0x800000)
			break;
		temp <<= 1;
		exponent += 1;
	}
	if (exponent < 12)
		val = (val << exponent) & 0x3FFFFF;
	else
		val <<= 11;
	val >>= 11;
	val |= sign << 15;
	val |= exponent << 11;

	return (u16)val;
}

s32 DYNACALL DSPBackend::UNPACK(u16 val)
{
	int sign, exponent, mantissa;
	s32 uval;

	sign = (val >> 15) & 0x1;
	exponent = (val >> 11) & 0xF;
	mantissa = val & 0x7FF;
	uval = mantissa << 11;
	if (exponent > 11)
		exponent = 11;
	else
		uval |= (sign ^ 1) << 22;
	uval |= sign << 23;
	uval <<= 8;
	uval >>= 8;
	uval >>= exponent;

	return uval;
}

void DSPBackend::DecodeInst(u32* IPtr, _INST* i)
{
	i->TRA = (IPtr[0] >> 9) & 0x7F;
	i->TWT = (IPtr[0] >> 8) & 0x01;
	i->TWA = (IPtr[0] >> 1) & 0x7F;

	i->XSEL = (IPtr[1] >> 15) & 0x01;
	i->YSEL = (IPtr[1] >> 13) & 0x03;
	i->IRA = (IPtr[1] >> 7) & 0x3F;
	i->IWT = (IPtr[1] >> 6) & 0x01;
	i->IWA = (IPtr[1] >> 1) & 0x1F;

	i->TABLE = (IPtr[2] >> 15) & 0x01;
	i->MWT = (IPtr[2] >> 14) & 0x01;
	i->MRD = (IPtr[2] >> 13) & 0x01;
	i->EWT = (IPtr[2] >> 12) & 0x01;
	i->EWA = (IPtr[2] >> 8) & 0x0F;
	i->ADRL = (IPtr[2] >> 7) & 0x01;
	i->FRCL = (IPtr[2] >> 6) & 0x01;
	i->SHIFT = (IPtr[2] >> 4) & 0x03;
	i->YRL = (IPtr[2] >> 3) & 0x01;
	i->NEGB = (IPtr[2] >> 2) & 0x01;
	i->ZERO = (IPtr[2] >> 1) & 0x01;
	i->BSEL = (IPtr[2] >> 0) & 0x01;

	i->NOFL = (IPtr[3] >> 15) & 1;		//????
	//i->COEF=(IPtr[3]>>9)&0x3f;

	i->MASA = (IPtr[3] >> 9) & 0x3f;	//???
	i->ADREB = (IPtr[3] >> 8) & 0x1;
	i->NXADR = (IPtr[3] >> 7) & 0x1;
}

struct DSP_impl final : DSP {
	u8* aica_ram;
	u32 aram_size;

	unique_ptr<DSPBackend> backend;

	DSP_impl(u8* aica_ram, u32 aram_size) : aica_ram(aica_ram), aram_size(aram_size) {
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
			backend.reset(DSPBackend::CreateInterpreter(aica_ram, aram_size));
			return true;
		}
#if FEAT_DSPREC == DYNAREC_JIT
		else if (type == DSPBE_DYNAREC) {
			backend.reset(DSPBackend::CreateJIT(aica_ram, aram_size));
			return true;
		}
#endif

		return false;
	}
};

DSP* DSP::Create(u8* aica_ram, u32 aram_size) {
	return new DSP_impl(aica_ram, aram_size);
}