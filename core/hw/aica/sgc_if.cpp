#include "sgc_if.h"
#include "dsp.h"
#include "aica_mem.h"
#include "aica_if.h"

#include <math.h>

#undef FAR

//#define CLIP_WARN
#define key_printf(...)
#define aeg_printf(...)
#define step_printf(...)

#ifdef CLIP_WARN
#define clip_verify(x) verify(x)
#else
#define clip_verify(x)
#endif

#define MAX_CHANNELS                    64

#define CH_REC_FORMAT_KEY_LOOP          0x01
#define CH_REC_FNS                      0x18
#define CH_REC_FNS_OCT                  0x19
#define CH_REC_AR_D1R                   0x10
#define CH_REC_D1R_D2R                  0x11
#define CH_REC_RR_DL                    0x14
#define CH_REC_DL_KRS_LS                0x15
#define CH_REC_ALFOS_ALFOWS_PLFOS       0x1c
#define CH_REC_PLFOWS_LFOF_RE           0x1d

#define CH_FORMAT_PCM16                 0
#define CH_FORMAT_PCM8                  1
#define CH_FORMAT_ADPCM1                2
#define CH_FORMAT_ADPCM2                3

#define CDDA_SIZE  (2352/2)

#define clip(x,min,max) if ((x)<(min)) (x)=(min); if ((x)>(max)) (x)=(max);
#define ICLIP16(x) clip(x,-32768,32767)

#define EG_SHIFT 16
#define SHIFT 12

#define FIX(v) ((uint32_t) ((float) (1<<SHIFT)*(v)))

//Fixed point mul w/ rounding :)
#define FPMul(a, b, bits) ((a * b) >> bits)

#define VOLPAN(value,vol,pan,outl,outr) \
{\
   s32 temp=FPMul((value),volume_lut[(vol)],15);\
   u32 t_pan=(pan);\
   SampleType Sc=FPMul(temp,volume_lut[0xF-(t_pan&0xF)],15);\
   if (t_pan& 0x10)\
   {\
      outl+=temp;\
      outr+=Sc ;\
   }\
   else\
   {\
      outl+=Sc;\
      outr+=temp;\
   }\
}

enum _EG_state
{
   EG_ATTACK = 0,
   EG_DECAY1,
   EG_DECAY2,
   EG_RELEASE
};

u32 m_AICARAM_LENGTH, m_RAM_MASK, m_RAM_MASK16;

s16 cdda_sector[CDDA_SIZE]={0};
u32 cdda_index=CDDA_SIZE<<1;

static SampleType mxlr[MAX_CHANNELS];

static const float SDLT[16]={-1000000.0,-42.0,-39.0,-36.0,-33.0,-30.0,-27.0,-24.0,-21.0,-18.0,-15.0,-12.0,-9.0,-6.0,-3.0,0.0};

int m_LPANTABLE[0x20000];
int m_RPANTABLE[0x20000];

//Sound generation, mixin, and channel regs emulation
//x.15
s32 volume_lut[16];
//255 -> mute
s32 tl_lut[256 + 768];	//xx.15 format. >=255 is muted

/* Envelope time in milliseconds */

double ARTimes[64]=
{
	-1, -1 ,8100.0,6900.0,6000.0,4800.0,4000.0,3400.0,3000.0,2400.0,2000.0,1700.0,1500.0,
	1200.0,1000.0,860.0,760.0,600.0,500.0,430.0,380.0,300.0,250.0,220.0,190.0,150.0,130.0,110.0,95.0,
	76.0,63.0,55.0,47.0,38.0,31.0,27.0,24.0,19.0,15.0,13.0,12.0,9.4,7.9,6.8,6.0,4.7,3.8,3.4,3.0,2.4,
	2.0,1.8,1.6,1.3,1.1,0.93,0.85,0.65,0.53,0.44,0.40,0.35,0.0,0.0
};
double DRTimes[64]=
{	-1, -1,118200.0,101300.0,88600.0,70900.0,59100.0,50700.0,44300.0,35500.0,29600.0,25300.0,22200.0,17700.0,
	14800.0,12700.0,11100.0,8900.0,7400.0,6300.0,5500.0,4400.0,3700.0,3200.0,2800.0,2200.0,1800.0,1600.0,1400.0,1100.0,
	920.0,790.0,690.0,550.0,460.0,390.0,340.0,270.0,230.0,200.0,170.0,140.0,110.0,98.0,85.0,68.0,57.0,49.0,43.0,34.0,
	28.0,25.0,22.0,18.0,14.0,12.0,11.0,8.5,7.1,6.1,5.4,4.3,3.6,3.1
};
//Steps per sample
u32 ARTABLE[64];
u32 DRTABLE[64];

const char* stream_names[]=
{
   "0: 16-bit PCM (two's complement format)",
   "1: 8-bit PCM (two's complement format)",
   "2: 4-bit ADPCM (Yamaha format)",
   "3: 4-bit ADPCM long stream"
};


s16 pl=0,pr=0;

struct DSP_OUT_VOL_REG
{
   //--	EFSDL[3:0]	--	EFPAN[4:0]

   u32 EFPAN:5;
   u32 res_1:3;

   u32 EFSDL:4;
   u32 res_2:4;

   u32 pad:16;
};
DSP_OUT_VOL_REG* dsp_out_vol;

#pragma pack (1)
//All regs are 16b , aligned to 32b (upper bits 0?)
struct ChannelCommonData
{
   //+00 [0]
   //SA is half at reg 0 and the rest at reg 1
   u32 SA_hi:7;
   u32 PCMS:2;
   u32 LPCTL:1;
   u32 SSCTL:1;
   u32 res_1:3;
   u32 KEYONB:1;
   u32 KEYONEX:1;

   u32 pad_2:16;

