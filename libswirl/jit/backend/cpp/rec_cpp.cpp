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

#define SHIL_MODE 1
#include "hw/sh4/dyna/shil_canonical.h"

#define MIPS_COUNTER 0

int cycle_counter;
extern int mips_counter;

typedef void (*DynarecCodeEntryCppPtr)(void* prm);
static void* fnb_lookup[FPCB_SIZE];
#define FNB_FROM_PC(pc) fnb_lookup[((pc)>>1)&FPCB_MASK]

struct DynaRBI : RuntimeBlockInfo
{
	virtual u32 Relink() {
		//verify(false);
		return 0;
	}

	virtual void Relocate(void* dst) {
		verify(false);
	}

	virtual void Discard() {
		verify(FNB_FROM_PC(addr) != 0);
		FNB_FROM_PC(addr) = 0;

		return RuntimeBlockInfo::Discard();
	}
};


void rec_mainloop(void* v_cntx)
{
	Sh4RCB* ctx = (Sh4RCB*)((u8*)v_cntx - sizeof(Sh4RCB));

	cycle_counter = 0;

	while (sh4_int_bCpuRun) {
		cycle_counter += SH4_TIMESLICE;
		do {
			auto rcb = reinterpret_cast<DynarecCodeEntryCppPtr>(bm_GetCode(ctx->cntx.pc));

			void* fnb = FNB_FROM_PC(ctx->cntx.pc);

			rcb(fnb);
		} while (cycle_counter > 0);

		if (UpdateSystem()) {
			rdv_DoInterrupts_pc(ctx->cntx.pc);
		}
	}
}

static void ngen_blockcheckfail(u32 pc) {
	printf("REC CPP: SMC invalidation at %08X\n", pc);
	rdv_BlockCheckFail(pc);
}

class opcodeExec {
	public:
		size_t(*executeWithSizeCB)(u8* p);

		opcodeExec(size_t(*executeWithSizeCB)(u8* p)) : executeWithSizeCB(executeWithSizeCB) { }
		
		opcodeExec() : executeWithSizeCB(nullptr) { }

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

#define GEN_EXCEC(klass) \
	klass() : opcodeExec([](u8* p) { \
		auto ctx = reinterpret_cast<klass*>(p); \
		ctx->execute(); \
		return sizeof(klass); \
	}) {}

class opcodeDie : public opcodeExec {
public:
	
	GEN_EXCEC(opcodeDie)

	INLINE void execute() {
		die("death opcode");
	}
};

struct CC_PS
{
	CanonicalParamType type;
	shil_param* prm;
};

typedef vector<CC_PS> CC_pars_t;


struct opcode_cc_aBaCbC {
	template <typename T>
	struct opex2 : public opcodeExec  {
		
		u32 rs2;
		u32* rs1;
		u32* rd;

		void setup(const CC_pars_t& prms, void* fun) {
			rs2 = prms[0].prm->imm_value();
			rs1 = prms[1].prm->reg_ptr();
			rd = prms[2].prm->reg_ptr();
			verify(prms.size() == 3);
		}

