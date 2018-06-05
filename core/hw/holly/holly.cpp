#include "types.h"
#include "holly.h"

#include "hw/sh4/sh4_mem.h"
#include "hw/sh4/sh4_interrupts.h"
#include "hw/pvr/pvr.h"
#include "hw/pvr/pvr_sb_regs.h"
#include "hw/pvr/pvr_mem.h"
#include "hw/gdrom/gdrom_if.h"
#include "hw/maple/maple_if.h"
#include "hw/aica/aica.h"

#include "hw/flashrom/flashrom.h"
#include "reios/reios.h"
#include "hw/naomi/naomi.h"

/*
	ASIC Interrupt controller
	part of the holly block on dc
*/

//asic_RLXXPending: Update the intc flags for pending interrupts
static INLINE void asic_RL6Pending(void)
{
	bool t1=(SB_ISTNRM & SB_IML6NRM)!=0;
	bool t2=(SB_ISTERR & SB_IML6ERR)!=0;
	bool t3=(SB_ISTEXT & SB_IML6EXT)!=0;

	InterruptPend(sh4_IRL_9,t1|t2|t3);
}

static INLINE void asic_RL4Pending(void)
{
	bool t1=(SB_ISTNRM & SB_IML4NRM)!=0;
	bool t2=(SB_ISTERR & SB_IML4ERR)!=0;
	bool t3=(SB_ISTEXT & SB_IML4EXT)!=0;

	InterruptPend(sh4_IRL_11,t1|t2|t3);
}

static INLINE void asic_RL2Pending(void)
{
	bool t1=(SB_ISTNRM & SB_IML2NRM)!=0;
	bool t2=(SB_ISTERR & SB_IML2ERR)!=0;
	bool t3=(SB_ISTEXT & SB_IML2EXT)!=0;

	InterruptPend(sh4_IRL_13,t1|t2|t3);
}

static void asic_RaiseInterruptInternal(HollyInterruptID inter)
{
   u8 m=inter>>8;

   switch(m)
   {
      case 0:
         if (inter==holly_SCANINT2)
            maple_vblank();

         SB_ISTNRM |= 1 << (u8)inter;
         break;
      case 1:
         SB_ISTEXT |= 1 << (u8)inter;
         break;
      case 2:
         SB_ISTERR |= 1 << (u8)inter;
         break;
   }

   if ((m >= 0) && (m <= 2))
   {
      asic_RL2Pending();
      asic_RL4Pending();
      asic_RL6Pending();
   }
}

static HollyInterruptID dmatmp1;
static HollyInterruptID dmatmp2;
static HollyInterruptID OldDmaId;

void asic_RaiseInterrupt(HollyInterruptID inter)
{
   asic_RaiseInterruptInternal(inter);
}

void asic_RaiseInterruptWait(HollyInterruptID inter)
{
#if 0
   /* IRQ wait slots are here. This is a hack.
    * Up until now, more than 3 and less than 2 wait slots
    * break stuff.
    *
    * Currently using 2 wait slots. */
   OldDmaId = dmatmp2;
   dmatmp2  = dmatmp1;
   dmatmp1  = inter;

   switch (OldDmaId)
   {
      case holly_CH2_DMA:
      case holly_EXT_DMA1:
      case holly_GDROM_CMD:
      case holly_MAPLE_DMA:
      case holly_YUV_DMA:
      case holly_PVR_DMA:
      case holly_PVR_SortDMA:
      case holly_SPU_DMA:
         asic_RaiseInterruptInternal(OldDmaId);
         break;
   }
#else
   asic_RaiseInterruptInternal(inter);
#endif
}

static u32 Read_SB_ISTNRM(u32 addr)
{
   /* Note that the two highest bits indicate
    * the OR'ed result of all the bits in
    * SB_ISTEXT and SB_ISTERR, respectively,
    * and writes to these two bits are ignored. */
	u32 v = SB_ISTNRM & 0x3FFFFFFF;

	if (SB_ISTEXT)
		v |= 0x40000000;

	if (SB_ISTERR)
		v |= 0x80000000;

	return v;
}

static INLINE void holly_update_interrupts(void)
{
   asic_RL2Pending();
   asic_RL4Pending();
   asic_RL6Pending();
}

static void Write_SB_ISTNRM(u32 addr, u32 value)
{
   /* writing a 1 clears the interrupt */
	SB_ISTNRM &= ~value;

   holly_update_interrupts();
}

void asic_CancelInterrupt(HollyInterruptID inter)
{
	SB_ISTEXT&=~(1<<(u8)inter);
   holly_update_interrupts();
}

static void Write_SB_ISTEXT(u32 addr, u32 data)
{
	//nothing happens -- asic_CancelInterrupt is used instead
}

static void Write_SB_ISTERR(u32 addr, u32 data)
{
	SB_ISTERR &= ~data;

   holly_update_interrupts();
}

static void Write_SB_IML2NRM(u32 addr, u32 data)
{
	SB_IML2NRM=data;

	asic_RL2Pending();
}

static void Write_SB_IML4NRM(u32 addr, u32 data)
{
	SB_IML4NRM=data;

	asic_RL4Pending();
}

static void Write_SB_IML6NRM(u32 addr, u32 data)
{
	SB_IML6NRM=data;

	asic_RL6Pending();
}


static void Write_SB_IML6EXT(u32 addr, u32 data)
{
	SB_IML6EXT=data;

	asic_RL6Pending();
}

static void Write_SB_IML4EXT(u32 addr, u32 data)
{
	SB_IML4EXT=data;

	asic_RL4Pending();
}

static void Write_SB_IML2EXT(u32 addr, u32 data)
{
	SB_IML2EXT=data;

	asic_RL2Pending();
}

static void Write_SB_IML6ERR(u32 addr, u32 data)
{
	SB_IML6ERR=data;

	asic_RL6Pending();
}

static void Write_SB_IML4ERR(u32 addr, u32 data)
{
	SB_IML4ERR=data;

	asic_RL4Pending();
}

static void Write_SB_IML2ERR(u32 addr, u32 data)
{
	SB_IML2ERR=data;

	asic_RL2Pending();
}