   //+04 [1]
   //SA (defined above)
   u32 SA_low:16;

   u32 pad_3:16;

   //+08 [2]
   u32 LSA:16;

   u32 pad_4:16;

   //+0C [3]
   u32 LEA:16;

   u32 pad_5:16;

   //+10 [4]
   u32 AR:5;
   u32 res_2:1;
   u32 D1R:5;
   u32 D2R:5;

   u32 pad_7:16;

   //+14 [5]
   u32 RR:5;
   u32 DL:5;
   u32 KRS:4;
   u32 LPSLNK:1;
   u32 res_3:1;

   u32 pad_8:16;

   //+18[6]
   u32 FNS:10;
   u32 rez_8_1:1;
   u32 OCT:4;
   u32 rez_8_2:1;

   u32 pad_9:16;

   //+1C	RE	LFOF[4:0]	PLFOWS	PLFOS[2:0]	ALFOWS	ALFOS[2:0]
   u32 ALFOS:3;
   u32 ALFOWS:2;

   u32 PLFOS:3;
   u32 PLFOWS:2;

   u32 LFOF:5;
   u32 LFORE:1;

   u32 pad_10:16;

   //+20	--	IMXL[3:0]	ISEL[3:0]
   u32 ISEL:4;
   u32 IMXL:4;
   u32 rez_20_0:8;

   u32 pad_11:16;

   //+24	--	DISDL[3:0]	--	DIPAN[4:0]
   u32 DIPAN:5;
   u32 rez_24_0:3;

   u32 DISDL:4;
   u32 rez_24_1:4;

   u32 pad_12:16;


   //+28	TL[7:0]	--	Q[4:0]
   u32 Q:5;
   u32 rez_28_0:3;

   u32 TL:8;

   u32 pad_13:16;

   //+2C	--	FLV0[12:0]
   u32 FLV0:13;
   u32 rez_2C_0:3;

   u32 pad_14:16;

   //+30	--	FLV1[12:0]
   u32 FLV1:13;
   u32 rez_30_0:3;

   u32 pad_15:16;

   //+34	--	FLV2[12:0]
   u32 FLV2:13;
   u32 rez_34_0:3;

   u32 pad_16:16;

   //+38	--	FLV3[12:0]
   u32 FLV3:13;
   u32 rez_38_0:3;

   u32 pad_17:16;

   //+3C	--	FLV4[12:0]
   u32 FLV4:13;
   u32 rez_3C_0:3;

   u32 pad_18:16;

   //+40	--	FAR[4:0]	--	FD1R[4:0]
   u32 FD1R:5;
   u32 rez_40_0:3;
   u32 FAR:5;
   u32 rez_40_1:3;

   u32 pad_19:16;

   //+44	--	FD2R[4:0]	--	FRR[4:0]
   u32 FRR:5;
   u32 rez_44_0:3;
   u32 FD2R:5;
   u32 rez_44_1:3;

   u32 pad_20:16;
};

/*
   KEY_OFF->KEY_ON : Resets everything, and starts playback (EG: A)
   KEY_ON->KEY_ON  : nothing
   KEY_ON->KEY_OFF : Switches to RELEASE state (does not disable channel)

*/

struct ChannelEx
{
   static ChannelEx Chans[MAX_CHANNELS];

   ChannelCommonData* ccd;

   u8* SA;
   u32 CA;
   fp_22_10 step;
   u32 update_rate;

   SampleType s0,s1;

   struct
   {
      u32 LSA;
      u32 LEA;

   } loop;
   u8 lpend;

   s32 curstep;
   s32 cur_quant;
   u32 noise_state;//for Noise generator

   struct
   {
      u32 DLAtt;
      u32 DRAtt;
      u32 DSPAtt;
      SampleType* DSPOut;
   } VolMix;

   void (* StepAEG)(ChannelEx* ch);
   void (* StepFEG)(ChannelEx* ch);
   void (* StepStream)(ChannelEx* ch);
   void (* StepStreamInitial)(ChannelEx* ch);

   struct
   {
      s32 volume;

      _EG_state state;

      u32 AR;           /* AttackRate */
      u32 D1R;          /* Decay1Rate */
      u32 DL;           /* DecayLevel */
      u32 D2R;          /* Decay2Rate */
      u32 RR;           /* ReleaseRate */
   } EG;

   struct
   {
      s32 value;
      _EG_state state;
   } FEG;//i have to figure out how this works w/ AEG and channel state, and the IIR values

   struct 
   {
      u32 counter;
      u32 start_value;
      u8 state;
      u8 alfo;
      u8 alfo_shft;
      u8 plfo;
      u8 plfo_shft;
      void (* alfo_calc)(ChannelEx* ch);
      void (* plfo_calc)(ChannelEx* ch);
   } lfo;

   bool active;	//set to false to 'freeze' the channel
   int ChannelNumber;
};

//make these DYNACALL ? they were fastcall before ..
void (* STREAM_STEP_LUT[5][2][2])(ChannelEx* ch);
void (* STREAM_INITAL_STEP_LUT[5])(ChannelEx* ch);
void (* AEG_STEP_LUT[4])(ChannelEx* ch);
void (* FEG_STEP_LUT[4])(ChannelEx* ch);
void (* ALFOWS_CALC[4])(ChannelEx* ch);
void (* PLFOWS_CALC[4])(ChannelEx* ch);

static __forceinline s32 EG_GetValue(struct ChannelEx *slot)
{
   return slot->EG.volume >> EG_SHIFT;
}

static void ADPCM_Reset(ChannelEx *slot)
{
   slot->cur_quant        = 127;
   slot->s0               = 0;
}

static __forceinline SampleType InterpolateSample(struct ChannelEx *slot)
{
   return FPMul(slot->s0,(s32)(1024 - slot->step.fp),10) + FPMul(slot->s1,(s32)(slot->step.fp),10);
}

