#include "aica_mem.h"
#include "dsp.h"
#include "sgc_if.h"
#include "hw/aica/aica_if.h"

u8 aica_reg[0x8000];

//00000000~007FFFFF @DRAM_AREA* 
//00800000~008027FF @CHANNEL_DATA 
//00802800~00802FFF @COMMON_DATA 
//00803000~00807FFF @DSP_DATA 
template<u32 sz>
u32 ReadReg(u32 addr)
{
	if (addr<0x2800)
	{
		ReadMemArrRet(aica_reg,addr,sz);
	}
	if (addr < 0x2818)
	{
		if (sz==1)
		{
			ReadCommonReg(addr,true);
         return aica_reg[addr];
		}
		else
		{
			ReadCommonReg(addr,false);
			//ReadCommonReg8(addr+1);
         return *(u16*)&aica_reg[addr];
		}
	}

	ReadMemArrRet(aica_reg,addr,sz);
}
template<u32 sz>
void WriteReg(u32 addr,u32 data)
{
	if (addr < 0x2000)
	{
		//Channel data
		u32 chan=addr>>7;
		u32 reg=addr&0x7F;
		if (sz==1)
		{
         aica_reg[addr]=(u8)data;
			WriteChannelReg8(chan,reg);
		}
		else
		{
         *(u16*)&aica_reg[addr]=(u16)data;
			WriteChannelReg8(chan,reg);
			WriteChannelReg8(chan,reg+1);
		}
		return;
	}

	if (addr<0x2800)
	{
		if (sz==1)
         aica_reg[addr]=(u8)data;
		else 
         *(u16*)&aica_reg[addr]=(u16)data;
		return;
	}

	if (addr < 0x2818)
	{
		if (sz==1)
		{
			WriteCommonReg8(addr,data);
		}
		else
		{
			WriteCommonReg8(addr,data&0xFF);
			WriteCommonReg8(addr+1,data>>8);
		}
		return;
	}

	if (addr>=0x3000)
	{
		if (sz==1)
		{
         aica_reg[addr]=(u8)data;
			dsp_writenmem(addr);
		}
		else
		{
         *(u16*)&aica_reg[addr]=(u16)data;
			dsp_writenmem(addr);
			dsp_writenmem(addr+1);
		}
	}
	if (sz==1)
		WriteAicaReg<1>(addr,data);
	else
		WriteAicaReg<2>(addr,data);
}

/* Aica reads (both sh4&arm) */
u32 libAICA_ReadReg(u32 addr,u32 size)
{
   if (size==1)
      return ReadReg<1>(addr & 0x7FFF);
   return ReadReg<2>(addr & 0x7FFF);
}

void libAICA_WriteReg(u32 addr,u32 data,u32 size)
{
	if (size==1)
		WriteReg<1>(addr & 0x7FFF,data);
	else
		WriteReg<2>(addr & 0x7FFF,data);
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

