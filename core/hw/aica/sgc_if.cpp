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

s16 cdda_sector[CDDA_SIZE]={0};
u32 cdda_index=CDDA_SIZE<<1;

static SampleType mxlr[MAX_CHANNELS];

//Sound generation, mixin, and channel regs emulation
//x.15
s32 volume_lut[16];
//255 -> mute
//Converts Send levels to TL-compatible values (DISDL, etc)
u32 SendLevel[16]={255,14<<3,13<<3,12<<3,11<<3,10<<3,9<<3,8<<3,7<<3,6<<3,5<<3,4<<3,3<<3,2<<3,1<<3,0<<3};
s32 tl_lut[256 + 768];	//xx.15 format. >=255 is muted

/* Envelope time in milliseconds */

double AEG_Attack_Time[]=
{
	-1,-1,8100.0,6900.0,6000.0,4800.0,4000.0,3400.0,3000.0,2400.0,2000.0,1700.0,1500.0,
	1200.0,1000.0,860.0,760.0,600.0,500.0,430.0,380.0,300.0,250.0,220.0,190.0,150.0,130.0,110.0,95.0,
	76.0,63.0,55.0,47.0,38.0,31.0,27.0,24.0,19.0,15.0,13.0,12.0,9.4,7.9,6.8,6.0,4.7,3.8,3.4,3.0,2.4,
	2.0,1.8,1.6,1.3,1.1,0.93,0.85,0.65,0.53,0.44,0.40,0.35,0.0,0.0
};
double AEG_DSR_Time[]=
{	-1,-1,118200.0,101300.0,88600.0,70900.0,59100.0,50700.0,44300.0,35500.0,29600.0,25300.0,22200.0,17700.0,
	14800.0,12700.0,11100.0,8900.0,7400.0,6300.0,5500.0,4400.0,3700.0,3200.0,2800.0,2200.0,1800.0,1600.0,1400.0,1100.0,
	920.0,790.0,690.0,550.0,460.0,390.0,340.0,270.0,230.0,200.0,170.0,140.0,110.0,98.0,85.0,68.0,57.0,49.0,43.0,34.0,
	28.0,25.0,22.0,18.0,14.0,12.0,11.0,8.5,7.1,6.1,5.4,4.3,3.6,3.1
};
//Steps per sample
u32 AEG_ATT_SPS[MAX_CHANNELS];
u32 AEG_DSR_SPS[MAX_CHANNELS];

const char* stream_names[]=
{
   "0: 16-bit PCM (two's complement format)",
   "1: 8-bit PCM (two's complement format)",
   "2: 4-bit ADPCM (Yamaha format)",
   "3: 4-bit ADPCM long stream"
};

//x.8 format
const s32 adpcm_qs[8] = 
{
   0x0e6, 0x0e6, 0x0e6, 0x0e6, 0x133, 0x199, 0x200, 0x266,
};
//x.3 format
const s32 adpcm_scale[16] = 
{
   1,3,5,7,9,11,13,15,
   -1,-3,-5,-7,-9,-11,-13,-15,
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
struct ChannelEx;

//make these DYNACALL ? they were fastcall before ..
void (* STREAM_STEP_LUT[5][2][2])(ChannelEx* ch);
void (* STREAM_INITAL_STEP_LUT[5])(ChannelEx* ch);
void (* AEG_STEP_LUT[4])(ChannelEx* ch);
void (* FEG_STEP_LUT[4])(ChannelEx* ch);
void (* ALFOWS_CALC[4])(ChannelEx* ch);
void (* PLFOWS_CALC[4])(ChannelEx* ch);


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

      u8 looped;
   } loop;

   s32 adpcm_last_quant;
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
      __forceinline s32 GetValue() { return volume >> EG_SHIFT;}
      void SetValue(u32 aegb) { volume = aegb << EG_SHIFT; }

      _EG_state state;

      u32 AttackRate;
      u32 Decay1Rate;
      u32 Decay2Value;
      u32 Decay2Rate;
      u32 ReleaseRate;
   } AEG;

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

   bool enabled;	//set to false to 'freeze' the channel
   int ChannelNumber;
};

static void ADPCM_Reset(ChannelEx* ch)
{
   ch->adpcm_last_quant = 127;
   ch->s0               = 0;
}