static __forceinline void LFO_Step(ChannelEx *slot)
{
   slot->lfo.counter--;

   if (slot->lfo.counter != 0)
      return;

   slot->lfo.state++;
   slot->lfo.counter  = slot->lfo.start_value;
   slot->lfo.alfo_calc(slot);
   slot->lfo.plfo_calc(slot);
}

static __forceinline bool SlotStep(struct ChannelEx *slot, SampleType *oLeft, SampleType *oRight, SampleType *oDsp)
{
   if (!slot->active)
      return false;

   SampleType sample=InterpolateSample(slot);

   //Volume & Mixer processing
   //All attenuations are added together then applied and mixed :)

   //offset is up to 511
   //*Att is up to 511
   //logtable handles up to 1024, anything >=255 is mute

   u32 ofsatt    = slot->lfo.alfo + (EG_GetValue(slot) >> 2);
   s32* logtable = ofsatt+tl_lut;
   *oLeft        = FPMul(sample,logtable[slot->VolMix.DLAtt],15);
   *oRight       = FPMul(sample,logtable[slot->VolMix.DRAtt],15);
   *oDsp         = FPMul(sample,logtable[slot->VolMix.DSPAtt],15);

   clip_verify(((s16)*oLeft)      == *oLeft);
   clip_verify(((s16)*oRight)     == *oRight);
   clip_verify(((s16)*oDsp)       == *oDsp);
   clip_verify(sample * (*oLeft)  >= 0);
   clip_verify(sample * (*oRight) >= 0);
   clip_verify(sample * (*oDsp)   >= 0);

   slot->StepAEG(slot);
   slot->StepFEG(slot);
   slot->StepStream(slot);
   LFO_Step(slot);
   return true;
}

static __forceinline void SlotStep(struct ChannelEx *slot, SampleType *mixl, SampleType *mixr)
{
   SampleType oLeft  = 0;
   SampleType oRight = 0;
   SampleType oDsp   = 0;

   SlotStep(slot, &oLeft, &oRight, &oDsp);

   *slot->VolMix.DSPOut += oDsp;
   *mixl              += oLeft;
   *mixr              += oRight;
}

static void SetAegState(struct ChannelEx *slot, _EG_state newstate)
{
   slot->StepAEG    = AEG_STEP_LUT[newstate];
   slot->EG.state  = newstate;
   if (newstate == EG_RELEASE)
      slot->ccd->KEYONB = 0;
}

static void SetFegState(struct ChannelEx *slot, _EG_state newstate)
{
   slot->StepFEG   = FEG_STEP_LUT[newstate];
   slot->FEG.state = newstate;
}

static u32 SlotUpdatePitch(struct ChannelEx *slot);
static void Compute_EG(struct ChannelEx *slot);

static void StartSlot(struct ChannelEx *slot)
{
   if (slot->EG.state != EG_RELEASE)
      return;

   slot->active = true;         /* if it was off then turn it on ! */
   SetAegState(slot, EG_ATTACK); /* reset AEG */
   slot->EG.volume = 0x17f << EG_SHIFT;
   SetFegState(slot, EG_ATTACK); /* reset FEG */

   //Reset sampling state
   slot->CA          = 0;
   slot->step.full   = 0;
   slot->lpend       = 0;
   Compute_EG(slot);
   slot->update_rate = SlotUpdatePitch(slot);

   if (slot->ccd->PCMS >= 2)
   {
      slot->curstep = slot->loop.LSA;
      ADPCM_Reset(slot);
   }

   slot->StepStreamInitial(slot);

   key_printf("[%d] KEY_ON %s @ %f Hz, loop : %d\n",Channel,stream_names[ChanData->PCMS],(44100.0*update_rate)/1024,ChanData->LPCTL);
}

static void KEY_OFF(struct ChannelEx *slot)
{
   if (slot->EG.state == EG_RELEASE)
      return;

   key_printf("[%d] KEY_OFF -> Release\n",Channel);
   SetAegState(slot, EG_RELEASE);
   //switch to release state
}

//PCMS,SSCTL,LPCTL,LPSLNK
void SlotUpdateStreamStep(struct ChannelEx *slot)
{
   s32 fmt = slot->ccd->PCMS;
   if (slot->ccd->SSCTL)
      fmt=4;

   slot->StepStream        = STREAM_STEP_LUT[fmt][slot->ccd->LPCTL][slot->ccd->LPSLNK];
   slot->StepStreamInitial = STREAM_INITAL_STEP_LUT[fmt];
}

//SA,PCMS
static void SlotUpdateSA(struct ChannelEx *slot)
{
   u32 addr = (slot->ccd->SA_hi << 16) | slot->ccd->SA_low;
   if (slot->ccd->PCMS == 0)
      addr&=~1; //0: 16 bit

   slot->SA= &aica_ram.data[addr];
}

static s32 Get_AR(s32 base, s32 R)
{
   s32 rate = base + (R << 1);
   if (rate > 0x3f)
      rate =0x3f;
   if (rate < 0)
      rate = 0;
   return ARTABLE[rate];
}

static s32 Get_DR(s32 base, s32 R)
{
   int Rate = base+(R<<1);
   if(Rate>63)
      Rate=63;
   if(Rate<0)
      Rate=0;
   return DRTABLE[Rate];
}

static s32 Get_RR(s32 base, s32 R)
{
   int Rate=base+(R<<1);
   if(Rate>63)
      Rate=63;
   if(Rate<0)
      Rate=0;
   return DRTABLE[Rate];
}

