/*
	aica interface
		Handles RTC, Display mode reg && arm reset reg !
	arm7 is handled on a separate arm plugin now
*/
#include "aica.h"
#include "sgc_if.h"
#include "aica_mmio.h"
#include "aica_mem.h"
#include <math.h>
#include "hw/holly/holly_intc.h"
#include "hw/holly/sb.h"
#include "hw/arm7/SoundCPU.h"
#include "hw/arm7/arm7.h"

#define SH4_IRQ_BIT (1<<(holly_SPU_IRQ&255))

CommonData_struct* CommonData;
DSPData_struct* DSPData;
InterruptInfo* MCIEB;
InterruptInfo* MCIPD;
InterruptInfo* MCIRE;
InterruptInfo* SCIEB;
InterruptInfo* SCIPD;
InterruptInfo* SCIRE;

//Interrupts
//arm side
u32 GetL(u32 witch)
{
	if (witch > 7)
		witch = 7; //higher bits share bit 7

	u32 bit = 1 << witch;
	u32 rv = 0;

	if (CommonData->SCILV0 & bit)
		rv = 1;

	if (CommonData->SCILV1 & bit)
		rv |= 2;

	if (CommonData->SCILV2 & bit)
		rv |= 4;

	return rv;
}
void update_arm_interrupts()
{
	u32 p_ints = SCIEB->full & SCIPD->full;

	u32 Lval = 0;
	if (p_ints)
	{
		u32 bit_value = 1;//first bit
		//scan all interrupts , lo to hi bit.I assume low bit ints have higher priority over others
		for (u32 i = 0; i < 11; i++)
		{
			if (p_ints & bit_value)
			{
				//for the first one , Set the L reg & exit
				Lval = GetL(i);
				break;
			}
			bit_value <<= 1; //next bit
		}
	}

	libARM_InterruptChange(p_ints, Lval);
}

//sh4 side
void UpdateSh4Ints()
{
	u32 p_ints = MCIEB->full & MCIPD->full;
	if (p_ints)
	{
		if ((SB_ISTEXT & SH4_IRQ_BIT) == 0)
		{
			//if no interrupt is already pending then raise one :)
			asic_RaiseInterrupt(holly_SPU_IRQ);
		}
	}
	else
	{
		if (SB_ISTEXT & SH4_IRQ_BIT)
		{
			asic_CancelInterrupt(holly_SPU_IRQ);
		}
	}

}


#include "hw/sh4/sh4_mmio.h"

#include "types.h"
#include "aica_mmio.h"
#include "sgc_if.h"
#include "dsp.h"

#include "hw/sh4/sh4_mem.h"
#include "hw/holly/sb.h"
#include "hw/holly/holly_intc.h"
#include "hw/sh4/sh4_sched.h"

#include "libswirl.h"
#include <time.h>

struct AICARTC_impl : MMIODevice
{
	int rtc_schid = -1;
	u32 rtc_EN = 0;
	u32 RealTimeClock;

	u32 GetRTC_now()
	{
		// The Dreamcast Epoch time is 1/1/50 00:00 but without support for time zone or DST.
		// We compute the TZ/DST current time offset and add it to the result
		// as if we were in the UTC time zone (as well as the DC Epoch)
		time_t rawtime = time(NULL);
		struct tm localtm, gmtm;
		localtm = *localtime(&rawtime);
		gmtm = *gmtime(&rawtime);
		gmtm.tm_isdst = -1;
		time_t time_offset = mktime(&localtm) - mktime(&gmtm);
		// 1/1/50 to 1/1/70 is 20 years and 5 leap days
		return (20 * 365 + 5) * 24 * 60 * 60 + rawtime + time_offset;
	}

	bool Init() {
		rtc_schid = sh4_sched_register(this, 0, STATIC_FORWARD(AICARTC_impl, Update));
		sh4_sched_request(rtc_schid, SH4_MAIN_CLOCK);

		return true;
	}

	void Reset(bool m) {
		RealTimeClock = GetRTC_now();
	}

	u32 Read(u32 addr, u32 sz)
	{
		switch (addr & 0xFF)
		{
		case 0:
			return RealTimeClock >> 16;
		case 4:
			return RealTimeClock & 0xFFFF;
		case 8:
			return 0;
		}

		printf("ReadMem_aica_rtc : invalid address\n");
		return 0;
	}

