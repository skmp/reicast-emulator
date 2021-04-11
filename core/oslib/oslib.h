#pragma once
#include "types.h"

double os_GetSeconds();

void os_DoEvents();

#ifdef _MSC_VER
#include <intrin.h>
#endif

u32 static INLINE bitscanrev(u32 v)
{
#ifdef __GNUC__
	return 31-__builtin_clz(v);
#else
	unsigned long rv;
	_BitScanReverse(&rv,v);
	return rv;
#endif
}

void os_DebugBreak();