//D2R,D1R,AR,DL,RR,KRS, [OCT,FNS] for now
static void Compute_EG(struct ChannelEx *slot)
{
   s32 octave = (slot->ccd->OCT ^ 8) - 8;
   s32 rate   = 0;
   
   if (slot->ccd->KRS != 0xF)
      rate    = octave + 2 * slot->ccd->KRS + ((slot->ccd->FNS>>9)&1);

   slot->EG.volume       = 0x17f << EG_SHIFT;
   slot->EG.AR           = Get_AR(rate, slot->ccd->AR);
   slot->EG.D1R          = Get_DR(rate, slot->ccd->D1R);
   slot->EG.DL           = 0x1f - slot->ccd->DL;
   slot->EG.D2R          = Get_DR(rate, slot->ccd->D2R);
   slot->EG.RR           = Get_RR(rate, slot->ccd->RR);
}

/* OCT,FNS */
static u32 SlotUpdatePitch(struct ChannelEx *slot)
{
   u32 octave      = slot->ccd->OCT;
   u32 Fn          = 1024 | slot->ccd->FNS;

   if (octave & 8)
      Fn >>= (16-octave);
   else
      Fn <<= octave;

   return Fn;
}


static void LFO_Reset(struct ChannelEx *slot)
{
   slot->lfo.state   = 0;
   slot->lfo.counter = slot->lfo.start_value;
   slot->lfo.alfo_calc(slot);
   slot->lfo.plfo_calc(slot);
}

static void LFO_SetStartValue(struct ChannelEx *slot, int val)
{
   slot->lfo.start_value  = val;
   slot->lfo.counter      = slot->lfo.start_value;
}

//LFORE,LFOF,PLFOWS,PLFOS,LFOWS,ALFOS
static void Compute_LFO(struct ChannelEx *slot)
{
   int N = slot->ccd->LFOF;
   int S = N     >> 2;
   int M = (~N)   & 3;
   int G = 128   >> S;
   int L = (G-1) << 2;
   int O = L + G * (M+1);

   LFO_SetStartValue(slot, O);

   slot->lfo.plfo_shft = 8 - slot->ccd->PLFOS;
   slot->lfo.alfo_shft = 8 - slot->ccd->ALFOS;

   slot->lfo.alfo_calc=ALFOWS_CALC[slot->ccd->ALFOWS];
   slot->lfo.plfo_calc=PLFOWS_CALC[slot->ccd->PLFOWS];

   if (slot->ccd->LFORE)
      LFO_Reset(slot);
   else
   {
      slot->lfo.alfo_calc(slot);
      slot->lfo.plfo_calc(slot);
   }

   slot->ccd->LFORE=0;
}

//TL,DISDL,DIPAN,IMXL
static void SlotUpdateAtts(struct ChannelEx *slot)
{
   //Converts Send levels to TL-compatible values (DISDL, etc)
   static u32 SendLevel[16]={255,14<<3,13<<3,12<<3,11<<3,10<<3,9<<3,8<<3,7<<3,6<<3,5<<3,4<<3,3<<3,2<<3,1<<3,0<<3};
   u32 attFull = slot->ccd->TL + SendLevel[slot->ccd->DISDL];
   u32 attPan  = attFull+SendLevel[(~slot->ccd->DIPAN)&0xF];

   //0x1* -> R decreases
   if (slot->ccd->DIPAN & 0x10)
   {
      slot->VolMix.DLAtt=attFull;
      slot->VolMix.DRAtt=attPan;
   }
   else //0x0* -> L decreases
   {
      slot->VolMix.DLAtt=attPan;
      slot->VolMix.DRAtt=attFull;
   }

   slot->VolMix.DSPAtt = slot->ccd->TL+SendLevel[slot->ccd->IMXL];
}

//Q,FLV0,FLV1,FLV2,FLV3,FLV4,FAR,FD1R,FD2R,FRR
static void SlotUpdateFEG(struct ChannelEx *slot)
{
   //this needs to be filled
}

static void UpdateSlotReg(struct ChannelEx *slot, u32 offset)
{
   switch (offset & 0x7f)
   {
      case 0x00:
      case CH_REC_FORMAT_KEY_LOOP:
         SlotUpdateStreamStep(slot);
         SlotUpdateSA(slot);
         if (slot->ccd->KEYONEX)
         {
            slot->ccd->KEYONEX=0;
            for (int i = 0; i < 64; i++)
            {
               if (slot->Chans[i].ccd->KEYONB)
                  StartSlot(&slot->Chans[i]);
               else
                  KEY_OFF(&slot->Chans[i]);
            }
         }
         break;

      case 0x04:
      case 0x05:
         SlotUpdateSA(slot);
         break;

      case 0x08://LSA
      case 0x09://LSA
      case 0x0C://LEA
      case 0x0D://LEA
         slot->loop.LSA = slot->ccd->LSA;
         slot->loop.LEA = slot->ccd->LEA;
         break;

      case CH_REC_FNS:
      case CH_REC_FNS_OCT:
         slot->update_rate = SlotUpdatePitch(slot);
         break;

      case CH_REC_RR_DL:
      case CH_REC_DL_KRS_LS:
         SlotUpdateStreamStep(slot);
         slot->EG.RR           = Get_RR(0, slot->ccd->RR);
         slot->EG.DL           = 0x1f - slot->ccd->DL;
         break;


      case CH_REC_ALFOS_ALFOWS_PLFOS:
      case CH_REC_PLFOWS_LFOF_RE:
         Compute_LFO(slot);
         break;

      case 0x20://ISEL,IMXL
         //case 0x21://nothing here !
         //ISEL
         slot->VolMix.DSPOut = &dsp.MIXS[slot->ccd->ISEL];
         SlotUpdateAtts(slot);
         break;

      case 0x24://DIPAN
      case 0x25://DISDL
         SlotUpdateAtts(slot);
         break;

      case 0x28://Q
         SlotUpdateFEG(slot);
         break;

      case 0x29://TL
         SlotUpdateAtts(slot);
         break;

      case 0x2C: //FLV0
      case 0x2D: //FLV0
      case 0x30: //FLV1
      case 0x31: //FLV1
      case 0x34: //FLV2
      case 0x35: //FLV2
      case 0x38: //FLV3
      case 0x39: //FLV3
      case 0x3C: //FLV4
      case 0x3D: //FLV4
      case 0x40: //FD1R
      case 0x41: //FAR
      case 0x44: //FRR
      case 0x45: //FD2R
         SlotUpdateFEG(slot);
         break;

   }
} 

