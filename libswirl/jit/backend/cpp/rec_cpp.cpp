/*
	This file is part of libswirl
*/
#include "license/bsd"



#include "types.h"

#include <map>
#include <algorithm>

#if FEAT_SHREC == DYNAREC_CPP
#include "hw/sh4/sh4_opcode_list.h"
#include "hw/sh4/modules/ccn.h"
#include "hw/sh4/sh4_interrupts.h"

#include "hw/sh4/sh4_core.h"
#include "hw/sh4/dyna/ngen.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/sh4/dyna/regalloc.h"
#include "profiler/profiler.h"
#include "oslib/oslib.h"

#define MIPS_COUNTER 0

int cycle_counter;
extern int mips_counter;

struct DynaRBI : RuntimeBlockInfo
{
	virtual u32 Relink() {
		//verify(false);
		return 0;
	}

	virtual void Relocate(void* dst) {
		verify(false);
	}
};




string fnList;

#include "rec_cpp.inl"

#define DREP_COUNT 512
	#define DREP_1(x, phrase) case x: { ops_bytes += reinterpret_cast<opcodeExec*>(ops_bytes)->executeWithSizeCB(ops_bytes); };
	#define DREP_2(x, phrase) DREP_1(x, phrase) DREP_1(x+1, phrase)
	#define DREP_4(x, phrase) DREP_2(x, phrase) DREP_2(x+2, phrase)
	#define DREP_8(x, phrase) DREP_4(x, phrase) DREP_4(x+4, phrase)
	#define DREP_16(x, phrase) DREP_8(x, phrase) DREP_8(x+8, phrase)
	#define DREP_32(x, phrase) DREP_16(x, phrase) DREP_16(x+16, phrase)
	#define DREP_64(x, phrase) DREP_32(x, phrase) DREP_32(x+32, phrase)
	#define DREP_128(x, phrase) DREP_64(x, phrase) DREP_64(x+64, phrase)
	#define DREP_256(x, phrase) DREP_128(x, phrase) DREP_128(x+128, phrase)
	#define DREP_512(x, phrase) DREP_256(x, phrase) DREP_256(x+256, phrase)

class fnblock : public fnblock_base {
public:
	opcodeExec ops[0];
	
	INLINE void execute() {
		cycle_counter -= cc;

		u8* ops_bytes = reinterpret_cast<u8*>(ops);

#if MIPS_COUNTER
		mips_counter += cnt;
#endif
		switch (ncntd)
		{
			DREP_512(0, phrase)
		case 512:
			break;
		}
	}

	static void* operator new(std::size_t sz)
	{
		auto rv = emit_GetCCPtr();
		emit_Skip(sz);
		return rv;
	}
	static void* operator new[](std::size_t sz)
	{
		auto rv = emit_GetCCPtr();
		emit_Skip(sz);
		return rv;
	}
	static void operator delete(void* ptr, bool b)
	{

	}
};

struct fnrv {
	fnblock* fnb;
	opcodeExec* ptrs;
};

fnrv fnnCtor(int cycles, int opcode_slots) {
	auto rv = new fnblock();
	rv->cc = cycles;
	rv->ncntd = DREP_COUNT - opcode_slots;
	fnrv rvb = { rv, rv->ops };
	return rvb;
}

#define XREP_COUNT 512
#define XREP_1(x, phrase) case x: { fnblock<x>::runner(fnb); break; };
#define XREP_2(x, phrase)   XREP_1(x, phrase)   XREP_1(x+1, phrase)
#define XREP_4(x, phrase)   XREP_2(x, phrase)   XREP_2(x+2, phrase)
#define XREP_8(x, phrase)   XREP_4(x, phrase)   XREP_4(x+4, phrase)
#define XREP_16(x, phrase)  XREP_8(x, phrase)   XREP_8(x+8, phrase)
#define XREP_32(x, phrase)  XREP_16(x, phrase)  XREP_16(x+16, phrase)
#define XREP_64(x, phrase)  XREP_32(x, phrase)  XREP_32(x+32, phrase)
#define XREP_128(x, phrase) XREP_64(x, phrase)  XREP_64(x+64, phrase)
#define XREP_256(x, phrase) XREP_128(x, phrase) XREP_128(x+128, phrase)
#define XREP_512(x, phrase) XREP_256(x, phrase) XREP_256(x+256, phrase)


