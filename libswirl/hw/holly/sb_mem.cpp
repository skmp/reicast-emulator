/*
	Dreamcast 'area 0' emulation
	Pretty much all peripheral registers are mapped here

	Routing is mostly handled here, as well as flash/SRAM emulation
*/
#include "types.h"
#include "hw/sh4/sh4_mem.h"

#include "sb_mem.h"
#include "sb.h"
#include "hw/pvr/pvr_mem.h"
#include "hw/gdrom/gdrom_if.h"
#include "hw/aica/aica_if.h"
#include "hw/naomi/naomi.h"
#include "hw/modem/modem.h"

#include "hw/flashrom/flashrom.h"
#include "reios/reios.h"

struct romfiles_t {
	DCFlashChip* aw_rom;
	RomChip* sys_rom;
	DCFlashChip* sys_flash;
	SRamChip* sys_bbsram;

	bool init()
	{
		aw_rom = new DCFlashChip(BIOS_SIZE, BIOS_SIZE / 2);
		sys_rom = new RomChip(BIOS_SIZE);

		sys_flash = new DCFlashChip(FLASH_SIZE);
		sys_bbsram = new SRamChip(BBSRAM_SIZE);

		return true;
	}

	bool term()
	{
		delete aw_rom;
		delete sys_rom;

		delete sys_flash;
		delete sys_bbsram;
	}
} romfiles;

extern bool bios_loaded;

bool LoadRomFiles(const string& root)
{
	DreamcastPlatform dcp = dc_console.platform;
	romfiles.init();

	if (dcp != DCP_ATOMISWAVE)
	{
		if (!romfiles.sys_rom->Load(root, ROM_PREFIX, "%boot.bin;%boot.bin.bin;%bios.bin;%bios.bin.bin" + ROM_NAMES, "bootrom"))
		{
			if (dcp == DCP_DREAMCAST)
			{
				// Dreamcast absolutely needs a BIOS
				msgboxf("Unable to find bios in \n%s\nExiting...", MBX_ICONERROR, root.c_str());
				return false;
			}
		}
		else
			bios_loaded = true;
	}

	bool nvmem_loaded = false;

	if (dcp == DCP_DREAMCAST)
		nvmem_loaded = romfiles.sys_flash->Load(root, ROM_PREFIX, "%nvmem.bin;%flash_wb.bin;%flash.bin;%flash.bin.bin", "nvram");
	else
		nvmem_loaded = romfiles.sys_bbsram->Load(get_game_save_prefix() + ".nvmem");

	if (!nvmem_loaded)
	{
		if (NVR_OPTIONAL)
		{
			printf("flash/nvmem is missing, will create new file...\n");
		}
		else
		{
			msgboxf("Unable to find flash/nvmem in \n%s\nExiting...", MBX_ICONERROR, root.c_str());
			return false;
		}
	}

	if (dcp == DCP_DREAMCAST)
	{
		struct flash_syscfg_block syscfg;
		int res = romfiles.sys_flash->ReadBlock(FLASH_PT_USER, FLASH_USER_SYSCFG, &syscfg);

		if (!res)
		{
			// write out default settings
			memset(&syscfg, 0xff, sizeof(syscfg));
			syscfg.time_lo = 0;
			syscfg.time_hi = 0;
			syscfg.lang = 0;
			syscfg.mono = 0;
			syscfg.autostart = 1;
		}
		u32 time = GetRTC_now();
		syscfg.time_lo = time & 0xffff;
		syscfg.time_hi = time >> 16;
		if (settings.dreamcast.language <= 5)
			syscfg.lang = settings.dreamcast.language;

		romfiles.sys_flash->WriteBlock(FLASH_PT_USER, FLASH_USER_SYSCFG, &syscfg);
	}

	if (dcp == DCP_ATOMISWAVE)
	{
		romfiles.sys_rom->Load(get_game_save_prefix() + ".nvmem2");
	}
	
	return true;
}

