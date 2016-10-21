#pragma once
#include <stdint.h>

enum OpcodeAccessFlags
{
	//Explicit registers
	OAF_RN,
	OAF_RM,
	OAF_FN,
	OAF_FM,

	//Implicit registers
	OAF_SR_T,
	OAF_SR_S,
	OAF_FPSCR,
	OAF_PC,
	
	//Other storage
	OAF_MEM,
	OAF_SQWB,
};
