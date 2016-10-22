#include "arm7.h"
#include "arm_mem.h"


#include <map>


#define C_CORE

#if 0
	#define arm_printf printf
#else
	void arm_printf(...) { }
#endif

//#define CPUReadHalfWordQuick(addr) arm_ReadMem16(addr & 0x7FFFFF)
#define CPUReadMemoryQuick(addr) (*(u32*)&aica_ram.data[addr&ARAM_MASK])
#define CPUReadByte arm_ReadMem8
#define CPUReadMemory arm_ReadMem32
#define CPUReadHalfWord arm_ReadMem16
#define CPUReadHalfWordSigned(addr) ((s16)arm_ReadMem16(addr))

#define CPUWriteMemory arm_WriteMem32
#define CPUWriteHalfWord arm_WriteMem16
#define CPUWriteByte arm_WriteMem8


#define reg arm_Reg
#define armNextPC reg[R15_ARM_NEXT].I

#define CPUUpdateTicksAccesint(a) 1
#define CPUUpdateTicksAccessSeq32(a) 1
#define CPUUpdateTicksAccesshort(a) 1
#define CPUUpdateTicksAccess32(a) 1
#define CPUUpdateTicksAccess16(a) 1

//bool arm_FiqPending; -- not used , i use the input directly :)
//bool arm_IrqPending;

DECL_ALIGN(8) reg_pair arm_Reg[RN_ARM_REG_COUNT];

static INLINE void CPUSwap(u32 *a, u32 *b)
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

bool armIrqEnable;
bool armFiqEnable;
//bool armState;
int armMode;

bool Arm7Enabled=false;

u8 cpuBitsSet[256];

bool intState = false;
bool stopState = false;
bool holdState = false;

extern "C" void CPUFiq();

static void CPUUpdateCPSR(void)
{
	reg_pair CPSR;

	CPSR.I = reg[RN_CPSR].I & 0x40;

	/*
	if(N_FLAG)
		CPSR |= 0x80000000;
	if(Z_FLAG)
		CPSR |= 0x40000000;
	if(C_FLAG)
		CPSR |= 0x20000000;
	if(V_FLAG)
		CPSR |= 0x10000000;
	if(!armState)
		CPSR |= 0x00000020;
	*/

	CPSR.PSR.NZCV=reg[RN_PSR_FLAGS].FLG.NZCV;


	if (!armFiqEnable)
		CPSR.I |= 0x40;
	if(!armIrqEnable)
		CPSR.I |= 0x80;

	CPSR.PSR.M=armMode;
	
	reg[16].I = CPSR.I;
}

static void CPUUpdateFlags(void)
{
	u32 CPSR = reg[16].I;

	reg[RN_PSR_FLAGS].FLG.NZCV=reg[16].PSR.NZCV;

	/*
	N_FLAG = (CPSR & 0x80000000) ? true: false;
	Z_FLAG = (CPSR & 0x40000000) ? true: false;
	C_FLAG = (CPSR & 0x20000000) ? true: false;
	V_FLAG = (CPSR & 0x10000000) ? true: false;
	*/
	//armState = (CPSR & 0x20) ? false : true;
	armIrqEnable = (CPSR & 0x80) ? false : true;
	armFiqEnable = (CPSR & 0x40) ? false : true;
	update_armintc();
}

static void CPUSwitchMode(int mode, bool saveState, bool breakLoop)
{
	CPUUpdateCPSR();

	switch(armMode)
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
		reg[R13_IRQ].I  = reg[13].I;
		reg[R14_IRQ].I  = reg[14].I;
		reg[SPSR_IRQ].I =  reg[17].I;
		break;
	case 0x13:
		reg[R13_SVC].I  = reg[13].I;
		reg[R14_SVC].I  = reg[14].I;
		reg[SPSR_SVC].I =  reg[17].I;
		break;
	case 0x17:
		reg[R13_ABT].I  = reg[13].I;
		reg[R14_ABT].I  = reg[14].I;
		reg[SPSR_ABT].I =  reg[17].I;
		break;
	case 0x1b:
		reg[R13_UND].I  = reg[13].I;
		reg[R14_UND].I  = reg[14].I;
		reg[SPSR_UND].I =  reg[17].I;
		break;
	}

	u32 CPSR = reg[16].I;
	u32 SPSR = reg[17].I;

	switch(mode)
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
		if(saveState)
			reg[17].I = CPSR;
		else
			reg[17].I = reg[SPSR_FIQ].I;
		break;
	case 0x12:
		reg[13].I = reg[R13_IRQ].I;
		reg[14].I = reg[R14_IRQ].I;
		reg[16].I = SPSR;
		if(saveState)
			reg[17].I = CPSR;
		else
			reg[17].I = reg[SPSR_IRQ].I;
		break;
	case 0x13:
		reg[13].I = reg[R13_SVC].I;
		reg[14].I = reg[R14_SVC].I;
		reg[16].I = SPSR;
		if(saveState)
			reg[17].I = CPSR;
		else
			reg[17].I = reg[SPSR_SVC].I;
		break;
	case 0x17:
		reg[13].I = reg[R13_ABT].I;
		reg[14].I = reg[R14_ABT].I;
		reg[16].I = SPSR;
		if(saveState)
			reg[17].I = CPSR;
		else
			reg[17].I = reg[SPSR_ABT].I;
		break;
	case 0x1b:
		reg[13].I = reg[R13_UND].I;
		reg[14].I = reg[R14_UND].I;
		reg[16].I = SPSR;
		if(saveState)
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
	CPUUpdateFlags();
	CPUUpdateCPSR();
}