	void Write(u32 addr, u32 data, u32 sz)
	{
		switch (addr & 0xFF)
		{
		case 0:
			if (rtc_EN)
			{
				RealTimeClock &= 0xFFFF;
				RealTimeClock |= (data & 0xFFFF) << 16;
				rtc_EN = 0;
			}
			return;
		case 4:
			if (rtc_EN)
			{
				RealTimeClock &= 0xFFFF0000;
				RealTimeClock |= data & 0xFFFF;
				//TODO: Clean the internal timer ?
			}
			return;
		case 8:
			rtc_EN = data & 1;
			return;
		}

		return;
	}

	int Update(int tag, int c, int j)
	{
		RealTimeClock++;

		return SH4_MAIN_CLOCK;
	}


	void serialize(void** data, unsigned int* total_size) {
		REICAST_S(rtc_EN);
	}

	void unserialize(void** data, unsigned int* total_size) {
		REICAST_US(rtc_EN);
	}
};

MMIODevice* Create_RTCDevice() {
	return new AICARTC_impl();
}

u32 libAICA_GetRTC_now() {
	return sh4_cpu->GetA0H<AICARTC_impl>(A0H_RTC)->GetRTC_now();
}

struct AicaDevice final : AICA {
	u32 VREG;//video reg =P
	u32 ARMRST;//arm reset reg

	int dma_sched_id;

	AicaTimer timers[3];

	u8 aica_reg[0x8000];


	//Memory i/o
	template<u32 sz>
	void WriteAicaReg(u32 reg, u32 data)
	{
		switch (reg)
		{
		case SCIPD_addr:
			verify(sz != 1);
			if (data & (1 << 5))
			{
				SCIPD->SCPU = 1;
				update_arm_interrupts();
			}
			//Read only
			return;

		case SCIRE_addr:
		{
			verify(sz != 1);
			SCIPD->full &= ~(data /*& SCIEB->full*/);	//is the & SCIEB->full needed ? doesn't seem like it
			data = 0;//Write only
			update_arm_interrupts();
		}
		break;

		case MCIPD_addr:
			if (data & (1 << 5))
			{
				verify(sz != 1);
				MCIPD->SCPU = 1;
				UpdateSh4Ints();
			}
			//Read only
			return;

		case MCIRE_addr:
		{
			verify(sz != 1);
			MCIPD->full &= ~data;
			UpdateSh4Ints();
			//Write only
		}
		break;

		case TIMER_A:
			WriteMemArr(aica_reg, reg, data, sz);
			timers[0].RegisterWrite();
			break;

		case TIMER_B:
			WriteMemArr(aica_reg, reg, data, sz);
			timers[1].RegisterWrite();
			break;

		case TIMER_C:
			WriteMemArr(aica_reg, reg, data, sz);
			timers[2].RegisterWrite();
			break;

		default:
			WriteMemArr(aica_reg, reg, data, sz);
			break;
		}
	}


	//00000000~007FFFFF @DRAM_AREA* 
	//00800000~008027FF @CHANNEL_DATA 
	//00802800~00802FFF @COMMON_DATA 
	//00803000~00807FFF @DSP_DATA 
	template<u32 sz>
	u32 ReadReg_internal(u32 addr)
	{
		if (addr < 0x2800)
		{
			ReadMemArrRet(aica_reg, addr, sz);
		}
		if (addr < 0x2818)
		{
			if (sz == 1)
			{
				ReadCommonReg(addr, true);
				ReadMemArrRet(aica_reg, addr, 1);
			}
			else
			{
				ReadCommonReg(addr, false);
				//ReadCommonReg8(addr+1);
				ReadMemArrRet(aica_reg, addr, 2);
			}
		}

		ReadMemArrRet(aica_reg, addr, sz);
	}
	template<u32 sz>
	void WriteReg_internal(u32 addr, u32 data)
	{
		if (addr < 0x2000)
		{
			//Channel data
			u32 chan = addr >> 7;
			u32 reg = addr & 0x7F;
			if (sz == 1)
			{
				WriteMemArr(aica_reg, addr, data, 1);
				WriteChannelReg8(chan, reg);
			}
			else
			{
				WriteMemArr(aica_reg, addr, data, 2);
				WriteChannelReg8(chan, reg);
				WriteChannelReg8(chan, reg + 1);
			}
			return;
		}

		if (addr < 0x2800)
		{
			if (sz == 1)
			{
				WriteMemArr(aica_reg, addr, data, 1);
			}
			else
			{
				WriteMemArr(aica_reg, addr, data, 2);
			}
			return;
		}

		if (addr < 0x2818)
		{
			if (sz == 1)
			{
				WriteCommonReg8(addr, data);
			}
			else
			{
				WriteCommonReg8(addr, data & 0xFF);
				WriteCommonReg8(addr + 1, data >> 8);
			}
			return;
		}

		if (addr >= 0x3000)
		{
			if (sz == 1)
			{
				WriteMemArr(aica_reg, addr, data, 1);
				dsp->WritenMem(addr);
			}
			else
			{
				WriteMemArr(aica_reg, addr, data, 2);
				dsp->WritenMem(addr);
				dsp->WritenMem(addr + 1);
			}
		}
		if (sz == 1)
			WriteAicaReg<1>(addr, data);
		else
			WriteAicaReg<2>(addr, data);
	}

