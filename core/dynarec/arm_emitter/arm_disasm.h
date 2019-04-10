/*
 *	disasm.h,	a horribly static yet (hopefully) correct disassembler
 *
 */
#pragma once

namespace ARM
{
	
	
	
	inline static void armdis_cc(u32 cond, char *ccbuff)	// Length is always 8 for our static'{n,m}ess
	{
		switch(cond)
		{	
		case EQ:	swprintf(ccbuff, "EQ");	return;
		case NE:	swprintf(ccbuff, "NE");	return;
		case CS:	swprintf(ccbuff, "CS");	return;
		case CC:	swprintf(ccbuff, "CC");	return;
		case MI:	swprintf(ccbuff, "MI");	return;
		case PL:	swprintf(ccbuff, "PL");	return;
		case VS:	swprintf(ccbuff, "VS");	return;
		case VC:	swprintf(ccbuff, "VC");	return;

		case HI:	swprintf(ccbuff, "HI");	return;
		case LS:	swprintf(ccbuff, "LS");	return;

		case GE:	swprintf(ccbuff, "GE");	return;
		case LT:	swprintf(ccbuff, "LT");	return;
		case GT:	swprintf(ccbuff, "GT");	return;
		case LE:	swprintf(ccbuff, "LE");	return;

		case AL:	return;		// swprintf(ccbuff, "AL");	-- ALways doesn't need to be specified

		case UC:				// 
		default:	return;		// DIE
		}
	}


	inline static void armdis_dp(u32 dpop, char *dpbuff)		// Length is always 8 ...
	{
		switch(dpop)
		{
		case DP_AND:	swprintf(dpbuff, "AND");	return;
		case DP_EOR:	swprintf(dpbuff, "EOR");	return;
		case DP_SUB:	swprintf(dpbuff, "SUB");	return;
		case DP_RSB:	swprintf(dpbuff, "RSB");	return;
		case DP_ADD:	swprintf(dpbuff, "ADD");	return;
		case DP_ADC:	swprintf(dpbuff, "ADC");	return;
		case DP_SBC:	swprintf(dpbuff, "SBC");	return;
		case DP_RSC:	swprintf(dpbuff, "RSC");	return;
		case DP_TST:	swprintf(dpbuff, "TST");	return;
		case DP_TEQ:	swprintf(dpbuff, "TEQ");	return;
		case DP_CMP:	swprintf(dpbuff, "CMP");	return;
		case DP_CMN:	swprintf(dpbuff, "CMN");	return;
		case DP_ORR:	swprintf(dpbuff, "ORR");	return;
		case DP_MOV:	swprintf(dpbuff, "MOV");	return;
		case DP_BIC:	swprintf(dpbuff, "BIC");	return;
		case DP_MVN:	swprintf(dpbuff, "MVN");	return;
		}
	}



