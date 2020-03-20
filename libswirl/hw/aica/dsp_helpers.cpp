/*
	This file is part of libswirl
*/
#include "license/bsd"

#include "dsp_backend.h"

//float format is ?
u16 DYNACALL DSPBackend::PACK(s32 val)
{
	u32 temp;
	int sign, exponent, k;

	sign = (val >> 23) & 0x1;
	temp = (val ^ (val << 1)) & 0xFFFFFF;
	exponent = 0;
	for (k = 0; k < 12; k++)
	{
		if (temp & 0x800000)
			break;
		temp <<= 1;
		exponent += 1;
	}
	if (exponent < 12)
		val = (val << exponent) & 0x3FFFFF;
	else
		val <<= 11;
	val >>= 11;
	val |= sign << 15;
	val |= exponent << 11;

	return (u16)val;
}

s32 DYNACALL DSPBackend::UNPACK(u16 val)
{
	int sign, exponent, mantissa;
	s32 uval;

	sign = (val >> 15) & 0x1;
	exponent = (val >> 11) & 0xF;
	mantissa = val & 0x7FF;
	uval = mantissa << 11;
	if (exponent > 11)
		exponent = 11;
	else
		uval |= (sign ^ 1) << 22;
	uval |= sign << 23;
	uval <<= 8;
	uval >>= 8;
	uval >>= exponent;

	return uval;
}

void DSPBackend::DecodeInst(u32* IPtr, _INST* i)
{
	i->TRA = (IPtr[0] >> 9) & 0x7F;
	i->TWT = (IPtr[0] >> 8) & 0x01;
	i->TWA = (IPtr[0] >> 1) & 0x7F;

	i->XSEL = (IPtr[1] >> 15) & 0x01;
	i->YSEL = (IPtr[1] >> 13) & 0x03;
	i->IRA = (IPtr[1] >> 7) & 0x3F;
	i->IWT = (IPtr[1] >> 6) & 0x01;
	i->IWA = (IPtr[1] >> 1) & 0x1F;

	i->TABLE = (IPtr[2] >> 15) & 0x01;
	i->MWT = (IPtr[2] >> 14) & 0x01;
	i->MRD = (IPtr[2] >> 13) & 0x01;
	i->EWT = (IPtr[2] >> 12) & 0x01;
	i->EWA = (IPtr[2] >> 8) & 0x0F;
	i->ADRL = (IPtr[2] >> 7) & 0x01;
	i->FRCL = (IPtr[2] >> 6) & 0x01;
	i->SHIFT = (IPtr[2] >> 4) & 0x03;
	i->YRL = (IPtr[2] >> 3) & 0x01;
	i->NEGB = (IPtr[2] >> 2) & 0x01;
	i->ZERO = (IPtr[2] >> 1) & 0x01;
	i->BSEL = (IPtr[2] >> 0) & 0x01;

	i->NOFL = (IPtr[3] >> 15) & 1;		//????
	//i->COEF=(IPtr[3]>>9)&0x3f;

	i->MASA = (IPtr[3] >> 9) & 0x3f;	//???
	i->ADREB = (IPtr[3] >> 8) & 0x1;
	i->NXADR = (IPtr[3] >> 7) & 0x1;
}

void DSPBackend::EncodeInst(u32* IPtr, _INST* i)
{
	IPtr[0] = IPtr[1] = IPtr[2] = IPtr[3] = 0;

	/*
	i->TRA = (IPtr[0] >> 9) & 0x7F;
	i->TWT = (IPtr[0] >> 8) & 0x01;
	i->TWA = (IPtr[0] >> 1) & 0x7F;
	*/
	IPtr[0] |= (i->TRA & 0x7F) << 9;
	IPtr[0] |= (i->TWT & 0x01) << 8;
	IPtr[0] |= (i->TWA & 0x7F) << 1;

/*
	i->XSEL = (IPtr[1] >> 15) & 0x01;
	i->YSEL = (IPtr[1] >> 13) & 0x03;
	i->IRA  = (IPtr[1] >>  7) & 0x3F;
	i->IWT  = (IPtr[1] >>  6) & 0x01;
	i->IWA  = (IPtr[1] >>  1) & 0x1F;
*/
	IPtr[1] |= (i->XSEL & 0x01) << 15;
	IPtr[1] |= (i->YSEL & 0x03) << 13;
	IPtr[1] |= (i->IRA  & 0x3F) << 7;
	IPtr[1] |= (i->IWT  & 0x01) << 6;
	IPtr[1] |= (i->IWA  & 0x1F) << 1;

/*
	i->TABLE = (IPtr[2] >> 15) & 0x01;
	i->MWT   = (IPtr[2] >> 14) & 0x01;
	i->MRD   = (IPtr[2] >> 13) & 0x01;
	i->EWT   = (IPtr[2] >> 12) & 0x01;
	i->EWA   = (IPtr[2] >>  8) & 0x0F;
	i->ADRL  = (IPtr[2] >>  7) & 0x01;
	i->FRCL  = (IPtr[2] >>  6) & 0x01;
	i->SHIFT = (IPtr[2] >>  4) & 0x03;
	i->YRL   = (IPtr[2] >>  3) & 0x01;
	i->NEGB  = (IPtr[2] >>  2) & 0x01;
	i->ZERO  = (IPtr[2] >>  1) & 0x01;
	i->BSEL  = (IPtr[2] >>  0) & 0x01;
*/
	IPtr[2] |= (i->TABLE & 0x01) << 15;
	IPtr[2] |= (i->MWT   & 0x01) << 14;
	IPtr[2] |= (i->MRD   & 0x01) << 13;
	IPtr[2] |= (i->EWT   & 0x01) << 12;
	IPtr[2] |= (i->EWA   & 0x0F) <<  8;
	IPtr[2] |= (i->ADRL  & 0x01) <<  7;
	IPtr[2] |= (i->FRCL  & 0x01) <<  6;
	IPtr[2] |= (i->SHIFT & 0x03) <<  4;
	IPtr[2] |= (i->YRL   & 0x01) <<  3;
	IPtr[2] |= (i->NEGB  & 0x01) <<  2;
	IPtr[2] |= (i->ZERO  & 0x01) <<  1;
	IPtr[2] |= (i->BSEL  & 0x01) <<  0;

	//i->COEF=(IPtr[3]>>9)&0x3f;
	/*
	i->NOFL  = (IPtr[3] >> 15) & 1;		//????
	i->MASA  = (IPtr[3] >> 9) & 0x3f;	//???
	i->ADREB = (IPtr[3] >> 8) & 0x1;
	i->NXADR = (IPtr[3] >> 7) & 0x1;
	*/

	IPtr[3] |= (i->NOFL  & 0x01) << 15;
	IPtr[3] |= (i->MASA  & 0x3f) <<  9;
	IPtr[3] |= (i->ADREB & 0x01) <<  8;
	IPtr[3] |= (i->NXADR & 0x01) <<  7;
}
