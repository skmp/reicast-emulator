#include "types.h"

#include <errno.h>

#ifdef __MACH__
#define _XOPEN_SOURCE 1
#define __USE_GNU 1
#endif

#if !defined(TARGET_NO_EXCEPTIONS)
#ifndef _WIN32
#ifndef __HAIKU__
#include <ucontext.h>
#endif
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
#include <sys/time.h>
#include <unistd.h>
#include "hw/sh4/dyna/blockmanager.h"
#include "hw/mem/vmem32.h"

#include "hw/sh4/dyna/ngen.h"

#ifdef _WIN32
#include "../imgread/common.h"

#include <windows.h>

#define UNW_FLAG_NHANDLER 0x00
#define UNW_FLAG_UHANDLER 0x02

bool VramLockedWrite(u8* address);
bool ngen_Rewrite(size_t &addr, size_t retadr, size_t acc);
bool BM_LockedWrite(u8* address);

static LONG ExceptionHandler(EXCEPTION_POINTERS *ExceptionInfo)
{
   EXCEPTION_POINTERS* ep = ExceptionInfo;

   u32 dwCode = ep->ExceptionRecord->ExceptionCode;

   EXCEPTION_RECORD* pExceptionRecord=ep->ExceptionRecord;

   if (dwCode != EXCEPTION_ACCESS_VIOLATION)
      return EXCEPTION_CONTINUE_SEARCH;

   u8* address=(u8*)pExceptionRecord->ExceptionInformation[1];

   //printf("[EXC] During access to : 0x%X\n", address);

	if (bm_RamWriteAccess(address))
	{
		return EXCEPTION_CONTINUE_EXECUTION;
	}
	else if (VramLockedWrite(address))
      return EXCEPTION_CONTINUE_EXECUTION;
#ifndef TARGET_NO_NVMEM
   if (BM_LockedWrite(address))
      return EXCEPTION_CONTINUE_EXECUTION;
#endif
#if FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_X86
   if ( ngen_Rewrite((size_t&)ep->ContextRecord->Eip,*(size_t*)ep->ContextRecord->Esp,ep->ContextRecord->Eax) )
   {
      //remove the call from call stack
      ep->ContextRecord->Esp+=4;
      //restore the addr from eax to ecx so its valid again
      ep->ContextRecord->Ecx=ep->ContextRecord->Eax;
      return EXCEPTION_CONTINUE_EXECUTION;
   }
#endif
   else
   {
   	ERROR_LOG(COMMON, "[GPF]Unhandled access to : %p", address);
   }

   return EXCEPTION_CONTINUE_SEARCH;
}

#ifdef _WIN64
#include "hw/sh4/dyna/ngen.h"

typedef union _UNWIND_CODE {
   struct {
      u8 CodeOffset;
      u8 UnwindOp : 4;
      u8 OpInfo : 4;
   };
   USHORT FrameOffset;
} UNWIND_CODE, *PUNWIND_CODE;

typedef struct _UNWIND_INFO {
   u8 Version : 3;
   u8 Flags : 5;
   u8 SizeOfProlog;
   u8 CountOfCodes;
   u8 FrameRegister : 4;
   u8 FrameOffset : 4;
   //ULONG ExceptionHandler;
   UNWIND_CODE UnwindCode[1];
   /*  UNWIND_CODE MoreUnwindCode[((CountOfCodes + 1) & ~1) - 1];
    *   union {
    *       OPTIONAL ULONG ExceptionHandler;
    *       OPTIONAL ULONG FunctionEntry;
    *   };
    *   OPTIONAL ULONG ExceptionData[]; */
} UNWIND_INFO, *PUNWIND_INFO;

static RUNTIME_FUNCTION Table[1];
static _UNWIND_INFO unwind_info[1];

   EXCEPTION_DISPOSITION
__gnat_SEH_error_handler(struct _EXCEPTION_RECORD* ExceptionRecord,
      void *EstablisherFrame,
      struct _CONTEXT* ContextRecord,
      void *DispatcherContext)
{
   EXCEPTION_POINTERS ep;
   ep.ContextRecord = ContextRecord;
   ep.ExceptionRecord = ExceptionRecord;

   return (EXCEPTION_DISPOSITION)ExceptionHandler(&ep);
}