void SaveRomFiles(const string& root)
{
	DreamcastPlatform dcp = dc_console.platform;

	if (dcp == DCP_DREAMCAST)
	{
		romfiles.sys_flash->Save(root, ROM_PREFIX, "nvmem.bin", "nvmem");
	}
	else
	{
		romfiles.sys_bbsram->Save(get_game_save_prefix() + ".nvmem");
	}

	if (dcp == DCP_ATOMISWAVE)
	{
		romfiles.aw_rom->Save(get_game_save_prefix() + ".nvmem2");
	}
}

bool LoadHle(const string& root) {
	if (!romfiles.sys_flash->Load(root, ROM_PREFIX, "%nvmem.bin;%flash_wb.bin;%flash.bin;%flash.bin.bin", "nvram")) {
		printf("No nvmem loaded\n");
	}

	return reios_init(romfiles.sys_rom->data, romfiles.sys_flash->data);
}

u32 ReadNVMEM(u32 addr,u32 sz)
{
	if (dc_console.platform == DCP_DREAMCAST)
		return romfiles.sys_flash->Read(addr, sz);
	else
		return romfiles.sys_bbsram->Read(addr,sz);
}

void WriteNVMEM(u32 addr, u32 data, u32 sz)
{
	if (dc_console.platform == DCP_DREAMCAST)
		return romfiles.sys_flash->Write(addr, data, sz);
	else
		return romfiles.sys_bbsram->Write(addr, data, sz);
}

u32 ReadBios(u32 addr, u32 sz)
{
	if (dc_console.platform == DCP_ATOMISWAVE)
		return romfiles.aw_rom->Read(addr, sz);
	else
		return romfiles.sys_rom->Read(addr, sz);

}


void WriteBios(u32 addr, u32 data, u32 sz)
{
	if (dc_console.platform == DCP_ATOMISWAVE)
	{
		if (sz != 1)
		{
			EMUERROR("Invalid access size @%08x data %x sz %d\n", addr, data, sz);
			return;
		}
		romfiles.aw_rom->Write(addr, data, sz);
	}
	else
	{
		EMUERROR4("Write to [Boot ROM] is not possible, addr=%x,data=%x,size=%d", addr, data, sz);
	}
}

//Area 0 mem map
//0x00000000- 0x001FFFFF	:MPX	System/Boot ROM
//0x00200000- 0x0021FFFF	:Flash Memory
//0x00400000- 0x005F67FF	:Unassigned
//0x005F6800- 0x005F69FF	:System Control Reg.
//0x005F6C00- 0x005F6CFF	:Maple i/f Control Reg.
//0x005F7000- 0x005F70FF	:GD-ROM / NAOMI BD Reg.
//0x005F7400- 0x005F74FF	:G1 i/f Control Reg.
//0x005F7800- 0x005F78FF	:G2 i/f Control Reg.
//0x005F7C00- 0x005F7CFF	:PVR i/f Control Reg.
//0x005F8000- 0x005F9FFF	:TA / PVR Core Reg.
//0x00600000- 0x006007FF	:MODEM
//0x00600800- 0x006FFFFF	:G2 (Reserved)
//0x00700000- 0x00707FFF	:AICA- Sound Cntr. Reg.
//0x00710000- 0x0071000B	:AICA- RTC Cntr. Reg.
//0x00800000- 0x00FFFFFF	:AICA- Wave Memory
//0x01000000- 0x01FFFFFF	:Ext. Device
//0x02000000- 0x03FFFFFF*	:Image Area*	2MB

