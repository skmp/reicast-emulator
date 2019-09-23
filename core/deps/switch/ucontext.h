#include <switch.h>

typedef struct {
    u64 regs[29];
    u64 pc;
} mcontext_t;

typedef struct ucontext_t {
    mcontext_t        uc_mcontext;
} ucontext_t;

typedef struct {
    void* si_addr;
} siginfo_t;