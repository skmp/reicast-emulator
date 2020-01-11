#pragma once

#include "types.h"

extern u32 CCN_QACR_TR[2];

template<u32 idx>
void CCN_QACR_write(void* context, u32 addr, u32 value);