	void ArmSetRST()
	{
		ARMRST &= 1;
		libARM_SetResetState(ARMRST);
	}

	//Aica reads (both sh4&arm)
	u32 AICA_ReadReg(u32 addr, u32 size)
	{
		if (size == 1)
			return ReadReg_internal<1>(addr & 0x7FFF);
		else
			return ReadReg_internal<2>(addr & 0x7FFF);

		//must never come here
		return 0;
	}

	void AICA_WriteReg(u32 addr, u32 data, u32 size)
	{
		if (size == 1)
			WriteReg_internal<1>(addr & 0x7FFF, data);
		else
			WriteReg_internal<2>(addr & 0x7FFF, data);
	}


	int dma_end_sched(int tag, int cycl, int jitt)
	{
		u32 len = SB_ADLEN & 0x7FFFFFFF;

		if (SB_ADLEN & 0x80000000)
			SB_ADEN = 1;//
		else
			SB_ADEN = 0;//

		SB_ADSTAR += len;
		SB_ADSTAG += len;
		SB_ADST = 0x00000000;//dma done
		SB_ADLEN = 0x00000000;

		// indicate that dma is not happening, or has been paused
		SB_ADSUSP |= 0x10;

		asic->RaiseInterrupt(holly_SPU_DMA);

		return 0;
	}

	void Write_SB_ADST(u32 addr, u32 data)
	{
		//0x005F7800	SB_ADSTAG	RW	AICA:G2-DMA G2 start address 
		//0x005F7804	SB_ADSTAR	RW	AICA:G2-DMA system memory start address 
		//0x005F7808	SB_ADLEN	RW	AICA:G2-DMA length 
		//0x005F780C	SB_ADDIR	RW	AICA:G2-DMA direction 
		//0x005F7810	SB_ADTSEL	RW	AICA:G2-DMA trigger select 
		//0x005F7814	SB_ADEN	RW	AICA:G2-DMA enable 
		//0x005F7818	SB_ADST	RW	AICA:G2-DMA start 
		//0x005F781C	SB_ADSUSP	RW	AICA:G2-DMA suspend 

		if (data & 1)
		{
			if (SB_ADEN & 1)
			{
				u32 src = SB_ADSTAR;
				u32 dst = SB_ADSTAG;
				u32 len = SB_ADLEN & 0x7FFFFFFF;

				if ((SB_ADDIR & 1) == 1)
				{
					//swap direction
					u32 tmp = src;
					src = dst;
					dst = tmp;
					printf("**AICA DMA : SB_ADDIR==1: Not sure this works, please report if broken/missing sound or crash\n**");
				}

				WriteMemBlock_nommu_dma(dst, src, len);
				/*
				for (u32 i=0;i<len;i+=4)
				{
					u32 data=ReadMem32_nommu(src+i);
					WriteMem32_nommu(dst+i,data);
				}
				*/

				// idicate that dma is in progress
				SB_ADSUSP &= ~0x10;

				if (!settings.aica.OldSyncronousDma)
				{

					// Schedule the end of DMA transfer interrupt
					int cycles = len * (SH4_MAIN_CLOCK / 2 / 25000000);       // 16 bits @ 25 MHz
					if (cycles < 4096)
						dma_end_sched(0, 0, 0);
					else
						sh4_sched_request(dma_sched_id, cycles);
				}
				else
				{
					dma_end_sched(0, 0, 0);
				}
			}
		}
	}

