#pragma once
#include "types.h"
#include "assert.h"

#define SCIEB_addr 0x289C
#define SCIPD_addr (0x289C+4)
#define SCIRE_addr (0x289C+8)

#define MCIEB_addr 0x28B4
#define MCIPD_addr (0x28B4+4)
#define MCIRE_addr (0x28B4+8)

#define TIMER_A 0x2890 
#define TIMER_B (0x2890+4) 
#define TIMER_C (0x2890+8)
#define REG_L (0x2D00)
#define REG_M (0x2D04)
struct CommonData_struct
{
	//+0

   /* Master volume for digital output to DAC. */
	u32 MVOL:4;

   /* Reads out the LSI version. */
	u32 VER:4;

   /*
    * 0 : The digital output is the 16-bit DAC interface.
    * 1 : The digital output is the 18-bit DAC interface.
    */
	u32 DAC18B:1;

   /* Specifies the memory to be used
    * 0 : 16M_DRAM
    * 1 : 64M_DRAM
    * Relationships between sound memory space and used memory. */
	u32 MEM8MB:1;

	u32 pad0_0:5;

   /* 
    * 0 : Makes panpot information valid.
    * 1 : Makes panpot information invalid.
    *
    * When panpot information has been made invalid, the
    * sound coming from the channel on one side will
    * double in volume, so the setting of MVOL must be lowered. */
	u32 Mono:1;
	
	u32 :16;
	//+4

   /* Specifies the leading address of the ring buffer (1K word limit). */
	u32 RBP:12;

	u32 pad1_0:1;

   /* Specifies the length of the ring buffer.
    * 0 : 8K words
    * 1 : 16K words
    * 2 : 32K words
    * 3 : 64K words
    */
	u32 RBL:2;

	u32 TESTB0:1;

	u32 :16;
	//+8

   /* MIDI input data buffer. (Consists of 4-byte FIFO). */
	u32 MIBUF:8;
	u32 MIEMP:1;

   /* Indicates that the input FIFO is full.
    * (The above two flags show the status before MIBUS [7:0] is read.)
    */
	u32 MIFUL:1;

   /* Indicates that the input FIFO has overflowed. */
	u32 MIOVF:1;

   /* Indicates that the output FIFO is empty. */
	u32 MOEMP:1;

   /* Indicates that the output FIFO is full. */
	u32 MOFUL:1;
	u32 pad3_0:3;

	u32 :16;

	//+C
   /* MIDI output data buffer */
	u32 MOBUF:8;

   /* Specifies the slot number to monitor SGC, CA, EG, and LP. */
	u32 MSLC:6;

   /* Decides whether ot make the EG monitor AEG or FEG.
    * 0 : AEG monitor
    * 1 : FEG monitor
    */
	u32 AFSEL:1;

	u32 padC_0:1;

	u32 :16;
	//+10

   /* Monitors the 13 high-order bits of the current
    * EG value. During AEG monitoring, the low-order
    * three bits are always "0". */
	u32 EG:13;

   /* Monitors the status of the current EG.
    * 0 : Attack
    * 1 : Decay 1
    * 2 : Decay 2
    * 3 : Release
    */
	u32 SGC:2;

   /* Loop end flag.
    * The channel is selected by MSLC [5:0}. 
    * This flag shows that the loop has ended.
    * This flag is cleared to "0" when it is read. */
	u32 LP:1;
	
	u32 :16;
	//+14

   /* Currently shows the sample position read from
    * the sound source as the 16 high-order bits of
    * of the relative sample No. from SA. The lowest order
    * bit is equivalent to one sample. */
	u32 CA:16;

	u32 :16;
	
	//quite a bit padding here :)
	u8 pad_med_0[0x6C-4];

	//+80
	u32 MRWINH:4;
	u32 $T:1;
	u32 $TSCD:3;
	u32 pad80_0:1;
	u32 DMEA_hi:7;

	u32 :16;
	//+84
	u32 pad84_0:2;
	u32 DMEA_lo:14;

	u32 :16;
	//+88
	u32 pad88_0:2;
	u32 DRGA:13;
	u32 DGATE:1;

	u32 :16;
	//+8C
	u32 DEXE:1;
	u32 pad8C_0:1;
	u32 DLG:13;

   /* Specifies the direction for DMA transmission.
    * 0 : Transmits from sound memory to SCSP register.
    * 1 : Transmits from SCSP register to sound memory. */
	u32 DDIR:1;

	u32 :16;
	//+90

   /* Timer A (Generates an interrupt request
    * at the timing when the UP counter changes from
    * all "1" to all "0".)
    */
	u32 TIMA:8;

   /* Specifies the increment cycle of timer A.
    * 0 : One increment per sample.
    * 1 : One increment per 2 samples.
    * 2 : One increment per 4 samples.
    * 3 : One increment per 8 samples.
    * 4 : One increment per 16 samples.
    * 5 : One increment per 32 samples.
    * 6 : One increment per 64 samples.
    * 7 : One increment per 128 samples.
    */
	u32 TACTL:3;

	u32 pad90_0:5;

	u32 :16;
	//+94

   /* Timer B (Generation of interrupt is the same
    * as for Timer A. ) */
	u32 TIMB:8;

   /* Specifies the increment cycle for timer B.
    * (The code is the same as for timer A. */
	u32 TBCTL:3;
	u32 pad94_0:5;

	u32 :16;
	//+98
	u32 TIMC:8;
	u32 TCCTL:3;
	u32 pad98_0:5;

	u32 :16;

	//+9C
	u32 SCIEB:11;
	u32 pad9C_0:5;

