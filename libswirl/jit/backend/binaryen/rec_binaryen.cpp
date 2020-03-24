/*
	This file is part of libswirl
*/
#include "license/bsd"


#include "build.h"

#if FEAT_SHREC == DYNAREC_JIT
#include "types.h"
#include "hw/sh4/sh4_opcode_list.h"
#include "hw/sh4/dyna/ngen.h"
#include "hw/sh4/modules/ccn.h"
#include "hw/sh4/sh4_interrupts.h"

#include "hw/sh4/sh4_core.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/sh4/sh4_rom.h"
#include "profiler/profiler.h"
#include "oslib/oslib.h"
#include "x64_regalloc.h"

#include "binaryen-c.h"

struct DynaRBI : RuntimeBlockInfo
{
	virtual u32 Relink() {
		return 0;
	}

	virtual void Relocate(void* dst) {
		verify(false);
	}
};

extern "C" {
	int cycle_counter;
}

double host_cpu_time;
u64 guest_cpu_cycles;

static void ngen_blockcheckfail(u32 pc) {
	printf("emscripten JIT: SMC invalidation at %08X\n", pc);
	rdv_BlockCheckFail(pc);
}

static void ngen_blockcheckfail2(u32 pc) {
	printf("****** ERROR********* emscripten JIT: SMC invalidation at %08X\n", pc);
	rdv_BlockCheckFail(pc);
	die("ngen_blockcheckfail2")
}

class BlockCompiler
{
	BinaryenModuleRef module;
public:
	BlockCompiler(void *buffer)
	{

	}

	void compile(RuntimeBlockInfo* block, SmcCheckEnum smc_checks, bool reset, bool staging, bool optimise)
	{
		module = BinaryenModuleCreate();

		BinaryenType v[1] = {BinaryenTypeAnyref()};
		BinaryenType params = BinaryenTypeCreate(v, 1);
		BinaryenType results = BinaryenTypeNone();

		//printf("X86_64 compiling %08x to %p\n", block->addr, emit_GetCCPtr());
		CheckBlock(smc_checks, block);
		
		//sub(dword[rip + &cycle_counter], block->guest_cycles);
		
		auto mem_base = BinaryenLocalGet(module, 0, BinaryenTypeInt64()); // assuming 64 bit pointers
		auto ctx_base = BinaryenBinary(module, BinaryenSubInt64(),  mem_base, BinaryenConst(module, -192));

		auto cycles = BinaryenLoad(module, 4, 0, 32/* cycle counter offset here*/, 0, BinaryenTypeInt32(), ctx_base);
		cycles = BinaryenBinary(module, BinaryenSubInt64(), cycles, BinaryenConst(module, block->guest_cycles));
		BinaryenStore( module, 4, 0, 32/* cycle counter offset here*/, cycles, BinaryenTypeInt32(), ctx_base);

		for (size_t i = 0; i < block->oplist.size(); i++)
		{
			shil_opcode& op  = block->oplist[i];

			switch (op.op) {

			case shop_ifb:
				/* needs to call host code
					must use an export here ?
				*/

				if (op.rs1._imm)
				{
					StoreContex32(&next_pc, op.rs2._imm);
				}


				mov(call_regs[0], op.rs3._imm);

				GenCall(OpDesc[op.rs3._imm]->oph);
				break;

			case shop_jdyn:
				{
					if (op.rs2.is_imm())
					{
						mov(ecx, regalloc.MapRegister(op.rs1));
						add(ecx, op.rs2._imm);
						mov(regalloc.MapRegister(op.rd), ecx);
					}
					else
						mov(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1));
				}
				break;

			default:
				shil_chf[op.op](&op);
				break;
			}
		}

		mov(rax, (size_t)&next_pc);