static void asic_reg_Init(void)
{
	sb_rio_register(SB_ISTNRM_addr,RIO_FUNC,&Read_SB_ISTNRM,&Write_SB_ISTNRM);
	/*
	sb_regs[((SB_ISTNRM_addr-SB_BASE)>>2)].flags=REG_32BIT_READWRITE;
	sb_regs[((SB_ISTNRM_addr-SB_BASE)>>2)].readFunction=Read_SB_ISTNRM;
	sb_regs[((SB_ISTNRM_addr-SB_BASE)>>2)].writeFunction=Write_SB_ISTNRM;
	*/

	sb_rio_register(SB_ISTEXT_addr,RIO_WF,0,&Write_SB_ISTEXT);
	//sb_regs[((SB_ISTEXT_addr-SB_BASE)>>2)].writeFunction=Write_SB_ISTEXT;

	sb_rio_register(SB_ISTERR_addr,RIO_WF,0,&Write_SB_ISTERR);
//	sb_regs[((SB_ISTERR_addr-SB_BASE)>>2)].writeFunction=Write_SB_ISTERR;

	//NRM
	//6
	sb_rio_register(SB_IML6NRM_addr,RIO_WF,0,&Write_SB_IML6NRM);
	//sb_regs[((SB_IML6NRM_addr-SB_BASE)>>2)].writeFunction=Write_SB_SB_IML6NRM;
	//4
	sb_rio_register(SB_IML4NRM_addr,RIO_WF,0,&Write_SB_IML4NRM);
	//sb_regs[((SB_IML4NRM_addr-SB_BASE)>>2)].writeFunction=Write_SB_SB_IML4NRM;
	//2
	sb_rio_register(SB_IML2NRM_addr,RIO_WF,0,&Write_SB_IML2NRM);
	//sb_regs[((SB_IML2NRM_addr-SB_BASE)>>2)].writeFunction=Write_SB_SB_IML2NRM;
	//EXT
	//6
	sb_rio_register(SB_IML6EXT_addr,RIO_WF,0,&Write_SB_IML6EXT);
	//sb_regs[((SB_IML6EXT_addr-SB_BASE)>>2)].writeFunction=Write_SB_SB_IML6EXT;
	//4
	sb_rio_register(SB_IML4EXT_addr,RIO_WF,0,&Write_SB_IML4EXT);
	//sb_regs[((SB_IML4EXT_addr-SB_BASE)>>2)].writeFunction=Write_SB_SB_IML4EXT;
	//2
	sb_rio_register(SB_IML2EXT_addr,RIO_WF,0,&Write_SB_IML2EXT);
	//sb_regs[((SB_IML2EXT_addr-SB_BASE)>>2)].writeFunction=Write_SB_SB_IML2EXT;
	//ERR
	//6
	sb_rio_register(SB_IML6ERR_addr,RIO_WF,0,&Write_SB_IML6ERR);
	//sb_regs[((SB_IML6ERR_addr-SB_BASE)>>2)].writeFunction=Write_SB_SB_IML6ERR;
	//4
	sb_rio_register(SB_IML4ERR_addr,RIO_WF,0,&Write_SB_IML4ERR);
	//sb_regs[((SB_IML4ERR_addr-SB_BASE)>>2)].writeFunction=Write_SB_SB_IML4ERR;
	//2
	sb_rio_register(SB_IML2ERR_addr,RIO_WF,0,&Write_SB_IML2ERR);
	//sb_regs[((SB_IML2ERR_addr-SB_BASE)>>2)].writeFunction=Write_SB_SB_IML2ERR;
}

static void asic_reg_Term(void)
{

}

//Reset -> Reset - Initialise to default values
static void asic_reg_Reset(bool Manual)
{
}

/*
	System bus registers
	This doesn't implement any functionality, only routing
*/

Array<RegisterStruct> sb_regs(0x540, false);

//(addr>= 0x005F6800) && (addr<=0x005F7CFF) -> 0x1500 bytes -> 0x540 possible registers , 125 actually exist only
// System Control Reg.   //0x100 bytes
// Maple i/f Control Reg.//0x100 bytes
// GD-ROM                //0x100 bytes
// G1 i/f Control Reg.   //0x100 bytes
// G2 i/f Control Reg.   //0x100 bytes
// PVR i/f Control Reg.  //0x100 bytes
// much empty space

u32 SB_ISTNRM;

static u32 sb_ReadMem(u32 addr,u32 sz)
{
   u32 offset = addr-SB_BASE;

   offset>>=2;

   if ((sb_regs.data[offset].flags & REG_RF) )
      return sb_regs.data[offset].readFunctionAddr(addr);

   switch (sz)
   {
      case 4:
         return sb_regs.data[offset].data32;
      case 2:
         return sb_regs.data[offset].data16;
      default:
         break;
   }

   return sb_regs.data[offset].data8;

}

static void sb_WriteMem(u32 addr,u32 data,u32 sz)
{
   u32 offset = addr-SB_BASE;
   offset>>=2;
   if ((sb_regs.data[offset].flags & REG_WF) )
   {
      //printf("SBW: %08X\n",addr);
      sb_regs.data[offset].writeFunctionAddr(addr,data);
      return;
   }

   switch (sz)
   {
      case 4:
         sb_regs.data[offset].data32 = data;
         break;
      case 2:
         sb_regs.data[offset].data16 = (u16)data;
         break;
      default:
         sb_regs.data[offset].data8  = (u8)data;
         break;
   }
}

u32 sbio_read_noacc(u32 addr) { verify(false); return 0; }
void sbio_write_noacc(u32 addr, u32 data) { verify(false); }
void sbio_write_const(u32 addr, u32 data) { verify(false); }

void sb_write_zero(u32 addr, u32 data) { verify(data==0); }
void sb_write_gdrom_unlock(u32 addr, u32 data) { verify(data==0 || data==0x001fffff || data==0x42fe); } /* CS writes 0x42fe*/


void sb_rio_register(u32 reg_addr, RegIO flags, RegReadAddrFP* rf, RegWriteAddrFP* wf)
{
	u32 idx=(reg_addr-SB_BASE)/4;

	verify(idx<sb_regs.Size);

	sb_regs.data[idx].flags = flags | REG_ACCESS_32;

   switch (flags)
   {
      case RIO_NO_ACCESS:
         sb_regs.data[idx].readFunctionAddr=&sbio_read_noacc;
         sb_regs.data[idx].writeFunctionAddr=&sbio_write_noacc;
         break;
      case RIO_CONST:
         sb_regs.data[idx].writeFunctionAddr=&sbio_write_const;
         break;
      default:
         sb_regs.data[idx].data32=0;

         if (flags & REG_RF)
            sb_regs.data[idx].readFunctionAddr=rf;

         if (flags & REG_WF)
            sb_regs.data[idx].writeFunctionAddr=wf==0?&sbio_write_noacc:wf;
         break;
   }
}

template <u32 reg_addr>
static void sb_writeonly(u32 addr, u32 data)
{
	SB_REGN_32(reg_addr)=data;
}

u32 SB_FFST_rc;
u32 SB_FFST;

static u32 RegRead_SB_FFST(u32 addr)
{
	SB_FFST_rc++;
	if (SB_FFST_rc & 0x8)
		SB_FFST^=31;
	return 0; //SB_FFST -> does the fifo status has really to be faked ?
}

static void SB_SFRES_write32(u32 addr, u32 data)
{
#if 0
	if ((u16)data==0x7611)
	{
		printf("SB/HOLLY: System reset requested -- but cannot SOFT_RESET\n");
	}
#endif
}