PRUNTIME_FUNCTION
seh_callback(
      _In_ DWORD64 ControlPc,
      _In_opt_ PVOID Context
      ) {
   unwind_info[0].Version = 1;
   unwind_info[0].Flags = UNW_FLAG_UHANDLER;
   /* We don't use the unwinding info so fill the structure with 0 values.  */
   unwind_info[0].SizeOfProlog = 0;
   unwind_info[0].CountOfCodes = 0;
   unwind_info[0].FrameOffset = 0;
   unwind_info[0].FrameRegister = 0;
   /* Add the exception handler.  */

   //		unwind_info[0].ExceptionHandler =
   //	(DWORD)((u8 *)__gnat_SEH_error_handler - CodeCache);
   /* Set its scope to the entire program.  */
   Table[0].BeginAddress = 0;// (CodeCache - (u8*)__ImageBase);
   Table[0].EndAddress = /*(CodeCache - (u8*)__ImageBase) +*/ CODE_SIZE + TEMP_CODE_SIZE;
   Table[0].UnwindData = (ULONG)((u8 *)unwind_info - CodeCache);
   DEBUG_LOG(COMMON, "TABLE CALLBACK");
   return Table;
}

void setup_seh(void)
{
   /* Get the base of the module.  */
   /* Current version is always 1 and we are registering an
      exception handler.  */
   unwind_info[0].Version = 1;
   unwind_info[0].Flags = UNW_FLAG_NHANDLER;
   /* We don't use the unwinding info so fill the structure with 0 values.  */
   unwind_info[0].SizeOfProlog = 0;
   unwind_info[0].CountOfCodes = 1;
   unwind_info[0].FrameOffset = 0;
   unwind_info[0].FrameRegister = 0;
   /* Add the exception handler.  */

   unwind_info[0].UnwindCode[0].CodeOffset = 0;
   unwind_info[0].UnwindCode[0].UnwindOp = 2;// UWOP_ALLOC_SMALL;
   unwind_info[0].UnwindCode[0].OpInfo = 0x20 / 8;	// OpInfo * 8 + 8 bytes -> 0x28 bytes

   /* Set its scope to the entire program.  */
   Table[0].BeginAddress = 0;// (CodeCache - (u8*)__ImageBase);
   Table[0].EndAddress = /*(CodeCache - (u8*)__ImageBase) +*/ CODE_SIZE + TEMP_CODE_SIZE;
   Table[0].UnwindData = (ULONG)((u8 *)unwind_info - CodeCache);
   /* Register the unwind information.  */
   RtlAddFunctionTable(Table, 1, (DWORD64)CodeCache);
}
#endif
#endif

