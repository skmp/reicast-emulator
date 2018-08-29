#pragma once
#include "types.h"
#include "sh4_if.h"

#undef sh4op
#define sh4op(str) void  DYNACALL str (u32 op)
typedef void (DYNACALL OpCallFP) (u32 op);

enum OpcodeType
{
	/* Basic */
	Normal       = 0,   
	ReadsPC      = 1,   /* PC must be set upon calling it */
	WritesPC     = 2,   /* It will write PC (branch) */
	Delayslot    = 4,   /* Has a delayslot opcode , valid only when WritesPC is set */

	WritesSR     = 8,   /* Writes to SR , and UpdateSR needs to be called */
	WritesFPSCR  = 16,  /* Writes to FPSCR , and UpdateSR needs to be called */
	Invalid      = 128, /* Invalid */

	NO_FP        = 256,
	NO_GP        = 512,
	NO_SP        = 1024,

	ReadWritePC  = ReadsPC|WritesPC,     /* Read and writes PC */
	WritesSRRWPC = WritesSR|ReadsPC|WritesPC,

	/* Branches (not delay slot): */
	Branch_dir   = ReadWritePC,          /* Direct (eg , pc=r[xx]) -- this one is ReadWritePC b/c the delayslot may use pc ;) */
	Branch_rel   = ReadWritePC,          /* Relative (rg pc+=10); */

	/* Delay slot */
	Branch_dir_d = Delayslot|Branch_dir, /* Direct (eg , pc=r[xx]) */
	Branch_rel_d = Delayslot|Branch_rel  /* Relative (rg pc+=10); */
};

/* interface */
void Sh4_int_Run(void);
void Sh4_int_Stop(void);
void Sh4_int_Start(void);
void Sh4_int_Step(void);
void Sh4_int_Skip(void);
void Sh4_int_Reset(bool Manual);
void Sh4_int_Init(void);
void Sh4_int_Term(void);
bool Sh4_int_IsCpuRunning(void);
u32 Sh4_int_GetRegister(Sh4RegType reg);
void Sh4_int_SetRegister(Sh4RegType reg,u32 regdata);

/* Other things (mainly used by the cpu core */
void ExecuteDelayslot(void);
void ExecuteDelayslot_RTE(void);


#ifdef __cplusplus
extern "C" {
#endif

int UpdateSystem(void);
int UpdateSystem_INTC(void);

#ifdef __cplusplus
}
#endif
