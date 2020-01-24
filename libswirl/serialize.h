#pragma once
#include "types.h"

bool rc_serialize(void* src, unsigned int src_size, void** dest, unsigned int* total_size);
bool rc_unserialize(void* src, unsigned int src_size, void** dest, unsigned int* total_size);
bool dc_serialize(void** data, unsigned int* total_size);
bool dc_unserialize(void** data, unsigned int* total_size);

#define REICAST_S(v) rc_serialize(&(v), sizeof(v), data, total_size)
#define REICAST_US(v) rc_unserialize(&(v), sizeof(v), data, total_size)

#define REICAST_SA(v_arr,num) rc_serialize(v_arr, sizeof(v_arr[0])*num, data, total_size)
#define REICAST_USA(v_arr,num) rc_unserialize(v_arr, sizeof(v_arr[0])*num, data, total_size)

struct RegisterStruct;
bool register_serialize(RegisterStruct* regs, size_t size, void** data, unsigned int* total_size);
bool register_unserialize(RegisterStruct* regs, size_t size, void** data, unsigned int* total_size);