static void sb_Init(void)
{
   memset(sb_regs.data, 0, sizeof(RegisterStruct) * sb_regs.Size);

	for (u32 i=0;i<sb_regs.Size;i++)
	{
		sb_rio_register(SB_BASE+i*4,RIO_NO_ACCESS);
		//sb_regs[i].flags=REG_NOT_IMPL;
	}

	//0x005F6800    SB_C2DSTAT  RW  ch2-DMA destination address
	sb_rio_register(SB_C2DSTAT_addr,RIO_DATA);


	//0x005F6804    SB_C2DLEN   RW  ch2-DMA length
	sb_rio_register(SB_C2DLEN_addr,RIO_DATA);


	//0x005F6808    SB_C2DST    RW  ch2-DMA start
	sb_rio_register(SB_C2DST_addr,RIO_DATA);

	//0x005F6810    SB_SDSTAW   RW  Sort-DMA start link table address
	sb_rio_register(SB_SDSTAW_addr,RIO_DATA);

	//0x005F6814    SB_SDBAAW   RW  Sort-DMA link base address
	sb_rio_register(SB_SDBAAW_addr,RIO_DATA);

	//0x005F6818    SB_SDWLT    RW  Sort-DMA link address bit width
	sb_rio_register(SB_SDWLT_addr,RIO_DATA);


	//0x005F681C    SB_SDLAS    RW  Sort-DMA link address shift control
	sb_rio_register(SB_SDLAS_addr,RIO_DATA);


	//0x005F6820    SB_SDST RW  Sort-DMA start
	sb_rio_register(SB_SDST_addr,RIO_DATA);

	//0x005F6860 SB_SDDIV R(?) Sort-DMA LAT index (guess)
	sb_rio_register(SB_SDDIV_addr,RIO_RO);
	//sb_regs[((SB_SDDIV_addr-SB_BASE))>>2].flags=REG_32BIT_READWRITE | REG_READ_DATA;

	//0x005F6840    SB_DBREQM   RW  DBREQ# signal mask control
	sb_rio_register(SB_DBREQM_addr,RIO_DATA);

	//0x005F6844    SB_BAVLWC   RW  BAVL# signal wait count
	sb_rio_register(SB_BAVLWC_addr,RIO_DATA);

	//0x005F6848    SB_C2DPRYC  RW  DMA (TA/Root Bus) priority count
	sb_rio_register(SB_C2DPRYC_addr,RIO_DATA);

	//0x005F684C    SB_C2DMAXL  RW  ch2-DMA maximum burst length
	sb_rio_register(SB_C2DMAXL_addr,RIO_DATA);

	//0x005F6880    SB_TFREM    R   TA FIFO remaining amount
	sb_rio_register(SB_TFREM_addr,RIO_RO);

	//0x005F6884    SB_LMMODE0  RW  Via TA texture memory bus select 0
	sb_rio_register(SB_LMMODE0_addr,RIO_DATA);

	//0x005F6888    SB_LMMODE1  RW  Via TA texture memory bus select 1
	sb_rio_register(SB_LMMODE1_addr,RIO_DATA);

	//0x005F688C    SB_FFST     R   FIFO status
	sb_rio_register(SB_FFST_addr,RIO_RO_FUNC,RegRead_SB_FFST);
	/*
	sb_regs[((SB_FFST_addr-SB_BASE))>>2].flags=REG_32BIT_READWRITE ;
	sb_regs[((SB_FFST_addr-SB_BASE))>>2].readFunction=RegRead_SB_FFST;
	sb_regs[((SB_FFST_addr-SB_BASE))>>2].writeFunction=0;
	sb_regs[((SB_FFST_addr-SB_BASE))>>2].data32=0;*/


	//0x005F6890    SB_SFRES    W   System reset
	sb_rio_register(SB_SFRES_addr,RIO_WO_FUNC,0,SB_SFRES_write32);

	/*
	sb_regs[((SB_SFRES_addr-SB_BASE))>>2].flags=REG_32BIT_READWRITE ;
	sb_regs[((SB_SFRES_addr-SB_BASE))>>2].readFunction=0;
	sb_regs[((SB_SFRES_addr-SB_BASE))>>2].writeFunction=SB_SFRES_write32;
	sb_regs[((SB_SFRES_addr-SB_BASE))>>2].data32=&SB_SFRES;
	*/


	//0x005F689C    SB_SBREV    R   System bus revision number
	sb_rio_register(SB_SBREV_addr,RIO_CONST);
	//sb_regs[((SB_SBREV_addr-SB_BASE))>>2].flags=REG_32BIT_READWRITE | REG_READ_DATA | REG_CONST ;


	//0x005F68A0    SB_RBSPLT   RW  SH4 Root Bus split enable
	sb_rio_register(SB_RBSPLT_addr,RIO_DATA);



	//0x005F6900    SB_ISTNRM   RW  Normal interrupt status
	sb_rio_register(SB_ISTNRM_addr,RIO_DATA);


	//0x005F6904    SB_ISTEXT   R   External interrupt status
	sb_rio_register(SB_ISTEXT_addr,RIO_RO);


	//0x005F6908    SB_ISTERR   RW  Error interrupt status
	sb_rio_register(SB_ISTERR_addr,RIO_DATA);



	//0x005F6910    SB_IML2NRM  RW  Level 2 normal interrupt mask
	sb_rio_register(SB_IML2NRM_addr,RIO_DATA);


	//0x005F6914    SB_IML2EXT  RW  Level 2 external interrupt mask
	sb_rio_register(SB_IML2EXT_addr,RIO_DATA);


	//0x005F6918    SB_IML2ERR  RW  Level 2 error interrupt mask
	sb_rio_register(SB_IML2ERR_addr,RIO_DATA);



	//0x005F6920    SB_IML4NRM  RW  Level 4 normal interrupt mask
	sb_rio_register(SB_IML4NRM_addr,RIO_DATA);


	//0x005F6924    SB_IML4EXT  RW  Level 4 external interrupt mask
	sb_rio_register(SB_IML4EXT_addr,RIO_DATA);


	//0x005F6928    SB_IML4ERR  RW  Level 4 error interrupt mask
	sb_rio_register(SB_IML4ERR_addr,RIO_DATA);



	//0x005F6930    SB_IML6NRM  RW  Level 6 normal interrupt mask
	sb_rio_register(SB_IML6NRM_addr,RIO_DATA);


	//0x005F6934    SB_IML6EXT  RW  Level 6 external interrupt mask
	sb_rio_register(SB_IML6EXT_addr,RIO_DATA);


	//0x005F6938    SB_IML6ERR  RW  Level 6 error interrupt mask
	sb_rio_register(SB_IML6ERR_addr,RIO_DATA);



	//0x005F6940    SB_PDTNRM   RW  Normal interrupt PVR-DMA startup mask
	sb_rio_register(SB_PDTNRM_addr,RIO_DATA);


	//0x005F6944    SB_PDTEXT   RW  External interrupt PVR-DMA startup mask
	sb_rio_register(SB_PDTEXT_addr,RIO_DATA);



	//0x005F6950    SB_G2DTNRM  RW  Normal interrupt G2-DMA startup mask
	sb_rio_register(SB_G2DTNRM_addr,RIO_DATA);


	//0x005F6954    SB_G2DTEXT  RW  External interrupt G2-DMA startup mask
	sb_rio_register(SB_G2DTEXT_addr,RIO_DATA);



	//0x005F6C04    SB_MDSTAR   RW  Maple-DMA command table address
	sb_rio_register(SB_MDSTAR_addr,RIO_DATA);



	//0x005F6C10    SB_MDTSEL   RW  Maple-DMA trigger select
	sb_rio_register(SB_MDTSEL_addr,RIO_DATA);


	//0x005F6C14    SB_MDEN     RW  Maple-DMA enable
	sb_rio_register(SB_MDEN_addr,RIO_DATA);


	//0x005F6C18    SB_MDST     RW  Maple-DMA start
	sb_rio_register(SB_MDST_addr,RIO_DATA);



	//0x005F6C80    SB_MSYS     RW  Maple system control
	sb_rio_register(SB_MSYS_addr,RIO_DATA);


	//0x005F6C84    SB_MST      R   Maple status
	sb_rio_register(SB_MST_addr,RIO_RO);


	//0x005F6C88    SB_MSHTCL   W   Maple-DMA hard trigger clear
	sb_rio_register(SB_MSHTCL_addr,RIO_WO_FUNC,0,sb_writeonly<SB_MSHTCL_addr>);


	//0x005F6C8C    SB_MDAPRO   W   Maple-DMA address range
	sb_rio_register(SB_MDAPRO_addr,RIO_WO_FUNC,0,sb_writeonly<SB_MDAPRO_addr>);



	//0x005F6CE8    SB_MMSEL    RW  Maple MSB selection
	sb_rio_register(SB_MMSEL_addr,RIO_DATA);



	//0x005F6CF4    SB_MTXDAD   R   Maple Txd address counter
	sb_rio_register(SB_MTXDAD_addr,RIO_RO);


	//0x005F6CF8    SB_MRXDAD   R   Maple Rxd address counter
	sb_rio_register(SB_MRXDAD_addr,RIO_RO);


	//0x005F6CFC    SB_MRXDBD   R   Maple Rxd base address
	sb_rio_register(SB_MRXDBD_addr,RIO_RO);

	//0x005F7404    SB_GDSTAR   RW  GD-DMA start address
	sb_rio_register(SB_GDSTAR_addr,RIO_DATA);


	//0x005F7408    SB_GDLEN    RW  GD-DMA length
	sb_rio_register(SB_GDLEN_addr,RIO_DATA);


	//0x005F740C    SB_GDDIR    RW  GD-DMA direction
	sb_rio_register(SB_GDDIR_addr,RIO_DATA);



	//0x005F7414    SB_GDEN     RW  GD-DMA enable
	sb_rio_register(SB_GDEN_addr,RIO_DATA);


	//0x005F7418    SB_GDST     RW  GD-DMA start
	sb_rio_register(SB_GDST_addr,RIO_DATA);



	//0x005F7480    SB_G1RRC    W   System ROM read access timing
	sb_rio_register(SB_G1RRC_addr,RIO_WO_FUNC,0,sb_writeonly<SB_G1RRC_addr>);


	//0x005F7484    SB_G1RWC    W   System ROM write access timing
	sb_rio_register(SB_G1RWC_addr,RIO_WO_FUNC,0,sb_writeonly<SB_G1RWC_addr>);


	//0x005F7488    SB_G1FRC    W   Flash ROM read access timing
	sb_rio_register(SB_G1FRC_addr,RIO_WO_FUNC,0,sb_writeonly<SB_G1FRC_addr>);


	//0x005F748C    SB_G1FWC    W   Flash ROM write access timing
	sb_rio_register(SB_G1FWC_addr,RIO_WO_FUNC,0,sb_writeonly<SB_G1FWC_addr>);


	//0x005F7490    SB_G1CRC    W   GD PIO read access timing
	sb_rio_register(SB_G1CRC_addr,RIO_WO_FUNC,0,sb_writeonly<SB_G1CRC_addr>);


	//0x005F7494    SB_G1CWC    W   GD PIO write access timing
	sb_rio_register(SB_G1CWC_addr,RIO_WO_FUNC,0,sb_writeonly<SB_G1CWC_addr>);



	//0x005F74A0    SB_G1GDRC   W   GD-DMA read access timing
	sb_rio_register(SB_G1GDRC_addr,RIO_WO_FUNC,0,sb_writeonly<SB_G1GDRC_addr>);


	//0x005F74A4    SB_G1GDWC   W   GD-DMA write access timing
	sb_rio_register(SB_G1GDWC_addr,RIO_WO_FUNC,0,sb_writeonly<SB_G1GDWC_addr>);



	//0x005F74B0    SB_G1SYSM   R   System mode
	sb_rio_register(SB_G1SYSM_addr,RIO_RO);


	//0x005F74B4    SB_G1CRDYC  W   G1IORDY signal control
	sb_rio_register(SB_G1CRDYC_addr,RIO_WO_FUNC,0,sb_writeonly<SB_G1CRDYC_addr>);


	//0x005F74B8    SB_GDAPRO   W   GD-DMA address range
	sb_rio_register(SB_GDAPRO_addr,RIO_WO_FUNC,0,sb_writeonly<SB_GDAPRO_addr>);



	//0x005F74F4    SB_GDSTARD  R   GD-DMA address count (on Root Bus)
	sb_rio_register(SB_GDSTARD_addr,RIO_RO);


	//0x005F74F8    SB_GDLEND   R   GD-DMA transfer counter
	sb_rio_register(SB_GDLEND_addr,RIO_RO);



	//0x005F7800    SB_ADSTAG   RW  AICA:G2-DMA G2 start address
	sb_rio_register(SB_ADSTAG_addr,RIO_DATA);


	//0x005F7804    SB_ADSTAR   RW  AICA:G2-DMA system memory start address
	sb_rio_register(SB_ADSTAR_addr,RIO_DATA);


	//0x005F7808    SB_ADLEN    RW  AICA:G2-DMA length
	sb_rio_register(SB_ADLEN_addr,RIO_DATA);


	//0x005F780C    SB_ADDIR    RW  AICA:G2-DMA direction
	sb_rio_register(SB_ADDIR_addr,RIO_DATA);


	//0x005F7810    SB_ADTSEL   RW  AICA:G2-DMA trigger select
	sb_rio_register(SB_ADTSEL_addr,RIO_DATA);


	//0x005F7814    SB_ADEN     RW  AICA:G2-DMA enable
	sb_rio_register(SB_ADEN_addr,RIO_DATA);



	//0x005F7818    SB_ADST     RW  AICA:G2-DMA start
	sb_rio_register(SB_ADST_addr,RIO_DATA);


	//0x005F781C    SB_ADSUSP   RW  AICA:G2-DMA suspend
	sb_rio_register(SB_ADSUSP_addr,RIO_DATA);



	//0x005F7820    SB_E1STAG   RW  Ext1:G2-DMA G2 start address
	sb_rio_register(SB_E1STAG_addr,RIO_DATA);


	//0x005F7824    SB_E1STAR   RW  Ext1:G2-DMA system memory start address
	sb_rio_register(SB_E1STAR_addr,RIO_DATA);


	//0x005F7828    SB_E1LEN    RW  Ext1:G2-DMA length
	sb_rio_register(SB_E1LEN_addr,RIO_DATA);


	//0x005F782C    SB_E1DIR    RW  Ext1:G2-DMA direction
	sb_rio_register(SB_E1DIR_addr,RIO_DATA);


	//0x005F7830    SB_E1TSEL   RW  Ext1:G2-DMA trigger select
	sb_rio_register(SB_E1TSEL_addr,RIO_DATA);


	//0x005F7834    SB_E1EN     RW  Ext1:G2-DMA enable
	sb_rio_register(SB_E1EN_addr,RIO_DATA);


	//0x005F7838    SB_E1ST     RW  Ext1:G2-DMA start
	sb_rio_register(SB_E1ST_addr,RIO_DATA);


	//0x005F783C    SB_E1SUSP   RW  Ext1: G2-DMA suspend
	sb_rio_register(SB_E1SUSP_addr,RIO_DATA);



	//0x005F7840    SB_E2STAG   RW  Ext2:G2-DMA G2 start address
	sb_rio_register(SB_E2STAG_addr,RIO_DATA);


	//0x005F7844    SB_E2STAR   RW  Ext2:G2-DMA system memory start address
	sb_rio_register(SB_E2STAR_addr,RIO_DATA);


	//0x005F7848    SB_E2LEN    RW  Ext2:G2-DMA length
	sb_rio_register(SB_E2LEN_addr,RIO_DATA);


	//0x005F784C    SB_E2DIR    RW  Ext2:G2-DMA direction
	sb_rio_register(SB_E2DIR_addr,RIO_DATA);


	//0x005F7850    SB_E2TSEL   RW  Ext2:G2-DMA trigger select
	sb_rio_register(SB_E2TSEL_addr,RIO_DATA);


	//0x005F7854    SB_E2EN     RW  Ext2:G2-DMA enable
	sb_rio_register(SB_E2EN_addr,RIO_DATA);


	//0x005F7858    SB_E2ST     RW  Ext2:G2-DMA start
	sb_rio_register(SB_E2ST_addr,RIO_DATA);


	//0x005F785C    SB_E2SUSP   RW  Ext2: G2-DMA suspend
	sb_rio_register(SB_E2SUSP_addr,RIO_DATA);



	//0x005F7860    SB_DDSTAG   RW  Dev:G2-DMA G2 start address
	sb_rio_register(SB_DDSTAG_addr,RIO_DATA);


	//0x005F7864    SB_DDSTAR   RW  Dev:G2-DMA system memory start address
	sb_rio_register(SB_DDSTAR_addr,RIO_DATA);


	//0x005F7868    SB_DDLEN    RW  Dev:G2-DMA length
	sb_rio_register(SB_DDLEN_addr,RIO_DATA);


	//0x005F786C    SB_DDDIR    RW  Dev:G2-DMA direction
	sb_rio_register(SB_DDDIR_addr,RIO_DATA);


	//0x005F7870    SB_DDTSEL   RW  Dev:G2-DMA trigger select
	sb_rio_register(SB_DDTSEL_addr,RIO_DATA);


	//0x005F7874    SB_DDEN     RW  Dev:G2-DMA enable
	sb_rio_register(SB_DDEN_addr,RIO_DATA);


	//0x005F7878    SB_DDST     RW  Dev:G2-DMA start
	sb_rio_register(SB_DDST_addr,RIO_DATA);


	//0x005F787C    SB_DDSUSP   RW  Dev: G2-DMA suspend
	sb_rio_register(SB_DDSUSP_addr,RIO_DATA);



	//0x005F7880    SB_G2ID     R   G2 bus version
	sb_rio_register(SB_G2ID_addr,RIO_CONST);



	//0x005F7890    SB_G2DSTO   RW  G2/DS timeout
	sb_rio_register(SB_G2DSTO_addr,RIO_DATA);


	//0x005F7894    SB_G2TRTO   RW  G2/TR timeout
	sb_rio_register(SB_G2TRTO_addr,RIO_DATA);


	//0x005F7898    SB_G2MDMTO  RW  Modem unit wait timeout
	sb_rio_register(SB_G2MDMTO_addr,RIO_DATA);


	//0x005F789C    SB_G2MDMW   RW  Modem unit wait time
	sb_rio_register(SB_G2MDMW_addr,RIO_DATA);



	//0x005F78BC    SB_G2APRO   W   G2-DMA address range
	sb_rio_register(SB_G2APRO_addr,RIO_WO_FUNC,0,sb_writeonly<SB_G2APRO_addr>);



	//0x005F78C0    SB_ADSTAGD  R   AICA-DMA address counter (on AICA)
	sb_rio_register(SB_ADSTAGD_addr,RIO_RO);


	//0x005F78C4    SB_ADSTARD  R   AICA-DMA address counter (on root bus)
	sb_rio_register(SB_ADSTARD_addr,RIO_RO);


	//0x005F78C8    SB_ADLEND   R   AICA-DMA transfer counter
	sb_rio_register(SB_ADLEND_addr,RIO_RO);



	//0x005F78D0    SB_E1STAGD  R   Ext-DMA1 address counter (on Ext)
	sb_rio_register(SB_E1STAGD_addr,RIO_RO);


	//0x005F78D4    SB_E1STARD  R   Ext-DMA1 address counter (on root bus)
	sb_rio_register(SB_E1STARD_addr,RIO_RO);


	//0x005F78D8    SB_E1LEND   R   Ext-DMA1 transfer counter
	sb_rio_register(SB_E1LEND_addr,RIO_RO);



	//0x005F78E0    SB_E2STAGD  R   Ext-DMA2 address counter (on Ext)
	sb_rio_register(SB_E2STAGD_addr,RIO_RO);


	//0x005F78E4    SB_E2STARD  R   Ext-DMA2 address counter (on root bus)
	sb_rio_register(SB_E2STARD_addr,RIO_RO);


	//0x005F78E8    SB_E2LEND   R   Ext-DMA2 transfer counter
	sb_rio_register(SB_E2LEND_addr,RIO_RO);



	//0x005F78F0    SB_DDSTAGD  R   Dev-DMA address counter (on Ext)
	sb_rio_register(SB_DDSTAGD_addr,RIO_RO);


	//0x005F78F4    SB_DDSTARD  R   Dev-DMA address counter (on root bus)
	sb_rio_register(SB_DDSTARD_addr,RIO_RO);


	//0x005F78F8    SB_DDLEND   R   Dev-DMA transfer counter
	sb_rio_register(SB_DDLEND_addr,RIO_RO);



	//0x005F7C00    SB_PDSTAP   RW  PVR-DMA PVR start address
	sb_rio_register(SB_PDSTAP_addr,RIO_DATA);


	//0x005F7C04    SB_PDSTAR   RW  PVR-DMA system memory start address
	sb_rio_register(SB_PDSTAR_addr,RIO_DATA);


	//0x005F7C08    SB_PDLEN    RW  PVR-DMA length
	sb_rio_register(SB_PDLEN_addr,RIO_DATA);


	//0x005F7C0C    SB_PDDIR    RW  PVR-DMA direction
	sb_rio_register(SB_PDDIR_addr,RIO_DATA);


	//0x005F7C10    SB_PDTSEL   RW  PVR-DMA trigger select
	sb_rio_register(SB_PDTSEL_addr,RIO_DATA);


	//0x005F7C14    SB_PDEN     RW  PVR-DMA enable
	sb_rio_register(SB_PDEN_addr,RIO_DATA);


	//0x005F7C18    SB_PDST     RW  PVR-DMA start
	sb_rio_register(SB_PDST_addr,RIO_DATA);



	//0x005F7C80    SB_PDAPRO   W   PVR-DMA address range
	sb_rio_register(SB_PDAPRO_addr,RIO_WO_FUNC,0,sb_writeonly<SB_PDAPRO_addr>);



	//0x005F7CF0    SB_PDSTAPD  R   PVR-DMA address counter (on Ext)
	sb_rio_register(SB_PDSTAPD_addr,RIO_RO);


	//0x005F7CF4    SB_PDSTARD  R   PVR-DMA address counter (on root bus)
	sb_rio_register(SB_PDSTARD_addr,RIO_RO);


	//0x005F7CF8    SB_PDLEND   R   PVR-DMA transfer counter
	sb_rio_register(SB_PDLEND_addr,RIO_RO);


	//GDROM unlock register (bios checksumming, etc)
	//0x005f74e4
	sb_rio_register(0x005f74e4,RIO_WO_FUNC,0,sb_write_gdrom_unlock);

	//0x005f68a4, 0x005f68ac, 0x005f78a0,0x005f78a4, 0x005f78a8, 0x005f78b0, 0x005f78b4, 0x005f78b8
	sb_rio_register(0x005f68a4,RIO_WO_FUNC,0,sb_write_zero);
	sb_rio_register(0x005f68ac,RIO_WO_FUNC,0,sb_write_zero);
	sb_rio_register(0x005f78a0,RIO_WO_FUNC,0,sb_write_zero);
	sb_rio_register(0x005f78a4,RIO_WO_FUNC,0,sb_write_zero);
	sb_rio_register(0x005f78a8,RIO_WO_FUNC,0,sb_write_zero);
	sb_rio_register(0x005f78ac,RIO_WO_FUNC,0,sb_write_zero);
	sb_rio_register(0x005f78b0,RIO_WO_FUNC,0,sb_write_zero);
	sb_rio_register(0x005f78b4,RIO_WO_FUNC,0,sb_write_zero);
	sb_rio_register(0x005f78b8,RIO_WO_FUNC,0,sb_write_zero);


	SB_SBREV=0xB;
	SB_G2ID=0x12;
	SB_G1SYSM=((0x0<<4) | (0x1));

	asic_reg_Init();

#if DC_PLATFORM!=DC_PLATFORM_NAOMI
	gdrom_reg_Init();
#else
	naomi_reg_Init();
#endif

	pvr_sb_Init();
	maple_Init();
	aica_sb_Init();
}

