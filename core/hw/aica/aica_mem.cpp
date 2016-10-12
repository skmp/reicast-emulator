#include "aica_mem.h"
#include "dsp.h"
#include "sgc_if.h"
#include "hw/aica/aica_if.h"

u8 aica_reg[0x8000];

//00000000~007FFFFF @DRAM_AREA* 
//00800000~008027FF @CHANNEL_DATA 
//00802800~00802FFF @COMMON_DATA 
//00803000~00807FFF @DSP_DATA 

static u32 ReadReg1(u32 addr)
{
	if (addr<0x2800)
		ReadMemArrRet(aica_reg,addr,1);

	if (addr < 0x2818)
   {
      ReadCommonReg(addr,true);
      return aica_reg[addr];
   }

	ReadMemArrRet(aica_reg,addr,1);
}

static u32 ReadReg2(u32 addr)
{
	if (addr<0x2800)
		ReadMemArrRet(aica_reg,addr,2);

	if (addr < 0x2818)
   {
      ReadCommonReg(addr,false);
      return *(u16*)&aica_reg[addr];
   }

	ReadMemArrRet(aica_reg,addr,2);
}

/* Aica reads (both sh4&arm) */
u32 libAICA_ReadReg(u32 addr,u32 size)
{
   if (size==1)
      return ReadReg1(addr & 0x7FFF);

   return ReadReg2(addr & 0x7FFF);
}

static void WriteReg2(u32 addr,u32 data)
{
	if (addr < 0x2000)
	{
		//Channel data
		u32 chan=addr>>7;
		u32 reg=addr&0x7F;
      *(u16*)&aica_reg[addr]=(u16)data;
      WriteChannelReg8(chan,reg);
      WriteChannelReg8(chan,reg+1);
		return;
	}

	if (addr<0x2800)
	{
      *(u16*)&aica_reg[addr]=(u16)data;
		return;
	}

	if (addr < 0x2818)
	{
      WriteCommonReg8(addr,data&0xFF);
      WriteCommonReg8(addr+1,data>>8);
		return;
	}

	if (addr>=0x3000)
	{
      *(u16*)&aica_reg[addr]=(u16)data;
      dsp_writenmem(addr);
      dsp_writenmem(addr+1);
	}

   WriteAicaReg<2>(addr,data);
}

static void WriteReg1(u32 addr,u32 data)
{
	if (addr < 0x2000)
	{
		//Channel data
		u32 chan=addr>>7;
		u32 reg=addr&0x7F;
      aica_reg[addr]=(u8)data;
      WriteChannelReg8(chan,reg);
		return;
	}

	if (addr<0x2800)
	{
      aica_reg[addr]=(u8)data;
		return;
	}

	if (addr < 0x2818)
	{
      WriteCommonReg8(addr,data);
		return;
	}

	if (addr>=0x3000)
	{
      aica_reg[addr]=(u8)data;
      dsp_writenmem(addr);
	}

   WriteAicaReg<1>(addr,data);
}

void libAICA_WriteReg(u32 addr,u32 data,u32 size)
{
	if (size==1)
		WriteReg1(addr & 0x7FFF,data);
	else
		WriteReg2(addr & 0x7FFF,data);
}

//Map using _vmem .. yay
void init_mem(void)
{
	memset(aica_reg,0,sizeof(aica_reg));
	aica_ram.data[ARAM_SIZE-1]=1;
	aica_ram.Zero();
}

//kill mem map & free used mem ;)
void term_mem()
{

}

