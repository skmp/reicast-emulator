#include "types.h"

#include <errno.h>

#if defined(__MACH__) || defined(__linux__)

#ifdef __MACH__
#define _XOPEN_SOURCE 1
#define __USE_GNU 1
#endif

#if !defined(TARGET_NO_EXCEPTIONS)
#ifndef _WIN32
#include <ucontext.h>
#endif
#endif
#if defined(_ANDROID)
#include <asm/sigcontext.h>
#endif


#include <fcntl.h>
#include <semaphore.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/param.h>
#ifndef _WIN32
#include <sys/mman.h>
#endif
#include <sys/time.h>
#include <unistd.h>
#include "hw/sh4/dyna/blockmanager.h"

#include "hw/sh4/dyna/ngen.h"

struct rei_host_context_t
{
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

static void fault_handler (int sn, siginfo_t * si, void *segfault_ctx)
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

static void install_fault_handler (void)
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



static void print_mem_addr(void)
{
   char line [ 512 ];
   FILE *ifp, *ofp;

   ifp = fopen("/proc/self/maps", "r");

   if (ifp == NULL)
   {
      fprintf(stderr, "Can't open input file /proc/self/maps!\n");
      exit(1);
   }

   ofp = stderr;

   while (fgets(line, sizeof line, ifp) != NULL)
      fprintf(ofp, "%s", line);

   fclose(ifp);
   if (ofp != stderr)
      fclose(ofp);
}


static void enable_runfast(void)
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
#endif

void VArray2::LockRegion(u32 offset,u32 size)
{
#ifdef _WIN32
   verify(size!=0);
	DWORD old;
	VirtualProtect(((u8*)data)+offset , size, PAGE_READONLY,&old);
#else
#if !defined(TARGET_NO_EXCEPTIONS)
   u32 inpage=offset & PAGE_MASK;
   u32 rv=mprotect (data+offset-inpage, size+inpage, PROT_READ );
   if (rv!=0)
   {
      printf("mprotect(%08X,%08X,R) failed: %d | %d\n",data+offset-inpage,size+inpage,rv,errno);
      die("mprotect  failed ..\n");
   }
#endif
#endif
   printf("VA2: LockRegion\n");
}

void VArray2::UnLockRegion(u32 offset,u32 size)
{
#ifdef _WIN32
   verify(size!=0);
	DWORD old;
	VirtualProtect(((u8*)data)+offset , size, PAGE_READWRITE,&old);
#else
#if !defined(TARGET_NO_EXCEPTIONS)
   u32 inpage=offset & PAGE_MASK;
   u32 rv=mprotect (data+offset-inpage, size+inpage, PROT_READ | PROT_WRITE);
   if (rv!=0)
   {
      print_mem_addr();
      printf("mprotect(%8p,%08X,RW) failed: %d | %d\n",data+offset-inpage,size+inpage,rv,errno);
      die("mprotect  failed ..\n");
   }
#endif
#endif
   printf("VA2: UnLockRegion\n");
}

static void ReserveBottomMemory(void)
{
#if defined(_WIN64) && defined(_DEBUG)
    static bool s_initialized = false;
    if ( s_initialized )
        return;
    s_initialized = true;
 
    // Start by reserving large blocks of address space, and then
    // gradually reduce the size in order to capture all of the
    // fragments. Technically we should continue down to 64 KB but
    // stopping at 1 MB is sufficient to keep most allocators out.
 
    const size_t LOW_MEM_LINE = 0x100000000LL;
    size_t totalReservation = 0;
    size_t numVAllocs = 0;
    size_t numHeapAllocs = 0;
    size_t oneMB = 1024 * 1024;
    for (size_t size = 256 * oneMB; size >= oneMB; size /= 2)
    {
        for (;;)
        {
            void* p = VirtualAlloc(0, size, MEM_RESERVE, PAGE_NOACCESS);
            if (!p)
                break;
 
            if ((size_t)p >= LOW_MEM_LINE)
            {
                // We don't need this memory, so release it completely.
                VirtualFree(p, 0, MEM_RELEASE);
                break;
            }
 
            totalReservation += size;
            ++numVAllocs;
        }
    }
 
    // Now repeat the same process but making heap allocations, to use up
    // the already reserved heap blocks that are below the 4 GB line.
    HANDLE heap = GetProcessHeap();
    for (size_t blockSize = 64 * 1024; blockSize >= 16; blockSize /= 2)
    {
        for (;;)
        {
            void* p = HeapAlloc(heap, 0, blockSize);
            if (!p)
                break;
 
            if ((size_t)p >= LOW_MEM_LINE)
            {
                // We don't need this memory, so release it completely.
                HeapFree(heap, 0, p);
                break;
            }
 
            totalReservation += blockSize;
            ++numHeapAllocs;
        }
    }
 
    // Perversely enough the CRT doesn't use the process heap. Suck up
    // the memory the CRT heap has already reserved.
    for (size_t blockSize = 64 * 1024; blockSize >= 16; blockSize /= 2)
    {
        for (;;)
        {
            void* p = malloc(blockSize);
            if (!p)
                break;
 
            if ((size_t)p >= LOW_MEM_LINE)
            {
                // We don't need this memory, so release it completely.
                free(p);
                break;
            }
 
            totalReservation += blockSize;
            ++numHeapAllocs;
        }
    }
 
    // Print diagnostics showing how many allocations we had to make in
    // order to reserve all of low memory, typically less than 200.
    char buffer[1000];
    sprintf_s(buffer, "Reserved %1.3f MB (%d vallocs,"
                      "%d heap allocs) of low-memory.\n",
            totalReservation / (1024 * 1024.0),
            (int)numVAllocs, (int)numHeapAllocs);
    OutputDebugStringA(buffer);
#endif
}

void common_libretro_setup(void)
{
#if defined(_WIN32)
   ReserveBottomMemory();
#endif

#if defined(__MACH__) || defined(__linux__)
   enable_runfast();
#endif
   install_fault_handler();
   signal(SIGINT, exit);

   settings.profile.run_counts=0;

#if defined(__MACH__) || defined(__linux__)
   printf("Linux paging: %08X %08X %08X\n",sysconf(_SC_PAGESIZE),PAGE_SIZE,PAGE_MASK);
   verify(PAGE_MASK==(sysconf(_SC_PAGESIZE)-1));
#endif
}