		GEN_EXCEC(opex2)
		INLINE void execute() {
			*rd = ((u32(*)(u32, u32))&T::impl)(*rs1, rs2);
		}
	};
};

struct opcode_cc_aCaCbC {
	template<typename T> struct opex2 : public opcodeExec {
		u32* rs1;
		u32* rs2;
		u32* rd;

		void setup(const CC_pars_t& prms, void* fun) {
			rs2 = prms[0].prm->reg_ptr();
			rs1 = prms[1].prm->reg_ptr();
			rd = prms[2].prm->reg_ptr();
			verify(prms.size() == 3);
		}

		GEN_EXCEC(opex2) INLINE void execute() {
			*rd = ((u32(*)(u32, u32))&T::impl)(*rs1, *rs2);
		}
	};
};

struct opcode_cc_aCbC {
	template<typename T> struct opex2 : public opcodeExec {
		u32* rs1;
		u32* rd;

		void setup(const CC_pars_t& prms, void* fun) {
			rs1 = prms[0].prm->reg_ptr();
			rd = prms[1].prm->reg_ptr();
			verify(prms.size() == 2);
		}
	
		GEN_EXCEC(opex2) INLINE void execute() {
			*rd = ((u32(*)(u32))&T::impl)(*rs1);
		}
	};
};

struct opcode_cc_aC {
	template<typename T> struct opex2 : public opcodeExec {
		u32* rs1;

		void setup(const CC_pars_t& prms, void* fun) {
			rs1 = prms[0].prm->reg_ptr();
			verify(prms.size() == 1);
		}
	
		GEN_EXCEC(opex2) INLINE void execute() {
			((void(*)(u32))&T::impl)(*rs1);
		}
	};
};

struct opcode_cc_aCaCaCbC {
	template<typename T> struct opex2 : public opcodeExec {
		u32* rs1;
		u32* rs2;
		u32* rs3;
		u32* rd;

		void setup(const CC_pars_t& prms, void* fun) {
			rs3 = prms[0].prm->reg_ptr();
			rs2 = prms[1].prm->reg_ptr();
			rs1 = prms[2].prm->reg_ptr();
			rd = prms[3].prm->reg_ptr();
			verify(prms.size() == 4);
		}

		GEN_EXCEC(opex2) INLINE void execute() {
			*rd = ((u32(*)(u32, u32, u32))&T::impl)(*rs1, *rs2, *rs3);
		}
	};
};

struct opcode_cc_aCaCaCcCdC {
	//split this to two cases, u64 and u64L/u32H
	template<typename T> struct opex2 : public opcodeExec {
		u32* rs1;
		u32* rs2;
		u32* rs3;
		u32* rd;
		u32* rd2;

		void setup(const CC_pars_t& prms, void* fun) {
			rs3 = prms[0].prm->reg_ptr();
			rs2 = prms[1].prm->reg_ptr();
			rs1 = prms[2].prm->reg_ptr();
			rd = prms[3].prm->reg_ptr();
			rd2 = prms[4].prm->reg_ptr();

			//verify((u64*)(rd2 - 1) == rd);
			verify(prms.size() == 5);
		}

		GEN_EXCEC(opex2) INLINE void execute() {
			auto rv = ((u64(*)(u32, u32, u32))&T::impl)(*rs1, *rs2, *rs3);

			*rd = (u32)rv;
			*rd2 = rv >> 32;
		}
	};

};

struct opcode_cc_aCaCcCdC {
	template<typename T> struct opex2 : public opcodeExec {
		u32* rs1;
		u32* rs2;
		u32* rd;
		u32* rd2;

		void setup(const CC_pars_t& prms, void* fun) {
			rs2 = prms[0].prm->reg_ptr();
			rs1 = prms[1].prm->reg_ptr();
			rd = prms[2].prm->reg_ptr();
			rd2 = prms[3].prm->reg_ptr();

			verify(prms.size() == 4);
		}

		GEN_EXCEC(opex2) INLINE void execute() {
			auto rv = ((u64(*)(u32, u32))&T::impl)(*rs1, *rs2);
			*rd = (u32)rv;
			*rd2 = rv >> 32;
		}
	};
};

struct opcode_cc_eDeDeDfD {
	template<typename T> struct opex2 : public opcodeExec {
		f32* rs1;
		f32* rs2;
		f32* rs3;
		f32* rd;

		void setup(const CC_pars_t& prms, void* fun) {
			rs3 = (f32*)prms[0].prm->reg_ptr();
			rs2 = (f32*)prms[1].prm->reg_ptr();
			rs1 = (f32*)prms[2].prm->reg_ptr();
			rd = (f32*)prms[3].prm->reg_ptr();
		}

		GEN_EXCEC(opex2) INLINE void execute() {
			*rd = ((f32(*)(f32, f32, f32))&T::impl)(*rs1, *rs2, *rs3);
		}
	};

};

struct opcode_cc_eDeDfD {
	template<typename T> struct opex2 : public opcodeExec {
		f32* rs1;
		f32* rs2;
		f32* rd;

		void setup(const CC_pars_t& prms, void* fun) {
			rs2 = (f32*)prms[0].prm->reg_ptr();
			rs1 = (f32*)prms[1].prm->reg_ptr();
			rd = (f32*)prms[2].prm->reg_ptr();
		}

		GEN_EXCEC(opex2) INLINE void execute() {
			*rd = ((f32(*)(f32, f32))&T::impl)(*rs1, *rs2);
		}
	};
};

struct opcode_cc_eDeDbC {
	template<typename T> struct opex2 : public opcodeExec {
		f32* rs1;
		f32* rs2;
		u32* rd;

		void setup(const CC_pars_t& prms, void* fun) {
			rs2 = (f32*)prms[0].prm->reg_ptr();
			rs1 = (f32*)prms[1].prm->reg_ptr();
			rd = (u32*)prms[2].prm->reg_ptr();
		}

		GEN_EXCEC(opex2) INLINE void execute() {
			*rd = ((u32(*)(f32, f32))&T::impl)(*rs1, *rs2);
		}
	};
};

struct opcode_cc_eDbC {
	template<typename T> struct opex2 : public opcodeExec {
		f32* rs1;
		u32* rd;

		void setup(const CC_pars_t& prms, void* fun) {
			rs1 = (f32*)prms[0].prm->reg_ptr();
			rd = (u32*)prms[1].prm->reg_ptr();
		}

		GEN_EXCEC(opex2) INLINE void execute() {
			*rd = ((u32(*)(f32))&T::impl)(*rs1);
		}
	};
};

struct opcode_cc_aCfD {
	template<typename T> struct opex2 : public opcodeExec {
		u32* rs1;
		f32* rd;

		void setup(const CC_pars_t& prms, void* fun) {
			rs1 = (u32*)prms[0].prm->reg_ptr();
			rd = (f32*)prms[1].prm->reg_ptr();
		}

		GEN_EXCEC(opex2) INLINE void execute() {
			*rd = ((f32(*)(u32))&T::impl)(*rs1);
		}
	};
};

struct opcode_cc_eDfD {
	template<typename T> struct opex2 : public opcodeExec {
		f32* rs1;
		f32* rd;
		void setup(const CC_pars_t& prms, void* fun) {
			rs1 = (f32*)prms[0].prm->reg_ptr();
			rd = (f32*)prms[1].prm->reg_ptr();
		}

		GEN_EXCEC(opex2) INLINE void execute() {
			*rd = ((f32(*)(f32))&T::impl)(*rs1);
		}
	};
};

struct opcode_cc_aCgE {
	template<typename T> struct opex2 : public opcodeExec {
		u32* rs1;
		f32* rd;

		void setup(const CC_pars_t& prms, void* fun) {
			rs1 = (u32*)prms[0].prm->reg_ptr();
			rd = (f32*)prms[1].prm->reg_ptr();
		}

		GEN_EXCEC(opex2) INLINE void execute() {
			((void(*)(f32*, u32))&T::impl)(rd, *rs1);
		}
	};
};

struct opcode_cc_gJgHgH {
	template<typename T> struct opex2 : public opcodeExec {
		f32* rs2;
		f32* rs1;
		f32* rd;

		void setup(const CC_pars_t& prms, void* fun) {
			rs2 = (f32*)prms[0].prm->reg_ptr();
			rs1 = (f32*)prms[1].prm->reg_ptr();
			rd = (f32*)prms[2].prm->reg_ptr();
		}

		GEN_EXCEC(opex2) INLINE void execute() {
			((void(*)(f32*, f32*, f32*))&T::impl)(rd, rs1, rs2);
		}
	};
};

struct opcode_cc_gHgHfD {
	template<typename T> struct opex2 : public opcodeExec {
		f32* rs2;
		f32* rs1;
		f32* rd;

		void setup(const CC_pars_t& prms, void* fun) {
			rs2 = (f32*)prms[0].prm->reg_ptr();
			rs1 = (f32*)prms[1].prm->reg_ptr();
			rd = (f32*)prms[2].prm->reg_ptr();
		}

		GEN_EXCEC(opex2) INLINE void execute() {
			*rd = ((f32(*)(f32*, f32*))&T::impl)(rs1, rs2);
		}
	};
};

struct opcode_cc_vV {
	template<typename T> struct opex2 : public opcodeExec {
		void setup(const CC_pars_t& prms, void* fun) {
	
		}

		GEN_EXCEC(opex2) INLINE void execute() {
			((void(*)())&T::impl)();
		}
	};
};

//u64* fd1,u64* fd2,u64* fs1,u64* fs2
//slightly violates the type, as it's FV4PTR but we pass u64*
struct opcode_cc_gJgJgJgJ {
	template<typename T> struct opex2 : public opcodeExec {
		u64* rs2;
		u64* rs1;
		u64* rd;
		u64* rd2;

		void setup(const CC_pars_t& prms, void* fun) {
			rs2 = (u64*)prms[0].prm->reg_ptr();
			rs1 = (u64*)prms[1].prm->reg_ptr();
			rd2 = (u64*)prms[2].prm->reg_ptr();
			rd = (u64*)prms[3].prm->reg_ptr();
		}
		
		GEN_EXCEC(opex2) INLINE void execute() {
			((void(*)(u64*, u64*, u64*, u64*))&T::impl)(rd, rd2, rs1, rs2);
		}
	};
};

struct opcode_ifb_pc : public opcodeExec{
	OpCallFP* oph;
	u32 pc;
	u16 opcode;
	 