		switch (block->BlockType) {

		case BET_StaticJump:
		case BET_StaticCall:
			//next_pc = block->BranchBlock;
			mov(dword[rax], block->BranchBlock);
			break;

		case BET_Cond_0:
		case BET_Cond_1:
			{
				//next_pc = next_pc_value;
				//if (*jdyn == 0)
				//next_pc = branch_pc_value;

				mov(dword[rax], block->NextBlock);

				if (block->has_jcond)
					mov(rdx, (size_t)&Sh4cntx.jdyn);
				else
					mov(rdx, (size_t)&sr.T);

				cmp(dword[rdx], block->BlockType & 1);
				Xbyak::Label branch_not_taken;

				jne(branch_not_taken, T_SHORT);
				mov(dword[rax], block->BranchBlock);
				L(branch_not_taken);
			}
			break;

		case BET_DynamicJump:
		case BET_DynamicCall:
		case BET_DynamicRet:
			//next_pc = *jdyn;
			mov(rdx, (size_t)&Sh4cntx.jdyn);
			mov(edx, dword[rdx]);
			mov(dword[rax], edx);
			break;

		case BET_DynamicIntr:
		case BET_StaticIntr:
			if (block->BlockType == BET_DynamicIntr) {
				//next_pc = *jdyn;
				mov(rdx, (size_t)&Sh4cntx.jdyn);
				mov(edx, dword[rdx]);
				mov(dword[rax], edx);
			}
			else {
				//next_pc = next_pc_value;
				mov(dword[rax], block->NextBlock);
			}

			GenCall(UpdateINTC);
			break;

		default:
			die("Invalid block end type");
		}

		ret();

		ready();

		block->code = (DynarecCodeEntryPtr)getCode();
		block->host_code_size = (u32)getSize();

