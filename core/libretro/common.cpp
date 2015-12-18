#include "types.h"

#ifdef __MACH__
#define _XOPEN_SOURCE 1
#define __USE_GNU 1
#endif

#if !defined(TARGET_NO_EXCEPTIONS)
#include <ucontext.h>
#endif
#if defined(_ANDROID)
#include <asm/sigcontext.h>
#endif

#include <fcntl.h>
#include <semaphore.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>
#include "hw/sh4/dyna/blockmanager.h"

#include "hw/sh4/dyna/ngen.h"

struct rei_host_context_t {
#if HOST_CPU != CPU_GENERIC
	unat pc;
#endif

#if HOST_CPU == CPU_X86
	u32 eax;
	u32 ecx;
	u32 esp;
#elif HOST_CPU == CPU_ARM
	u32 r[15];
#endif
};

#define MCTX(p) (((ucontext_t *)(segfault_ctx))->uc_mcontext p)
template <typename Ta, typename Tb>
static void bicopy(Ta& rei, Tb& seg, bool to_segfault)
{
   if (to_segfault)
      seg = rei;
   else
      rei = seg;
}

static void context_segfault(rei_host_context_t* reictx, void* segfault_ctx, bool to_segfault)
{
#if !defined(TARGET_NO_EXCEPTIONS)
#if HOST_CPU == CPU_ARM
#ifdef __linux__
   bicopy(reictx->pc, MCTX(.arm_pc), to_segfault);
   u32* r =(u32*) &MCTX(.arm_r0);

   for (int i = 0; i < 15; i++)
      bicopy(reictx->r[i], r[i], to_segfault);
#elif defined(__MACH__)
   bicopy(reictx->pc, MCTX(->__ss.__pc), to_segfault);

   for (int i = 0; i < 15; i++)
      bicopy(reictx->r[i], MCTX(->__ss.__r[i]), to_segfault);
#endif

#elif HOST_CPU == CPU_X86
#ifdef __linux__
   bicopy(reictx->pc, MCTX(.gregs[REG_EIP]), to_segfault);
   bicopy(reictx->esp, MCTX(.gregs[REG_ESP]), to_segfault);
   bicopy(reictx->eax, MCTX(.gregs[REG_EAX]), to_segfault);
   bicopy(reictx->ecx, MCTX(.gregs[REG_ECX]), to_segfault);
#elif defined(__MACH__)
   bicopy(reictx->pc, MCTX(->__ss.__eip), to_segfault);
   bicopy(reictx->esp, MCTX(->__ss.__esp), to_segfault);
   bicopy(reictx->eax, MCTX(->__ss.__eax), to_segfault);
   bicopy(reictx->ecx, MCTX(->__ss.__ecx), to_segfault);
#endif
#elif HOST_CPU == CPU_X64
#ifdef __linux__
   bicopy(reictx->pc, MCTX(.gregs[REG_RIP]), to_segfault);
#elif defined(__MACH__)
   bicopy(reictx->pc, MCTX(->__ss.__rip), to_segfault);
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

static void context_from_segfault(rei_host_context_t* reictx, void* segfault_ctx)
{
   context_segfault(reictx, segfault_ctx, false);
}

static void context_to_segfault(rei_host_context_t* reictx, void* segfault_ctx)
{
	context_segfault(reictx, segfault_ctx, true);
}

#if !defined(TARGET_NO_EXCEPTIONS)
bool ngen_Rewrite(unat& addr,unat retadr,unat acc);
u32* ngen_readm_fail_v2(u32* ptr,u32* regs,u32 saddr);
bool VramLockedWrite(u8* address);
bool BM_LockedWrite(u8* address);

#ifdef __MACH__
static void sigill_handler(int sn, siginfo_t * si, void *segfault_ctx)
{
   rei_host_context_t ctx;

   context_from_segfault(&ctx, segfault_ctx);

   unat pc = (unat)ctx.pc;
   bool dyna_cde = (pc>(unat)CodeCache) && (pc<(unat)(CodeCache + CODE_SIZE));

   printf("SIGILL @ %08X, fault_handler+0x%08X ... %08X -> was not in vram, %d\n", pc, pc - (unat)sigill_handler, (unat)si->si_addr, dyna_cde);

   printf("Entering infiniloop");

   for (;;);
   printf("PC is used here %08X\n", pc);
}
#endif

#if !defined(TARGET_NO_EXCEPTIONS)
void fault_handler (int sn, siginfo_t * si, void *segfault_ctx)
{
   rei_host_context_t ctx;

   context_from_segfault(&ctx, segfault_ctx);

   bool dyna_cde = ((unat)ctx.pc>(unat)CodeCache) && ((unat)ctx.pc<(unat)(CodeCache + CODE_SIZE));

   //ucontext_t* ctx=(ucontext_t*)ctxr;
   //printf("mprot hit @ ptr 0x%08X @@ code: %08X, %d\n",si->si_addr,ctx->uc_mcontext.arm_pc,dyna_cde);


   if (VramLockedWrite((u8*)si->si_addr) || BM_LockedWrite((u8*)si->si_addr))
      return;
#if FEAT_SHREC == DYNAREC_JIT
#if HOST_CPU==CPU_ARM
   else if (dyna_cde)
   {
      ctx.pc = (u32)ngen_readm_fail_v2((u32*)ctx.pc, ctx.r, (unat)si->si_addr);

      context_to_segfault(&ctx, segfault_ctx);
   }
#elif HOST_CPU==CPU_X86
   else if (ngen_Rewrite((unat&)ctx.pc, *(unat*)ctx.esp, ctx.eax))
   {
      //remove the call from call stack
      ctx.esp += 4;
      //restore the addr from eax to ecx so it's valid again
      ctx.ecx = ctx.eax;

      context_to_segfault(&ctx, segfault_ctx);
   }
#elif HOST_CPU == CPU_X64
   //x64 has no rewrite support
#else
#error JIT: Not supported arch
#endif
#endif
   else
   {
      printf("SIGSEGV @ %p (fault_handler+0x%p) ... %p -> was not in vram\n", ctx.pc, ctx.pc - (unat)fault_handler, si->si_addr);
      die("segfault");
      signal(SIGSEGV, SIG_DFL);
   }
}
#endif

#endif
void install_fault_handler (void)
{
#if !defined(TARGET_NO_EXCEPTIONS)
   struct sigaction act, segv_oact;
   memset(&act, 0, sizeof(act));
   act.sa_sigaction = fault_handler;
   sigemptyset(&act.sa_mask);
   act.sa_flags = SA_SIGINFO;
   sigaction(SIGSEGV, &act, &segv_oact);
#ifdef __MACH__
   //this is broken on osx/ios/mach in general
   sigaction(SIGBUS, &act, &segv_oact);

   act.sa_sigaction = sigill_handler;
   sigaction(SIGILL, &act, &segv_oact);
#endif
#endif
}

//cResetEvent Calss
cResetEvent::cResetEvent(bool State,bool Auto)
{
   //sem_init((sem_t*)hEvent, 0, State?1:0);
   verify(State==false&&Auto==true);
   mutx = slock_new();
   cond = scond_new();
}

cResetEvent::~cResetEvent()
{
	//Destroy the event object ?

}

void cResetEvent::Set()//Signal
{
   slock_lock(mutx);
	state=true;
   scond_signal(cond);
   slock_unlock(mutx);
}
void cResetEvent::Reset()//reset
{
   slock_lock(mutx);
	state=false;
   slock_unlock(mutx);
}
void cResetEvent::Wait(u32 msec)//Wait for signal , then reset
{
	verify(false);
}
void cResetEvent::Wait()//Wait for signal , then reset
{
   slock_lock(mutx);
	if (!state)
		scond_wait( cond, mutx );
	state=false;
   slock_unlock(mutx);
}

//End AutoResetEvent

#include <errno.h>

void VArray2::LockRegion(u32 offset,u32 size)
{
#if !defined(TARGET_NO_EXCEPTIONS)
   u32 inpage=offset & PAGE_MASK;
   u32 rv=mprotect (data+offset-inpage, size+inpage, PROT_READ );
   if (rv!=0)
   {
      printf("mprotect(%08X,%08X,R) failed: %d | %d\n",data+offset-inpage,size+inpage,rv,errno);
      die("mprotect  failed ..\n");
   }

#else
   printf("VA2: LockRegion\n");
#endif
}

void print_mem_addr(void)
{
   FILE *ifp, *ofp;

   char outputFilename[] = "/data/data/com.reicast.emulator/files/mem_alloc.txt";

   ifp = fopen("/proc/self/maps", "r");

   if (ifp == NULL) {
      fprintf(stderr, "Can't open input file /proc/self/maps!\n");
      exit(1);
   }

   ofp = fopen(outputFilename, "w");

   if (ofp == NULL) {
      fprintf(stderr, "Can't open output file %s!\n",
            outputFilename);
#ifdef __linux__
      ofp = stderr;
#else
      exit(1);
#endif
   }

   char line [ 512 ];
   while (fgets(line, sizeof line, ifp) != NULL) {
      fprintf(ofp, "%s", line);
   }

   fclose(ifp);
   if (ofp != stderr)
      fclose(ofp);
}

void VArray2::UnLockRegion(u32 offset,u32 size)
{
#if !defined(TARGET_NO_EXCEPTIONS)
   u32 inpage=offset & PAGE_MASK;
   u32 rv=mprotect (data+offset-inpage, size+inpage, PROT_READ | PROT_WRITE);
   if (rv!=0)
   {
      print_mem_addr();
      printf("mprotect(%8p,%08X,RW) failed: %d | %d\n",data+offset-inpage,size+inpage,rv,errno);
      die("mprotect  failed ..\n");
   }
#else
   printf("VA2: UnLockRegion\n");
#endif
}

#if TARGET_IPHONE
void os_DebugBreak()
{
    __asm__("trap");
}

#if !defined(__linux__)
void os_DebugBreak()
{
	__builtin_trap();
}
#endif

void enable_runfast(void)
{
#if HOST_CPU==CPU_ARM && !defined(ARMCC)
   static const unsigned int x = 0x04086060;
   static const unsigned int y = 0x03000000;
   int r;
   asm volatile (
         "fmrx	%0, fpscr			\n\t"	//r0 = FPSCR
         "and	%0, %0, %1			\n\t"	//r0 = r0 & 0x04086060
         "orr	%0, %0, %2			\n\t"	//r0 = r0 | 0x03000000
         "fmxr	fpscr, %0			\n\t"	//FPSCR = r0
         : "=r"(r)
         : "r"(x), "r"(y)
         );

   printf("ARM VFP-Run Fast (NFP) enabled !\n");
#endif
}

void common_linux_setup(void)
{
   enable_runfast();
   install_fault_handler();
   signal(SIGINT, exit);

   settings.profile.run_counts=0;

   printf("Linux paging: %08X %08X %08X\n",sysconf(_SC_PAGESIZE),PAGE_SIZE,PAGE_MASK);
   verify(PAGE_MASK==(sysconf(_SC_PAGESIZE)-1));
}
#endif