static void CPUSoftwareInterrupt(int comment)
{
	u32 PC = reg[R15_ARM_NEXT].I+4;
	//bool savedArmState = armState;
	CPUSwitchMode(0x13, true, false);
	reg[14].I = PC;
//	reg[15].I = 0x08;
	
	armIrqEnable = false;
	armNextPC = 0x08;
//	reg[15].I += 4;
}

static void CPUUndefinedException(void)
{
	printf("arm7: CPUUndefinedException(). SOMETHING WENT WRONG\n");
	u32 PC = reg[R15_ARM_NEXT].I+4;
	CPUSwitchMode(0x1b, true, false);
	reg[14].I = PC;
//	reg[15].I = 0x04;
	armIrqEnable = false;
	armNextPC = 0x04;
//	reg[15].I += 4;  
}


void arm_Run_(u32 CycleCount)
{
	if (!Arm7Enabled)
		return;

	u32 clockTicks=0;
	while (clockTicks<CycleCount)
	{
		if (reg[INTR_PEND].I)
		{
			CPUFiq();
		}

		reg[15].I = armNextPC + 8;
		#include "arm-new.h"
	}
}

void armt_init(void);

void arm_Init()
{
	arm_Reset();

	for (int i = 0; i < 256; i++)
	{
		int count = 0;
		for (int j = 0; j < 8; j++)
			if (i & (1 << j))
				count++;

		cpuBitsSet[i] = count;
	}
}

static void FlushCache(void);

void arm_Reset()
{
	Arm7Enabled = false;
	// clean registers
	memset(&arm_Reg[0], 0, sizeof(arm_Reg));

	armMode = 0x1F;

	reg[13].I = 0x03007F00;
	reg[15].I = 0x0000000;
	reg[16].I = 0x00000000;
	reg[R13_IRQ].I = 0x03007FA0;
	reg[R13_SVC].I = 0x03007FE0;
	armIrqEnable = true;      
	armFiqEnable = false;
	update_armintc();

	//armState = true;
	C_FLAG = V_FLAG = N_FLAG = Z_FLAG = false;

	// disable FIQ
	reg[16].I |= 0x40;

	CPUUpdateCPSR();

	armNextPC = reg[15].I;
	reg[15].I += 4;
}

/*

//NO IRQ on aica ..
void CPUInterrupt()
{
	u32 PC = reg[15].I;
	//bool savedState = armState;
	CPUSwitchMode(0x12, true, false);
	reg[14].I = PC;
	//if(!savedState)
	//	reg[14].I += 2;
	reg[15].I = 0x18;
	//armState = true;
	armIrqEnable = false;

	armNextPC = reg[15].I;
	reg[15].I += 4;
}

*/

extern "C"
NOINLINE
void CPUFiq()
{
	u32 PC = reg[R15_ARM_NEXT].I+4;
	//bool savedState = armState;
	CPUSwitchMode(0x11, true, false);
	reg[14].I = PC;
	//if(!savedState)
	//	reg[14].I += 2;
	//reg[15].I = 0x1c;
	//armState = true;
	armIrqEnable = false;
	armFiqEnable = false;
	update_armintc();

	armNextPC = 0x1c;
	//reg[15].I += 4;
}


/*
	--Seems like aica has 3 interrupt controllers actualy (damn lazy sega ..)
	The "normal" one (the one that exists on scsp) , one to emulate the 68k intc , and , 
	of course , the arm7 one

	The output of the sci* bits is input to the e68k , and the output of e68k is inputed into the FIQ
	pin on arm7
*/
#include "hw/sh4/sh4_core.h"


void arm_SetEnabled(bool enabled)
{
	if(!Arm7Enabled && enabled)
			arm_Reset();
	
	Arm7Enabled=enabled;
}



//Emulate a single arm op, passed in opcode
//DYNACALL for ECX passing

u32 DYNACALL arm_single_op(u32 opcode)
{
	u32 clockTicks=0;

#define NO_OPCODE_READ

	//u32 static_opcode=((opcd_hash&0xFFF0)<<16) |  ((opcd_hash&0x000F)<<4);
	//u32 static_opcode=((opcd_hash)<<28);
#include "arm-new.h"

	return clockTicks;
}

void libAICA_TimeStep();

void arm_Run(u32 CycleCount)
{ 
	for (int i=0;i<32;i++)
	{
		arm_Run_(CycleCount/32);
		libAICA_TimeStep();
	}
}