	GEN_EXCEC(opcode_ifb_pc) INLINE void execute() {
		next_pc = pc;
		oph(opcode);
	}
};

struct opcode_ifb : public opcodeExec {
	OpCallFP* oph;
	u16 opcode;

	GEN_EXCEC(opcode_ifb) INLINE void execute() {
		oph(opcode);
	}
};

struct opcode_jdyn : public opcodeExec{
	u32* src;
	GEN_EXCEC(opcode_jdyn) INLINE void execute() {
		Sh4cntx.jdyn = *src;
	}
};

struct opcode_jdyn_imm : public opcodeExec {
	u32* src;
	u32 imm;
	GEN_EXCEC(opcode_jdyn_imm) INLINE void execute() {
		Sh4cntx.jdyn = *src + imm;
	}
};

struct opcode_mov32 : public opcodeExec {
	u32* src;
	u32* dst;
	
	GEN_EXCEC(opcode_mov32) INLINE void execute() {
		*dst = *src;
	}
};

struct opcode_mov32_imm : public opcodeExec {
	u32 src;
	u32* dst;

	GEN_EXCEC(opcode_mov32_imm) INLINE void execute() {
		*dst = src;
	}
};

struct opcode_mov64 : public opcodeExec {
	u64* src;
	u64* dst;

