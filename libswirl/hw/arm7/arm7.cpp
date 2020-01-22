#include "arm7.h"
#include "arm7_context.h"
#include "hw/sh4/sh4_core.h"
#include "hw/aica/aica_mmio.h"


//#define CPUReadHalfWordQuick(addr) arm_ReadMem16(addr & 0x7FFFFF)
#define CPUReadMemoryQuick(addr) (*(u32*)&ctx->aica_ram[addr&ctx->aram_mask])
#define CPUReadByte(a) arm_ReadMem8(a, ctx)
#define CPUReadMemory(a) arm_ReadMem32(a, ctx)

#define CPUWriteMemory(a, d) arm_WriteMem32(a, d, ctx)
#define CPUWriteByte(a, d) arm_WriteMem8(a, d, ctx)

#define armMode ctx->armMode
#define armIrqEnable ctx->armIrqEnable
#define armFiqEnable ctx->armFiqEnable
#define reg ctx->regs
#define armNextPC reg[R15_ARM_NEXT].I


#define CPUUpdateTicksAccesint(a) 1
#define CPUUpdateTicksAccessSeq32(a) 1
#define CPUUpdateTicksAccesshort(a) 1
#define CPUUpdateTicksAccess32(a) 1
#define CPUUpdateTicksAccess16(a) 1



//bool arm_FiqPending; -- not used , i use the input directly :)
//bool arm_IrqPending;

void CPUSwap(u32* a, u32* b)
{
	u32 c = *b;
	*b = *a;
	*a = c;
}

/*
bool N_FLAG;
bool Z_FLAG;
bool C_FLAG;
bool V_FLAG;
*/
#define N_FLAG (reg[RN_PSR_FLAGS].FLG.N)
#define Z_FLAG (reg[RN_PSR_FLAGS].FLG.Z)
#define C_FLAG (reg[RN_PSR_FLAGS].FLG.C)
#define V_FLAG (reg[RN_PSR_FLAGS].FLG.V)


#define arm_ReadMem8 ctx->read8
#define arm_ReadMem32 ctx->read32

#define arm_WriteMem8 ctx->write8
#define arm_WriteMem32 ctx->write32

void DYNACALL ARM7Backend::UpdateInterrupts(Arm7Context* ctx)
{
	reg[INTR_PEND].I = ctx->e68k_out && armFiqEnable;
}

void DYNACALL  ARM7Backend::CPUUpdateFlags(Arm7Context* ctx)
{
	u32 CPSR = reg[16].I;

	reg[RN_PSR_FLAGS].FLG.NZCV = reg[16].PSR.NZCV;

	/*
	N_FLAG = (CPSR & 0x80000000) ? true: false;
	Z_FLAG = (CPSR & 0x40000000) ? true: false;
	C_FLAG = (CPSR & 0x20000000) ? true: false;
	V_FLAG = (CPSR & 0x10000000) ? true: false;
	*/
	//armState = (CPSR & 0x20) ? false : true;
	armIrqEnable = (CPSR & 0x80) ? false : true;
	armFiqEnable = (CPSR & 0x40) ? false : true;
	UpdateInterrupts(ctx);
}

void DYNACALL ARM7Backend::CPUSwitchMode(Arm7Context* ctx, int mode, bool saveState)
{
	CPUUpdateCPSR(ctx);

	switch (armMode)
	{
	case 0x10:
	case 0x1F:
		reg[R13_USR].I = reg[13].I;
		reg[R14_USR].I = reg[14].I;
		reg[17].I = reg[16].I;
		break;
	case 0x11:
		CPUSwap(&reg[R8_FIQ].I, &reg[8].I);
		CPUSwap(&reg[R9_FIQ].I, &reg[9].I);
		CPUSwap(&reg[R10_FIQ].I, &reg[10].I);
		CPUSwap(&reg[R11_FIQ].I, &reg[11].I);
		CPUSwap(&reg[R12_FIQ].I, &reg[12].I);
		reg[R13_FIQ].I = reg[13].I;
		reg[R14_FIQ].I = reg[14].I;
		reg[SPSR_FIQ].I = reg[17].I;
		break;
	case 0x12:
		reg[R13_IRQ].I = reg[13].I;
		reg[R14_IRQ].I = reg[14].I;
		reg[SPSR_IRQ].I = reg[17].I;
		break;
	case 0x13:
		reg[R13_SVC].I = reg[13].I;
		reg[R14_SVC].I = reg[14].I;
		reg[SPSR_SVC].I = reg[17].I;
		break;
	case 0x17:
		reg[R13_ABT].I = reg[13].I;
		reg[R14_ABT].I = reg[14].I;
		reg[SPSR_ABT].I = reg[17].I;
		break;
	case 0x1b:
		reg[R13_UND].I = reg[13].I;
		reg[R14_UND].I = reg[14].I;
		reg[SPSR_UND].I = reg[17].I;
		break;
	}

	u32 CPSR = reg[16].I;
	u32 SPSR = reg[17].I;

	switch (mode)
	{
	case 0x10:
	case 0x1F:
		reg[13].I = reg[R13_USR].I;
		reg[14].I = reg[R14_USR].I;
		reg[16].I = SPSR;
		break;
	case 0x11:
		CPUSwap(&reg[8].I, &reg[R8_FIQ].I);
		CPUSwap(&reg[9].I, &reg[R9_FIQ].I);
		CPUSwap(&reg[10].I, &reg[R10_FIQ].I);
		CPUSwap(&reg[11].I, &reg[R11_FIQ].I);
		CPUSwap(&reg[12].I, &reg[R12_FIQ].I);
		reg[13].I = reg[R13_FIQ].I;
		reg[14].I = reg[R14_FIQ].I;
		if (saveState)
			reg[17].I = CPSR;
		else
			reg[17].I = reg[SPSR_FIQ].I;
		break;
	case 0x12:
		reg[13].I = reg[R13_IRQ].I;
		reg[14].I = reg[R14_IRQ].I;
		reg[16].I = SPSR;
		if (saveState)
			reg[17].I = CPSR;
		else
			reg[17].I = reg[SPSR_IRQ].I;
		break;
	case 0x13:
		reg[13].I = reg[R13_SVC].I;
		reg[14].I = reg[R14_SVC].I;
		reg[16].I = SPSR;
		if (saveState)
			reg[17].I = CPSR;
		else
			reg[17].I = reg[SPSR_SVC].I;
		break;
	case 0x17:
		reg[13].I = reg[R13_ABT].I;
		reg[14].I = reg[R14_ABT].I;
		reg[16].I = SPSR;
		if (saveState)
			reg[17].I = CPSR;
		else
			reg[17].I = reg[SPSR_ABT].I;
		break;
	case 0x1b:
		reg[13].I = reg[R13_UND].I;
		reg[14].I = reg[R14_UND].I;
		reg[16].I = SPSR;
		if (saveState)
			reg[17].I = CPSR;
		else
			reg[17].I = reg[SPSR_UND].I;
		break;
	default:
		printf("Unsupported ARM mode %02x\n", mode);
		die("Arm error..");
		break;
	}
	armMode = mode;
	CPUUpdateFlags(ctx);
	CPUUpdateCPSR(ctx);
}