struct rei_host_context_t
{
#if HOST_CPU != CPU_GENERIC
	size_t pc;
#endif

#if HOST_CPU == CPU_X86
	u32 eax;
	u32 ecx;
	u32 esp;
#elif HOST_CPU == CPU_ARM
	u32 r[15];
#elif HOST_CPU == CPU_ARM64
	u64 x2;
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
#elif HOST_CPU == CPU_ARM64
	bicopy(reictx->pc, MCTX(.pc), to_segfault);
	bicopy(reictx->x2, MCTX(.regs[2]), to_segfault);
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
bool ngen_Rewrite(size_t& addr,size_t retadr,size_t acc);
u32* ngen_readm_fail_v2(u32* ptr,u32* regs,u32 saddr);
bool VramLockedWrite(u8* address);
bool BM_LockedWrite(u8* address);

#ifdef __MACH__
static void sigill_handler(int sn, siginfo_t * si, void *segfault_ctx)
{
   rei_host_context_t ctx;

   context_from_segfault(&ctx, segfault_ctx);

   size_t pc     = (size_t)ctx.pc;
#if FEAT_SHREC == DYNAREC_JIT
   bool dyna_cde = (pc > (size_t)CodeCache) && (pc < (size_t)(CodeCache + CODE_SIZE + TEMP_CODE_SIZE));
#else
	bool dyna_cde = false;
#endif

	ERROR_LOG(COMMON, "SIGILL @ %p ... %p -> was not in vram, %d",
         pc, si->si_addr, dyna_cde);

	ERROR_LOG(COMMON, "Entering infiniloop");
   for (;;);
}
#endif

#if defined(__MACH__) || defined(__linux__) || defined(__HAIKU__) || \
   defined(__FreeBSD__) || defined(__DragonFly__) || defined(HAVE_LIBNX)
//#define LOG_SIGHANDLER

#ifdef HAVE_LIBNX
extern "C" char __start__;
#endif // HAVE_LIBNX

static void signal_handler(int sn, siginfo_t * si, void *segfault_ctx)
{
   rei_host_context_t ctx;

   context_from_segfault(&ctx, segfault_ctx);

#if FEAT_SHREC == DYNAREC_JIT
	bool dyna_cde = ((uintptr_t)CC_RX2RW(ctx.pc) > (uintptr_t)CodeCache) && ((uintptr_t)CC_RX2RW(ctx.pc) < (uintptr_t)(CodeCache + CODE_SIZE + TEMP_CODE_SIZE));
#else
	bool dyna_cde = false;
#endif

	DEBUG_LOG(COMMON, "mprot hit @ ptr %p @@ pc: %zx, %d", si->si_addr, ctx.pc, dyna_cde);

#if !defined(NO_MMU) && defined(HOST_64BIT_CPU)
#if HOST_CPU == CPU_ARM64
	u32 op = *(u32*)ctx.pc;
	bool write = (op & 0x00400000) == 0;
	u32 exception_pc = ctx.x2;
#elif HOST_CPU == CPU_X64
	bool write = false;	// TODO?
	u32 exception_pc = 0;
#endif
	if (vmem32_handle_signal(si->si_addr, write, exception_pc))
		return;
#endif
	if (bm_RamWriteAccess(si->si_addr))
		return;
	if (VramLockedWrite((u8*)si->si_addr))
      return;
#if !defined(TARGET_NO_NVMEM) && FEAT_SHREC != DYNAREC_NONE
   if (BM_LockedWrite((u8*)si->si_addr))
      return;
#endif
#if FEAT_SHREC == DYNAREC_JIT
#if HOST_CPU==CPU_ARM
   if (dyna_cde)
   {
      ctx.pc = (u32)ngen_readm_fail_v2((u32*)ctx.pc, ctx.r, (size_t)si->si_addr);

      context_to_segfault(&ctx, segfault_ctx);
   }
#elif HOST_CPU==CPU_X86
   if (ngen_Rewrite((size_t&)ctx.pc, *(size_t*)ctx.esp, ctx.eax))
   {
      //remove the call from call stack
      ctx.esp += 4;
      //restore the addr from eax to ecx so it's valid again
      ctx.ecx = ctx.eax;

      context_to_segfault(&ctx, segfault_ctx);
   }
#elif HOST_CPU == CPU_X64
	else if (dyna_cde && ngen_Rewrite((unat&)ctx.pc, 0, 0))
	{
		context_to_segfault(&ctx, segfault_ctx);
	}
#elif HOST_CPU == CPU_ARM64
	else if (dyna_cde && ngen_Rewrite(ctx.pc, 0, 0))
	{
		context_to_segfault(&ctx, segfault_ctx);
	}
#else
#error JIT: Not supported arch
#endif
#endif
   else
   {
   	ERROR_LOG(COMMON, "SIGSEGV @ %zx ... %p -> was not in vram (dyna code %d)", ctx.pc, si->si_addr, dyna_cde);
#ifdef HAVE_LIBNX
    MemoryInfo meminfo;
    u32 pageinfo;
    svcQueryMemory(&meminfo, &pageinfo, (u64)&__start__);
   	ERROR_LOG(COMMON, ".text base: %p", meminfo.addr);
#endif // HAVE_LIBNX
   	die("segfault");
   	signal(SIGSEGV, SIG_DFL);
   }
}
#endif

#endif

#ifndef _WIN32
#if !defined(TARGET_NO_EXCEPTIONS)
static struct sigaction old_sigsegv;
static struct sigaction old_sigill;
#endif

static int exception_handler_install_platform(void)
{
#if defined(HAVE_LIBNX)
   return 0;
#elif !defined(TARGET_NO_EXCEPTIONS)
   struct sigaction new_sa;
   new_sa.sa_flags = SA_SIGINFO;
   sigemptyset(&new_sa.sa_mask);
   new_sa.sa_sigaction = signal_handler;

   if (sigaction(SIGSEGV, &new_sa, &old_sigsegv) != 0)
      return 0;

#ifdef __MACH__
   /* this is broken on OSX/iOS/Mach in general */
   sigaction(SIGBUS, &new_sa, &old_sigsegv);
   new_sa.sa_sigaction = sigill_handler;
#endif

   if (sigaction(SIGILL, &new_sa, &old_sigill))
         return 0;
#endif

   return 1;
}
#endif

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