static void StopSlot(struct ChannelEx *slot)
{
   slot->active = false;
   SetAegState(slot, EG_RELEASE);
   slot->EG.volume = 0x3ff << EG_SHIFT;
}

static void SlotInit(struct ChannelEx *slot, int cn,u8* ccd_raw)
{
   unsigned i;
   if (!slot)
      return;

   slot->ccd           = (ChannelCommonData*)&ccd_raw[cn*0x80];
   slot->ChannelNumber = cn;

   for (i = 0;i < 0x80; i++)
      UpdateSlotReg(slot, i);

   StopSlot(slot);
}


static __forceinline SampleType DecodeADPCM(u32 sample,s32 prev,s32 *PrevQuant)
{
   //x.3 format
   static const s32 quant_mul[16] = { 1,3,5,7,9,11,13,15, -1,-3,-5,-7,-9,-11,-13,-15};
   //x.8 format
   static const s32 adpcm_qs[8] = { 0x0e6, 0x0e6, 0x0e6, 0x0e6, 0x133, 0x199, 0x200, 0x266 };
   s32 sign           = 1-2*(sample/8);
   u32 data           = sample&7;
   /*(1 - 2 * L4) * (L3 + L2/2 +L1/4 + 1/8) * quantized width (ƒΆn) + decode value (Xn - 1) */
   SampleType rv      = prev + sign*((*PrevQuant * quant_mul[data])>>3);
   *PrevQuant         = (*PrevQuant * adpcm_qs[data])>>8;

   clip(*PrevQuant,127,24576);
   ICLIP16(rv);
   return rv;
}

template<s32 PCMS,bool last>
static __forceinline void StepDecodeSample(ChannelEx *slot, u32 CA)
{
   SampleType s0,s1;

   if (!last && PCMS<2)
      return ;

   s16* sptr16 = (s16*)slot->SA;
   s8* sptr8   = (s8*)sptr16;
   u8* uptr8   = (u8*)sptr16;

   switch(PCMS)
   {
      case -1:
         slot->noise_state     = slot->noise_state*16807 + 0xbeef;	//beef is good
         s0                    = slot->noise_state;
         s0                  >>= 16;
         s1                    = slot->noise_state*16807 + 0xbeef;
         s1                  >>= 16;
         break;

      case CH_FORMAT_PCM16: /* 16-bit signed */
         sptr16             += CA;
         s0                  = sptr16[0];
         s1                  = sptr16[1];
         break;

      case CH_FORMAT_PCM8: /* 8-bit signed */
         sptr8              += CA;
         s0                  = sptr8[0]<<8;
         s1                  = sptr8[1]<<8;
         break;

      case CH_FORMAT_ADPCM1: /* 4-bit ADPCM */
      case CH_FORMAT_ADPCM2:
         {
            u8 ad1                 = uptr8[CA>>1];
            u8 ad2                 = uptr8[(CA+1)>>1];

            u8 sf                  = (CA&1)*4;
            ad1                  >>= sf;
            ad2                  >>= 4-sf;

            ad1                   &= 0xF;
            ad2                   &= 0xF;

            s32 q                  = slot->cur_quant;
            s0                     = DecodeADPCM(ad1, slot->s0, &q);
            slot->cur_quant        = q;
            s1                     = 0;

            if (last)
               s1=DecodeADPCM(ad2,s0,&q);
         }
         break;
   }

   ICLIP16(s0);
   ICLIP16(s1);
   slot->s0= s0;
   slot->s1= s1;
}

template<s32 PCMS>
static void StepDecodeSampleInitial(ChannelEx *slot)
{
   StepDecodeSample<PCMS,true>(slot, 0);
}

template<s32 PCMS,u32 LPCTL,u32 LPSLNK>
static void StreamStep(ChannelEx *slot)
{
   slot->step.full += slot->update_rate;
   fp_22_10 sp      = slot->step;
   slot->step.ip    = 0;

   while(sp.ip > 0)
   {
      sp.ip--;

      u32 CA  = slot->CA + 1;
      u32 ca_t= CA;

      if (PCMS==3)
         ca_t&=~3;	//adpcm "stream" mode needs this ...

      if (LPSLNK)
      {
         if ((slot->EG.state==EG_ATTACK) && (CA >= slot->loop.LSA))
            SetAegState(slot, EG_DECAY1);
      }

      if (ca_t>= slot->loop.LEA)
      {
         slot->lpend     = 1;
         CA              = slot->loop.LSA;

         switch (LPCTL)
         {
            case 0: /* no loop */
               StopSlot(slot);
               break;
            case 1: /* normal loop */
               if (PCMS >= 2)
               {
                  slot->curstep = slot->loop.LSA;
                  if (PCMS==2) //if in adpcm non-stream mode, reset the decoder
                     ADPCM_Reset(slot);
               }
               break;
         }
      }

      slot->CA=CA;

      //keep adpcm up to date
      if (sp.ip==0)
         StepDecodeSample<PCMS,true>(slot,CA);
      else
         StepDecodeSample<PCMS,false>(slot,CA);
   }
}