	GEN_EXCEC(opcode_mov64) INLINE void execute() {
		*dst = *src;
	}
};

#define do_readm(d, a, sz) do { if (sz == 1) { *d = (s32)(s8)ReadMem8(a); } else if (sz == 2) { *d = (s32)(s16)ReadMem16(a); } \
								else if (sz == 4) { *d = ReadMem32(a);} else if (sz == 8) { *(u64*)d = ReadMem64(a); } \
							  } while(0)
template <int sz>
struct opcode_readm : public opcodeExec {
	u32* src;
	u32* dst;

	GEN_EXCEC(opcode_readm) INLINE void execute() {
		auto a = *src;
		do_readm(dst, a, sz);
	}
};

template <int sz, typename T>
struct opcode_readm_imm_ptr : public opcodeExec {
	u8* src;
	u32* dst;

	GEN_EXCEC(opcode_readm_imm_ptr) INLINE void execute() {
		T a = *reinterpret_cast<T*>(src);
		if (sz <= 4)
		{
			*dst = (s32)a;
		}
		else
		{
			*(u64*)dst = a;
		}
	}
};

template <int sz>
struct opcode_readm_imm : public opcodeExec {
	u32 src;
	u32* dst;

	GEN_EXCEC(opcode_readm_imm) INLINE void execute() {
		auto a = src;
		do_readm(dst, a, sz);
	}
};