static void sb_Reset(bool Manual)
{
	asic_reg_Reset(Manual);
#if DC_PLATFORM!=DC_PLATFORM_NAOMI
	gdrom_reg_Reset(Manual);
#else
	naomi_reg_Reset(Manual);
#endif
	pvr_sb_Reset(Manual);
	maple_Reset(Manual);
	aica_sb_Reset(Manual);
}

static void sb_Term(void)
{
	aica_sb_Term();
	maple_Term();
	pvr_sb_Term();
#if DC_PLATFORM!=DC_PLATFORM_NAOMI
	gdrom_reg_Term();
#else
	naomi_reg_Term();
#endif
	asic_reg_Term();
}

/*
	Dreamcast 'area 0' emulation
	Pretty much all peripheral registers are mapped here

	Routing is mostly handled here, as well as flash/SRAM emulation
*/

RomChip sys_rom(BIOS_SIZE);

#ifdef FLASH_SIZE
DCFlashChip sys_nvmem(FLASH_SIZE);
#endif

#ifdef BBSRAM_SIZE
SRamChip sys_nvmem(BBSRAM_SIZE);
#endif

bool LoadRomFiles(const string& root)
{
	if (!sys_rom.Load(root, ROM_PREFIX, "%boot.bin;%boot.bin.bin;%bios.bin;%bios.bin.bin" ROM_NAMES, "bootrom"))
	{
		msgboxf("Unable to find bios in \n%s\nExiting...", MBX_ICONERROR, root.c_str());
		return false;
	}
	if (!sys_nvmem.Load(root, ROM_PREFIX, "%nvmem.bin;%flash_wb.bin;%flash.bin;%flash.bin.bin", "nvram"))
	{
		if (NVR_OPTIONAL)
		{
			printf("flash/nvmem is missing, will create new file...");
		}
		else
		{
			msgboxf("Unable to find flash/nvmem in \n%s\nExiting...", MBX_ICONERROR, root.c_str());
			return false;
		}
	}

	return true;
}