void DYNACALL ARM7Backend::CPUUpdateCPSR(Arm7Context* ctx)
{
	arm7_reg CPSR;

	CPSR.I = ctx->regs[RN_CPSR].I & 0x40;

	CPSR.PSR.NZCV = ctx->regs[RN_PSR_FLAGS].FLG.NZCV;


	if (!armFiqEnable)
		CPSR.I |= 0x40;
	if (!armIrqEnable)
		CPSR.I |= 0x80;

	CPSR.PSR.M = armMode;

	ctx->regs[16].I = CPSR.I;
}


static void CPUSoftwareInterrupt(Arm7Context* ctx, int comment)
{
	u32 PC = reg[R15_ARM_NEXT].I + 4;
	//bool savedArmState = armState;
	ARM7Backend::CPUSwitchMode(ctx, 0x13, true);
	reg[14].I = PC;
	//	reg[15].I = 0x08;

	armIrqEnable = false;
	armNextPC = 0x08;
	//	reg[15].I += 4;
}

static void CPUUndefinedException(Arm7Context* ctx)
{
	printf("arm7: CPUUndefinedException(). SOMETHING WENT WRONG\n");
	u32 PC = reg[R15_ARM_NEXT].I + 4;
	ARM7Backend::CPUSwitchMode(ctx, 0x1b, true);
	reg[14].I = PC;
	//	reg[15].I = 0x04;
	armIrqEnable = false;
	armNextPC = 0x04;
	//	reg[15].I += 4;  
}

void DYNACALL ARM7Backend::CPUFiq(Arm7Context* ctx)
{
	u32 PC = reg[R15_ARM_NEXT].I + 4;
	//bool savedState = armState;
	ARM7Backend::CPUSwitchMode(ctx, 0x11, true);
	reg[14].I = PC;
	//if(!savedState)
	//	reg[14].I += 2;
	//reg[15].I = 0x1c;
	//armState = true;
	armIrqEnable = false;
	armFiqEnable = false;
	ARM7Backend::UpdateInterrupts(ctx);

	armNextPC = 0x1c;
	//reg[15].I += 4;
}

#define CPUSwitchMode(mode, saveState) CPUSwitchMode(ctx, mode, saveState)
#define CPUUpdateFlags() CPUUpdateFlags(ctx)
#define CPUSoftwareInterrupt(comment) CPUSoftwareInterrupt(ctx, comment)
#define CPUUndefinedException() CPUUndefinedException(ctx)
#define CPUUpdateCPSR() CPUUpdateCPSR(ctx)

u32 DYNACALL ARM7Backend::singleOp(Arm7Context* ctx, u32 opcode) {
	u32 clockTicks = 0;

#define NO_OPCODE_READ

#include "arm-new.h"

#undef NO_OPCODE_READ

	return clockTicks;
}

u32 DYNACALL ARM7Backend::Step(Arm7Context* ctx) {
	u32 clockTicks = 0;

	if (reg[INTR_PEND].I)
	{
		CPUFiq(ctx);
	}

	reg[15].I = armNextPC + 8;
#include "arm-new.h"

	return clockTicks;
}


u32 DYNACALL ARM7Backend::StepMany(Arm7Context* ctx, u32 minCycles) {
	u32 clockTicks = 0;

	while (clockTicks < minCycles)
	{
		clockTicks += Step(ctx);
	}

	return clockTicks;
}