static __forceinline SampleType InterpolateSample(struct ChannelEx *ch)
{
   return FPMul(ch->s0,(s32)(1024 - ch->step.fp),10) + FPMul(ch->s1,(s32)(ch->step.fp),10);
}

static __forceinline void LFO_Step(ChannelEx* ch)
{
   ch->lfo.counter--;

   if (ch->lfo.counter != 0)
      return;

   ch->lfo.state++;
   ch->lfo.counter  = ch->lfo.start_value;
   ch->lfo.alfo_calc(ch);
   ch->lfo.plfo_calc(ch);
}

static __forceinline bool SlotStep(struct ChannelEx *ch, SampleType *oLeft, SampleType *oRight, SampleType *oDsp)
{
   if (!ch->enabled)
      return false;

   SampleType sample=InterpolateSample(ch);

   //Volume & Mixer processing
   //All attenuations are added together then applied and mixed :)

   //offset is up to 511
   //*Att is up to 511
   //logtable handles up to 1024, anything >=255 is mute

   u32 ofsatt    = ch->lfo.alfo + (ch->AEG.GetValue()>>2);
   s32* logtable = ofsatt+tl_lut;
   *oLeft        = FPMul(sample,logtable[ch->VolMix.DLAtt],15);
   *oRight       = FPMul(sample,logtable[ch->VolMix.DRAtt],15);
   *oDsp         = FPMul(sample,logtable[ch->VolMix.DSPAtt],15);

   clip_verify(((s16)*oLeft)      == *oLeft);
   clip_verify(((s16)*oRight)     == *oRight);
   clip_verify(((s16)*oDsp)       == *oDsp);
   clip_verify(sample * (*oLeft)  >= 0);
   clip_verify(sample * (*oRight) >= 0);
   clip_verify(sample * (*oDsp)   >= 0);

   ch->StepAEG(ch);
   ch->StepFEG(ch);
   ch->StepStream(ch);
   LFO_Step(ch);
   return true;
}

static __forceinline void SlotStep(struct ChannelEx *ch, SampleType *mixl, SampleType *mixr)
{
   SampleType oLeft  = 0;
   SampleType oRight = 0;
   SampleType oDsp   = 0;

   SlotStep(ch, &oLeft, &oRight, &oDsp);

   *ch->VolMix.DSPOut += oDsp;
   *mixl              += oLeft;
   *mixr              += oRight;
}

static void SetAegState(struct ChannelEx *ch, _EG_state newstate)
{
   ch->StepAEG    = AEG_STEP_LUT[newstate];
   ch->AEG.state  = newstate;
   if (newstate == EG_RELEASE)
      ch->ccd->KEYONB = 0;
}

static void SetFegState(struct ChannelEx *ch, _EG_state newstate)
{
   ch->StepFEG   = FEG_STEP_LUT[newstate];
   ch->FEG.state = newstate;
}

static void KEY_ON(struct ChannelEx *ch)
{
   if (ch->AEG.state != EG_RELEASE)
      return;

   ch->enabled = true;         /* if it was off then turn it on ! */
   SetAegState(ch, EG_ATTACK); /* reset AEG */
   ch->AEG.SetValue(0x3FF);    /* start from 0x3FF ? .. it seems so ! */

   SetFegState(ch, EG_ATTACK); /* reset FEG */

   //Reset sampling state
   ch->CA          = 0;
   ch->step.full   = 0;
   ch->loop.looped = false;

   ADPCM_Reset(ch);
   ch->StepStreamInitial(ch);

   key_printf("[%d] KEY_ON %s @ %f Hz, loop : %d\n",Channel,stream_names[ChanData->PCMS],(44100.0*update_rate)/1024,ChanData->LPCTL);
}

static void KEY_OFF(struct ChannelEx *ch)
{
   if (ch->AEG.state == EG_RELEASE)
      return;

   key_printf("[%d] KEY_OFF -> Release\n",Channel);
   SetAegState(ch, EG_RELEASE);
   //switch to release state
}