template<s32 ALFOWS>
static void CalcAlfo(ChannelEx *slot)
{
   u32 rv;
   switch(ALFOWS)
   {
      case 0: // Sawtooth
         rv = slot->lfo.state;
         break;

      case 1: // Square
         rv = slot->lfo.state&0x80?255:0;
         break;

      case 2: // Triangle
         rv = (slot->lfo.state&0x7f) ^ (slot->lfo.state&0x80 ? 0x7F:0);
         rv<<=1;
         break;

      case 3:// Random ! .. not :p
         rv=(slot->lfo.state>>3) ^ (slot->lfo.state<<3) ^ (slot->lfo.state&0xE3);
         break;
   }

   slot->lfo.alfo = rv >> slot->lfo.alfo_shft;
}

template<s32 PLFOWS>
static void CalcPlfo(ChannelEx *slot)
{
   u32 rv;

   switch(PLFOWS)
   {
      case 0: // sawtooth
         rv   = slot->lfo.state;
         break;

      case 1: // square
         rv   = slot->lfo.state&0x80?0x80:0x7F;
         break;

      case 2: // triangle
         rv   = (slot->lfo.state&0x7f)^(slot->lfo.state&0x80 ? 0x7F:0);
         rv <<= 1;
         rv   = (u8)(rv-0x80); //2's complement
         break;

      case 3:// random ! .. not :p
         rv   = (slot->lfo.state >> 3) ^ (slot->lfo.state << 3) ^ (slot->lfo.state & 0xE3);
         break;
   }
   slot->lfo.alfo = rv >> slot->lfo.plfo_shft;
}

template<u32 state>
static void EG_Step(ChannelEx *slot)
{
   switch(state)
   {
      case EG_ATTACK:
         slot->EG.volume -= slot->EG.AR;
         if (EG_GetValue(slot) <= 0)
         {
            slot->EG.volume = 0 << EG_SHIFT;
            if (!slot->ccd->LPSLNK)
               SetAegState(slot, EG_DECAY1);
         }
         break;
      case EG_DECAY1:
         slot->EG.volume += slot->EG.D1R;
         if (((u32)EG_GetValue(slot)) >= slot->EG.DL)
            SetAegState(slot, EG_DECAY2);
         break;
      case EG_DECAY2:
         slot->EG.volume += slot->EG.D2R;
         if (EG_GetValue(slot) >= 0x3FF)
         {
            slot->EG.volume = 0x3ff << EG_SHIFT;
            SetAegState(slot, EG_RELEASE);
         }
         break;
      case EG_RELEASE: //only on key_off ?
         slot->EG.volume += slot->EG.RR;

         if (EG_GetValue(slot) >= 0x3FF)
         {
            StopSlot(slot); /* TODO: Is this ok here? It's a speed optimisation (since the channel is muted) */
         }
         break;
   }
}

template<u32 state>
static void FegStep(ChannelEx *slot)
{
}

#define AicaChannel ChannelEx

AicaChannel AicaChannel::Chans[MAX_CHANNELS];

#define Chans AicaChannel::Chans 

