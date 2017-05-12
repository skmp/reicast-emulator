#include "deps/xbyak/xbyak.h"

#include "types.h"

#if FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_X64
#include "hw/sh4/sh4_opcode_list.h"
#include "hw/sh4/modules/ccn.h"
#include "hw/sh4/sh4_interrupts.h"

#include "hw/sh4/sh4_core.h"
#include "hw/sh4/dyna/ngen.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/sh4/dyna/regalloc.h"

extern int cycle_counter;

class BlockCompilerx64 : public Xbyak::CodeGenerator{
public:

	vector<Xbyak::Reg32> call_regs;
	vector<Xbyak::Reg64> call_regs64;
	vector<Xbyak::Xmm> call_regsxmm;

	BlockCompilerx64() : Xbyak::CodeGenerator(64 * 1024, emit_GetCCPtr()) {
#ifdef _WIN32
      call_regs.push_back(ecx);
      call_regs.push_back(edx);
      call_regs.push_back(r8d);
      call_regs.push_back(r9d);

      call_regs64.push_back(rcx);
      call_regs64.push_back(rdx);
      call_regs64.push_back(r8);
      call_regs64.push_back(r9);
#else
      call_regs.push_back(edi);
      call_regs.push_back(esi);
      call_regs.push_back(edx);
      call_regs.push_back(ecx);

      call_regs64.push_back(rdi);
      call_regs64.push_back(rsi);
      call_regs64.push_back(rdx);
      call_regs64.push_back(rcx);
#endif

		call_regsxmm.push_back(xmm0);
		call_regsxmm.push_back(xmm1);
		call_regsxmm.push_back(xmm2);
		call_regsxmm.push_back(xmm3);
	}

#define sh_to_reg(prm, op, rd) \
			if (prm.is_imm())			\
				op(rd, prm._imm);	\
			else if (prm.is_reg()) \
         {							\
				mov(rax, (size_t)prm.reg_ptr());	\
				op(rd, dword[rax]);				\
			}

#define sh_to_reg_noimm(prm, op, rd) \
			if (prm.is_reg()) {							\
				mov(rax, (size_t)prm.reg_ptr());	\
				op(rd, dword[rax]);				\
				}

#define reg_to_sh(prm, rs) \
   mov(rax, (size_t)prm.reg_ptr()); \
   mov(dword[rax], rs)

#define reg_to_sh_ss(prm, rs) \
   mov(rax, (size_t)prm.reg_ptr()); \
   movss(dword[rax], rs)