	u32 :16;

	//+A0
	u32 SCIPD:11;
	u32 padA0_0:5;

	u32 :16;

	//+A4
	u32 SCIRE:11;
	u32 padA4_0:5;

	u32 :16;

	//+A8
	u32 SCILV0:8;
	u32 padA8_0:8;

	u32 :16;

	//+AC
	u32 SCILV1:8;
	u32 padAC_0:8;

	u32 :16;

	//+B0
	u32 SCILV2:8;
	u32 padB0_0:8;

	u32 :16;

	//+B4
	u32 MCIEB:11;
	u32 padB4_0:5;

	u32 :16;

	//+B8
	u32 MCIPD:11;
	u32 padB8_0:5;

	u32 :16;

	//+BC
	u32 MCIRE:11;
	u32 padBC_0:5;

	u32 :16;
	
	//some other misc shit FAR away is here :p
	u8 pad_lot_0[0x344-4];

	//+400 , hopefully :p
	u32 AR:1;
	u32 pad400_0:7;
	u32 VREG:2;
	u32 pad400_1:6;

	u32 :16;

	//Even more
	u8 pad_lot_1[0x100-4];

	//+500 , hopefully :p
	u32 L0_r:1;
	u32 L1_r:1;
	u32 L2_r:1;
	u32 L3_r:1;
	u32 L4_r:1;
	u32 L5_r:1;
	u32 L6_r:1;
	u32 L7_r:1;
	
	u32 pad500_0:8;

	u32 :16;

	//+504

	u32 M0_r:1;
	u32 M1_r:1;
	u32 M2_r:1;
	u32 M3_r:1;
	u32 M4_r:1;
	u32 M5_r:1;
	u32 M6_r:1;
	u32 M7_r:1;
	u32 RP:1;
	
	u32 pad504_0:7;

	u32 :16;
};

//should be 0x15C8 in size
struct DSPData_struct
{
	//+0x000

   /* Buffer for the DSP data quantity. (Data quantity: 128) */
	u32 COEF[128];		//15:3

	//+0x200
	u32 MADRS[64];		//15:0
	
	//+0x300
	u8 PAD0[0x100];

	//+0x400
   /* Buffer for DSP microprograms. (Data quantity: 128) */
	u32 MPRO[128*4];	//15:0
	
	//+0xC00
	u8 PAD1[0x400];

	//+0x1000
	struct 
	{ 
		u32 l;			//7:0
		u32 h;			//15:0 (23:8)
	} 
	TEMP[128];

	//+0x1400

   /* Input data buffer from wave memory (Data quantity: 32)
    * The actual write to MEMS [7:0] is executed at the
    * same time as the write to MEMS [23:16]. */
	struct 
	{ 
		u32 l;			//7:0
		u32 h;			//15:0 (23:8)
	} 
	MEMS[32];
	
	//+0x1500

   /* Buffer for sound data from input mixer. (Data quantity: 16) */
	struct 
	{ 
		u32 l;			//3:0
		u32 h;			//15:0 (19:4)
	} 
	MIXS[16];

	//+0x1580
   /* DSP output buffer. (Data quantity: 16 */
	u32 EFREG[16];		//15:0
	
	//+0x15C0
   /* Data buffer for digital audio input. (Data quantity: 4) */
	u32 EXTS[2];		//15:0
};
union InterruptInfo
{
	struct
	{
		//Bit 0 (R): Requests interrupt to external interrupt input pin "INTON". (SCSI) 
		u32 INTON:1;
		//Bit 1 (R): Reserved. 
		u32 res_1:1;
		//Bit 2 (R): Reserved. 
		u32 res_3:1;
		//Bit 3 (R): MIDI input interrupt. 
		//(Interrupt request generated when input FIFO has fetched valid data. Hence, if the CPU reads FIFO data, it must read the lot once and leave the FIFO empty. When the FIFO has changed to empty status, the interrupt request is canceled automatically.) 
		u32 MIDI_IN:1;
		//Bit 4 (R): DMA end interrupt 
		u32 DMA_END:1;
		//Bit 5 (R/W): SCPU interrupt caused by data being written to the CPU, so only "1" can be written. (Writing "0" has no effect.) This flag can be set from either the MCPU or the SCPU. 
		u32 SCPU:1;
		//Bit 6 (R): Timer A interrupt 
		u32 TimerA:1;
		//Bit 7 (R): Timer B interrupt 
		u32 TimerB:1;
		//Bit 8 (R): Timer C interrupt 
		u32 TimerC:1;
		//Bit 9 (R): MIDI output interrupt. 
		//(If the output FIFO changes to empty status, an interrupt request is generated.)
		//(If the status is no longer empty because data is written to the output FIFO, the interrupt request is canceled automatically.) 
		u32 MIDI_OUT:1;
		//Bit 10 (R): Interrupt of one sample interval 
		u32 SAMPLE_DONE:1;
	};
	u32 full;
};
extern InterruptInfo* MCIEB;
extern InterruptInfo* MCIPD;
extern InterruptInfo* MCIRE;
extern InterruptInfo* SCIEB;
extern InterruptInfo* SCIPD;
extern InterruptInfo* SCIRE;

extern CommonData_struct* CommonData;
extern DSPData_struct*	  DSPData;

void UpdateAICA(u32 Cycles);

void AICA_Init();
void AICA_Term();

//u32 ReadAicaReg(u32 reg);
void WriteAicaReg8(u32 reg,u32 data);

template<u32 sz>
void WriteAicaReg(u32 reg,u32 data);