void rec_mainloop(void* v_cntx)
{
	Sh4RCB* ctx = (Sh4RCB*)((u8*)v_cntx - sizeof(Sh4RCB));

	cycle_counter = 0;

	while (sh4_int_bCpuRun) {
		cycle_counter += SH4_TIMESLICE;
		do {
			auto fnb = reinterpret_cast<void*>(bm_GetCode(ctx->cntx.pc));

			reinterpret_cast<fnblock_base*>(fnb)->execute();
		} while (cycle_counter > 0);

		if (UpdateSystem()) {
			rdv_DoInterrupts_pc(ctx->cntx.pc);
		}
	}
}


template <typename shilop, typename CTR>
opcodeExec* createType2(const CC_pars_t& prms, void* fun) {
	typedef typename CTR::template opex2<shilop> thetype;
	auto rv = new thetype();

	rv->setup(prms, fun);

	return rv;
}


map<void*, int> funs;


int funs_id_count;

template <typename CTR>
opcodeExec* createType_fast(const CC_pars_t& prms, void* fun, shil_opcode* opcode) {
	return 0;
}

#define OPCODE_CC(sig) opcode_cc_##sig

#define FAST_sig(sig, ...) \
template <> \
opcodeExec* createType_fast<OPCODE_CC(sig)>(const CC_pars_t& prms, void* fun, shil_opcode* opcode) { \
	typedef OPCODE_CC(sig) CTR; \
	\
	static map<void*, opcodeExec* (*)(const CC_pars_t& prms, void* fun)> funsf = {\
		
#define FAST_gis \
};\
	\
	if (funsf.count(fun)) { \
		return funsf[fun](prms, fun); \
	} \
	else { \
		return 0; \
	} \
}

#define FAST_po2(n,fn) { (void*)&shil_opcl_##n::fn::impl, &createType2 < shil_opcl_##n::fn, CTR > },
#define FAST_po(n) FAST_po2(n, f1)

FAST_sig(aCaCbC)
FAST_po(and)
FAST_po(or)
FAST_po(xor)
FAST_po(add)
FAST_po(sub)
FAST_po(ror)
FAST_po(shl)
FAST_po(shr)
FAST_po(sar)
FAST_po(shad)
FAST_po(shld)
FAST_po(test)
FAST_po(seteq)
FAST_po(setge)
FAST_po(setgt)
FAST_po(setae)
FAST_po(setab)
FAST_po(setpeq)
FAST_po(mul_u16)
FAST_po(mul_s16)
FAST_po(mul_i32)
FAST_gis

FAST_sig(aBaCbC)
FAST_po(and)
FAST_po(or)
FAST_po(xor)
FAST_po(add)
FAST_po(sub)
FAST_po(ror)
FAST_po(shl)
FAST_po(shr)
FAST_po(sar)
FAST_po(shad)
FAST_po(shld)
FAST_po(test)
FAST_po(seteq)
FAST_po(setge)
FAST_po(setgt)
FAST_po(setae)
FAST_po(setab)
FAST_po(setpeq)
FAST_po(mul_u16)
FAST_po(mul_s16)
FAST_po(mul_i32)
FAST_gis

FAST_sig(eDeDfD)
FAST_po(fadd)
FAST_po(fsub)
FAST_po(fmul)
FAST_po(fdiv)
FAST_gis

FAST_sig(eDfD)
FAST_po(fneg)
FAST_po(fabs)
FAST_po(fsrra)
FAST_po(fsqrt)
FAST_gis


FAST_sig(eDeDbC)
FAST_po(fseteq)
FAST_po(fsetgt)
FAST_gis

FAST_sig(eDeDeDfD)
FAST_po(fmac)
FAST_gis

FAST_sig(gHgHfD)
FAST_po(fipr)
FAST_gis

FAST_sig(aCaCcCdC)
FAST_po(div32u)
FAST_po(div32s)
FAST_po(rocr)
FAST_po(rocl)
FAST_po(mul_u64)
FAST_po(mul_s64)
FAST_gis

FAST_sig(aCaCaCcCdC)
FAST_po(adc)
FAST_po(sbc)
FAST_gis

FAST_sig(aCaCaCbC)
FAST_po(div32p2)
FAST_gis

FAST_sig(aCbC)
FAST_po(neg)
FAST_po(not)
FAST_po(ext_s8)
FAST_po(ext_s16)
FAST_po(swaplb)
FAST_gis