	void compile(RuntimeBlockInfo* block, bool force_checks, bool reset, bool staging, bool optimise)
   {
		mov(rax, (size_t)&cycle_counter);

		sub(dword[rax], block->guest_cycles);

		sub(rsp, 0x28);

		for (size_t i = 0; i < block->oplist.size(); i++)
      {
         shil_opcode& op  = block->oplist[i];
         switch (op.op)
         {

            case shop_ifb:
               if (op.rs1._imm)
               {
                  mov(rax, (size_t)&next_pc);
                  mov(dword[rax], op.rs2._imm);
               }

               mov(call_regs[0], op.rs3._imm);

               call((void*)OpDesc[op.rs3._imm]->oph);
               break;

            case shop_jcond:
            case shop_jdyn:
               {
                  mov(rax, (size_t)op.rs1.reg_ptr());

                  mov(ecx, dword[rax]);

                  if (op.rs2.is_imm())
                     add(ecx, op.rs2._imm);

                  mov(rdx, (size_t)op.rd.reg_ptr());
                  mov(dword[rdx], ecx);
               }
               break;

            case shop_mov32:
               {
                  sh_to_reg(op.rs1, mov, ecx);

                  reg_to_sh(op.rd, ecx);
               }
               break;

            case shop_mov64:
               {
                  sh_to_reg(op.rs1, mov, rcx);

                  reg_to_sh(op.rd, rcx);
               }
               break;

            case shop_readm:
               {
                  sh_to_reg(op.rs1, mov, call_regs[0]);
                  sh_to_reg(op.rs3, add, call_regs[0]);

                  u32 size = op.flags & 0x7f;

                  switch (size)
                  {
                     case 1:
                        call((void*)ReadMem8);
                        movsx(rcx, al);

                        reg_to_sh(op.rd, ecx);
                        break;
                     case 2:
                        call((void*)ReadMem16);
                        movsx(rcx, ax);

                        reg_to_sh(op.rd, ecx);
                        break;
                     case 4:
                        call((void*)ReadMem32);
                        mov(rcx, rax);

                        reg_to_sh(op.rd, ecx);
                        break;
                     case 8:
                        call((void*)ReadMem64);
                        mov(rcx, rax);

                        reg_to_sh(op.rd, rcx);
                        break;
                     default:
                        die("1..8 bytes");
                        break;
                  }
               }
               break;

            case shop_writem:
               {
                  u32 size = op.flags & 0x7f;
                  sh_to_reg(op.rs1, mov, call_regs[0]);
                  sh_to_reg(op.rs3, add, call_regs[0]);

                  switch (size)
                  {
                     case 1:
                        sh_to_reg(op.rs2, mov, call_regs[1]);
                        call((void*)WriteMem8);
                        break;
                     case 2:
                        sh_to_reg(op.rs2, mov, call_regs[1]);
                        call((void*)WriteMem16);
                        break;
                     case 4:
                        sh_to_reg(op.rs2, mov, call_regs[1]);
                        call((void*)WriteMem32);
                        break;
                     case 8:
                        sh_to_reg(op.rs2, mov, call_regs64[1]);
                        call((void*)WriteMem64);
                        break;
                     default:
                        die("1..8 bytes");
                        break;
                  }
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

			call((void*)UpdateINTC);
			break;

		default:
			die("Invalid block end type");
		}


		add(rsp, 0x28);
		ret();

		ready();

		block->code = (DynarecCodeEntryPtr)getCode();

		emit_Skip(getSize());
	}

	struct CC_PS
	{
		CanonicalParamType type;
		shil_param* prm;
	};
	vector<CC_PS> CC_pars;

	void ngen_CC_Start(shil_opcode* op)
	{
		CC_pars.clear();
	}

	void ngen_CC_param(shil_opcode& op, shil_param& prm, CanonicalParamType tp) {
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


		//store from EAX
		case CPT_u64rvL:
		case CPT_u32rv:
			mov(rcx, rax);
			reg_to_sh(prm, ecx);
			break;

		case CPT_u64rvH:
			shr(rcx, 32);
			reg_to_sh(prm, ecx);
			break;

			//Store from ST(0)
		case CPT_f32rv:
			reg_to_sh_ss(prm, xmm0);
			break;
		}
	}

	void ngen_CC_Call(shil_opcode*op, void* function)
	{
		int regused = 0;
		int xmmused = 0;

		for (int i = CC_pars.size(); i-- > 0;)
		{
			shil_param& prm = *CC_pars[i].prm;
			switch (CC_pars[i].type)
         {
            //push the contents

            case CPT_u32:
               sh_to_reg(prm, mov, call_regs[regused++]);
               break;

            case CPT_f32:
               sh_to_reg_noimm(prm, movss, call_regsxmm[xmmused++]);
               break;

               //push the ptr itself
            case CPT_ptr:
               mov(call_regs64[regused++], (size_t)prm.reg_ptr());

               //die("FAIL");

               break;
         }
		}
		call(function);
	}

};

void ngen_Compile_x64(RuntimeBlockInfo* block, bool force_checks, bool reset, bool staging, bool optimise)
{
	compiler_data = static_cast<void*>(new BlockCompilerx64());

   BlockCompilerx64 *compiler = (BlockCompilerx64*)compiler_data;
	
	compiler->compile(block, force_checks, reset, staging, optimise);

	delete compiler;
}

void ngen_CC_Call_x64(shil_opcode*op, void* function)
{
   BlockCompilerx64 *compiler = (BlockCompilerx64*)compiler_data;
	compiler->ngen_CC_Call(op, function);
}

void ngen_CC_Param_x64(shil_opcode* op,shil_param* par,CanonicalParamType tp)
{
   BlockCompilerx64 *compiler = (BlockCompilerx64*)compiler_data;
   compiler->ngen_CC_param(*op, *par, tp);
}

void ngen_CC_Start_x64(shil_opcode* op)
{
   BlockCompilerx64 *compiler = (BlockCompilerx64*)compiler_data;
   compiler->ngen_CC_Start(op);
}

void ngen_CC_Finish_x64(shil_opcode* op)
{
}
#endif
