#include "types.h"

#if FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_X86

#include "rec_x86_ngen.h"

u32 gas_offs=offsetof(Sh4RCB,cntx.jdyn);
void ngen_LinkBlock_Shared_stub(void)
{
	__asm__ __volatile__ (
        ".intel_syntax noprefix    \n\t"          // Tell gcc that we're intel, not at&t
        "pop ecx                \n\t"
        "sub ecx,5              \n\t"
        "call _rdv_LinkBlock     \n\t"
        "jmp eax                \n\t"                               // use ret not retn and let compiler decide whats near or far
   );
}

void ngen_LinkBlock_cond_Next_stub()
{
	 __asm__ __volatile__  (
        ".intel_syntax noprefix    \n\t"          // Tell gcc that we're intel, not at&t
        "mov edx,0   \n\t"
        "jmp _ngen_LinkBlock_Shared_stub             \n\t"
    );
}

void ngen_LinkBlock_cond_Branch_stub()
{
	 __asm__ __volatile__  (
        ".intel_syntax noprefix    \n\t"          // Tell gcc that we're intel, not at&t
        "mov edx,1   \n\t"
        "jmp _ngen_LinkBlock_Shared_stub              \n\t"
    );
}


void ngen_LinkBlock_Generic_stub()
{
	 __asm__ __volatile__  (
        ".intel_syntax noprefix    \n\t"          // Tell gcc that we're intel, not at&t
        "mov edx,[_p_sh4rcb]  \n\t"
        "add edx,[_gas_offs]              \n\t"
        "mov edx,[edx]              \n\t"
        "jmp _ngen_LinkBlock_Shared_stub              \n\t"
    );
}

void ngen_FailedToFindBlock_()
{
	 __asm__ __volatile__  (
        ".intel_syntax noprefix    \n\t"          // Tell gcc that we're intel, not at&t
        "mov ecx,esi  \n\t"
        "call _rdv_FailedToFindBlock              \n\t"
        "jmp eax             \n\t"
    );
}

void (*ngen_FailedToFindBlock)()=&ngen_FailedToFindBlock_;
void ngen_mainloop(void* cntx)
{
	 __asm__ __volatile__  (
        ".intel_syntax noprefix    \n\t"          // Tell gcc that we're intel, not at&t
		"push esi \n\t"
		"push edi \n\t"
		"push ebp \n\t"
		"push ebx \n\t"

		"mov ecx,0xA0000000 \n\t"
		//"mov [cycle_counter],SH4_TIMESLICE \n\t"

		//"mov [loop_no_update],offset no_update \n\t"
		//"mov [intc_sched],offset intc_sched_offs \n\t"
		
		"mov eax,0 \n\t"
		//next_pc _MUST_ be on ecx
"no_update:\n\t"
		"mov esi,ecx \n\t"
		"call bm_GetCode \n\t"
		"jmp eax \n\t"

"intc_sched_offs: \n\t"
	//	"add [cycle_counter],SH4_TIMESLICE \n\t"
		"call _UpdateSystem \n\t"
		"cmp eax,0 \n\t"
		"jnz do_iter \n\t"
		"ret \n\t"

"do_iter: \n\t"
		"pop ecx \n\t"
		"call _rdv_DoInterrupts \n\t"
		"mov ecx,eax \n\t"
//		cmp byte ptr [sh4_int_bCpuRun],0;
	//	jz cleanup;
		"jmp no_update \n\t"
"cleanup: \n\t"
		"pop ebx \n\t"
		"pop ebp \n\t"
		"pop edi \n\t"
		"pop esi \n\t"
		"ret \n\t"
	);
}


void DYNACALL ngen_blockcheckfail(u32 addr)
{
	 __asm__ __volatile__  (
       ".intel_syntax noprefix    \n\t"          // Tell gcc that we're intel, not at&t
       "call _rdv_BlockCheckFail  \n\t"
       "jmp eax              \n\t"
    );
}

void DYNACALL ngen_blockcheckfail2(u32 addr)
{
	 __asm__ __volatile__  (
       ".intel_syntax noprefix    \n\t"          // Tell gcc that we're intel, not at&t
       "call _rdv_BlockCheckFail  \n\t"
       "jmp eax             \n\t"
    );
}
#endif