#pragma once
#include "types.h"

typedef u32  RegReadFP(void* that);
typedef u32  RegReadAddrFP(void* that, u32 addr);

typedef void RegWriteFP(void* that, u32 data);
typedef void RegWriteAddrFP(void* that, u32 addr, u32 data);

/*
	Read Write Const
	D    D     N      -> 0			-> RIO_DATA
	D    F     N      -> WF			-> RIO_WF
	F    F     N      -> RF|WF		-> RIO_FUNC
	D    X     N      -> RO|WF		-> RIO_RO
	F    X     N      -> RF|WF|RO	-> RIO_RO_FUNC
	D    X     Y      -> CONST|RO|WF-> RIO_CONST
	X    F     N      -> RF|WF|WO	-> RIO_WO_FUNC
*/
enum RegStructFlags
{
	//Basic :
	REG_ACCESS_8 = 1,
	REG_ACCESS_16 = 2,
	REG_ACCESS_32 = 4,

	REG_RF = 8,
	REG_WF = 16,
	REG_RO = 32,
	REG_WO = 64,
	REG_CONST = 128,
	REG_NO_ACCESS = REG_RO | REG_WO,
};

enum RegIO
{
	RIO_DATA = 0,
	RIO_WF = REG_WF,
	RIO_FUNC = REG_WF | REG_RF,
	RIO_RO = REG_RO | REG_WF,
	RIO_RO_FUNC = REG_RO | REG_RF | REG_WF,
	RIO_CONST = REG_RO | REG_WF,
	RIO_WO_FUNC = REG_WF | REG_RF | REG_WO,
	RIO_NO_ACCESS = REG_WF | REG_RF | REG_NO_ACCESS
};

struct RegisterStruct
{
	union
	{
		u32 data32;					//stores data of reg variable [if used] 32b
		u16 data16;					//stores data of reg variable [if used] 16b
		u8  data8;					//stores data of reg variable [if used]	8b

		RegReadFP* readFunction;	//stored pointer to reg read function
		RegReadAddrFP* readFunctionAddr;
	};

	union
	{
		RegWriteFP* writeFunction;	//stored pointer to reg write function
		RegWriteAddrFP* writeFunctionAddr;
	};

	void* context;
	u32 flags;					//Access flags !
};