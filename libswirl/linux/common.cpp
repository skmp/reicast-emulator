#include "types.h"
#include "cfg/cfg.h"

#include <pthread.h>

#if HOST_OS==OS_LINUX || HOST_OS == OS_DARWIN
#if HOST_OS == OS_DARWIN
	#define _XOPEN_SOURCE 1
	#define __USE_GNU 1
	#include <TargetConditionals.h>
#endif
#if !defined(TARGET_NACL32)
#include <poll.h>
#include <termios.h>
#endif  
//#include <curses.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/time.h>
#if !defined(TARGET_BSD) && !defined(TARGET_IPHONE) && !defined(TARGET_NACL32) && !defined(TARGET_EMSCRIPTEN) && !defined(TARGET_OSX) && !defined(TARGET_OSX_X64)
  #include <sys/personality.h>
  #include <dlfcn.h>
#endif
#include <unistd.h>
#include "hw/sh4/dyna/blockmanager.h"

#include "linux/context.h"

#include "hw/sh4/dyna/ngen.h"
#include "hw/mem/_vmem.h"
#include "rend/TexCache.h"

#include "libswirl.h"

#define fault_printf(...)

#if !defined(TARGET_NO_EXCEPTIONS)

#if HOST_OS == OS_DARWIN
void sigill_handler(int sn, siginfo_t * si, void *segfault_ctx) {
	
    rei_host_context_t ctx;
    
    context_from_segfault(&ctx, segfault_ctx);

	unat pc = (unat)ctx.pc;
	bool dyna_cde = (pc>(unat)CodeCache) && (pc<(unat)(CodeCache + CODE_SIZE));
	
	printf("SIGILL @ %lx -> %p was not in vram, dynacode:%d\n", pc, si->si_addr, dyna_cde);
	
	//printf("PC is used here %08X\n", pc);
    kill(getpid(), SIGABRT);
}
#endif

#define TRAP_STACK_SIZE (1024 * 1024)

static u8* trap_ptr_fault;
static u8* trap_ptr_stack;
static u8* trap_ptr_stack_top;

static unat trap_si_addr;
static unat trap_pc;
static bool trap_handled = true;
static bool in_handler = false;

extern "C" u8* generic_fault_handler ()
{
	fault_printf("generic_fault_handler\n");
	rei_host_context_t ctx;

	fault_printf("generic_fault_handler\n");
	context_from_segfault(&ctx);

	fault_printf("generic_fault_handler: original pc: %p, trap pc: %p\n", ctx.pc, trap_pc);
	ctx.pc = trap_pc;

	fault_printf("generic_fault_handler\n");
	
	if (dc_handle_fault(trap_si_addr, &ctx))
	{
		fault_printf("generic_fault_handler: final pc: %p, trap pc: %p\n", ctx.pc, trap_pc);
		context_to_segfault(&ctx);
		fault_printf("generic_fault_handler: handled\n");

		trap_handled = true;
	}
	else
	{
		fault_printf("generic_fault_handler: not in handled SIGSEGV pc: %lx addr:%p\n", ctx.pc, (void*)trap_si_addr);
		trap_handled = false;
	}

	fault_printf("generic_fault_handler: end\n");
	return trap_ptr_fault;
}



naked void re_raise_fault()
{
	__asm__ __volatile__(
		#if HOST_CPU == CPU_X86
	        "movl %0, %%esp\n"
	        "call generic_fault_handler\n"
	        "movb $0, (%%eax)"
		#elif HOST_CPU == CPU_X64
	        "movq %0, %%rsp\n"
	        "call generic_fault_handler\n"
	        "movb $0, (%%rax)"
        #elif HOST_CPU == CPU_ARM
			"mov sp, %0\n"
	        "bl generic_fault_handler\n"
	        "ldr r1, [r0]"
	    #elif HOST_CPU == CPU_ARM64
	        "mov sp, %0\n"
	        "bl generic_fault_handler\n"
	        "ldr w1, [x0]"
	    #else
	    	#error missing cpu
        #endif
	        : : "r"(trap_ptr_stack_top) : "memory"

    );
}


static void fatal_error()
{
    for (;;)
    {
        printf("fault_handler: Blocking before restoring default SIGSEGV handler\n");
        sleep(1);
    }

    signal(SIGSEGV, SIG_DFL);
}

