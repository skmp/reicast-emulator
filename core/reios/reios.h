#include "types.h"

extern char reios_hardware_id[17];
extern char reios_maker_id[17];
extern char reios_device_info[17];
extern char reios_area_symbols[9];
extern char reios_peripherals[9];
extern char reios_product_number[11];
extern char reios_product_version[7];
extern char reios_releasedate[17];
extern char reios_boot_filename[17];
extern char reios_software_company[17];
extern char reios_software_name[129];

bool reios_init(u8* rom, u8* flash);

void reios_reset();

void reios_term();

void DYNACALL reios_trap(u32 op);

const char* reios_locate_ip(void);

bool reios_locate_bootfile(const char* bootfile);

typedef void hook_fp();
u32 hook_addr(hook_fp* fn);

#define REIOS_OPCODE 0x085B