template <int sz>
struct opcode_readm_offs : public opcodeExec {
	u32* src;
	u32* dst;
	u32* offs;

	GEN_EXCEC(opcode_readm_offs) INLINE void execute() {
		auto a = *src + *offs;
		do_readm(dst, a, sz);
	}
};

template <int sz>
struct opcode_readm_offs_imm : public opcodeExec {
	u32* src;
	u32* dst;
	u32 offs;

	GEN_EXCEC(opcode_readm_offs_imm) INLINE void execute() {
		auto a = *src + offs;
		do_readm(dst, a, sz);
	}
};

#define do_writem(d, a, sz) do { if (sz == 1) { WriteMem8(a, *d);} else if (sz == 2) { WriteMem16(a, *d); } \
										else if (sz == 4) { WriteMem32(a, *d);} else if (sz == 8) { WriteMem64(a, *(u64*)d); } \
							  } while(0)
template <int sz>
struct opcode_writem : public opcodeExec {
	u32* src;
	u32* src2;

	GEN_EXCEC(opcode_writem) INLINE void execute() {
		auto a = *src;
		do_writem(src2, a, sz);
	}
};

template <int sz>
struct opcode_writem_imm : public opcodeExec {
	u32 src;
	u32* src2;

	GEN_EXCEC(opcode_writem_imm) INLINE void execute() {
		auto a = src;
		do_writem(src2, a, sz);
	}
};

template <int sz>
struct opcode_writem_offs : public opcodeExec {
	u32* src;
	u32* src2;
	u32* offs;

	GEN_EXCEC(opcode_writem_offs) INLINE void execute() {
		auto a = *src + *offs;
		do_writem(src2, a, sz);
	}
};

template <int sz>
struct opcode_writem_offs_imm : public opcodeExec {
	u32* src;
	u32* src2;
	u32 offs;

	GEN_EXCEC(opcode_writem_offs_imm) INLINE void execute() {
		auto a = *src + offs;
		do_writem(src2, a, sz);
	}
};

template<int end_type>
struct opcode_blockend : public opcodeExec {
	int next_pc_value;
	int branch_pc_value;
	u32* jdyn;

	opcodeExec* setup(RuntimeBlockInfo* block) {
		next_pc_value = block->NextBlock;
		branch_pc_value = block->BranchBlock;

		jdyn = &Sh4cntx.jdyn;
		if (!block->has_jcond && BET_GET_CLS(block->BlockType) == BET_CLS_COND) {
			jdyn = &sr.T;
		}
		return this;
	}

	GEN_EXCEC(opcode_blockend) INLINE void execute() {
		//do whatever
		

		switch (end_type) {

		case BET_StaticJump:
		case BET_StaticCall:
			next_pc = branch_pc_value;
			break;

		case BET_Cond_0:
			if (*jdyn != 0)
				next_pc = next_pc_value;
			else
				next_pc = branch_pc_value;
			break;

		case BET_Cond_1:
			if (*jdyn != 1)
				next_pc = next_pc_value;
			else
				next_pc = branch_pc_value;
			break;

		case BET_DynamicJump:
		case BET_DynamicCall:
		case BET_DynamicRet:
			next_pc = *jdyn;
			break;

		case BET_DynamicIntr:
		case BET_StaticIntr:
			if (end_type == BET_DynamicIntr)
				next_pc = *jdyn;
			else
				next_pc = next_pc_value;

			UpdateINTC();
			break;

		default:
			die("NOT GONNA HAPPEN TODAY, ALRIGHY?");
		}
	}
};

template <int sz>
struct opcode_check_block : public opcodeExec {
	RuntimeBlockInfo* block;
	vector<u8> code;
	void* ptr;