		emit_Skip((u32)getSize());
	}

	void GenReadMemoryImmediate(const shil_opcode& op) {
		bool isram = false;
		u32 size = op.flags & 0x7f;
		void* ptr = _vmem_read_const(op.rs1._imm, isram, size);

		if (isram) {
			// Immediate pointer to RAM: super-duper fast access
			mov(rax, reinterpret_cast<uintptr_t>(ptr));
			switch (size) {
			case 2:
				movsx(regalloc.MapRegister(op.rd), word[rax]);
				break;

			case 4:
				if (regalloc.IsAllocg(op.rd))
					mov(regalloc.MapRegister(op.rd), dword[rax]);
				else
					movd(regalloc.MapXRegister(op.rd), dword[rax]);
				break;

			default:
				die("Invalid immediate size");
				break;
			}
		}
		else
		{
			// Not RAM: the returned pointer is a memory handler
			mov(call_regs[0].cvt64(), (uintptr_t)sh4_cpu);
			mov(call_regs[1], op.rs1._imm);

			switch(size) {
			case 2:
				GenCall((void (*)())ptr);
				movsx(ecx, ax);
				break;

			case 4:
				GenCall((void (*)())ptr);
				mov(ecx, eax);
				break;

			default:
				die("Invalid immediate size");
				break;
			}
			host_reg_to_shil_param(op.rd, ecx);
		}
	}

	void GenMemAddr(const shil_opcode& op, const Xbyak::Reg& raddr) {
		shil_param_to_host_reg(op.rs1, raddr);
		if (!op.rs3.is_null())
		{
			if (op.rs3.is_imm())
				add(raddr, op.rs3._imm);
			else
				add(raddr, regalloc.MapRegister(op.rs3));
		}
	}

	void GenReadMemoryFast(const shil_opcode& op, size_t opid, RuntimeBlockInfo *block) {
		// Assuming 512MB addr space for now

		auto temp_reg = call_regs[2].cvt64();
		unsigned initial_size = (unsigned)getSize();
		// This addr calculation is variable sized, minimum of 14 bytes it seems.
		mov(rax, call_regs[0]);               // 3-4 bytes
		and_(rax, 0x1fffffff);                // Min 5 bytes
		mov(temp_reg, (uintptr_t)virt_ram_base);   // 6-10 bytes
		add(rax, temp_reg);   // 6-10 bytes

		void *inst_ptr = ((char*)getCode()) + getSize();
		unsigned prologue_size = (unsigned)(getSize() - initial_size);

		// Memory access is at least 3 bytes long
		u32 size = op.flags & 0x7f;
		switch(size) {
		case 1:
			movsx(regalloc.MapRegister(op.rd), byte[rax]);
			break;

		case 2:
			movsx(regalloc.MapRegister(op.rd), word[rax]);
			break;

		case 4:
			if (!op.rd.is_r32f())
				mov(regalloc.MapRegister(op.rd), dword[rax]);
			else
				movd(regalloc.MapXRegister(op.rd), dword[rax]);
			break;

		case 8:
			mov(rax, qword[rax]);
			break;
		}

		if (size == 8) {
			#ifdef EXPLODE_SPANS
				verify(op.rd.count() == 2 && regalloc.IsAllocf(op.rd, 0) && regalloc.IsAllocf(op.rd, 1));
				movd(regalloc.MapXRegister(op.rd, 0), eax);
				shr(rax, 32);
				movd(regalloc.MapXRegister(op.rd, 1), eax);
			#else
				mov(rax, (uintptr_t)op.rd.reg_ptr());
				mov(qword[rax], rax);
			#endif
		}

		// Store ptr to start of load/store, opid and size of the whole thing in bytes.
		unsigned code_size = (unsigned)(getSize() - initial_size);
		verify(code_size < 256 && prologue_size < 256);
		block->memory_accesses[inst_ptr] = { (uint16_t)opid, (uint8_t)prologue_size, (uint8_t)code_size };
	}

	void GenReadMemorySlow(const shil_opcode& op, unsigned pad_to_bytes = 0) {
		unsigned initial_size = (unsigned)getSize();

		u32 size = op.flags & 0x7f;
		if (size == 1) {
			call(mem_handlers[0]);
			movsx(ecx, al);
		}
		else if (size == 2) {
			call(mem_handlers[1]);
			movsx(ecx, ax);
		}
		else if (size == 4) {
			call(mem_handlers[2]);
			mov(ecx, eax);
		}
		else if (size == 8) {
			call(mem_handlers[3]);
			mov(rcx, rax);
		}
		else {
			die("1..8 bytes");
		}

		if (size != 8)
			host_reg_to_shil_param(op.rd, ecx);
		else {
			#ifdef EXPLODE_SPANS
				verify(op.rd.count() == 2 && regalloc.IsAllocf(op.rd, 0) && regalloc.IsAllocf(op.rd, 1));
				movd(regalloc.MapXRegister(op.rd, 0), ecx);
				shr(rcx, 32);
				movd(regalloc.MapXRegister(op.rd, 1), ecx);
			#else
				mov(rax, (uintptr_t)op.rd.reg_ptr());
				mov(qword[rax], rcx);
			#endif
		}

		// Emit extra nops to pad the access if necessary
		if (pad_to_bytes > 0) {
			verify(getSize() - initial_size <= pad_to_bytes);
			nop(pad_to_bytes - (getSize() - initial_size));
		}
	}

	void GenWriteMemorySlow(const shil_opcode& op, unsigned pad_to_bytes = 0) {
		unsigned initial_size = (unsigned)getSize();
		u32 size = op.flags & 0x7f;
		if (size == 1)
			call(mem_handlers[4]);
		else if (size == 2)
			call(mem_handlers[5]);
		else if (size == 4)
			call(mem_handlers[6]);
		else if (size == 8)
			call(mem_handlers[7]);
		else {
			die("1..8 bytes");
		}

		// Emit extra nops to pad the access if necessary
		if (pad_to_bytes > 0) {
			verify(getSize() - initial_size <= pad_to_bytes);
			nop(pad_to_bytes - (getSize() - initial_size));
		}
	}

	void GenWriteMemoryFast(const shil_opcode& op, size_t opid, RuntimeBlockInfo *block) {
		// Assuming 512MB addr space for now
		auto temp_reg = call_regs[2].cvt64();
		unsigned initial_size = (unsigned)getSize();
		// This addr calculation is variable sized, minimum of 14 bytes it seems.
		mov(rax, call_regs[0]);               // 3-4 bytes
		and_(rax, 0x1fffffff);                // Min 5 bytes
		mov(temp_reg, (uintptr_t)virt_ram_base);   // 6-10 bytes
		add(rax, temp_reg);   // 6-10 bytes

		void *inst_ptr = ((char*)getCode()) + getSize();
		unsigned prologue_size = (unsigned)(getSize() - initial_size);

		u32 size = op.flags & 0x7f;
		if (size == 1)
			mov(byte[rax], call_regs[1].cvt8());
		else if (size == 2)
			mov(word[rax], call_regs[1].cvt16());
		else if (size == 4)
			mov(dword[rax], call_regs[1]);
		else if (size == 8)
			mov(qword[rax], call_regs[1].cvt64());
		else {
			die("1..8 bytes");
		}

		// Store ptr to start of load/store, opid and size of the whole thing in bytes.
		unsigned code_size = (unsigned)(getSize() - initial_size);
		verify(code_size < 256 && prologue_size < 256);
		block->memory_accesses[inst_ptr] = { (uint16_t)opid, (uint8_t)prologue_size, (uint8_t)code_size };
	}

	void ngen_CC_Start(const shil_opcode& op)
	{
		CC_pars.clear();
	}

	void ngen_CC_param(const shil_opcode& op, const shil_param& prm, CanonicalParamType tp) {
		switch (tp)
		{

		case CPT_u32:
		case CPT_ptr:
		case CPT_f32:
		{
			CC_PS t = { tp, &prm };
			CC_pars.push_back(t);
		}
		break;


		// store from EAX
		case CPT_u64rvL:
		case CPT_u32rv:
			mov(rcx, rax);
			host_reg_to_shil_param(prm, ecx);
			break;

		case CPT_u64rvH:
			// assuming CPT_u64rvL has just been called
			shr(rcx, 32);
			host_reg_to_shil_param(prm, ecx);
			break;

		// store from xmm0
		case CPT_f32rv:
			host_reg_to_shil_param(prm, xmm0);
#ifdef EXPLODE_SPANS
			// The x86 dynarec saves to mem as well
			//mov(rax, (uintptr_t)prm.reg_ptr());
			//movd(dword[rax], xmm0);
#endif
			break;
		}
	}

	void ngen_CC_Call(const shil_opcode& op, void* function)
	{
		int regused = 0;
		int xmmused = 0;

		for (size_t i = CC_pars.size(); i-- > 0;)
		{
			verify(xmmused < 4 && regused < 4);
			const shil_param& prm = *CC_pars[i].prm;
			switch (CC_pars[i].type) {
				//push the contents

			case CPT_u32:
				shil_param_to_host_reg(prm, call_regs[regused++]);
				break;

			case CPT_f32:
				shil_param_to_host_reg(prm, call_regsxmm[xmmused++]);
				break;

				//push the ptr itself
			case CPT_ptr:
				verify(prm.is_reg());

				mov(call_regs[regused++].cvt64(), (size_t)prm.reg_ptr());

				break;
            default:
               // Other cases handled in ngen_CC_param
               break;
			}
		}
		GenCall((void (*)())function);
	}