	void Write_SB_E1ST(u32 addr, u32 data)
	{
		//0x005F7800	SB_ADSTAG	RW	AICA:G2-DMA G2 start address 
		//0x005F7804	SB_ADSTAR	RW	AICA:G2-DMA system memory start address 
		//0x005F7808	SB_ADLEN	RW	AICA:G2-DMA length 
		//0x005F780C	SB_ADDIR	RW	AICA:G2-DMA direction 
		//0x005F7810	SB_ADTSEL	RW	AICA:G2-DMA trigger select 
		//0x005F7814	SB_ADEN	RW	AICA:G2-DMA enable 
		//0x005F7818	SB_ADST	RW	AICA:G2-DMA start 
		//0x005F781C	SB_ADSUSP	RW	AICA:G2-DMA suspend 

		if (data & 1)
		{
			if (SB_E1EN & 1)
			{
				u32 src = SB_E1STAR;
				u32 dst = SB_E1STAG;
				u32 len = SB_E1LEN & 0x7FFFFFFF;

				if (SB_E1DIR == 1)
				{
					u32 t = src;
					src = dst;
					dst = t;
					printf("G2-EXT1 DMA : SB_E1DIR==1 DMA Read to 0x%X from 0x%X %d bytes\n", dst, src, len);
				}
				else
					printf("G2-EXT1 DMA : SB_E1DIR==0:DMA Write to 0x%X from 0x%X %d bytes\n", dst, src, len);

				WriteMemBlock_nommu_dma(dst, src, len);

				/*
				for (u32 i=0;i<len;i+=4)
				{
					u32 data=ReadMem32_nommu(src+i);
					WriteMem32_nommu(dst+i,data);
				}*/

				if (SB_E1LEN & 0x80000000)
					SB_E1EN = 1;//
				else
					SB_E1EN = 0;//

				SB_E1STAR += len;
				SB_E1STAG += len;
				SB_E1ST = 0x00000000;//dma done
				SB_E1LEN = 0x00000000;


				asic->RaiseInterrupt(holly_EXT_DMA1);
			}
		}
	}

	void Write_SB_E2ST(u32 addr, u32 data)
	{
		if ((data & 1) && (SB_E2EN & 1))
		{
			u32 src = SB_E2STAR;
			u32 dst = SB_E2STAG;
			u32 len = SB_E2LEN & 0x7FFFFFFF;

			if (SB_E2DIR == 1)
			{
				u32 t = src;
				src = dst;
				dst = t;
				printf("G2-EXT2 DMA : SB_E2DIR==1 DMA Read to 0x%X from 0x%X %d bytes\n", dst, src, len);
			}
			else
				printf("G2-EXT2 DMA : SB_E2DIR==0:DMA Write to 0x%X from 0x%X %d bytes\n", dst, src, len);

			WriteMemBlock_nommu_dma(dst, src, len);

			if (SB_E2LEN & 0x80000000)
				SB_E2EN = 1;
			else
				SB_E2EN = 0;

			SB_E2STAR += len;
			SB_E2STAG += len;
			SB_E2ST = 0x00000000;//dma done
			SB_E2LEN = 0x00000000;


			asic->RaiseInterrupt(holly_EXT_DMA2);
		}
	}


	void Write_SB_DDST(u32 addr, u32 data)
	{
		if (data & 1)
			die("SB_DDST DMA not implemented");
	}

	SystemBus* sb;
	ASIC* asic;
	DSP* dsp;
	u8* aica_ram;

	AicaDevice(SystemBus* sb, ASIC* asic, DSP* dsp, u8* aica_ram) : sb(sb), asic(asic), dsp(dsp), aica_ram(aica_ram) { }

	u32 Read(u32 addr, u32 sz) {
		addr &= 0x7FFF;
		if (sz == 1)
		{
			if (addr == 0x2C01)
			{
				return VREG;
			}
			else if (addr == 0x2C00)
			{
				return ARMRST;
			}
			else
			{
				return AICA_ReadReg(addr, sz);
			}
		}
		else
		{
			if (addr == 0x2C00)
			{
				return (VREG << 8) | ARMRST;
			}
			else
			{
				return AICA_ReadReg(addr, sz);
			}
		}
	}
	void Write(u32 addr, u32 data, u32 sz) {
		addr &= 0x7FFF;

		if (sz == 1)
		{
			if (addr == 0x2C01)
			{
				VREG = data;
				printf("VREG = %02X\n", VREG);
			}
			else if (addr == 0x2C00)
			{
				ARMRST = data;
				printf("ARMRST = %02X\n", ARMRST);
				ArmSetRST();
			}
			else
			{
				libAICA_WriteReg(addr, data, sz);
			}
		}
		else
		{
			if (addr == 0x2C00)
			{
				VREG = (data >> 8) & 0xFF;
				ARMRST = data & 0xFF;
				printf("VREG = %02X ARMRST %02X\n", VREG, ARMRST);
				ArmSetRST();
			}
			else
			{
				libAICA_WriteReg(addr, data, sz);
			}
		}
	}