void SaveRomFiles(const string& root)
{
	sys_nvmem.Save(root, ROM_PREFIX, "nvmem.bin", "nvmem");
}

bool LoadHle(const string& root) {
	if (!sys_nvmem.Load(root, ROM_PREFIX, "%nvmem.bin;%flash_wb.bin;%flash.bin;%flash.bin.bin", "nvram")) {
		printf("No nvmem loaded\n");
	}

	return reios_init(sys_rom.data, sys_nvmem.data);
}

#if (DC_PLATFORM == DC_PLATFORM_DREAMCAST) || (DC_PLATFORM == DC_PLATFORM_DEV_UNIT) || (DC_PLATFORM == DC_PLATFORM_NAOMI) || (DC_PLATFORM == DC_PLATFORM_NAOMI2)

u32 ReadBios(u32 addr,u32 sz) { return sys_rom.Read(addr,sz); }
void WriteBios(u32 addr,u32 data,u32 sz) { EMUERROR4("Write to [Boot ROM] is not possible, addr=%x,data=%x,size=%d",addr,data,sz); }

u32 ReadFlash(u32 addr,u32 sz) { return sys_nvmem.Read(addr,sz); }
void WriteFlash(u32 addr,u32 data,u32 sz) { sys_nvmem.Write(addr,data,sz); }