FAST_sig(aCfD)
FAST_po(cvt_i2f_z)
FAST_po(cvt_i2f_n)
FAST_gis


FAST_sig(aCgE)
FAST_po2(fsca, fsca_table)
FAST_gis

FAST_sig(eDbC)
FAST_po(cvt_f2i_t)
FAST_gis

FAST_sig(gJgHgH)
FAST_po(ftrv)
FAST_gis

FAST_sig(aC)
FAST_po2(pref, f1)
FAST_po2(pref, f2)
FAST_gis

FAST_sig(vV)
FAST_po(sync_sr)
FAST_po(sync_fpscr)
FAST_gis

FAST_sig(gJgJgJgJ)
FAST_po(frswap)
FAST_gis


typedef opcodeExec*(*foas)(const CC_pars_t& prms, void* fun, shil_opcode* opcode);

string getCTN(foas code);

template <typename CTR>
opcodeExec* createType(const CC_pars_t& prms, void* fun, shil_opcode* opcode) {

	auto frv = createType_fast<CTR>(prms, fun, opcode);
	if (frv)
		return frv;

	if (!funs.count(fun)) {
		funs[fun] = funs_id_count++;

		printf("DEFINE %s: FAST_po(%s)\n", getCTN(&createType<CTR>).c_str(), shil_opcode_name(opcode->op));
	}

	typedef typename CTR::opex thetype;
	auto rv = new thetype();

	rv->setup(prms, fun);

	return rv;
}

map< string, foas> unmap = {
	{ "aBaCbC", &createType_fast<opcode_cc_aBaCbC> },
	{ "aCaCbC", &createType_fast<opcode_cc_aCaCbC> },
	{ "aCbC", &createType_fast<opcode_cc_aCbC> },
	{ "aC", &createType_fast<opcode_cc_aC> },

	{ "eDeDeDfD", &createType_fast<opcode_cc_eDeDeDfD> },
	{ "eDeDfD", &createType_fast<opcode_cc_eDeDfD> },

	{ "aCaCaCbC", &createType_fast<opcode_cc_aCaCaCbC> },
	{ "aCaCcCdC", &createType_fast<opcode_cc_aCaCcCdC> },
	{ "aCaCaCcCdC", &createType_fast<opcode_cc_aCaCaCcCdC> },

	{ "eDbC", &createType_fast<opcode_cc_eDbC> },
	{ "aCfD", &createType_fast<opcode_cc_aCfD> },

	{ "eDeDbC", &createType_fast<opcode_cc_eDeDbC> },
	{ "eDfD", &createType_fast<opcode_cc_eDfD> },

	{ "aCgE", &createType_fast<opcode_cc_aCgE> },
	{ "gJgHgH", &createType_fast<opcode_cc_gJgHgH> },
	{ "gHgHfD", &createType_fast<opcode_cc_gHgHfD> },
	{ "gJgJgJgJ", &createType_fast<opcode_cc_gJgJgJgJ> },
	{ "vV", &createType_fast<opcode_cc_vV> },
};

string getCTN(foas f) {
	auto it = find_if(unmap.begin(), unmap.end(), [f](const map< string, foas>::value_type& s) { return s.second == f; });

	return it->first;
}

#define REP_1(x, phrase) phrase < x >
#define REP_2(x, phrase) REP_1(x, phrase), REP_1(x+1, phrase)
#define REP_4(x, phrase) REP_2(x, phrase), REP_2(x+2, phrase)
#define REP_8(x, phrase) REP_4(x, phrase), REP_4(x+4, phrase)
#define REP_16(x, phrase) REP_8(x, phrase), REP_8(x+8, phrase)
#define REP_32(x, phrase) REP_16(x, phrase), REP_16(x+16, phrase)
#define REP_64(x, phrase) REP_32(x, phrase), REP_32(x+32, phrase)
#define REP_128(x, phrase) REP_64(x, phrase), REP_64(x+64, phrase)
#define REP_256(x, phrase) REP_128(x, phrase), REP_128(x+128, phrase)
#define REP_512(x, phrase) REP_256(x, phrase), REP_256(x+256, phrase)
#define REP_1024(x, phrase) REP_512(x, phrase), REP_512(x+512, phrase)
#define REP_2048(x, phrase) REP_1024(x, phrase), REP_1024(x+1024, phrase)
#define REP_4096(x, phrase) REP_2048(x, phrase), REP_2048(x+2048, phrase)
#define REP_8192(x, phrase) REP_4096(x, phrase), REP_4096(x+4096, phrase)