//PCMS,SSCTL,LPCTL,LPSLNK
void SlotUpdateStreamStep(struct ChannelEx *ch)
{
   s32 fmt = ch->ccd->PCMS;
   if (ch->ccd->SSCTL)
      fmt=4;

   ch->StepStream        = STREAM_STEP_LUT[fmt][ch->ccd->LPCTL][ch->ccd->LPSLNK];
   ch->StepStreamInitial = STREAM_INITAL_STEP_LUT[fmt];
}

//SA,PCMS
static void SlotUpdateSA(struct ChannelEx *ch)
{
   u32 addr = (ch->ccd->SA_hi << 16) | ch->ccd->SA_low;
   if (ch->ccd->PCMS == 0)
      addr&=~1; //0: 16 bit

   ch->SA= &aica_ram.data[addr];
}

static u32 SlotAEG_EffRate(struct ChannelEx *ch, u32 re)
{
   s32 rv = ch->ccd->KRS + (ch->ccd->FNS>>9) + re*2;

   if (ch->ccd->KRS==0xF)
      rv -= 0xF;

   if (ch->ccd->OCT&8)
      rv -= (16 - ch->ccd->OCT)*2;
   else
      rv += ch->ccd->OCT*2;

   if (rv<0)
      rv=0;
   if (rv>0x3f)
      rv=0x3f;
   return rv;
}

//D2R,D1R,AR,DL,RR,KRS, [OCT,FNS] for now
static void SlotUpdateAEG(struct ChannelEx *ch)
{
   ch->AEG.AttackRate   = AEG_ATT_SPS[SlotAEG_EffRate(ch, ch->ccd->AR)];
   ch->AEG.Decay1Rate   = AEG_DSR_SPS[SlotAEG_EffRate(ch, ch->ccd->D1R)];
   ch->AEG.Decay2Value  = ch->ccd->DL<<5;
   ch->AEG.Decay2Rate   = AEG_DSR_SPS[SlotAEG_EffRate(ch, ch->ccd->D2R)];
   ch->AEG.ReleaseRate  = AEG_DSR_SPS[SlotAEG_EffRate(ch, ch->ccd->RR)];
}

/* OCT,FNS */
static void SlotUpdatePitch(struct ChannelEx *ch)
{
   u32 oct         = ch->ccd->OCT;
   u32 update_rate = 1024 | ch->ccd->FNS;

   if (oct& 8)
      update_rate>>=(16-oct);
   else
      update_rate<<=oct;

   ch->update_rate=update_rate;
}


static void LFO_Reset(struct ChannelEx *ch)
{
   ch->lfo.state   = 0;
   ch->lfo.counter = ch->lfo.start_value;
   ch->lfo.alfo_calc(ch);
   ch->lfo.plfo_calc(ch);
}

static void LFO_SetStartValue(struct ChannelEx *ch, int val)
{
   ch->lfo.start_value  = val;
   ch->lfo.counter      = ch->lfo.start_value;
}

//LFORE,LFOF,PLFOWS,PLFOS,LFOWS,ALFOS
static void SlotUpdateLFO(struct ChannelEx *ch)
{
   int N = ch->ccd->LFOF;
   int S = N     >> 2;
   int M = (~N)   & 3;
   int G = 128   >> S;
   int L = (G-1) << 2;
   int O = L + G * (M+1);

   LFO_SetStartValue(ch, O);

   ch->lfo.plfo_shft=8-ch->ccd->PLFOS;
   ch->lfo.alfo_shft=8-ch->ccd->ALFOS;

   ch->lfo.alfo_calc=ALFOWS_CALC[ch->ccd->ALFOWS];
   ch->lfo.plfo_calc=PLFOWS_CALC[ch->ccd->PLFOWS];

   if (ch->ccd->LFORE)
      LFO_Reset(ch);
   else
   {
      ch->lfo.alfo_calc(ch);
      ch->lfo.plfo_calc(ch);
   }

   ch->ccd->LFORE=0;
}

//TL,DISDL,DIPAN,IMXL
static void SlotUpdateAtts(struct ChannelEx *ch)
{
   u32 attFull = ch->ccd->TL+SendLevel[ch->ccd->DISDL];
   u32 attPan  = attFull+SendLevel[(~ch->ccd->DIPAN)&0xF];

   //0x1* -> R decreases
   if (ch->ccd->DIPAN & 0x10)
   {
      ch->VolMix.DLAtt=attFull;
      ch->VolMix.DRAtt=attPan;
   }
   else //0x0* -> L decreases
   {
      ch->VolMix.DLAtt=attPan;
      ch->VolMix.DRAtt=attFull;
   }

   ch->VolMix.DSPAtt = ch->ccd->TL+SendLevel[ch->ccd->IMXL];
}

