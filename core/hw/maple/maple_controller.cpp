#include "hw/maple/maple_controller.h"

u16 kcode[MAPLE_NUM_PORTS] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
u8 rt[MAPLE_NUM_PORTS] = {0, 0, 0, 0};
u8 lt[MAPLE_NUM_PORTS] = {0, 0, 0, 0};
u32 vks[MAPLE_NUM_PORTS];
s8 joyx[MAPLE_NUM_PORTS];
s8 joyy[MAPLE_NUM_PORTS];
bool port_enabled[MAPLE_NUM_PORTS] = {1, 0, 0, 0};