private:
	typedef void (BlockCompiler::*X64BinaryOp)(const Xbyak::Operand&, const Xbyak::Operand&);
	typedef void (BlockCompiler::*X64BinaryFOp)(const Xbyak::Xmm&, const Xbyak::Operand&);

	void CheckBlock(SmcCheckEnum smc_checks, RuntimeBlockInfo* block) {

		switch (smc_checks) {
			case FaultCheck:
			case NoCheck:
				return;

		 	case FastCheck: {
		 		void* ptr = (void*)GetMemPtr(block->addr, 4);
                if (ptr)
				{
					mov(call_regs[0], block->addr);
					mov(rax, reinterpret_cast<uintptr_t>(ptr));
					mov(edx, *(u32*)ptr);
					cmp(dword[rax], edx);
					jne(reinterpret_cast<const void*>(CC_RX2RW(&ngen_blockcheckfail)));
				}
		 	}
		 	break;

		 	case ValidationCheck:
		 	case FullCheck: {
		 		s32 sz=block->sh4_code_size;
				u32 sa=block->addr;

				void* ptr = (void*)GetMemPtr(sa, sz > 8 ? 8 : sz);
				if (ptr)
				{
					mov(call_regs[0], block->addr);

					while (sz > 0)
					{
						mov(rax, reinterpret_cast<uintptr_t>(ptr));

						if (sz >= 8) {
							mov(rdx, *(u64*)ptr);
							cmp(qword[rax], rdx);
							sz -= 8;
							sa += 8;
						}
						else if (sz >= 4) {
							mov(edx, *(u32*)ptr);
							cmp(dword[rax], edx);
							sz -= 4;
							sa += 4;
						}
						else {
							mov(edx, *(u16*)ptr);
							cmp(word[rax],dx);
							sz -= 2;
							sa += 2;
						}
						
						if (smc_checks == FullCheck)
							jne(reinterpret_cast<const void*>(CC_RX2RW(&ngen_blockcheckfail)));
						else
							jne(reinterpret_cast<const void*>(CC_RX2RW(&ngen_blockcheckfail2)));

						ptr = (void*)GetMemPtr(sa, sz > 8 ? 8 : sz);
					}
		 		}
		 	}
		 	break;

			default:
				die("unhandled smc_checks");
		}
	}

	void GenBinaryOp(const shil_opcode &op, X64BinaryOp natop)
	{
		if (regalloc.mapg(op.rd) != regalloc.mapg(op.rs1))
			mov(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs1));
		if (op.rs2.is_imm())
		{
			mov(ecx, op.rs2._imm);
			(this->*natop)(regalloc.MapRegister(op.rd), ecx);
		}
		else
			(this->*natop)(regalloc.MapRegister(op.rd), regalloc.MapRegister(op.rs2));
	}

	void GenBinaryFOp(const shil_opcode &op, X64BinaryFOp natop)
	{
		if (regalloc.mapf(op.rd) != regalloc.mapf(op.rs1))
			movss(regalloc.MapXRegister(op.rd), regalloc.MapXRegister(op.rs1));
		(this->*natop)(regalloc.MapXRegister(op.rd), regalloc.MapXRegister(op.rs2));
	}

	template<class Ret, class... Params>
	void GenCall(Ret(*function)(Params...))
	{
#ifndef _WIN32
		// Need to save xmm registers as they are not preserved in linux/mach
		sub(rsp, 16);
		movd(ptr[rsp + 0], xmm8);
		movd(ptr[rsp + 4], xmm9);
		movd(ptr[rsp + 8], xmm10);
		movd(ptr[rsp + 12], xmm11);
#endif

		call(CC_RX2RW(function));

#ifndef _WIN32
		movd(xmm8, ptr[rsp + 0]);
		movd(xmm9, ptr[rsp + 4]);
		movd(xmm10, ptr[rsp + 8]);
		movd(xmm11, ptr[rsp + 12]);
		add(rsp, 16);
#endif
	}

	// uses eax/rax
	void shil_param_to_host_reg(const shil_param& param, const Xbyak::Reg& reg)
	{
		if (param.is_imm())
		{
			if (!reg.isXMM())
				mov(reg, param._imm);
			else
			{
				mov(eax, param._imm);
				movd((const Xbyak::Xmm &)reg, eax);
			}
		}
		else if (param.is_reg())
		{
			if (param.is_r32f())
			{
				if (!reg.isXMM())
					movd((const Xbyak::Reg32 &)reg, regalloc.MapXRegister(param));
				else
					movss((const Xbyak::Xmm &)reg, regalloc.MapXRegister(param));
			}
			else
			{
				if (!reg.isXMM())
					mov((const Xbyak::Reg32 &)reg, regalloc.MapRegister(param));
				else
					movd((const Xbyak::Xmm &)reg, regalloc.MapRegister(param));
			}
		}
		else
		{
			verify(param.is_null());
		}
	}

	// uses rax
	void host_reg_to_shil_param(const shil_param& param, const Xbyak::Reg& reg)
	{
		if (regalloc.IsAllocg(param))
		{
			if (!reg.isXMM())
				mov(regalloc.MapRegister(param), (const Xbyak::Reg32 &)reg);
			else
				movd(regalloc.MapRegister(param), (const Xbyak::Xmm &)reg);
		}
		else
		{
			if (!reg.isXMM())
				movd(regalloc.MapXRegister(param), (const Xbyak::Reg32 &)reg);
			else
				movss(regalloc.MapXRegister(param), (const Xbyak::Xmm &)reg);
		}
	}

	vector<Xbyak::Reg32> call_regs;
	vector<Xbyak::Xmm> call_regsxmm;

	struct CC_PS
	{
		CanonicalParamType type;
		const shil_param* prm;
	};
	vector<CC_PS> CC_pars;

	static const u32 float_sign_mask;
	static const u32 float_abs_mask;
	static const f32 cvtf2i_pos_saturation;
	static void *mem_handlers[8]; // Read/Write handlers for every size
};