//Q,FLV0,FLV1,FLV2,FLV3,FLV4,FAR,FD1R,FD2R,FRR
static void SlotUpdateFEG(struct ChannelEx *ch)
{
   //this needs to be filled
}

static void SlotRegWrite(struct ChannelEx *ch, u32 offset)
{
   switch(offset)
   {
      case 0x00: //yay ?
         SlotUpdateStreamStep(ch);
         SlotUpdateSA(ch);
         break;

      case CH_REC_FORMAT_KEY_LOOP:
         SlotUpdateStreamStep(ch);
         SlotUpdateSA(ch);
         if (ch->ccd->KEYONEX)
         {
            ch->ccd->KEYONEX=0;
            for (int i = 0; i < MAX_CHANNELS; i++)
            {
               if (ch->Chans[i].ccd->KEYONB)
                  KEY_ON(&ch->Chans[i]);
               else
                  KEY_OFF(&ch->Chans[i]);
            }
         }
         break;

      case 0x04:
      case 0x05:
         SlotUpdateSA(ch);
         break;

      case 0x08://LSA
      case 0x09://LSA
      case 0x0C://LEA
      case 0x0D://LEA
         ch->loop.LSA = ch->ccd->LSA;
         ch->loop.LEA = ch->ccd->LEA;
         break;

      case CH_REC_AR_D1R:
      case CH_REC_D1R_D2R:
         SlotUpdateAEG(ch);
         break;

      case CH_REC_RR_DL:
      case CH_REC_DL_KRS_LS:
         SlotUpdateStreamStep(ch);
         SlotUpdateAEG(ch);
         break;

      case CH_REC_FNS:
      case CH_REC_FNS_OCT:
         SlotUpdatePitch(ch);
         break;

      case CH_REC_ALFOS_ALFOWS_PLFOS:
      case CH_REC_PLFOWS_LFOF_RE:
         SlotUpdateLFO(ch);
         break;

      case 0x20://ISEL,IMXL
         //case 0x21://nothing here !
         //ISEL
         ch->VolMix.DSPOut = &dsp.MIXS[ch->ccd->ISEL];
         SlotUpdateAtts(ch);
         break;

      case 0x24://DIPAN
      case 0x25://DISDL
         SlotUpdateAtts(ch);
         break;

      case 0x28://Q
         SlotUpdateFEG(ch);
         break;

      case 0x29://TL
         SlotUpdateAtts(ch);
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
         SlotUpdateFEG(ch);
         break;

   }
} 

static void StopSlot(struct ChannelEx *ch)
{
   ch->enabled = false;
   SetAegState(ch, EG_RELEASE);
   ch->AEG.SetValue(0x3FF);
}

static void SlotInit(struct ChannelEx *ch, int cn,u8* ccd_raw)
{
   unsigned i;
   if (!ch)
      return;

   ch->ccd           = (ChannelCommonData*)&ccd_raw[cn*0x80];
   ch->ChannelNumber = cn;

   for (i = 0;i < 0x80; i++)
      SlotRegWrite(ch, i);

   StopSlot(ch);
}


static __forceinline SampleType DecodeADPCM(u32 sample,s32 prev,s32& quant)
{
   s32 sign      = 1-2*(sample/8);
   u32 data      = sample&7;
   /*(1 - 2 * L4) * (L3 + L2/2 +L1/4 + 1/8) * quantized width (ƒΆn) + decode value (Xn - 1) */
   SampleType rv = prev + sign*((quant*adpcm_scale[data])>>3);
   quant         = (quant * adpcm_qs[data])>>8;

   clip(quant,127,24576);
   ICLIP16(rv);
   return rv;
}

