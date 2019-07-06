#pragma once

#include "types.h"
#include "oslib/context.h"


enum DreamcastPlatform
{
	DCP_DREAMCAST,		/* Works, for the most part */
	DCP_DEV_UNIT,		/* This is missing hardware */
	DCP_NAOMI,			/* Works, for the most part */
	DCP_NAOMI2,			/* Needs to be done, 2xsh4 + 2xpvr + custom TNL */
	DCP_ATOMISWAVE,		/* Needs to be done, DC-like hardware with possibly more ram */
	DCP_HIKARU,			/* Needs to be done, 2xsh4, 2x aica , custom vpu */
	DCP_AURORA			/* Needs to be done, Uses newer 300 mhz sh4 + 150 mhz pvr mbx SoC */
};

struct dreamcast_console_t
{
	u32 ram_mask;
	u32 vram_mask;
	u32 aram_mask;
	DreamcastPlatform platform;

	u32 ram_size;
	u32 vram_size;
	u32 aram_size;
	u32 bios_size;
	u32 bbsram_size;

	string rom_prefix;
	string rom_names;

	bool has_flash;
	bool nvr_optional;
};

extern dreamcast_console_t dc_console;

void dc_loadstate();
void dc_savestate();
void dc_stop();
void dc_reset();
void dc_resume();
void dc_term();
int dc_start_game(DreamcastPlatform dcp, const char* path);

void* dc_run(void*);

void dc_request_reset();

bool dc_handle_fault(unat addr, rei_host_context_t* ctx);

// TODO: rename these
int reicast_init(int argc, char* argv[]);

#define RAM_SIZE (dc_console.ram_size)
#define VRAM_SIZE (dc_console.vram_size)
#define ARAM_SIZE (dc_console.aram_size)
#define BIOS_SIZE (dc_console.bios_size)
#define FLASH_SIZE (dc_console.bbsram_size)
#define BBSRAM_SIZE (dc_console.bbsram_size)

#define ROM_PREFIX dc_console.rom_prefix
#define ROM_NAMES dc_console.rom_names
#define NVR_OPTIONAL dc_console.nvr_optional


#define RAM_MASK	(dc_console.ram_mask)
#define VRAM_MASK	(dc_console.vram_mask)
#define ARAM_MASK	(dc_console.aram_mask)
#define BIOS_MASK	(BIOS_SIZE-1)

#ifdef FLASH_SIZE
#define FLASH_MASK	(FLASH_SIZE-1)
#endif

#ifdef BBSRAM_SIZE
#define BBSRAM_MASK	(BBSRAM_SIZE-1)
#endif