	bool Init()
	{	
		verify(sizeof(*CommonData) == 0x508);
		verify(sizeof(*DSPData) == 0x15C8);

		CommonData = (CommonData_struct*)&aica_reg[0x2800];
		DSPData = (DSPData_struct*)&aica_reg[0x3000];
		//slave cpu (arm7)

		SCIEB = (InterruptInfo*)&aica_reg[0x289C];
		SCIPD = (InterruptInfo*)&aica_reg[0x289C + 4];
		SCIRE = (InterruptInfo*)&aica_reg[0x289C + 8];
		//Main cpu (sh4)
		MCIEB = (InterruptInfo*)&aica_reg[0x28B4];
		MCIPD = (InterruptInfo*)&aica_reg[0x28B4 + 4];
		MCIRE = (InterruptInfo*)&aica_reg[0x28B4 + 8];

		sgc_Init(aica_reg, aica_ram);

		for (int i = 0; i < 3; i++)
			timers[i].Init(aica_reg, i);

		//NRM
		//6
		sb->RegisterRIO(this, SB_ADST_addr, RIO_WF, 0, STATIC_FORWARD(AicaDevice, Write_SB_ADST));
		
		//I really need to implement G2 dma (and rest dmas actually) properly
		//THIS IS NOT AICA, its G2-EXT (BBA)

		sb->RegisterRIO(this, SB_E1ST_addr, RIO_WF, 0, STATIC_FORWARD(AicaDevice, Write_SB_E1ST));
		sb->RegisterRIO(this, SB_E2ST_addr, RIO_WF, 0, STATIC_FORWARD(AicaDevice, Write_SB_E2ST));
		sb->RegisterRIO(this, SB_DDST_addr, RIO_WF, 0, STATIC_FORWARD(AicaDevice, Write_SB_DDST));

		dma_sched_id = sh4_sched_register(this, 0, STATIC_FORWARD(AicaDevice, dma_end_sched));

		return true;
	}

	void Reset(bool Manual)
	{
		memset(aica_reg, 0, sizeof(aica_reg));
		aica_ram[ARAM_SIZE - 1] = 1;
		
		ARMRST = 0;
		VREG = 0;

		ArmSetRST();
	}

	void Term()
	{
		sgc_Term();
	}

	//Mainloop
	void Update(u32 Samples)
	{
		AICA_Sample32();
	}

	//Aica reads (both sh4&arm)
	u32 ReadReg(u32 addr, u32 size) {
		return AICA_ReadReg(addr, size);
	}

	void WriteReg(u32 addr, u32 data, u32 size) {
		AICA_WriteReg(addr, data, size);
	}


	void TimeStep()
	{
		for (int i = 0; i < 3; i++)
			timers[i].StepTimer(1);

		SCIPD->SAMPLE_DONE = 1;

		if (settings.aica.NoBatch)
			AICA_Sample();

		//Make sure sh4/arm interrupt system is up to date :)
		update_arm_interrupts();
		UpdateSh4Ints();
	}

	void serialize(void** data, unsigned int* total_size) {
		for (int i = 0; i < 3; i++)
		{
			REICAST_S(timers[i].c_step);
			REICAST_S(timers[i].m_step);
		}

		REICAST_S(VREG);
		REICAST_S(ARMRST);

		REICAST_SA(aica_reg, 0x8000);
	}

	void unserialize(void** data, unsigned int* total_size) {
		for (int i = 0; i < 3; i++)
		{
			REICAST_US(timers[i].c_step);
			REICAST_US(timers[i].m_step);
		}

		REICAST_US(VREG);
		REICAST_US(ARMRST);

		REICAST_USA(aica_reg, 0x8000);
	}

};

AICA* Create_AicaDevice(SystemBus* sb, ASIC* asic, DSP* dsp, u8* aica_ram) {
	return new AicaDevice(sb, asic, dsp, aica_ram);
}

u32 libAICA_ReadReg(u32 addr, u32 sz) {
	return sh4_cpu->GetA0H<AicaDevice>(A0H_AICA)->ReadReg(addr, sz);
}

void libAICA_WriteReg(u32 addr, u32 data, u32 sz) {
	sh4_cpu->GetA0H<AicaDevice>(A0H_AICA)->WriteReg(addr, data, sz);
}

void libAICA_TimeStep() {
	sh4_cpu->GetA0H<AicaDevice>(A0H_AICA)->TimeStep();
}