template<s32 PCMS,bool last>
static __forceinline void StepDecodeSample(ChannelEx* ch,u32 CA)
{
   SampleType s0,s1;

   if (!last && PCMS<2)
      return ;

   s16* sptr16 = (s16*)ch->SA;
   s8* sptr8   = (s8*)sptr16;
   u8* uptr8   = (u8*)sptr16;

   switch(PCMS)
   {
      case -1:
         ch->noise_state     = ch->noise_state*16807 + 0xbeef;	//beef is good
         s0                  = ch->noise_state;
         s0                >>= 16;
         s1                  = ch->noise_state*16807 + 0xbeef;
         s1                >>= 16;
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

            s32 q                  = ch->adpcm_last_quant;
            s0                     = DecodeADPCM(ad1,ch->s0,q);
            ch->adpcm_last_quant   = q;
            s1                     = 0;

            if (last)
               s1=DecodeADPCM(ad2,s0,q);
         }
         break;
   }

   ch->s0=s0;
   ch->s1=s1;
}

template<s32 PCMS>
static void StepDecodeSampleInitial(ChannelEx* ch)
{
   StepDecodeSample<PCMS,true>(ch,0);
}

template<s32 PCMS,u32 LPCTL,u32 LPSLNK>
static void StreamStep(ChannelEx* ch)
{
   ch->step.full+=ch->update_rate;
   fp_22_10 sp=ch->step;
   ch->step.ip=0;

   while(sp.ip > 0)
   {
      sp.ip--;

      u32 CA=ch->CA + 1;

      u32 ca_t=CA;
      if (PCMS==3)
         ca_t&=~3;	//adpcm "stream" mode needs this ...

      if (LPSLNK)
      {
         if ((ch->AEG.state==EG_ATTACK) && (CA>=ch->loop.LSA))
         {

            step_printf("[%d]LPSLNK : Switching to EG_DECAY1 %X\n",Channel,AEG.GetValue());
            SetAegState(ch, EG_DECAY1);
         }
      }

      if (ca_t>=ch->loop.LEA)
      {
         ch->loop.looped = 1;
         CA              = ch->loop.LSA;

         switch (LPCTL)
         {
            case 0: /* no loop */
               StopSlot(ch);
               break;
            case 1: /* normal loop */
               if (PCMS==2) //if in adpcm non-stream mode, reset the decoder
                  ADPCM_Reset(ch);
               break;
         }
      }

      ch->CA=CA;

      //keep adpcm up to date
      if (sp.ip==0)
         StepDecodeSample<PCMS,true>(ch,CA);
      else
         StepDecodeSample<PCMS,false>(ch,CA);
   }
}

template<s32 ALFOWS>
static void CalcAlfo(ChannelEx* ch)
{
   u32 rv;
   switch(ALFOWS)
   {
      case 0: // Sawtooth
         rv=ch->lfo.state;
         break;

      case 1: // Square
         rv=ch->lfo.state&0x80?255:0;
         break;

      case 2: // Triangle
         rv=(ch->lfo.state&0x7f)^(ch->lfo.state&0x80 ? 0x7F:0);
         rv<<=1;
         break;

      case 3:// Random ! .. not :p
         rv=(ch->lfo.state>>3)^(ch->lfo.state<<3)^(ch->lfo.state&0xE3);
         break;
   }

   ch->lfo.alfo = rv >> ch->lfo.alfo_shft;
}

template<s32 PLFOWS>
static void CalcPlfo(ChannelEx* ch)
{
   u32 rv;

   switch(PLFOWS)
   {
      case 0: // sawtooth
         rv   = ch->lfo.state;
         break;

      case 1: // square
         rv   = ch->lfo.state&0x80?0x80:0x7F;
         break;

      case 2: // triangle
         rv   = (ch->lfo.state&0x7f)^(ch->lfo.state&0x80 ? 0x7F:0);
         rv <<= 1;
         rv   = (u8)(rv-0x80); //2's complement
         break;

      case 3:// random ! .. not :p
         rv   = (ch->lfo.state >> 3) ^ (ch->lfo.state << 3) ^ (ch->lfo.state & 0xE3);
         break;
   }
   ch->lfo.alfo = rv >> ch->lfo.plfo_shft;
}