static void fault_handler (int sn, siginfo_t * si, void *segfault_ctx)
{
	fault_printf("fault_handler: thread: %p si_addr:%p trap_ptr_fault: %p\n", (void*)pthread_self(), si->si_addr, trap_ptr_fault);

	if ((unat)si->si_addr == (unat)trap_ptr_fault)
	{
		if (!in_handler) {
			fault_printf("fault_handler: !in_handler\n");

            fatal_error();
		}

		segfault_load(segfault_ctx);
		rei_host_context_t ctx;
		context_from_segfault(&ctx);

		in_handler = false;
		fault_printf("fault_handler:resume %p\n", ctx.pc);
	}
	else
	{
		if (in_handler)
		{
			fault_printf("fault_handler: double fault\n");

            fatal_error();
		}
		
		in_handler = true;

		if (!trap_handled)
		{
			fault_printf("fault_handler: trap not handled\n");

            fatal_error();
		}
		else 
		{
			fault_printf("fault_handler:catch -> thunking to %p\n", trap_pc);
			trap_si_addr = (unat)si->si_addr;
			segfault_store(segfault_ctx);
			segfault_set_pc(segfault_ctx, (unat)&re_raise_fault, &trap_pc);
		}
	}

	fault_printf("fault_handler exit\n");
}

typedef int sigaction_fn(int signum, const struct sigaction *act,
                     struct sigaction *oldact);

static sigaction_fn* get_sigaction()
{

	void* libc;
	sigaction_fn* rv = nullptr;

	libc = dlopen("libc.so", RTLD_NOLOAD);
	
	if (libc)
	{
		printf("get_sigaction: found libc.so\n");
		*(void**) (&rv) = dlsym(libc, "sigaction");

		if (rv && rv != sigaction) {
			printf("get_sigaction: libc override detected, working around...\n");
			return rv;
		}
	}

	return &sigaction;
}

static void install_fault_handler(void)
{
	// initialize trap state
	trap_ptr_fault = (u8*)mmap(0, PAGE_SIZE, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
    trap_ptr_stack = (u8*)mmap(0, TRAP_STACK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
    trap_ptr_stack_top = trap_ptr_stack + TRAP_STACK_SIZE - 128;

	fault_printf("trap_ptr_fault %p\n", trap_ptr_fault);

	struct sigaction act, segv_oact;
	memset(&act, 0, sizeof(act));
	act.sa_sigaction = fault_handler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_SIGINFO;

	auto libc_sigaction = get_sigaction();

	libc_sigaction(SIGSEGV, &act, &segv_oact);
#if HOST_OS == OS_DARWIN
    //this is broken on osx/ios/mach in general
    libc_sigaction(SIGBUS, &act, &segv_oact);
    
    act.sa_sigaction = sigill_handler;
    libc_sigaction(SIGILL, &act, &segv_oact);
#endif
}
#else  // !defined(TARGET_NO_EXCEPTIONS)
// No exceptions/nvmem dummy handlers.
void install_fault_handler(void) {}
#endif // !defined(TARGET_NO_EXCEPTIONS)

#include <errno.h>

double os_GetSeconds()
{
	timeval a;
	gettimeofday (&a,0);
	static u64 tvs_base=a.tv_sec;
	return a.tv_sec-tvs_base+a.tv_usec/1000000.0;
}

#if TARGET_IPHONE
void os_DebugBreak() {
    __asm__("trap");
}
#elif HOST_OS != OS_LINUX
void os_DebugBreak()
{
	__builtin_trap();
}
#endif

void enable_runfast()
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

void linux_fix_personality() {
        #if !defined(TARGET_BSD) && !defined(__ANDROID__) && !defined(TARGET_OS_MAC) && !defined(TARGET_NACL32) && !defined(TARGET_EMSCRIPTEN)
          printf("Personality: %08X\n", personality(0xFFFFFFFF));
          personality(~READ_IMPLIES_EXEC & personality(0xFFFFFFFF));
          printf("Updated personality: %08X\n", personality(0xFFFFFFFF));
        #endif
}

void linux_rpi2_init() {
#if !defined(TARGET_BSD) && !defined(__ANDROID__) && !defined(TARGET_NACL32) && !defined(TARGET_EMSCRIPTEN) && defined(TARGET_VIDEOCORE)
	void* handle;
	void (*rpi_bcm_init)(void);

	handle = dlopen("libbcm_host.so", RTLD_LAZY);
	
	if (handle) {
		printf("found libbcm_host\n");
		*(void**) (&rpi_bcm_init) = dlsym(handle, "bcm_host_init");
		if (rpi_bcm_init) {
			printf("rpi2: bcm_init\n");
			rpi_bcm_init();
		}
	}
#endif
}

void common_linux_setup()
{
	linux_fix_personality();
	linux_rpi2_init();

	enable_runfast();
	install_fault_handler();
	signal(SIGINT, exit);
	
	settings.profile.run_counts=0;
	
	printf("Linux paging: %ld %08X %08X\n",sysconf(_SC_PAGESIZE),PAGE_SIZE,PAGE_MASK);
	verify(PAGE_MASK==(sysconf(_SC_PAGESIZE)-1));
}
#endif
