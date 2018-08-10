#pragma once
#include "types.h"
#include "hw/aica/aica_if.h"

template <u32 sz,class T>
T arm_ReadReg(u32 addr);
template <u32 sz,class T>
void arm_WriteReg(u32 addr,T data);

/* TODO/FIXME - might need to cleanup */
//Set to true when the out of the intc is 1
extern bool e68k_out;
extern u32 e68k_reg_L;
extern u32 e68k_reg_M;

static INLINE u8 DYNACALL ReadMemArm1(u32 addr)
{
	addr&=0x00FFFFFF;
	if (addr<0x800000)
		return *(u8*)&aica_ram.data[addr&(ARAM_MASK)];

	addr&=0x7FFF;

   switch (addr)
   {
      case REG_L:
         return e68k_reg_L;
      case REG_M:
         return e68k_reg_M;	//shouldn't really happen
      default:
         break;
   }

   return libAICA_ReadReg(addr,1);
}

static INLINE u16 DYNACALL ReadMemArm2(u32 addr)
{
	addr&=0x00FFFFFF;
	if (addr<0x800000)
		return *(u16*)&aica_ram.data[addr&(ARAM_MASK-(1))];

	addr&=0x7FFF;

   switch (addr)
   {
      case REG_L:
         return e68k_reg_L;
      case REG_M:
         return e68k_reg_M;	//shouldn't really happen
      default:
         break;
   }

   return libAICA_ReadReg(addr,2);
}

static INLINE u32 DYNACALL ReadMemArm4(u32 addr)
{
	addr&=0x00FFFFFF;
	if (addr<0x800000)
	{
		u32 rv=*(u32*)&aica_ram.data[addr&(ARAM_MASK-(3))];

		if (unlikely(addr&3))
		{
			u32 sf=(addr&3)*8;
			return (rv>>sf) | (rv<<(32-sf));
		}
      return rv;
	}

	addr&=0x7FFF;

   switch (addr)
   {
      case REG_L:
         return e68k_reg_L;
      case REG_M:
         return e68k_reg_M;	//shouldn't really happen
      default:
         break;
   }

   return libAICA_ReadReg(addr,4);
}

static INLINE void DYNACALL WriteMemArm1(u32 addr,u8 data)
{
	addr&=0x00FFFFFF;
	if (addr<0x800000)
   {
		*(u8*)&aica_ram.data[addr&(ARAM_MASK)]=data;
      return;
   }

   arm_WriteReg<1,u8>(addr,data);
}

static INLINE void DYNACALL WriteMemArm2(u32 addr,u16 data)
{
	addr&=0x00FFFFFF;
	if (addr<0x800000)
   {
		*(u16*)&aica_ram.data[addr&(ARAM_MASK-(1))]=data;
      return;
   }

   arm_WriteReg<2,u16>(addr,data);
}

static INLINE void DYNACALL WriteMemArm4(u32 addr,u32 data)
{
	addr&=0x00FFFFFF;
	if (addr<0x800000)
   {
		*(u32*)&aica_ram.data[addr&(ARAM_MASK-(3))]=data;
      return;
   }

   arm_WriteReg<4,u32>(addr,data);
}

#define arm_ReadMem8 ReadMemArm1
#define arm_ReadMem16 ReadMemArm2
#define arm_ReadMem32 ReadMemArm4

#define arm_WriteMem8 WriteMemArm1
#define arm_WriteMem16 WriteMemArm2
#define arm_WriteMem32 WriteMemArm4

extern bool e68k_out;

#define update_armintc() arm_Reg[INTR_PEND].I=e68k_out && armFiqEnable