//use unified size handler for registers
//it really makes no sense to use different size handlers on em -> especially when we can use templates :p
template<u32 sz, class T>
T DYNACALL ReadMem_area0(u32 addr)
{
	addr &= 0x01FFFFFF;//to get rid of non needed bits
	const u32 base=(addr>>16);
	//map 0x0000 to 0x01FF to Default handler
	//mirror 0x0200 to 0x03FF , from 0x0000 to 0x03FFF
	//map 0x0000 to 0x001F
	if (base<=0x001F)//	:MPX	System/Boot ROM
	{
		// TODO: only 128K for AW
		return ReadBios(addr,sz);
	}
	//map 0x0020 to 0x0021
	else if ((base>= 0x0020) && (base<= 0x0021)) // :Flash Memory
	{
		return ReadNVMEM(addr&0x1FFFF,sz);
	}
	//map 0x005F to 0x005F
	else if (likely(base==0x005F))
	{
		if ( /*&& (addr>= 0x00400000)*/ (addr<= 0x005F67FF)) // :Unassigned
		{
			EMUERROR2("Read from area0_32 not implemented [Unassigned], addr=%x",addr);
		}
		else if ((addr>= 0x005F7000) && (addr<= 0x005F70FF)) // GD-ROM
		{
			if (dc_console.platform == DCP_NAOMI || dc_console.platform == DCP_ATOMISWAVE)
				return (T)ReadMem_naomi(addr, sz);
			else
				return (T)ReadMem_gdrom(addr, sz);
		}
		else if (likely((addr>= 0x005F6800) && (addr<=0x005F7CFF))) //	/*:PVR i/f Control Reg.*/ -> ALL SB registers now
		{
			return (T)sb_ReadMem(addr,sz);
		}
		else if (likely((addr>= 0x005F8000) && (addr<=0x005F9FFF))) //	:TA / PVR Core Reg.
		{
			if (sz != 4)
				// House of the Dead 2
				return 0;
			return (T)pvr_ReadReg(addr);
		}
	}
	//map 0x0060 to 0x0060
	else if ((base ==0x0060) /*&& (addr>= 0x00600000)*/ && (addr<= 0x006007FF)) //	:MODEM
	{
		//TODO: Modem selection

		if (dc_console.platform == DCP_NAOMI || dc_console.platform == DCP_ATOMISWAVE)
			return (T)libExtDevice_ReadMem_A0_006(addr, sz);
		else
			return (T)ModemReadMem_A0_006(addr, sz);

	}
	//map 0x0060 to 0x006F
	else if ((base >=0x0060) && (base <=0x006F) && (addr>= 0x00600800) && (addr<= 0x006FFFFF)) //	:G2 (Reserved)
	{
		EMUERROR2("Read from area0_32 not implemented [G2 (Reserved)], addr=%x",addr);
	}
	//map 0x0070 to 0x0070
	else if ((base ==0x0070) /*&& (addr>= 0x00700000)*/ && (addr<=0x00707FFF)) //	:AICA- Sound Cntr. Reg.
	{
		return (T) ReadMem_aica_reg(addr,sz);//libAICA_ReadReg(addr,sz);
	}
	//map 0x0071 to 0x0071
	else if ((base ==0x0071) /*&& (addr>= 0x00710000)*/ && (addr<= 0x0071000B)) //	:AICA- RTC Cntr. Reg.
	{
		return (T)ReadMem_aica_rtc(addr,sz);
	}
	//map 0x0080 to 0x00FF
	else if ((base >=0x0080) && (base <=0x00FF) /*&& (addr>= 0x00800000) && (addr<=0x00FFFFFF)*/) //	:AICA- Wave Memory
	{
		ReadMemArrRet(aica_ram.data,addr&ARAM_MASK,sz);
	}
	//map 0x0100 to 0x01FF
	else if ((base >=0x0100) && (base <=0x01FF) /*&& (addr>= 0x01000000) && (addr<= 0x01FFFFFF)*/) //	:Ext. Device
	{
		return (T)libExtDevice_ReadMem_A0_010(addr,sz);
	}
	return 0;
}