#elif (DC_PLATFORM == DC_PLATFORM_ATOMISWAVE)
	u32 ReadFlash(u32 addr,u32 sz) { EMUERROR3("Read from [Flash ROM] is not possible, addr=%x,size=%d",addr,sz); return 0; }
	void WriteFlash(u32 addr,u32 data,u32 sz) { EMUERROR4("Write to [Flash ROM] is not possible, addr=%x,data=%x,size=%d",addr,data,sz); }

	u32 ReadBios(u32 addr,u32 sz)
	{
		if (!(addr&0x10000)) //upper 64 kb is flashrom
		{
			return sys_rom.Read(addr,sz);
		}
		else
		{
			return sys_nvmem.Read(addr,sz);
		}
	}

	void WriteBios(u32 addr,u32 data,u32 sz)
	{
		if (!(addr&0x10000)) //upper 64 kb is flashrom
		{
			EMUERROR4("Write to  [Boot ROM] is not possible, addr=%x,data=%x,size=%d",addr,data,sz);
		}
		else
		{
			sys_nvmem.Write(addr,data,sz);
		}
	}

#else
#error unknown flash
#endif

//Area 0 mem map
//0x00000000- 0x001FFFFF	:MPX	System/Boot ROM
//0x00200000- 0x0021FFFF	:Flash Memory
//0x00400000- 0x005F67FF	:Unassigned
//0x005F6800- 0x005F69FF	:System Control Reg.
//0x005F6C00- 0x005F6CFF	:Maple i/f Control Reg.
//0x005F7000- 0x005F70FF	:GD-ROM / NAOMI BD Reg.
//0x005F7400- 0x005F74FF	:G1 i/f Control Reg.
//0x005F7800- 0x005F78FF	:G2 i/f Control Reg.
//0x005F7C00- 0x005F7CFF	:PVR i/f Control Reg.
//0x005F8000- 0x005F9FFF	:TA / PVR Core Reg.
//0x00600000- 0x006007FF	:MODEM
//0x00600800- 0x006FFFFF	:G2 (Reserved)
//0x00700000- 0x00707FFF	:AICA- Sound Cntr. Reg.
//0x00710000- 0x0071000B	:AICA- RTC Cntr. Reg.
//0x00800000- 0x00FFFFFF	:AICA- Wave Memory
//0x01000000- 0x01FFFFFF	:Ext. Device
//0x02000000- 0x03FFFFFF*	:Image Area*	2MB