   DEBUG_LOG(BOOT, "ARM VFP-Run Fast (NFP) enabled !");
#endif
}

#if !defined(TARGET_NO_THREADS)

//Thread class
cThread::cThread(ThreadEntryFP* function,void* prm) : hThread(NULL)
{
	Entry=function;
	param=prm;
}

void cThread::Start()
{
	verify(hThread == NULL);
   hThread = sthread_create((void (*)(void *))Entry, param);
}

void cThread::WaitToEnd()
{
	if (hThread != NULL)
	{
		sthread_join(hThread);
		hThread = NULL;
	}
}

//End thread class
#endif

//cResetEvent Class
cResetEvent::cResetEvent()
{
#if !defined(TARGET_NO_THREADS)
   mutx = slock_new();
   cond = scond_new();
#endif
   state = false;
}

cResetEvent::~cResetEvent()
{
#if !defined(TARGET_NO_THREADS)
   slock_free(mutx);
   scond_free(cond);
#endif
}
void cResetEvent::Set()//Signal
{
#if !defined(TARGET_NO_THREADS)
	slock_lock(mutx );
#endif
	state=true;
#if !defined(TARGET_NO_THREADS)
   scond_signal(cond);
	slock_unlock(mutx );
#endif
}
void cResetEvent::Reset()//reset
{
#if !defined(TARGET_NO_THREADS)
	slock_lock(mutx );
#endif
	state=false;
#if !defined(TARGET_NO_THREADS)
	slock_unlock(mutx );
#endif
}
bool cResetEvent::Wait(u32 msec)//Wait for signal , then reset
{
#if !defined(TARGET_NO_THREADS)
	slock_lock(mutx);
	if (!state)
		scond_wait_timeout(cond, mutx, (int64_t)msec * 1000);
#endif
	bool ret = state;
	state = false;
#if !defined(TARGET_NO_THREADS)
   slock_unlock(mutx);
#endif
	return ret;
}
void cResetEvent::Wait()//Wait for signal , then reset
{
#if !defined(TARGET_NO_THREADS)
	slock_lock(mutx);
	if (!state)
      scond_wait(cond, mutx);
#endif
	state=false;
#if !defined(TARGET_NO_THREADS)
   slock_unlock(mutx);
#endif
}

//End AutoResetEvent
#ifndef TARGET_NO_EXCEPTIONS
void VArray2::LockRegion(u32 offset,u32 size_bytes)
{
   mem_region_lock(&data[offset], size_bytes);
}

void VArray2::UnLockRegion(u32 offset,u32 size_bytes)
{
   mem_region_unlock(&data[offset], size_bytes);
}
#endif

#if defined(_WIN64) && defined(_DEBUG)
static void ReserveBottomMemory(void)
{
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
}
#endif

void common_libretro_setup(void)
{
#if defined(_WIN64) && defined(_DEBUG)
   ReserveBottomMemory();
#endif

#if defined(__MACH__) || defined(__linux__)
   enable_runfast();
#endif
#ifdef _WIN32
#ifdef _WIN64
   AddVectoredExceptionHandler(1, ExceptionHandler);
#else
   SetUnhandledExceptionFilter(&ExceptionHandler);
#endif
#else
   exception_handler_install_platform();
   signal(SIGINT, exit);
#endif

#if defined(__MACH__) || defined(__linux__)
   DEBUG_LOG(BOOT, "Linux paging: %08lX %08X %08X\n",sysconf(_SC_PAGESIZE),PAGE_SIZE,PAGE_MASK);
   verify(PAGE_MASK==(sysconf(_SC_PAGESIZE)-1));
#endif
}