void sgc_Init(void)
{
   m_AICARAM_LENGTH = 2 * 1024 * 1024;
   m_RAM_MASK       = m_AICARAM_LENGTH-1;
   m_RAM_MASK16     = m_RAM_MASK & 0x7ffffe;

   STREAM_STEP_LUT[0][0][0]=&StreamStep<0,0,0>;
   STREAM_STEP_LUT[1][0][0]=&StreamStep<1,0,0>;
   STREAM_STEP_LUT[2][0][0]=&StreamStep<2,0,0>;
   STREAM_STEP_LUT[3][0][0]=&StreamStep<3,0,0>;
   STREAM_STEP_LUT[4][0][0]=&StreamStep<-1,0,0>;

   STREAM_STEP_LUT[0][0][1]=&StreamStep<0,0,1>;
   STREAM_STEP_LUT[1][0][1]=&StreamStep<1,0,1>;
   STREAM_STEP_LUT[2][0][1]=&StreamStep<2,0,1>;
   STREAM_STEP_LUT[3][0][1]=&StreamStep<3,0,1>;
   STREAM_STEP_LUT[4][0][1]=&StreamStep<-1,0,1>;

   STREAM_STEP_LUT[0][1][0]=&StreamStep<0,1,0>;
   STREAM_STEP_LUT[1][1][0]=&StreamStep<1,1,0>;
   STREAM_STEP_LUT[2][1][0]=&StreamStep<2,1,0>;
   STREAM_STEP_LUT[3][1][0]=&StreamStep<3,1,0>;
   STREAM_STEP_LUT[4][1][0]=&StreamStep<-1,1,0>;

   STREAM_STEP_LUT[0][1][1]=&StreamStep<0,1,1>;
   STREAM_STEP_LUT[1][1][1]=&StreamStep<1,1,1>;
   STREAM_STEP_LUT[2][1][1]=&StreamStep<2,1,1>;
   STREAM_STEP_LUT[3][1][1]=&StreamStep<3,1,1>;
   STREAM_STEP_LUT[4][1][1]=&StreamStep<-1,1,1>;

   STREAM_INITAL_STEP_LUT[0]=&StepDecodeSampleInitial<0>;
   STREAM_INITAL_STEP_LUT[1]=&StepDecodeSampleInitial<1>;
   STREAM_INITAL_STEP_LUT[2]=&StepDecodeSampleInitial<2>;
   STREAM_INITAL_STEP_LUT[3]=&StepDecodeSampleInitial<3>;
   STREAM_INITAL_STEP_LUT[4]=&StepDecodeSampleInitial<-1>;

   AEG_STEP_LUT[0]=&EG_Step<0>;
   AEG_STEP_LUT[1]=&EG_Step<1>;
   AEG_STEP_LUT[2]=&EG_Step<2>;
   AEG_STEP_LUT[3]=&EG_Step<3>;

   FEG_STEP_LUT[0]=&FegStep<0>;
   FEG_STEP_LUT[1]=&FegStep<1>;
   FEG_STEP_LUT[2]=&FegStep<2>;
   FEG_STEP_LUT[3]=&FegStep<3>;

   ALFOWS_CALC[0]=&CalcAlfo<0>;
   ALFOWS_CALC[1]=&CalcAlfo<1>;
   ALFOWS_CALC[2]=&CalcAlfo<2>;
   ALFOWS_CALC[3]=&CalcAlfo<3>;

   PLFOWS_CALC[0]=&CalcPlfo<0>;
   PLFOWS_CALC[1]=&CalcPlfo<1>;
   PLFOWS_CALC[2]=&CalcPlfo<2>;
   PLFOWS_CALC[3]=&CalcPlfo<3>;

   memset(m_LPANTABLE, 0, sizeof(m_LPANTABLE));
	memset(m_RPANTABLE, 0, sizeof(m_RPANTABLE));

   for(int i=0;i<0x20000;++i)
   {
      int iTL =(i>>0x0)&0xff;
      int iPAN=(i>>0x8)&0x1f;
      int iSDL=(i>>0xD)&0x0F;
      float TL=1.0;
      float SegaDB=0;
      float fSDL=1.0;
      float PAN=1.0;
      float LPAN,RPAN;

      if(iTL&0x01) SegaDB-=0.4f;
      if(iTL&0x02) SegaDB-=0.8f;
      if(iTL&0x04) SegaDB-=1.5f;
      if(iTL&0x08) SegaDB-=3.0f;
      if(iTL&0x10) SegaDB-=6.0f;
      if(iTL&0x20) SegaDB-=12.0f;
      if(iTL&0x40) SegaDB-=24.0f;
      if(iTL&0x80) SegaDB-=48.0f;

      TL=pow(10.0,SegaDB/20.0);

      SegaDB=0;
      if(iPAN&0x1) SegaDB-=3.0f;
      if(iPAN&0x2) SegaDB-=6.0f;
      if(iPAN&0x4) SegaDB-=12.0f;
      if(iPAN&0x8) SegaDB-=24.0f;

      if((iPAN&0xf)==0xf) PAN=0.0;
      else PAN=pow(10.0,SegaDB/20.0);

      if(iPAN<0x10)
      {
         LPAN=PAN;
         RPAN=1.0;
      }
      else
      {
         RPAN=PAN;
         LPAN=1.0;
      }

      if(iSDL)
         fSDL=pow(10.0,(SDLT[iSDL])/20.0);
      else
         fSDL=0.0;

      m_LPANTABLE[i]=FIX((4.0*LPAN*TL*fSDL));
      m_RPANTABLE[i]=FIX((4.0*RPAN*TL*fSDL));
   }

   for (int i=0;i<16;i++)
   {
      volume_lut[i]=(s32)((1<<15)/pow(2.0,(15-i)/2.0));
      if (i==0)
         volume_lut[i]=0;
   }

   for (int i=0;i<256;i++)
      tl_lut[i]=(s32)((1<<15)/pow(2.0,i/16.0));

   //tl entries 256 to 1023 are 0
   for (int i=256;i<1024;i++)
      tl_lut[i]=0;

   ARTABLE[0]=DRTABLE[0]=0;    //Infinite time
	ARTABLE[1]=DRTABLE[1]=0;    //Infinite time
   for (int i = 2; i < 64; i++)
   {
      double t,step,scale;
      t=ARTimes[i];   //In ms
      if(t!=0.0)
      {
         step=(1023*1000.0)/(44100.0*t);
         scale=(double) (1<<EG_SHIFT);
         ARTABLE[i]=(int) (step*scale);
      }
      else
         ARTABLE[i]=1024<<EG_SHIFT;

      t=DRTimes[i];   //In ms
      step=(1023*1000.0)/(44100.0*t);
      scale=(double) (1<<EG_SHIFT);
      DRTABLE[i]=(int) (step*scale);
   }

   for (int i = 0; i < MAX_CHANNELS; i++)
      SlotInit(&Chans[i], i,aica_reg);
   dsp_out_vol=(DSP_OUT_VOL_REG*)&aica_reg[0x2000];

   dsp_init();
}

void sgc_Term(void)
{

}

void WriteChannelReg8(u32 channel,u32 reg)
{
   UpdateSlotReg(&Chans[channel], reg);
}

void ReadCommonReg(u32 reg,bool byte)
{
   switch(reg)
   {
      case 8:
      case 9:
         CommonData->MIEMP=1;
         CommonData->MOEMP=1;
         break;
      case 0x10: /* LP check */
      case 0x11:
         {
            u32 slotnum=CommonData->MSLC;
            struct ChannelEx *slot = &Chans[slotnum];

            CommonData->LP= slot->lpend;
            verify(CommonData->AFSEL==0);

            CommonData->EG  = EG_GetValue(slot); //AEG is only 10 bits, FEG is 13 bits
            CommonData->SGC = slot->EG.state;

            if (! (byte && reg==0x2810))
               slot->lpend = 0;
         }
         break;
      case 0x14: /* CA (slot address */
      case 0x15:
         {
            u32 slotnum=CommonData->MSLC;
            struct ChannelEx *slot = &Chans[slotnum];

            CommonData->CA = slot->CA /*& (~1023)*/; //mmnn??
            //printf("[%d] CA read %d\n",slotnum , slot->CA);
         }
         break;
      case 0xb8:
      case 0xb9:
         /* TODO - to implement */
         break;
   }
}

void WriteCommonReg8(u32 reg,u32 data)
{
   aica_reg[reg]=(u8)data;

   if (reg==0x2804 || reg==0x2805)
   {
      dsp.RBL=(8192<<CommonData->RBL)-1;
      dsp.RBP=( CommonData->RBP*2048&AICA_RAM_MASK);
      dsp.dyndirty=true;
   }
}