typedef opcodeExec*(*FNAFB_SCBE)(RuntimeBlockInfo* block);
typedef FNAFB_SCBE(*FNAFB_SCB)();

template<int opcode_slots>
FNAFB_SCBE fnnCtor_scb() {
	return [](RuntimeBlockInfo* block) { 
		return (new opcode_check_block<opcode_slots>())->setup(block);
	};
}

FNAFB_SCB FNA_SCB[] = { REP_512(1, &fnnCtor_scb) };

FNAFB_SCB fnnCtor_scb_forreal(size_t n) {
	verify(n > 0);
	verify(n <= 512);
	return FNA_SCB[n - 1];
}

static map<string, int> fnlist_cnts;
map<string, void* (*)(void*, int)> fastblocks;

class BlockCompiler {
public:

	size_t opcode_index;
	opcodeExec* ptrsg;
	void compile(RuntimeBlockInfo* block,  SmcCheckEnum smc_checks, bool reset, bool staging, bool optimise) {
		
		fnList = "";

		static u32 id_fr = 0;
		//we need an extra one for the end opcode and optionally one more for block check
		auto block_size = block->oplist.size() + 1 + (smc_checks != NoCheck ? 1 : 0);
		auto ptrs = fnnCtor(block->guest_cycles, block_size);
		
		ptrsg = ptrs.ptrs;

		// WATCH // see rec_mainloop
		block->code = reinterpret_cast<DynarecCodeEntryPtr>(ptrs.fnb);

		size_t i = 0;
		if (smc_checks != NoCheck)
		{
			verify (smc_checks == FastCheck || smc_checks == FullCheck)
			opcodeExec* op;
			int check_size = block->sh4_code_size;

			if (smc_checks == FastCheck) {
				check_size = 4;
			}

			// this is trippy
			op = fnnCtor_scb_forreal(check_size)()(block);

		}

		for (size_t opnum = 0; opnum < block->oplist.size(); opnum++, i++) {
			opcode_index = i;
			shil_opcode& op = block->oplist[opnum];
			switch (op.op) {

			case shop_ifb:
			{
				if (op.rs1.imm_value()) {
					auto opc = new opcode_ifb_pc();
					
					
					opc->pc = op.rs2.imm_value();
					opc->opcode = op.rs3.imm_value();

					opc->oph = OpDesc[op.rs3.imm_value()]->oph;
				}
				else {
					auto opc = new opcode_ifb();
					

					opc->opcode = op.rs3.imm_value();

					opc->oph = OpDesc[op.rs3.imm_value()]->oph;
				}
			}
			break;
			
			case shop_jcond:
			case shop_jdyn:
			{
				if (op.rs2.is_imm()) {
					auto opc = new opcode_jdyn_imm();
					

					opc->src = op.rs1.reg_ptr();
					opc->imm = op.rs2.imm_value();
				}
				else {
					auto opc = new opcode_jdyn();
					

					opc->src = op.rs1.reg_ptr();
				}
				
			}
			break;

			case shop_mov32:
			{
				verify(op.rd.is_reg());

				verify(op.rs1.is_reg() || op.rs1.is_imm());

			
				if (op.rs1.is_imm()) {
					auto opc = new opcode_mov32_imm();
					

					opc->src = op.rs1.imm_value();
					opc->dst = op.rd.reg_ptr();
				}
				else {
					auto opc = new opcode_mov32();
					

					opc->src = op.rs1.reg_ptr();
					opc->dst = op.rd.reg_ptr();
				}
				

			}
			break;

			case shop_mov64:
			{
				verify(op.rd.is_reg());

				verify(op.rs1.is_reg());

				auto opc = new opcode_mov64();
				

				opc->src = (u64*) op.rs1.reg_ptr();
				opc->dst = (u64*)op.rd.reg_ptr();
			}
			break;

			case shop_readm:
			{
				u32 size = op.flags & 0x7f;
				if (op.rs1.is_imm()) {
					verify(op.rs2.is_null() && op.rs3.is_null());
					auto ptr = GetMemPtr(op.rs1.imm_value(), size);

					if (ptr)
					{
						if (size == 1)
						{
							auto opc = new opcode_readm_imm_ptr<1, s8>();  opc->src = ptr; opc->dst = op.rd.reg_ptr();
						}
						else if (size == 2)
						{
							auto opc = new opcode_readm_imm_ptr<2, s16>();  opc->src = ptr; opc->dst = op.rd.reg_ptr();
						}
						else if (size == 4)
						{
							auto opc = new opcode_readm_imm_ptr<4, u32>();  opc->src = ptr; opc->dst = op.rd.reg_ptr();
						}
						else if (size == 8)
						{
							auto opc = new opcode_readm_imm_ptr<8, u64>();  opc->src = ptr; opc->dst = op.rd.reg_ptr();
						}
					}
					else
					{
						if (size == 1)
						{
							auto opc = new opcode_readm_imm<1>();  opc->src = op.rs1.imm_value(); opc->dst = op.rd.reg_ptr();
						}
						else if (size == 2)
						{
							auto opc = new opcode_readm_imm<2>();  opc->src = op.rs1.imm_value(); opc->dst = op.rd.reg_ptr();
						}
						else if (size == 4)
						{
							auto opc = new opcode_readm_imm<4>();  opc->src = op.rs1.imm_value(); opc->dst = op.rd.reg_ptr();
						}
						else if (size == 8)
						{
							auto opc = new opcode_readm_imm<8>();  opc->src = op.rs1.imm_value(); opc->dst = op.rd.reg_ptr();
						}
					}
				}
				else if (op.rs3.is_imm()) {
					verify(op.rs2.is_null());
					if (size == 1)
					{
						auto opc = new opcode_readm_offs_imm<1>();  opc->src = op.rs1.reg_ptr(); opc->offs = op.rs3.imm_value(); opc->dst = op.rd.reg_ptr();
					}
					else if (size == 2)
					{
						auto opc = new opcode_readm_offs_imm<2>();  opc->src = op.rs1.reg_ptr(); opc->offs = op.rs3.imm_value(); opc->dst = op.rd.reg_ptr();
					}
					else if (size == 4)
					{
						auto opc = new opcode_readm_offs_imm<4>();  opc->src = op.rs1.reg_ptr(); opc->offs = op.rs3.imm_value(); opc->dst = op.rd.reg_ptr();
					}
					else if (size == 8)
					{
						auto opc = new opcode_readm_offs_imm<8>();  opc->src = op.rs1.reg_ptr(); opc->offs = op.rs3.imm_value(); opc->dst = op.rd.reg_ptr();
					}
				}
				else if (op.rs3.is_reg()) {
					verify(op.rs2.is_null());
					if (size == 1)
					{
						auto opc = new opcode_readm_offs<1>();  opc->src = op.rs1.reg_ptr(); opc->offs = op.rs3.reg_ptr(); opc->dst = op.rd.reg_ptr();
					}
					else if (size == 2)
					{
						auto opc = new opcode_readm_offs<2>();  opc->src = op.rs1.reg_ptr(); opc->offs = op.rs3.reg_ptr(); opc->dst = op.rd.reg_ptr();
					}
					else if (size == 4)
					{
						auto opc = new opcode_readm_offs<4>();  opc->src = op.rs1.reg_ptr(); opc->offs = op.rs3.reg_ptr(); opc->dst = op.rd.reg_ptr();
					}
					else if (size == 8)
					{
						auto opc = new opcode_readm_offs<8>();  opc->src = op.rs1.reg_ptr(); opc->offs = op.rs3.reg_ptr(); opc->dst = op.rd.reg_ptr();
					}
				}
				else {
					verify(op.rs2.is_null() && op.rs3.is_null());
					if (size == 1)
					{
						auto opc = new opcode_readm<1>();  opc->src = op.rs1.reg_ptr(); opc->dst = op.rd.reg_ptr();
					}
					else if (size == 2)
					{
						auto opc = new opcode_readm<2>();  opc->src = op.rs1.reg_ptr(); opc->dst = op.rd.reg_ptr();
					}
					else if (size == 4)
					{
						auto opc = new opcode_readm<4>();  opc->src = op.rs1.reg_ptr(); opc->dst = op.rd.reg_ptr();
					}
					else if (size == 8)
					{
						auto opc = new opcode_readm<8>();  opc->src = op.rs1.reg_ptr(); opc->dst = op.rd.reg_ptr();
					}
				}
			}
			break;

			case shop_writem:
			{
				u32 size = op.flags & 0x7f;
				
				if (op.rs1.is_imm()) {
					verify(op.rs3.is_null());
					if (size == 1)
					{
						auto opc = new opcode_writem_imm<1>();  opc->src = op.rs1.imm_value(); opc->src2 = op.rs2.reg_ptr();
					}
					else if (size == 2)
					{
						auto opc = new opcode_writem_imm<2>();  opc->src = op.rs1.imm_value(); opc->src2 = op.rs2.reg_ptr();
					}
					else if (size == 4)
					{
						auto opc = new opcode_writem_imm<4>();  opc->src = op.rs1.imm_value(); opc->src2 = op.rs2.reg_ptr();
					}
					else if (size == 8)
					{
						auto opc = new opcode_writem_imm<8>();  opc->src = op.rs1.imm_value(); opc->src2 = op.rs2.reg_ptr();
					}
				}
				else if (op.rs3.is_imm()) {
					if (size == 1)
					{
						auto opc = new opcode_writem_offs_imm<1>();  opc->src = op.rs1.reg_ptr(); opc->offs = op.rs3.imm_value(); opc->src2 = op.rs2.reg_ptr();
					}
					else if (size == 2)
					{
						auto opc = new opcode_writem_offs_imm<2>();  opc->src = op.rs1.reg_ptr(); opc->offs = op.rs3.imm_value(); opc->src2 = op.rs2.reg_ptr();
					}
					else if (size == 4)
					{
						auto opc = new opcode_writem_offs_imm<4>();  opc->src = op.rs1.reg_ptr(); opc->offs = op.rs3.imm_value(); opc->src2 = op.rs2.reg_ptr();
					}
					else if (size == 8)
					{
						auto opc = new opcode_writem_offs_imm<8>();  opc->src = op.rs1.reg_ptr(); opc->offs = op.rs3.imm_value(); opc->src2 = op.rs2.reg_ptr();
					}
				}
				else if (op.rs3.is_reg()) {
					if (size == 1)
					{
						auto opc = new opcode_writem_offs<1>();  opc->src = op.rs1.reg_ptr(); opc->offs = op.rs3.reg_ptr(); opc->src2 = op.rs2.reg_ptr();
					}
					else if (size == 2)
					{
						auto opc = new opcode_writem_offs<2>();  opc->src = op.rs1.reg_ptr(); opc->offs = op.rs3.reg_ptr(); opc->src2 = op.rs2.reg_ptr();
					}
					else if (size == 4)
					{
						auto opc = new opcode_writem_offs<4>();  opc->src = op.rs1.reg_ptr(); opc->offs = op.rs3.reg_ptr(); opc->src2 = op.rs2.reg_ptr();
					}
					else if (size == 8)
					{
						auto opc = new opcode_writem_offs<8>();  opc->src = op.rs1.reg_ptr(); opc->offs = op.rs3.reg_ptr(); opc->src2 = op.rs2.reg_ptr();
					}
				}
				else {
					verify(op.rs3.is_null());
					if (size == 1)
					{
						auto opc = new opcode_writem<1>();  opc->src = op.rs1.reg_ptr(); opc->src2 = op.rs2.reg_ptr();
					}
					else if (size == 2)
					{
						auto opc = new opcode_writem<2>();  opc->src = op.rs1.reg_ptr(); opc->src2 = op.rs2.reg_ptr();
					}
					else if (size == 4)
					{
						auto opc = new opcode_writem<4>();  opc->src = op.rs1.reg_ptr(); opc->src2 = op.rs2.reg_ptr();
					}
					else if (size == 8)
					{
						auto opc = new opcode_writem<8>();  opc->src = op.rs1.reg_ptr(); opc->src2 = op.rs2.reg_ptr();
					}
				}
			}
			break;
			
			default:
				shil_chf[op.op](&op);
				break;
			}
		}

		//Block end opcode
		{
			opcodeExec* op;

			#define CASEWS(n) case n: op = (new opcode_blockend<n>())->setup(block); break

			switch (block->BlockType) {
				CASEWS(BET_StaticJump);
				CASEWS(BET_StaticCall);
				CASEWS(BET_StaticIntr);

				CASEWS(BET_DynamicJump);
				CASEWS(BET_DynamicCall);
				CASEWS(BET_DynamicRet);
				CASEWS(BET_DynamicIntr);

				CASEWS(BET_Cond_0);
				CASEWS(BET_Cond_1);
			}
		}

		fnlist_cnts[fnList]++;
		//printf("fnlist %s\n", fnList.c_str());

		if (fastblocks[fnList]) {
			auto blockCtor = fastblocks[fnList];
			blockCtor(ptrs.fnb, ptrs.fnb->cc);
			//printf("Fastblock found %08X\n", next_pc);
		}
	}

