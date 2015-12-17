#pragma once
#include "types.h"

void os_SetWindowText(const char* text);
void os_MakeExecutable(void* ptr, u32 sz);

void os_DoEvents();
void os_CreateWindow();
void WriteSample(s16 right, s16 left);

#ifdef _MSC_VER
#include <intrin.h>
#endif

u32 static INLINE bitscanrev(u32 v)
{
#ifdef _MSC_VER
	unsigned long rv;
	_BitScanReverse(&rv,v);
	return rv;
#else
	return 31-__builtin_clz(v);
#endif
}

//FIX ME
#define __assume(x)

void os_DebugBreak();
