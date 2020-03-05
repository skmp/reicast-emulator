#pragma once
#include "types.h"

typedef unsigned int pvraddr_t;
typedef unsigned int pvr64addr_t;
typedef unsigned int sh4addr_t;

void lxd_ta_write( unsigned char *buf, uint32_t length );
void lxd_ta_write_burst( sh4addr_t addr, unsigned char *data );
void lxd_ta_reset();
void lxd_ta_init(u8* vram);