const u32 BlockCompiler::float_sign_mask = 0x80000000;
const u32 BlockCompiler::float_abs_mask = 0x7fffffff;
const f32 BlockCompiler::cvtf2i_pos_saturation = 2147483520.0f;		// IEEE 754: 0x4effffff;
void *BlockCompiler::mem_handlers[8];


static void ngen_FailedToFindBlock_cpp() {
	rdv_FailedToFindBlock(Sh4cntx.pc);
}

struct X64NGenBackend : NGenBackend
{
	BlockCompiler* compiler;

	void Compile(RuntimeBlockInfo* block, SmcCheckEnum smc_checks, bool reset, bool staging, bool optimise)
	{
		verify(emit_FreeSpace() >= 16 * 1024);

		compiler = new BlockCompiler(emit_GetCCPtr());
		compiler->compile(block, smc_checks, reset, staging, optimise);
		delete compiler;
	}

	bool Rewrite(rei_host_context_t* ctx)
	{
		die("Not supported")
		return false;
	}

	bool Init()
	{
		// Here we emit some trampolines for Mem Read/Write functions.
		// The purpose is making the calls smaller so that we can have rewrites.
		// This is inspired on the 32 bit rec, that does (almost) the same.

		{
			BlockCompiler comp(emit_GetCCPtr());
			comp.compile_trampolines();
		}

		// Align the start to 32 bytes for perf.
		while (((uintptr_t)emit_GetCCPtr()) & 31)
			emit_Skip(1);

		// Generate the mainloop logic here, so that it is more portable.
		{
			this->Mainloop = (MainloopFnPtr_t)emit_GetCCPtr();
			BlockCompiler comp(emit_GetCCPtr());
			comp.build_mainloop();
		}

		// To prevent code cache reset from wiping these out.
		emit_SetBaseAddr();

		this->FailedToFindBlock = &ngen_FailedToFindBlock_cpp;

		return true;
	}

	void CC_Start(shil_opcode* op)
	{
		compiler->ngen_CC_Start(*op);
	}

	void CC_Param(shil_opcode* op, shil_param* par, CanonicalParamType tp)
	{
		compiler->ngen_CC_param(*op, *par, tp);
	}

	void CC_Call(shil_opcode* op, void* function)
	{
		compiler->ngen_CC_Call(*op, function);
	}

	void CC_Finish(shil_opcode* op)
	{
	}

	RuntimeBlockInfo* AllocateBlock()
	{
		return new DynaRBI();
	}

	void OnResetBlocks()
	{
	}

	void GetFeatures(ngen_features* dst)
	{
		dst->InterpreterFallback = true;
		dst->OnlyDynamicEnds = true;
	}
	
};

static NGenBackend* create_ngen_binaryen() {
	return new BinaryenNGenBackend();
}

static auto rec_x64 = rdv_RegisterShilBackend(ngen_backend_t{"binaryen", "binaryen wasm backend", &create_ngen_binaryen });
#endif
