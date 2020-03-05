/*
	This file is part of libswirl
*/
#include "license/bsd"


#include "context.h"

#define fault_printf(...)

#if defined(_ANDROID)
	#include <asm/sigcontext.h>
#else
	#if HOST_OS == OS_DARWIN
		#define _XOPEN_SOURCE 1
		#define __USE_GNU 1
	#endif

  #if !defined(TARGET_NO_EXCEPTIONS)
    #include <ucontext.h>
  #endif
#endif


//////

#define UCTX(p) (((ucontext_t *)(segfault_ctx)) p)

#if HOST_OS == OS_DARWIN
	#define UCTX_type ucontext_t
	#define UCTX_data (UCTX())
#else
	#define UCTX_type ucontext_t
	#define UCTX_data (UCTX())
#endif

template <typename Ta, typename Tb>
void bicopy(Ta& rei, Tb& seg, bool to_segfault) {
	if (to_segfault) {
		seg = rei;
	}
	else {
		rei = seg;
	}
}
#if !defined(TARGET_NO_EXCEPTIONS)
	static thread_local UCTX_type segfault_copy;
#endif

void segfault_store(void* segfault_ctx) {
#if !defined(TARGET_NO_EXCEPTIONS)
	memcpy(&segfault_copy, UCTX_data, sizeof(segfault_copy));

	fault_printf("sgctx stored, %d\n", sizeof(segfault_copy));
#endif
}

void segfault_load(void* segfault_ctx) {
#if !defined(TARGET_NO_EXCEPTIONS)
	memcpy(UCTX_data, &segfault_copy, sizeof(*UCTX_data));

	fault_printf("sgctx loaded, %d\n", sizeof(*UCTX_data));
#endif
}

#define MCTX(p) (((UCTX_type*)segfault_ctx)->uc_mcontext p)

void context_segfault_bicopy(rei_host_context_t* reictx, void* segfault_ctx, bool to_segfault) {

#if !defined(TARGET_NO_EXCEPTIONS)
#if HOST_CPU == CPU_ARM
	#if defined(__FreeBSD__)
		bicopy(reictx->pc, MCTX(.__gregs[_REG_PC]), to_segfault);

		for (int i = 0; i < 15; i++)
			bicopy(reictx->regs[i], MCTX(.__gregs[i]), to_segfault);
	#elif HOST_OS == OS_LINUX
		bicopy(reictx->pc, MCTX(.arm_pc), to_segfault);
		u32* r =(u32*) &MCTX(.arm_r0);

		for (int i = 0; i < 15; i++)
			bicopy(reictx->regs[i], r[i], to_segfault);

	#elif HOST_OS == OS_DARWIN
		bicopy(reictx->pc, MCTX(->__ss.__pc), to_segfault);

		for (int i = 0; i < 15; i++)
			bicopy(reictx->regs[i], MCTX(->__ss.__r[i]), to_segfault);
	#else
		#error HOST_OS
	#endif
#elif HOST_CPU == CPU_ARM64
	bicopy(reictx->pc, MCTX(.pc), to_segfault);
#elif HOST_CPU == CPU_X86
	#if defined(__FreeBSD__)
		bicopy(reictx->pc, MCTX(.mc_eip), to_segfault);
		bicopy(reictx->esp, MCTX(.mc_esp), to_segfault);
		bicopy(reictx->eax, MCTX(.mc_eax), to_segfault);
		bicopy(reictx->ecx, MCTX(.mc_ecx), to_segfault);
	#elif HOST_OS == OS_LINUX
		bicopy(reictx->pc, MCTX(.gregs[REG_EIP]), to_segfault);
		bicopy(reictx->esp, MCTX(.gregs[REG_ESP]), to_segfault);
		bicopy(reictx->eax, MCTX(.gregs[REG_EAX]), to_segfault);
		bicopy(reictx->ecx, MCTX(.gregs[REG_ECX]), to_segfault);
	#elif HOST_OS == OS_DARWIN
		bicopy(reictx->pc, MCTX(->__ss.__eip), to_segfault);
		bicopy(reictx->esp, MCTX(->__ss.__esp), to_segfault);
		bicopy(reictx->eax, MCTX(->__ss.__eax), to_segfault);
		bicopy(reictx->ecx, MCTX(->__ss.__ecx), to_segfault);
	#else
		#error HOST_OS
	#endif
#elif HOST_CPU == CPU_X64
	#if defined(__FreeBSD__) || defined(__DragonFly__)
		bicopy(reictx->pc, MCTX(.mc_rip), to_segfault);
	#elif defined(__NetBSD__)
		bicopy(reictx->pc, MCTX(.__gregs[_REG_RIP]), to_segfault);
	#elif HOST_OS == OS_LINUX
		fault_printf("pc set: %d, value: %p %p\n", to_segfault, reictx->pc, MCTX(.gregs[REG_RIP]));
		bicopy(reictx->pc, MCTX(.gregs[REG_RIP]), to_segfault);
		fault_printf("pc set: %d, value: %p %p\n", to_segfault, reictx->pc, MCTX(.gregs[REG_RIP]));
    #elif HOST_OS == OS_DARWIN
        bicopy(reictx->pc, MCTX(->__ss.__rip), to_segfault);
    #else
	    #error HOST_OS
	#endif
#elif HOST_CPU == CPU_MIPS
	bicopy(reictx->pc, MCTX(.pc), to_segfault);
#elif HOST_CPU == CPU_GENERIC
    //nothing!
#else
	#error Unsupported HOST_CPU
#endif
	#endif
	
}


void segfault_set_pc(void* segfault_ctx, unat new_pc, unat* old_pc)
{
#if !defined(TARGET_NO_EXCEPTIONS)
	rei_host_context_t ctx;

	context_segfault_bicopy(&ctx, UCTX_data, false);
	fault_printf("segfault_set_pc: old pc: %lx\n", ctx.pc);

	*old_pc = ctx.pc;

#if HOST_CPU == CPU_ARM
	// if THUMB, store the pointer in BX-compatible format
	if (MCTX(.arm_cpsr) & (1<<5))
		*old_pc += 1;

	ctx.pc = new_pc & ~1; // THUMB Bit set on CPSR, no need to set it here

	// restore from BX-compatible pointer
	// set THUMB bit accordingly
	if (new_pc & 1)
		MCTX(.arm_cpsr) |= (1<<5);
	else
		MCTX(.arm_cpsr) &= ~(1<<5);
#else
	ctx.pc = new_pc;
#endif
	
	context_segfault_bicopy(&ctx, UCTX_data, true);
	fault_printf("segfault_set_pc: new pc: %lx\n", ctx.pc);
#endif
}


void context_from_segfault(rei_host_context_t* reictx) {
#if !defined(TARGET_NO_EXCEPTIONS)
	fault_printf("context from segfault pc: %p, %p\n",reictx->pc, segfault_copy.gregs[REG_RIP]);
	context_segfault_bicopy(reictx, &segfault_copy, false);
	fault_printf("context from segfault pc: %p, %p\n",reictx->pc, segfault_copy.gregs[REG_RIP]);
#endif
}

void context_to_segfault(rei_host_context_t* reictx) {
#if !defined(TARGET_NO_EXCEPTIONS)
	fault_printf("context to segfault pc: %p, %p\n",reictx->pc, segfault_copy.gregs[REG_RIP]);
	context_segfault_bicopy(reictx, &segfault_copy, true);
	fault_printf("context to segfault pc: %p, %p\n",reictx->pc, segfault_copy.gregs[REG_RIP]);
#endif
}
