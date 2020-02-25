#pragma once
/*
	This file is part of libswirl
*/
#include "license/bsd"

#include "types.h"

#pragma pack(push, 1) 
union RegionArrayEntryControl {
    struct {
        u32 res0 : 2;
        u32 tilex : 6;
        u32 tiley : 6;
        u32 res1 : 14;
        u32 no_writeout : 1;
        u32 pre_sort : 1;
        u32 z_keep : 1;
        u32 last_region : 1;
    };
    u32 full;
};

typedef u32 pvr32addr_t;

union ListPointer {
    struct
    {
        u32 pad0 : 2;
        u32 ptr_in_words : 22;
        u32 pad1 : 7;
        u32 empty : 1;
    };
    u32 full;
};

union ObjectListEntry {
    struct {
        u32 pad0 : 31;
        u32 is_not_triangle_strip : 1;
    };

    struct {
        u32 pad1 : 29;
        u32 type : 3;
    };

    struct {
        u32 param_offs_in_words : 21;
        u32 skip : 3;
        u32 shadow : 1;
        u32 mask : 6;
    } tstrip;

    struct {
        u32 param_offs_in_words : 21;
        u32 skip : 3;
        u32 shadow : 1;
        u32 prims : 4;
    } tarray;

    struct {
        u32 param_offs_in_words : 21;
        u32 skip : 3;
        u32 shadow : 1;
        u32 prims : 4;
    } qarray;

    struct {
        u32 pad3 : 2;
        u32 next_block_ptr_in_words : 22;
        u32 pad4 : 4;
        u32 end_of_list : 1;
    } link;

    u32 full;
};

#pragma pack(pop)