	opcodeExec* setup(RuntimeBlockInfo* block) {
		this->block = block;
		ptr = GetMemPtr(block->addr, block->sh4_code_size);
		verify(ptr != nullptr);
		code.resize(sz == -1 ? block->sh4_code_size : sz);
		memcpy(&code[0], ptr, sz == -1 ? block->sh4_code_size : sz);

		return this;
	}

	GEN_EXCEC(opcode_check_block)
	INLINE void execute() {
		switch (sz)
		{
		case 4:
			if (*(u32 *)ptr != *(u32 *)&code[0])
				ngen_blockcheckfail(block->addr);
			break;
		case 6:
			if (*(u32 *)ptr != *(u32 *)&code[0] || *((u16 *)ptr + 2) != *((u16 *)&code[0] + 2))
				ngen_blockcheckfail(block->addr);
			break;
		case 8:
			if (*(u32 *)ptr != *(u32 *)&code[0] || *((u32 *)ptr + 1) != *((u32 *)&code[0] + 1))
				ngen_blockcheckfail(block->addr);
			break;
		default:
			if (memcmp(ptr, &code[0], sz == -1 ? block->sh4_code_size : sz) != 0)
				ngen_blockcheckfail(block->addr);
			break;
		}
	}
};

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

template <int id>
class fnblock {
public:
	int cc;
	int ncntd;

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

	static void runner(void* fnb) {
		((fnblock<id>*)fnb)->execute();
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
	void* fnb;
	void(*runner)(void* fnb);
	opcodeExec* ptrs;
};

template<int id>
fnrv fnnCtor(int cycles, int opcode_slots) {
	auto rv = new fnblock<id>();
	rv->cc = cycles;
	rv->ncntd = DREP_COUNT - opcode_slots;
	fnrv rvb = { rv, &fnblock<id>::runner, rv->ops };
	return rvb;
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

#define FNA_COUNT 512

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


typedef fnrv(*FNAFB)(int cycles, int len);

FNAFB FNA[] = { REP_512(0, &fnnCtor) };

FNAFB fnnCtor_forreal(size_t n) {
	verify(n >= 0);
	verify(n < FNA_COUNT);
	return FNA[n];
}

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

class BlockCompiler {
public:

	size_t opcode_index;
	opcodeExec* ptrsg;
	void compile(RuntimeBlockInfo* block,  SmcCheckEnum smc_checks, bool reset, bool staging, bool optimise) {
		
		static u32 id_fr = 0;
		//we need an extra one for the end opcode and optionally one more for block check
		auto block_size = block->oplist.size() + 1 + (smc_checks != NoCheck ? 1 : 0);
		auto ptrs = fnnCtor_forreal((id_fr++)%FNA_COUNT)(block->guest_cycles, block_size);

		ptrsg = ptrs.ptrs;
		verify(FNB_FROM_PC(block->addr) == 0);
		FNB_FROM_PC(block->addr) = ptrs.fnb;

		// WATCH // see rec_mainloop
		block->code = reinterpret_cast<DynarecCodeEntryPtr>(ptrs.runner);

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
			op = fnnCtor_scb_forreal(block->sh4_code_size)()(block);

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

BlockCompiler* compiler;

static void ngen_FailedToFindBlock_cpp(void* fnb) {
	verify(fnb == nullptr);
	rdv_FailedToFindBlock(Sh4cntx.pc);
}

struct CPPNGenBackend: NGenBackend
{
	bool Init()
	{
		this->Mainloop = &rec_mainloop;
		this->FailedToFindBlock = reinterpret_cast<DynarecCodeEntryPtr>(&ngen_FailedToFindBlock_cpp);
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
		memset(fnb_lookup, 0 , sizeof(fnb_lookup));
		/*
		while (dispatchb[id].fnb)
			delete dispatchb[id].fnb;
		*/
	}
};

static NGenBackend* create_ngen_cpp() {
	return new CPPNGenBackend();
}


static auto rec_cpp = rdv_RegisterShilBackend(ngen_backend_t{"cpp", "Cached Shil Interpreter", &create_ngen_cpp });
#endif