/* No DSP for now in this version */
void AICA_Sample32(void)
{
   if (settings.aica.NoBatch)
      return;

   memset(mxlr,0,sizeof(mxlr));

   /* Generate 32 samples for each channel, 
    * before moving to next channel.
    * much more cache efficient ! */
   u32 sg=0;

   for (int ch = 0; ch < MAX_CHANNELS; ch++)
   {
      for (int i=0;i<32;i++)
      {
         SampleType oLeft  = 0;
         SampleType oRight = 0;
         SampleType oDsp   = 0;

         /*stop working on this channel if its turned off ... */
         if (!SlotStep(&Chans[ch], &oLeft, &oRight, &oDsp))
            break;

         sg++;

         if ((oLeft+oRight) == 0)
            oLeft = oRight = oDsp;

         mxlr[i*2+0] += oLeft;
         mxlr[i*2+1] += oRight;
      }
   }

   //OK , generated all Channels  , now DSP/ect + final mix ;p
   //CDDA EXTS input

   for (int i=0;i<32;i++)
   {
      SampleType mixl,mixr;

      mixl=mxlr[i*2+0];
      mixr=mxlr[i*2+1];

      if (cdda_index>=CDDA_SIZE)
      {
         cdda_index=0;
         libCore_CDDA_Sector(cdda_sector);
      }
      s32 EXTS0L=cdda_sector[cdda_index];
      s32 EXTS0R=cdda_sector[cdda_index+1];
      cdda_index+=2;

      /* Final mix
       * Add CDDA / DSP effect(s) */

      /* CDDA */
      if (settings.aica.CDDAMute==0) 
      {
         VOLPAN(EXTS0L,dsp_out_vol[16].EFSDL,dsp_out_vol[16].EFPAN,mixl,mixr);
         VOLPAN(EXTS0R,dsp_out_vol[17].EFSDL,dsp_out_vol[17].EFPAN,mixl,mixr);
      }

      /* Mono */
      if (CommonData->Mono)
      {
         /* Yay for mono =P */
         mixl+=mixr;
         mixr=mixl;
      }

      /* MVOL !
       * we want to make sure mix* is *At least* 23 bits wide here, so 64 bit mul ! */
      u32 mvol = CommonData->MVOL;
      s32 val  = volume_lut[mvol];
      mixl     = (s32)FPMul((s64)mixl,val,15);
      mixr     = (s32)FPMul((s64)mixr,val,15);

      if (CommonData->DAC18B)
      {
         /* If 18 bit output , make it 16bit */
         /* Remove the fractional part by chopping.. */
         mixl >>= 2;
         mixr >>= 2;
      }

      /* Sample is ready. clip/saturate and store */

#ifdef CLIP_WARN
      if (((s16)mixl) != mixl)
         printf("Clipped mixl %d\n",mixl);
      if (((s16)mixr) != mixr)
         printf("Clipped mixr %d\n",mixr);
#endif

      ICLIP16(mixl);
      ICLIP16(mixr);

      pl=mixl;
      pr=mixr;

      if (!settings.aica.NoSound) WriteSample(mixr,mixl);
   }
}

void AICA_Sample(void)
{
   SampleType mixl = 0;
   SampleType mixr = 0;
   memset(dsp.MIXS, 0, sizeof(dsp.MIXS));

   for (int i = 0; i < MAX_CHANNELS; i++)
      SlotStep(&ChannelEx::Chans[i], &mixl, &mixr);

   /* OK , generated all Channels  , now DSP/ect + final mix
    * CDDA EXTS input */

   if (cdda_index>=CDDA_SIZE)
   {
      cdda_index=0;
      libCore_CDDA_Sector(cdda_sector);
   }

   s32 EXTS0L=cdda_sector[cdda_index];
   s32 EXTS0R=cdda_sector[cdda_index+1];
   cdda_index+=2;

   /* Final mix
    * Add CDDA / DSP effect(s)
    */

   /* CDDA */
   if (settings.aica.CDDAMute==0) 
   {
      VOLPAN(EXTS0L,dsp_out_vol[16].EFSDL,dsp_out_vol[16].EFPAN,mixl,mixr);
      VOLPAN(EXTS0R,dsp_out_vol[17].EFSDL,dsp_out_vol[17].EFPAN,mixl,mixr);
   }

   if (settings.aica.DSPEnabled)
   {
      dsp_step();

      for (int i=0;i<16;i++)
         VOLPAN( (*(s16*)&DSPData->EFREG[i]) ,dsp_out_vol[i].EFSDL,dsp_out_vol[i].EFPAN,mixl,mixr);
   }

   if (settings.aica.NoSound)
      return;

   /* Mono */
   if (CommonData->Mono)
   {
      /* Yay for mono =P */
      mixl+=mixr;
      mixr=mixl;
   }

   //MVOL !
   /* we want to make sure mix* is *At least* 23 bits wide here, so 64 bit mul ! */
   u32 mvol = CommonData->MVOL;
   s32 val  = volume_lut[mvol];
   mixl     = (s32)FPMul((s64)mixl,val,15);
   mixr     = (s32)FPMul((s64)mixr,val,15);

   if (CommonData->DAC18B)
   {
      /* If 18 bit output , make it 16bit */
      /* Remove the fractional part by chopping.. */
      mixl >>= 2;
      mixr >>= 2;
   }

   /* Sample is ready. clip/saturate and store */
#ifdef CLIP_WARN
   if (((s16)mixl) != mixl)
      printf("Clipped mixl %d\n",mixl);
   if (((s16)mixr) != mixr)
      printf("Clipped mixr %d\n",mixr);
#endif

   ICLIP16(mixl);
   ICLIP16(mixr);

   pl=mixl;
   pr=mixr;

   WriteSample(mixr,mixl);
}