//use unified size handler for registers
//it really makes no sense to use different size handlers on em -> especially when we can use templates :p
template<u32 sz, class T>
T DYNACALL ReadMem_area0(u32 addr)
{
	addr &= 0x01FFFFFF;//to get rid of non needed bits
	const u32 base=(addr>>16);
	//map 0x0000 to 0x01FF to Default handler
	//mirror 0x0200 to 0x03FF , from 0x0000 to 0x03FFF
	//map 0x0000 to 0x001F
	if (base<=0x001F)//	:MPX	System/Boot ROM
	{
		return ReadBios(addr,sz);
	}
	//map 0x0020 to 0x0021
	else if ((base>= 0x0020) && (base<= 0x0021)) // :Flash Memory
	{
		return ReadFlash(addr&0x1FFFF,sz);
	}
	//map 0x005F to 0x005F
	else if (likely(base==0x005F))
	{
		if ( /*&& (addr>= 0x00400000)*/ (addr<= 0x005F67FF)) // :Unassigned
		{
			EMUERROR2("Read from area0_32 not implemented [Unassigned], addr=%x",addr);
		}
		else if ((addr>= 0x005F7000) && (addr<= 0x005F70FF)) // GD-ROM
		{
			//EMUERROR3("Read from area0_32 not implemented [GD-ROM], addr=%x,size=%d",addr,sz);
	#if DC_PLATFORM == DC_PLATFORM_NAOMI
			return (T)ReadMem_naomi(addr,sz);
	#else
			return (T)ReadMem_gdrom(addr,sz);
	#endif
		}
		else if (likely((addr>= 0x005F6800) && (addr<=0x005F7CFF))) //	/*:PVR i/f Control Reg.*/ -> ALL SB registers now
		{
			//EMUERROR2("Read from area0_32 not implemented [PVR i/f Control Reg], addr=%x",addr);
			return (T)sb_ReadMem(addr,sz);
		}
		else if (likely((addr>= 0x005F8000) && (addr<=0x005F9FFF))) //	:TA / PVR Core Reg.
		{
			//EMUERROR2("Read from area0_32 not implemented [TA / PVR Core Reg], addr=%x",addr);
			verify(sz==4);
			return (T)PvrReg(addr, u32);
		}
	}
	//map 0x0060 to 0x0060
	else if ((base ==0x0060) /*&& (addr>= 0x00600000)*/ && (addr<= 0x006007FF)) //	:MODEM
	{
		return (T)libExtDevice_ReadMem_A0_006(addr,sz);
		//EMUERROR2("Read from area0_32 not implemented [MODEM], addr=%x",addr);
	}
	//map 0x0060 to 0x006F
	else if ((base >=0x0060) && (base <=0x006F) && (addr>= 0x00600800) && (addr<= 0x006FFFFF)) //	:G2 (Reserved)
	{
		EMUERROR2("Read from area0_32 not implemented [G2 (Reserved)], addr=%x",addr);
	}
	//map 0x0070 to 0x0070
	else if ((base ==0x0070) /*&& (addr>= 0x00700000)*/ && (addr<=0x00707FFF)) //	:AICA- Sound Cntr. Reg.
	{
		//EMUERROR2("Read from area0_32 not implemented [AICA- Sound Cntr. Reg], addr=%x",addr);
		return (T) ReadMem_aica_reg(addr,sz);//libAICA_ReadReg(addr,sz);
	}
	//map 0x0071 to 0x0071
	else if ((base ==0x0071) /*&& (addr>= 0x00710000)*/ && (addr<= 0x0071000B)) //	:AICA- RTC Cntr. Reg.
	{
		//EMUERROR2("Read from area0_32 not implemented [AICA- RTC Cntr. Reg], addr=%x",addr);
		return (T)aica_rtc_reg_read(addr,sz);
	}
	//map 0x0080 to 0x00FF
	else if ((base >=0x0080) && (base <=0x00FF) /*&& (addr>= 0x00800000) && (addr<=0x00FFFFFF)*/) //	:AICA- Wave Memory
	{
		//EMUERROR2("Read from area0_32 not implemented [AICA- Wave Memory], addr=%x",addr);
		//return (T)libAICA_ReadMem_aica_ram(addr,sz);
      switch (sz)
      {
         case 1:
            return aica_ram.data[addr & ARAM_MASK];
         case 2:
            return *(u16*)&aica_ram.data[addr & ARAM_MASK];
         case 4:
            return *(u32*)&aica_ram.data[addr & ARAM_MASK];
      }
	}
	//map 0x0100 to 0x01FF
	else if ((base >=0x0100) && (base <=0x01FF) /*&& (addr>= 0x01000000) && (addr<= 0x01FFFFFF)*/) //	:Ext. Device
	{
	//	EMUERROR2("Read from area0_32 not implemented [Ext. Device], addr=%x",addr);
		return (T)libExtDevice_ReadMem_A0_010(addr,sz);
	}
	return 0;
}