template<u32 sz, class T>
void  DYNACALL WriteMem_area0(u32 addr,T data)
{
	addr &= 0x01FFFFFF;//to get rid of non needed bits

	const u32 base=(addr>>16);

	//map 0x0000 to 0x001F

	if ((base <=0x001F) /*&& (addr<=0x001FFFFF)*/)// :MPX System/Boot ROM
	{
		//TODO: Check AW
		//if (base <= 0x0001) // Only 128k BIOS on AtomisWave

		if (dc_console.platform != DCP_ATOMISWAVE )
			WriteBios(addr,data,sz);
		else if (base <= 0x0001)
			WriteBios(addr, data, sz);
	}
	//map 0x0020 to 0x0021
	else if ((base >=0x0020) && (base <=0x0021) /*&& (addr>= 0x00200000) && (addr<= 0x0021FFFF)*/) // Flash Memory
	{
		WriteNVMEM(addr,data,sz);
	}
	//map 0x0040 to 0x005F -> actually, I'll only map 0x005F to 0x005F, b/c the rest of it is unspammed (left to default handler)
	//map 0x005F to 0x005F
	else if ( likely(base==0x005F) )
	{
		if (/*&& (addr>= 0x00400000) */ (addr<= 0x005F67FF)) // Unassigned
		{
			EMUERROR4("Write to area0_32 not implemented [Unassigned], addr=%x,data=%x,size=%d",addr,data,sz);
		}
		else if ((addr>= 0x005F7000) && (addr<= 0x005F70FF)) // GD-ROM
		{
			if (dc_console.platform == DCP_NAOMI || dc_console.platform == DCP_ATOMISWAVE)
			{
				WriteMem_naomi(addr, data, sz);
			}
			else
			{
				WriteMem_gdrom(addr, data, sz);
			}
		}
		else if ( likely((addr>= 0x005F6800) && (addr<=0x005F7CFF)) ) // /*:PVR i/f Control Reg.*/ -> ALL SB registers
		{
			sb_WriteMem(addr,data,sz);
		}
		else if ( likely((addr>= 0x005F8000) && (addr<=0x005F9FFF)) ) // TA / PVR Core Reg.
		{
			verify(sz==4);
			pvr_WriteReg(addr,data);
		}
	}
	//map 0x0060 to 0x0060
	else if ((base ==0x0060) /*&& (addr>= 0x00600000)*/ && (addr<= 0x006007FF)) // MODEM
	{
		if (dc_console.platform == DCP_NAOMI || dc_console.platform == DCP_ATOMISWAVE)
			libExtDevice_WriteMem_A0_006(addr, data, sz);
		else
			ModemWriteMem_A0_006(addr, data, sz);
	}
	//map 0x0060 to 0x006F
	else if ((base >=0x0060) && (base <=0x006F) && (addr>= 0x00600800) && (addr<= 0x006FFFFF)) // G2 (Reserved)
	{
		EMUERROR4("Write to area0_32 not implemented [G2 (Reserved)], addr=%x,data=%x,size=%d",addr,data,sz);
	}
	//map 0x0070 to 0x0070
	else if ((base >=0x0070) && (base <=0x0070) /*&& (addr>= 0x00700000)*/ && (addr<=0x00707FFF)) // AICA- Sound Cntr. Reg.
	{
		WriteMem_aica_reg(addr,data,sz);
		return;
	}
	//map 0x0071 to 0x0071
	else if ((base >=0x0071) && (base <=0x0071) /*&& (addr>= 0x00710000)*/ && (addr<= 0x0071000B)) // AICA- RTC Cntr. Reg.
	{
		WriteMem_aica_rtc(addr,data,sz);
		return;
	}
	//map 0x0080 to 0x00FF
	else if ((base >=0x0080) && (base <=0x00FF) /*&& (addr>= 0x00800000) && (addr<=0x00FFFFFF)*/) // AICA- Wave Memory
	{
		WriteMemArrRet(aica_ram.data,addr&ARAM_MASK,data,sz);
		return;
	}
	//map 0x0100 to 0x01FF
	else if ((base >=0x0100) && (base <=0x01FF) /*&& (addr>= 0x01000000) && (addr<= 0x01FFFFFF)*/) // Ext. Device
	{
		libExtDevice_WriteMem_A0_010(addr,data,sz);
	}
	return;
}

//Init/Res/Term
void sh4_area0_Init()
{
	sb_Init();
}

void sh4_area0_Reset(bool Manual)
{
	sb_Reset(Manual);
}

void sh4_area0_Term()
{
	sb_Term();
}


//AREA 0
_vmem_handler area0_handler;


void map_area0_init()
{

	area0_handler = _vmem_register_handler_Template(ReadMem_area0,WriteMem_area0);
}
void map_area0(u32 base)
{
	verify(base<0xE0);

	_vmem_map_handler(area0_handler,0x00|base,0x01|base);

	//0x0240 to 0x03FF mirrors 0x0040 to 0x01FF (no flashrom or bios)
	//0x0200 to 0x023F are unused
	_vmem_mirror_mapping(0x02|base,0x00|base,0x02);
}