	CC_pars_t CC_pars;
	void* ccfn;

	void ngen_CC_Start(shil_opcode* op)
	{
		CC_pars.clear();
		ccfn = 0;
	}

	void ngen_CC_param(shil_opcode& op, shil_param& prm, CanonicalParamType tp) {
		CC_PS t = { tp, &prm };
		CC_pars.push_back(t);
	}

	void ngen_CC_Call(shil_opcode*op, void* function)
	{
		ccfn = function;
	}

	void ngen_CC_Finish(shil_opcode* op)
	{
		string nm = "";
		for (auto m : CC_pars) {
			nm += (char)(m.type + 'a');
			nm += (char)(m.prm->type + 'A');
		}
		if (!nm.size())
			nm = "vV";
		
		if (unmap.count(nm)) {
			auto exec = unmap[nm](CC_pars, ccfn, op);
		}
		else {
			printf("IMPLEMENT CC_CALL CLASS: %s\n", nm.c_str());
			auto exec = new opcodeDie();
		}
	}

};

#if 0
struct block_0 : fnblock_base {
	opcodeExec base_ptr[0];
	
	void execute()
	{
		cycle_counter -= cc;
		auto ptr_bytes = reinterpret_cast<u8*>(base_ptr);

		reinterpret_cast<struct opcode_blockend<0>*>(ptr_bytes)->execute();
		ptr_bytes += sizeof(struct opcode_blockend<0>);
	}