template<u32 sz, class T>
void  DYNACALL WriteMem_area0(u32 addr,T data)
{
	addr &= 0x01FFFFFF;//to get rid of non needed bits

	const u32 base=(addr>>16);

	//map 0x0000 to 0x001F
	if ((base <=0x001F) /*&& (addr<=0x001FFFFF)*/)// :MPX System/Boot ROM
	{
		//EMUERROR4("Write to  [MPX	System/Boot ROM] is not possible, addr=%x,data=%x,size=%d",addr,data,sz);
		WriteBios(addr,data,sz);
	}
	//map 0x0020 to 0x0021
	else if ((base >=0x0020) && (base <=0x0021) /*&& (addr>= 0x00200000) && (addr<= 0x0021FFFF)*/) // Flash Memory
	{
		//EMUERROR4("Write to [Flash Memory] , sz?!, addr=%x,data=%x,size=%d",addr,data,sz);
		WriteFlash(addr,data,sz);
	}
	//map 0x0040 to 0x005F -> actually, I'll only map 0x005F to 0x005F, b/c the rest of it is unspammed (left to default handler)
	//map 0x005F to 0x005F
	else if ( likely(base==0x005F) )
	{
		if (/*&& (addr>= 0x00400000) */ (addr<= 0x005F67FF)) // Unassigned
		{
			EMUERROR4("Write to area0_32 not implemented [Unassigned], addr=%x,data=%x,size=%d",addr,data,sz);
		}
		else if ((addr>= 0x005F7000) && (addr<= 0x005F70FF)) // GD-ROM
		{
			//EMUERROR4("Write to area0_32 not implemented [GD-ROM], addr=%x,data=%x,size=%d",addr,data,sz);
#if DC_PLATFORM == DC_PLATFORM_NAOMI || DC_PLATFORM == DC_PLATFORM_ATOMISWAVE
			WriteMem_naomi(addr,data,sz);
#else
			WriteMem_gdrom(addr,data,sz);
#endif
		}
		else if ( likely((addr>= 0x005F6800) && (addr<=0x005F7CFF)) ) // /*:PVR i/f Control Reg.*/ -> ALL SB registers
		{
			//EMUERROR4("Write to area0_32 not implemented [PVR i/f Control Reg], addr=%x,data=%x,size=%d",addr,data,sz);
			sb_WriteMem(addr,data,sz);
		}
		else if ( likely((addr>= 0x005F8000) && (addr<=0x005F9FFF)) ) // TA / PVR Core Reg.
		{
			//EMUERROR4("Write to area0_32 not implemented [TA / PVR Core Reg], addr=%x,data=%x,size=%d",addr,data,sz);
			verify(sz==4);
			pvr_WriteReg(addr,data);
		}
	}
	//map 0x0060 to 0x0060
	else if ((base ==0x0060) /*&& (addr>= 0x00600000)*/ && (addr<= 0x006007FF)) // MODEM
	{
		//EMUERROR4("Write to area0_32 not implemented [MODEM], addr=%x,data=%x,size=%d",addr,data,sz);
		libExtDevice_WriteMem_A0_006(addr,data,sz);
	}
	//map 0x0060 to 0x006F
	else if ((base >=0x0060) && (base <=0x006F) && (addr>= 0x00600800) && (addr<= 0x006FFFFF)) // G2 (Reserved)
	{
		EMUERROR4("Write to area0_32 not implemented [G2 (Reserved)], addr=%x,data=%x,size=%d",addr,data,sz);
	}
	//map 0x0070 to 0x0070
	else if ((base >=0x0070) && (base <=0x0070) /*&& (addr>= 0x00700000)*/ && (addr<=0x00707FFF)) // AICA- Sound Cntr. Reg.
	{
		//EMUERROR4("Write to area0_32 not implemented [AICA- Sound Cntr. Reg], addr=%x,data=%x,size=%d",addr,data,sz);
		WriteMem_aica_reg(addr,data,sz);
		return;
	}
	//map 0x0071 to 0x0071
	else if ((base >=0x0071) && (base <=0x0071) /*&& (addr>= 0x00710000)*/ && (addr<= 0x0071000B)) // AICA- RTC Cntr. Reg.
	{
		//EMUERROR4("Write to area0_32 not implemented [AICA- RTC Cntr. Reg], addr=%x,data=%x,size=%d",addr,data,sz);
		aica_rtc_reg_write(addr,data,sz);
		return;
	}
	//map 0x0080 to 0x00FF
	else if ((base >=0x0080) && (base <=0x00FF) /*&& (addr>= 0x00800000) && (addr<=0x00FFFFFF)*/) // AICA- Wave Memory
	{
		//EMUERROR4("Write to area0_32 not implemented [AICA- Wave Memory], addr=%x,data=%x,size=%d",addr,data,sz);
		//aica_writeram(addr,data,sz);

      switch (sz)
      {
         case 1:
            aica_ram.data[addr & ARAM_MASK]         = (u8)data;
            break;
         case 2:
            *(u16*)&aica_ram.data[addr & ARAM_MASK] = (u16)data;
            break;
         case 4:
            *(u32*)&aica_ram.data[addr & ARAM_MASK] = data;
            break;
      }
		return;
	}
	//map 0x0100 to 0x01FF
	else if ((base >=0x0100) && (base <=0x01FF) /*&& (addr>= 0x01000000) && (addr<= 0x01FFFFFF)*/) // Ext. Device
	{
		//EMUERROR4("Write to area0_32 not implemented [Ext. Device], addr=%x,data=%x,size=%d",addr,data,sz);
		libExtDevice_WriteMem_A0_010(addr,data,sz);
	}
	return;
}

//Init/Res/Term
void sh4_area0_Init(void)
{
	sb_Init();
}

void sh4_area0_Reset(bool Manual)
{
	sb_Reset(Manual);
}

void sh4_area0_Term(void)
{
	sb_Term();
}


//AREA 0
_vmem_handler area0_handler;


void map_area0_init()
{

	area0_handler = _vmem_register_handler_Template(ReadMem_area0,WriteMem_area0);
}
void map_area0(u32 base)
{
	verify(base<0xE0);

	_vmem_map_handler(area0_handler,0x00|base,0x01|base);

	//0x0240 to 0x03FF mirrors 0x0040 to 0x01FF (no flashrom or bios)
	//0x0200 to 0x023F are unused
	_vmem_mirror_mapping(0x02|base,0x00|base,0x02);
}
