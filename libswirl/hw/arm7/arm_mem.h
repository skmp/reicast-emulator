#pragma once
#include "types.h"
#include "hw/aica/aica_if.h"

template <u32 sz,class T>
T arm_ReadReg(u32 addr);
template <u32 sz,class T>
void arm_WriteReg(u32 addr,T data);

template<int sz,typename T>
static inline T DYNACALL scpu_ReadMemArm(u32 addr)
{
	addr&=0x00FFFFFF;
	if (addr<0x800000)
	{
		T rv=*(T*)&aica_ram[addr&(ARAM_MASK-(sz-1))];
		
		if (unlikely(sz==4 && addr&3))
		{
			u32 sf=(addr&3)*8;
			return (rv>>sf) | (rv<<(32-sf));
		}
		else
			return rv;
	}
	else
	{
		return arm_ReadReg<sz,T>(addr);
	}
}

template<int sz,typename T>
static inline void DYNACALL scpu_WriteMemArm(u32 addr,T data)
{
	addr&=0x00FFFFFF;
	if (addr<0x800000)
	{
		*(T*)&aica_ram[addr&(ARAM_MASK-(sz-1))]=data;
	}
	else
	{
		arm_WriteReg<sz,T>(addr,data);
	}
}

#define arm_ReadMem8 scpu_ReadMemArm<1,u8>
#define arm_ReadMem16 scpu_ReadMemArm<2,u16>
#define arm_ReadMem32 scpu_ReadMemArm<4,u32>

#define arm_WriteMem8 scpu_WriteMemArm<1,u8>
#define arm_WriteMem16 scpu_WriteMemArm<2,u16>
#define arm_WriteMem32 scpu_WriteMemArm<4,u32>

void init_mem();
void term_mem();

extern bool e68k_out;

void update_armintc();