	static void* create(void* buffer, int cc) {
		auto rv = new (buffer) block_0();
		rv->cc = cc;
		return rv;
	}
};
#endif


void init_blocks(map<string, void* (*)(void*, int)>* dest) {
	void init_blocks_9600(map<string, void* (*)(void*, int)> * dest);
	init_blocks_9600(dest);
}
BlockCompiler* compiler;



struct CPPNGenBackend: NGenBackend
{
	bool Init()
	{
		init_blocks(&fastblocks);
		
		FILE* f = fopen("fnlist.txt", "w");
		for (auto kv : fnlist_cnts) {
			fprintf(f, "%s\n", kv.first.c_str());
		}
		fclose(f);

		this->Mainloop = &rec_mainloop;
		
		auto failedtofind = fnnCtor(0, 1).fnb;
		auto op = new opcodeCompileBlocks();

		emit_SetBaseAddr();

		this->FailedToFindBlock = reinterpret_cast<DynarecCodeEntryPtr>(failedtofind);
		return true;
	}

	void GetFeatures(ngen_features* dst)
	{
		dst->InterpreterFallback = false;
		dst->OnlyDynamicEnds = false;
	}

	RuntimeBlockInfo* AllocateBlock()
	{
		return new DynaRBI();
	}


	void Compile(RuntimeBlockInfo* block, SmcCheckEnum smc_checks, bool reset, bool staging, bool optimise)
	{
		verify(emit_FreeSpace() >= 16 * 1024);

		compiler = new BlockCompiler();


		compiler->compile(block, smc_checks, reset, staging, optimise);

		delete compiler;
	}

	bool Rewrite(rei_host_context_t* ctx)
	{
		die("Not implemented");
		return false;
	}


	void CC_Start(shil_opcode* op)
	{
		compiler->ngen_CC_Start(op);
	}

	void CC_Param(shil_opcode* op, shil_param* par, CanonicalParamType tp)
	{
		compiler->ngen_CC_param(*op, *par, tp);
	}

	void CC_Call(shil_opcode*op, void* function)
	{
		compiler->ngen_CC_Call(op, function);
	}

	void CC_Finish(shil_opcode* op)
	{
		compiler->ngen_CC_Finish(op);
	}

	void OnResetBlocks()
	{

	}
};

static NGenBackend* create_ngen_cpp() {
	return new CPPNGenBackend();
}


static auto rec_cpp = rdv_RegisterShilBackend(ngen_backend_t{"cpp", "Cached Shil Interpreter", &create_ngen_cpp });
#endif