#if !defined(TARGET_NO_EXCEPTIONS) && defined(HAVE_LIBNX)
extern "C"
{

alignas(16) u8 __nx_exception_stack[0x1000];
u64 __nx_exception_stack_size = sizeof(__nx_exception_stack);
#include <malloc.h>
extern void context_switch_aarch64(void* context);

void __libnx_exception_handler(ThreadExceptionDump *ctx)
{   
   mcontext_t m_ctx;

   m_ctx.pc = ctx->pc.x;

   for(int i=0; i<29; i++)
   {
      // printf("X%d: %p\n", i, ctx->cpu_gprs[i].x);
      m_ctx.regs[i] = ctx->cpu_gprs[i].x;
   }

   /*
   printf("PC: %p\n", ctx->pc.x);
   printf("FP: %p\n", ctx->fp.x);
   printf("LR: %p\n", ctx->lr.x);
   printf("SP: %p\n", ctx->sp.x);
   */

   ucontext_t u_ctx;
   u_ctx.uc_mcontext = m_ctx;

   siginfo_t sig_info;

   sig_info.si_addr = (void*)ctx->far.x;

   signal_handler(0, &sig_info, (void*) &u_ctx);

   uint64_t handle[64] = { 0 };

   uint64_t *ptr = (uint64_t*)handle;
   ptr[0]  = m_ctx.regs[0]; /* x0 0  */
   ptr[1]  = m_ctx.regs[1]; /* x1 8 */
   ptr[2]  = m_ctx.regs[2]; /* x2 16 */
   ptr[3]  = m_ctx.regs[3]; /* x3 24 */
   ptr[4]  = m_ctx.regs[4]; /* x4 32 */
   ptr[5]  = m_ctx.regs[5]; /* x5 40 */
   ptr[6]  = m_ctx.regs[6]; /* x6 48 */
   ptr[7]  = m_ctx.regs[7]; /* x7 56 */
   /* Non-volatiles.  */
   ptr[8]  = m_ctx.regs[8]; /* x8 64 */
   ptr[9]  = m_ctx.regs[9]; /* x9 72 */
   ptr[10]  = m_ctx.regs[10]; /* x10 80 */
   ptr[11]  = m_ctx.regs[11]; /* x11 88 */
   ptr[12]  = m_ctx.regs[12]; /* x12 96 */
   ptr[13]  = m_ctx.regs[13]; /* x13 104 */
   ptr[14]  = m_ctx.regs[14]; /* x14 112 */
   ptr[15]  = m_ctx.regs[15]; /* x15 120 */
   ptr[16]  = m_ctx.regs[16]; /* x16 128 */
   ptr[17]  = m_ctx.regs[17]; /* x17 136 */
   ptr[18]  = m_ctx.regs[18]; /* x18 144 */
   ptr[19]  = m_ctx.regs[19]; /* x19 152 */
   ptr[20] = m_ctx.regs[20]; /* x20 160 */
   ptr[21] = m_ctx.regs[21]; /* x21 168 */
   ptr[22] = m_ctx.regs[22]; /* x22 176 */
   ptr[23] = m_ctx.regs[23]; /* x23 184 */
   ptr[24] = m_ctx.regs[24]; /* x24 192 */
   ptr[25] = m_ctx.regs[25]; /* x25 200 */
   ptr[26] = m_ctx.regs[26]; /* x26 208 */
   ptr[27] = m_ctx.regs[27]; /* x27 216 */
   ptr[28] = m_ctx.regs[28]; /* x28 224 */
   /* Special regs */
   ptr[29] = ctx->fp.x; /* frame pointer 232 */
   ptr[30] = ctx->lr.x; /* link register 240 */
   ptr[31] = ctx->sp.x; /* stack pointer 248 */
   ptr[32] = (uintptr_t)ctx->pc.x; /* PC 256 */

   context_switch_aarch64(ptr);
}
}

#endif