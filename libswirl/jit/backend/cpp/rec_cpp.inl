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

extern int cycle_counter;
extern string fnList;

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

#if BUILD_COMPILER != COMPILER_VC
#include <cxxabi.h>
static std::string demangled(char const* tname) {
	std::unique_ptr<char, void(*)(void*)>
		name{ abi::__cxa_demangle(tname, 0, 0, nullptr), std::free };
	return { name.get() };
}
#else
#define demangled(x) x
#endif

#define GEN_EXCEC(klass) \
	klass() : opcodeExec([](u8* p) { \
		auto ctx = reinterpret_cast<klass*>(p); \
		ctx->execute(); \
		return sizeof(klass); \
	}) { \
		fnList += demangled(typeid(klass).name()); \
		fnList += ";"; \
	}

#define GEN_EXCEC_PLUS(klass, plus) \
	klass() : opcodeExec([](u8* p) { \
		auto ctx = reinterpret_cast<klass*>(p); \
		ctx->execute(); \
		return sizeof(klass) + plus; \
	})  { \
		fnList += typeid(klass).name(); \
		fnList += ";"; \
	}

class opcodeDie : public opcodeExec {
public:

	GEN_EXCEC(opcodeDie)

		INLINE void execute() {
		die("death opcode");
	}
};

class opcodeCompileBlocks : public opcodeExec {
public:

	GEN_EXCEC(opcodeCompileBlocks)

		INLINE void execute() {
		rdv_FailedToFindBlock(Sh4cntx.pc);
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
	struct opex2 : public opcodeExec {

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
			*rd = ((u32(*)(u32, u32)) & T::impl)(*rs1, rs2);
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
			*rd = ((u32(*)(u32, u32)) & T::impl)(*rs1, *rs2);
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
			*rd = ((u32(*)(u32)) & T::impl)(*rs1);
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
			((void(*)(u32)) & T::impl)(*rs1);
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
			*rd = ((u32(*)(u32, u32, u32)) & T::impl)(*rs1, *rs2, *rs3);
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
			auto rv = ((u64(*)(u32, u32, u32)) & T::impl)(*rs1, *rs2, *rs3);

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
			auto rv = ((u64(*)(u32, u32)) & T::impl)(*rs1, *rs2);
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
			*rd = ((f32(*)(f32, f32, f32)) & T::impl)(*rs1, *rs2, *rs3);
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
			*rd = ((f32(*)(f32, f32)) & T::impl)(*rs1, *rs2);
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
			*rd = ((u32(*)(f32, f32)) & T::impl)(*rs1, *rs2);
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
			*rd = ((u32(*)(f32)) & T::impl)(*rs1);
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
			*rd = ((f32(*)(u32)) & T::impl)(*rs1);
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
			*rd = ((f32(*)(f32)) & T::impl)(*rs1);
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
			((void(*)(f32*, u32)) & T::impl)(rd, *rs1);
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
			((void(*)(f32*, f32*, f32*)) & T::impl)(rd, rs1, rs2);
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
			*rd = ((f32(*)(f32*, f32*)) & T::impl)(rs1, rs2);
		}
	};
};

struct opcode_cc_vV {
	template<typename T> struct opex2 : public opcodeExec {
		void setup(const CC_pars_t& prms, void* fun) {

		}

		GEN_EXCEC(opex2) INLINE void execute() {
			((void(*)()) & T::impl)();
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
			((void(*)(u64*, u64*, u64*, u64*)) & T::impl)(rd, rd2, rs1, rs2);
		}
	};
};

struct opcode_ifb_pc : public opcodeExec {
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

struct opcode_jdyn : public opcodeExec {
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
	void* ptr;
	u8 code[sz];

	opcodeExec* setup(RuntimeBlockInfo* block) {
		this->block = block;
		ptr = GetMemPtr(block->addr, block->sh4_code_size);
		verify(ptr != nullptr);
		verify(sz != -1);

		memcpy(&code[0], ptr, sz);

		return this;
	}

	GEN_EXCEC(opcode_check_block)
		INLINE void execute() {
		switch (sz)
		{
		case 4:
			if (*(u32*)ptr != *(u32*)&code[0])
				ngen_blockcheckfail(block->addr);
			break;
		case 6:
			if (*(u32*)ptr != *(u32*)&code[0] || *((u16*)ptr + 2) != *((u16*)&code[0] + 2))
				ngen_blockcheckfail(block->addr);
			break;
		case 8:
			if (*(u32*)ptr != *(u32*)&code[0] || *((u32*)ptr + 1) != *((u32*)&code[0] + 1))
				ngen_blockcheckfail(block->addr);
			break;
		default:
			if (memcmp(ptr, &code[0], sz) != 0)
				ngen_blockcheckfail(block->addr);
			break;
		}
	}
};


class fnblock_base {
public:
	int cc;
	int ncntd;

	virtual void execute() = 0;

};