template<u32 state>
static void AegStep(ChannelEx* ch)
{
   switch(state)
   {
      case EG_ATTACK:
         ch->AEG.volume -= ch->AEG.AttackRate;
         if (ch->AEG.GetValue()<=0)
         {
            ch->AEG.SetValue(0);
            if (!ch->ccd->LPSLNK)
            {
               aeg_printf("[%d]AEG_step : Switching to EG_DECAY1 %d\n",ch->AEG.GetValue());
               SetAegState(ch, EG_DECAY1);
            }
         }
         break;
      case EG_DECAY1:
         //x2
         ch->AEG.volume += ch->AEG.Decay1Rate;
         if (((u32)ch->AEG.GetValue())>=ch->AEG.Decay2Value)
         {
            aeg_printf("[%d]AEG_step : Switching to EG_DECAY2 @ %x\n",ch->AEG.GetValue());

            // No transition to Decay 2 when DL is zero.
            if (settings.aica.AegStepHack && ch->ccd->DL == 0)
               SetAegState(ch, EG_ATTACK);
            else
               SetAegState(ch, EG_DECAY2);

         }
         break;
      case EG_DECAY2:
         //x3
         ch->AEG.volume += ch->AEG.Decay2Rate;
         if (ch->AEG.GetValue()>=0x3FF)
         {
            aeg_printf("[%d]AEG_step : Switching to EG_RELEASE @ %x\n",ch->AEG.GetValue());
            ch->AEG.SetValue(0x3FF);
            SetAegState(ch, EG_RELEASE);
         }
         break;
      case EG_RELEASE: //only on key_off ?
         ch->AEG.volume += ch->AEG.ReleaseRate;

         if (ch->AEG.GetValue()>=0x3FF)
         {
            aeg_printf("[%d]AEG_step : EG_RELEASE End @ %x\n",ch->AEG.GetValue());
            ch->AEG.SetValue(0x3FF); // TODO: mnn, should we do anything about it running wild ?
            StopSlot(ch); /* TODO: Is this ok here? It's a speed optimisation (since the channel is muted) */
         }
         break;
   }
}

template<u32 state>
static void FegStep(ChannelEx* ch)
{
}

#define AicaChannel ChannelEx

AicaChannel AicaChannel::Chans[MAX_CHANNELS];

#define Chans AicaChannel::Chans 

static u32 CalcAegSteps(float t)
{
   const double aeg_allsteps=1024*(1<< EG_SHIFT)-1;

   if (t<0)
      return 0;
   if (t==0)
      return (u32)aeg_allsteps;

   //44.1*ms = samples
   double scnt=44.1*t;
   double steps=aeg_allsteps/scnt;
   return (u32)(steps+0.5);
}

void sgc_Init(void)
{
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

   AEG_STEP_LUT[0]=&AegStep<0>;
   AEG_STEP_LUT[1]=&AegStep<1>;
   AEG_STEP_LUT[2]=&AegStep<2>;
   AEG_STEP_LUT[3]=&AegStep<3>;

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

   for (int i = 0; i < MAX_CHANNELS; i++)
   {
      AEG_ATT_SPS[i]=CalcAegSteps(AEG_Attack_Time[i]);
      AEG_DSR_SPS[i]=CalcAegSteps(AEG_DSR_Time[i]);
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
   SlotRegWrite(&Chans[channel], reg);
}

void ReadCommonReg(u32 reg,bool byte)
{
   switch(reg)
   {
      case 0x2808:
      case 0x2809:
         CommonData->MIEMP=1;
         CommonData->MOEMP=1;
         break;
      case 0x2810: /* LP check */
      case 0x2811:
         {
            u32 chan=CommonData->MSLC;

            CommonData->LP=Chans[chan].loop.looped;
            verify(CommonData->AFSET==0);

            CommonData->EG=Chans[chan].AEG.GetValue(); //AEG is only 10 bits, FEG is 13 bits
            CommonData->SGC=Chans[chan].AEG.state;

            if (! (byte && reg==0x2810))
               Chans[chan].loop.looped=0;
         }
         break;
      case 0x2814: /* CA (slot address */
      case 0x2815:
         {
            u32 chan=CommonData->MSLC;
            CommonData->CA = Chans[chan].CA /*& (~1023)*/; //mmnn??
            //printf("[%d] CA read %d\n",chan,Chans[chan].CA);
         }
         break;
   }
}

void WriteCommonReg8(u32 reg,u32 data)
{
   WriteMemArr(aica_reg,reg,data,1);
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
