#pragma once

#include "types.h"
#include "hw/aica/aica_if.h"

#define REG_L (0x2D00)
#define REG_M (0x2D04)

extern u32 e68k_reg_L;
extern u32 e68k_reg_M; //constant ?

template <u32 sz,class T>
void arm_WriteReg(u32 addr,T data);

static INLINE u8 DYNACALL ReadMemArm1(u32 addr)
{
	addr&=0x00FFFFFF;
	if (addr<0x800000)
		return *(u8*)&aica_ram[addr&(ARAM_MASK)];

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
		return *(u16*)&aica_ram[addr&(ARAM_MASK-(1))];

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
		u32 rv=*(u32*)&aica_ram[addr&(ARAM_MASK-(3))];
		
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
		*(u8*)&aica_ram[addr&(ARAM_MASK)]=data;
      return;
   }

   arm_WriteReg<1,u8>(addr,data);
}

static INLINE void DYNACALL WriteMemArm2(u32 addr,u16 data)
{
	addr&=0x00FFFFFF;
	if (addr<0x800000)
   {
		*(u16*)&aica_ram[addr&(ARAM_MASK-(1))]=data;
      return;
   }

   arm_WriteReg<2,u16>(addr,data);
}

static INLINE void DYNACALL WriteMemArm4(u32 addr,u32 data)
{
	addr&=0x00FFFFFF;
	if (addr<0x800000)
   {
		*(u32*)&aica_ram[addr&(ARAM_MASK-(3))]=data;
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

u32 sh4_ReadMem_reg(u32 addr,u32 size);
void sh4_WriteMem_reg(u32 addr,u32 data,u32 size);

void init_mem();
void term_mem();

#define aica_reg_16 ((u16*)aica_reg)

#define AICA_RAM_SIZE (ARAM_SIZE)
#define AICA_RAM_MASK (ARAM_MASK)

#define AICA_MEMMAP_RAM_SIZE (8*1024*1024)				//this is the max for the map, the actual ram size is AICA_RAM_SIZE
#define AICA_MEMMAP_RAM_MASK (AICA_MEMMAP_RAM_SIZE-1)

extern bool e68k_out;

void update_armintc();