	inline static void armdis(u32 op, char *disbuf, u32 len=512)
	{
		char ipref[8]={0}, isuff[8]={0}, icond[8]={0} ;


		//	u32 uOP = ((op>>12)&0xFF00) | ((op>>4)&255) ;

		u32 uCC = ((op>>28) & 0x0F) ;		// 

		u32 uO1 = ((op>>25) & 0x07) ;		// 
		u32 uO2 = ((op>> 4) & 0x01) ;		// 
		u32 uC1 = ((op>>21) & 0x0F) ;		// 
		u32 uC2 = ((op>> 5) & 0x07) ;		// 
		u32 uSB = ((op>>20) & 0x01) ;		// Sign Change Bit


		/*
		if (uCC == UC) {

			wprintf ("DBG armdis has UC instruction %X\n", op);
			swprintf (disbuf, "UNCONDITIONAL / UNHANDLED INSTRUCTION");
			return;

		}


		if (uCC != AL) {
			armdis_cc(uCC,isuff);
		}


		if (uO1 == 0) 
		{

			if (uO2 == 0) {

				if ((uC1 & 0xC) == 8) {
					wprintf ("DBG armdis 0:0 10xx misc instruction \n", uCC);
					swprintf (disbuf, "UNHANDLED INSTRUCTION 0:");
					return;
				}

				// DP imm.shift			



			}

			else if (uO2 == 1) {

				swprintf (disbuf, "UNHANDLED INSTRUCTION 0:");
			}

		}
		else if (uO1 == 1) {

			swprintf (disbuf, "UNHANDLED INSTRUCTION 1:");
		}
		else if (uO1 == 2) {

			swprintf (disbuf, "UNHANDLED INSTRUCTION 2:");
		}
		else if (uO1 == 3) {

			swprintf (disbuf, "UNHANDLED INSTRUCTION 3:");
		}
		else if (uO1 == 4) {

			swprintf (disbuf, "UNHANDLED INSTRUCTION 4:");
		}
		else if (uO1 == 5) {

			swprintf (disbuf, "UNHANDLED INSTRUCTION 5:");
		}
		else if (uO1 == 6) {

			swprintf (disbuf, "UNHANDLED INSTRUCTION 6:");
		}
		else if (uO1 == 7) {

			swprintf (disbuf, "UNHANDLED INSTRUCTION 7:");
		}
		else if (uO1 == 8) {

			swprintf (disbuf, "UNHANDLED INSTRUCTION 8:");
		}
		else if (uO1 == 9) {

			swprintf (disbuf, "UNHANDLED INSTRUCTION 9:");
		}
		else if (uO1 == 10) {

			swprintf (disbuf, "UNHANDLED INSTRUCTION 10:");
		}
		else if (uO1 == 11) {

			swprintf (disbuf, "UNHANDLED INSTRUCTION 11:");
		}
		else if (uO1 == 12) {

			swprintf (disbuf, "UNHANDLED INSTRUCTION 12:");
		}
		else if (uO1 == 13) {

			swprintf (disbuf, "UNHANDLED INSTRUCTION 13:");
		}
		else if (uO1 == 14) {

			swprintf (disbuf, "UNHANDLED INSTRUCTION 14:");
		}
		else if (uO1 == 15)	{

			swprintf (disbuf, "UNHANDLED INSTRUCTION 15:");
		}
		else {

			swprintf (disbuf, "INVALID INSTRUCTION");
		}
		*/
		if (!uC1 && uO1==5) {
		 //B
			char tmp[20];
			tmp[0]='\0';
			armdis_cc(uCC, tmp);
			swprintf(disbuf, "B%s %08X", tmp, (op&0xffffff)<<2);
		} else {
			armdis_dp(uC1, disbuf);
			char tmp[20];
			tmp[0]='\0';
			armdis_cc(uCC, tmp);
			if (tmp[0]) {
				wcscat(disbuf, ".\0");
				wcscat(disbuf, tmp);
			}
			if (uSB) wcscat(disbuf, ".S\0");
			bool shifter=false;
			switch (uO1) {
				case 0:
					// reg_reg
					swprintf(tmp,"\tr%d, r%d", (op>>12)&0x0f, (op)&0x0f);
					shifter=true;
					break;
				case 1:
					// reg_imm
					swprintf(tmp,"\tr%d, %04X", (op>>16)&0x0f, (op)&0xffff);
					break;
				default:
					shifter=true;
					swprintf(tmp, " 0x%0X", uO1);
			}
			wcscat(disbuf, tmp);
			wchar_t* ShiftOpStr[]={"LSL","LSR","ASR","ROR"};
			u32 shiftop=(op>>5)&0x3;
			u32 shiftoptype=(op>>4)&0x1;
			u32 shiftopreg=(op>>8)&0xf;
			u32 shiftopimm=(op>>7)&0x1f;
			if (shifter) {
				if (!shiftop && !shiftoptype && !shiftopimm)
				{
					//nothing
				} else {
					if ((shiftop==1) || (shiftop==2)) if (!shiftoptype) if (!shiftopimm) shiftopimm=32;
					swprintf(tmp, " ,%s %s%d", ShiftOpStr[shiftop], (shiftoptype)?" r":" #", (shiftoptype)?shiftopreg:shiftopimm);
					wcscat(disbuf, tmp);
				}
			}
		}
	}







};

