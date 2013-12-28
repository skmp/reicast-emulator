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
//#include "naomi/naomi.h"

#include "types.h"
#include "hw/flashrom/flashrom.h"

RomChip sys_rom(BIOS_SIZE);

#ifdef FLASH_SIZE
DCFlashChip sys_nvmem(FLASH_SIZE);
#endif

#ifdef BBSRAM_SIZE
SRamChip sys_nvmem(BBSRAM_SIZE);
#endif

bool LoadRomFiles(const string& root)
{
	if (!sys_rom.Load(root, ROM_PREFIX, "%boot.bin;%boot.bin.bin;%bios.bin;%bios.bin.bin" ROM_NAMES, "bootrom"))
	{
		msgboxf("Unable to find bios in \n%s\nExiting...", MBX_ICONERROR, root.c_str());
		return false;
	}
	if (!sys_nvmem.Load(root, ROM_PREFIX, "%nvmem.bin;%flash_wb.bin;%flash.bin;%flash.bin.bin", "nvram"))
	{
		if (NVR_OPTIONAL)
		{
			printf("flash/nvmem is missing, will create new file...");
		}
		else
		{
			msgboxf("Unable to find flash/nvmem in \n%s\nExiting...", MBX_ICONERROR, root.c_str());
			return false;
		}
	}

	return true;
}

void SaveRomFiles(const string& root)
{
	sys_nvmem.Save(root, ROM_PREFIX, "nvmem.bin", "nvmem");
}

#if (DC_PLATFORM == DC_PLATFORM_NORMAL) || (DC_PLATFORM == DC_PLATFORM_DEV_UNIT) || (DC_PLATFORM == DC_PLATFORM_NAOMI) || (DC_PLATFORM == DC_PLATFORM_NAOMI2)

u32 ReadBios(u32 addr, u32 size) { return sys_rom.Read(addr, size); }
void WriteBios(u32 addr, u32 data, u32 size) { EMUERROR4("Write to Boot ROM is not possible - addr=%x,data=%x,size=%d", addr, data, size); }

u32 ReadFlash(u32 addr, u32 size) { return sys_nvmem.Read(addr, size); }
void WriteFlash(u32 addr, u32 data, u32 size) { sys_nvmem.Write(addr, data, size); }

#elif (DC_PLATFORM == DC_PLATFORM_ATOMISWAVE)
	u32 ReadFlash(u32 addr, u32 size) { EMUERROR3("Read from Flash ROM is not possible - addr=%x, size=%d", addr, size); return 0; }
	void WriteFlash(u32 addr, u32 data, u32 size) { EMUERROR4("Write to Flash ROM is not possible - addr=%x, data=%x, size=%d", addr, data, size); }

	u32 ReadBios(u32 addr, u32 size)
	{
		if (!(addr&0x10000)) //upper 64 kb is flashrom
		{
			return sys_rom.Read(addr, size);
		}
		else
		{
			return sys_nvmem.Read(addr, size);
		}
	}

	void WriteBios(u32 addr, u32 data, u32 size)
	{
		if (!(addr&0x10000)) //upper 64 kb is flashrom
		{
			EMUERROR4("Write to Boot ROM is not possible - addr=%x, data=%x, size=%d", addr, data, size);
		}
		else
		{
			sys_nvmem.Write(addr, data, size);
		}
	}

#else
#error unknown flash
#endif

//Area 0 mem map
//0x00000000- 0x001FFFFF	:MPX   System/Boot ROM
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
template<u32 size, class T>
T DYNACALL ReadMem_area0(u32 addr)
{
	addr &= 0x01FFFFFF;//to get rid of non needed bits
	const u32 base=(addr>>16);
	//Map 0x0000 to 0x01FF to Default handler
	//Mirror 0x0200 to 0x03FF, from 0x0000 to 0x03FFF
	//Map 0x0000 to 0x001F
	if (base<=0x001F)// MPX   System/Boot ROM
	{
		return ReadBios(addr, size);
	}
	//Map 0x0020 to 0x0021
	else if ((base>= 0x0020) && (base<= 0x0021)) // :Flash Memory
	{
		return ReadFlash(addr&0x1FFFF, size);
	}
	//Map 0x005F to 0x005F
	else if (likely(base==0x005F))
	{
		if ( /*&& (addr>= 0x00400000)*/ (addr<= 0x005F67FF)) // :Unassigned
		{
			EMUERROR2("Read from area0_32 not implemented [Unassigned] - addr=%x",addr);
		}
		else if ((addr>= 0x005F7000) && (addr<= 0x005F70FF)) // GD-ROM
		{
			//EMUERROR3("Read from area0_32 not implemented [GD-ROM], addr=%x, size=%d", addr, size);
	#if defined(BUILD_NAOMI) || defined(BUILD_ATOMISWAVE)
			return (T)ReadMem_naomi(addr, size);
	#else
			return (T)ReadMem_gdrom(addr, size);
	#endif
		}
		else if (likely((addr>= 0x005F6800) && (addr<=0x005F7CFF))) // /*PVR i/f Control Reg.*/ -> ALL SB registers now
		{
			//EMUERROR2("Read from area0_32 not implemented [PVR i/f Control Reg], addr=%x", addr);
			return (T)sb_ReadMem(addr, size);
		}
		else if (likely((addr>= 0x005F8000) && (addr<=0x005F9FFF))) // TA / PVR Core Reg.
		{
			//EMUERROR2("Read from area0_32 not implemented [TA / PVR Core Reg], addr=%x", addr);
			verify(size == 4);
			return (T)pvr_ReadReg(addr);
		}
	}
	//map 0x0060 to 0x0060
	else if ((base ==0x0060) /*&& (addr>= 0x00600000)*/ && (addr<= 0x006007FF)) //	:MODEM
	{
		return (T)libExtDevice_ReadMem_A0_006(addr, size);
		//EMUERROR2("Read from area0_32 not implemented [MODEM], addr=%x",addr);
	}
	//map 0x0060 to 0x006F
	else if ((base >=0x0060) && (base <=0x006F) && (addr>= 0x00600800) && (addr<= 0x006FFFFF)) // G2 (Reserved)
	{
		EMUERROR2("Read from area0_32 not implemented [G2 (Reserved)] - addr=%x", addr);
	}
	//map 0x0070 to 0x0070
	else if ((base ==0x0070) /*&& (addr>= 0x00700000)*/ && (addr<=0x00707FFF)) // AICA- Sound Control Reg.
	{
		//EMUERROR2("Read from area0_32 not implemented [AICA - Sound Control Reg] - addr=%x", addr);
		return (T)ReadMem_aica_reg(addr, size); //libAICA_ReadReg(addr, size);
	}
	//map 0x0071 to 0x0071
	else if ((base ==0x0071) /*&& (addr>= 0x00710000)*/ && (addr<= 0x0071000B)) // AICA- RTC Control Reg.
	{
		//EMUERROR2("Read from area0_32 not implemented [AICA - RTC Control Reg] - addr=%x", addr);
		return (T)ReadMem_aica_rtc(addr, size);
	}
	//map 0x0080 to 0x00FF
	else if ((base >=0x0080) && (base <=0x00FF) /*&& (addr>= 0x00800000) && (addr<=0x00FFFFFF)*/) // AICA - Wave Memory
	{
		//EMUERROR2("Read from area0_32 not implemented [AICA - Wave Memory], addr=%x",addr);
		//return (T)libAICA_ReadMem_aica_ram(addr, size);
		ReadMemArrRet(aica_ram.data, addr&ARAM_MASK, size);
	}
	//map 0x0100 to 0x01FF
	else if ((base >=0x0100) && (base <=0x01FF) /*&& (addr>= 0x01000000) && (addr<= 0x01FFFFFF)*/) //	:Ext. Device
	{
	//	EMUERROR2("Read from area0_32 not implemented [Ext. Device], addr=%x",addr);
		return (T)libExtDevice_ReadMem_A0_010(addr, size);
	}
	return 0;
}

template<u32 size, class T>
void  DYNACALL WriteMem_area0(u32 addr,T data)
{
	addr &= 0x01FFFFFF;//to get rid of non needed bits

	const u32 base=(addr>>16);

	//map 0x0000 to 0x001F
	if ((base >=0x0000) && (base <=0x001F) /*&& (addr<=0x001FFFFF)*/)// MPX System/Boot ROM
	{
		//EMUERROR4("Write to MPX System/Boot ROM is not possible, addr=%x, data=%x, size=%d", addr, data, size);
		WriteBios(addr, data, size);
	}
	//map 0x0020 to 0x0021
	else if ((base >=0x0020) && (base <=0x0021) /*&& (addr>= 0x00200000) && (addr<= 0x0021FFFF)*/) // Flash Memory
	{
		//EMUERROR4("Write to Flash Memory, size?! - addr=%x, data=%x, size=%d", addr, data, size);
		WriteFlash(addr, data, size);
	}
	//map 0x0040 to 0x005F -> actually, I'll only map 0x005F to 0x005F, b/c the rest of it is unspammed (left to default handler)
	//map 0x005F to 0x005F
	else if (likely(base==0x005F))
	{
		if (/*&& (addr>= 0x00400000) */ (addr<= 0x005F67FF)) // Unassigned
		{
			EMUERROR4("Write to area0_32 not implemented [Unassigned] - addr=%x, data=%x, size=%d", addr, data, size);
		}
		else if ((addr>= 0x005F7000) && (addr<= 0x005F70FF)) // GD-ROM
		{
			//EMUERROR4("Write to area0_32 not implemented [GD-ROM], addr=%x, data=%x, size=%d", addr, data, size);
#if defined(BUILD_NAOMI) || defined(BUILD_ATOMISWAVE)
			WriteMem_naomi(addr, data, size);
#else
			WriteMem_gdrom(addr, data, size);
#endif
		}
		else if (likely((addr>= 0x005F6800) && (addr<=0x005F7CFF))) // /*PVR i/f Control Reg.*/ -> ALL SB registers
		{
			//EMUERROR4("Write to area0_32 not implemented [PVR i/f Control Reg] - addr=%x, data=%x, size=%d", addr, data, size);
			sb_WriteMem(addr, data, size);
		}
		else if (likely((addr>= 0x005F8000) && (addr<=0x005F9FFF))) // TA / PVR Core Reg.
		{
			//EMUERROR4("Write to area0_32 not implemented [TA / PVR Core Reg] - addr=%x, data=%x, size=%d", addr, data, size);
			verify(size == 4);
			pvr_WriteReg(addr, data);
		}
	}
	//map 0x0060 to 0x0060
	else if ((base ==0x0060) /*&& (addr>= 0x00600000)*/ && (addr<= 0x006007FF)) // MODEM
	{
		//EMUERROR4("Write to area0_32 not implemented [MODEM], addr=%x, data=%x, size=%d", addr, data, size);
		libExtDevice_WriteMem_A0_006(addr, data, size);
	}
	//map 0x0060 to 0x006F
	else if ((base >=0x0060) && (base <=0x006F) && (addr>= 0x00600800) && (addr<= 0x006FFFFF)) // G2 (Reserved)
	{
		EMUERROR4("Write to area0_32 not implemented [G2 (Reserved)] - addr=%x, data=%x, size=%d", addr, data, size);
	}
	//map 0x0070 to 0x0070
	else if ((base >=0x0070) && (base <=0x0070) /*&& (addr>= 0x00700000)*/ && (addr<=0x00707FFF)) // AICA - Sound Control Reg.
	{
		//EMUERROR4("Write to area0_32 not implemented [AICA - Sound Control Reg] - addr=%x, data=%x, size=%d", addr, data, size);
		WriteMem_aica_reg(addr, data, size);
		return;
	}
	//map 0x0071 to 0x0071
	else if ((base >=0x0071) && (base <=0x0071) /*&& (addr>= 0x00710000)*/ && (addr<= 0x0071000B)) // AICA - RTC Control Reg.
	{
		//EMUERROR4("Write to area0_32 not implemented [AICA - RTC Control Reg] - addr=%x, data=%x, size=%d", addr, data, size);
		WriteMem_aica_rtc(addr, data, size);
		return;
	}
	//map 0x0080 to 0x00FF
	else if ((base >=0x0080) && (base <=0x00FF) /*&& (addr>= 0x00800000) && (addr<=0x00FFFFFF)*/) // AICA - Wave Memory
	{
		//EMUERROR4("Write to area0_32 not implemented [AICA - Wave Memory] - addr=%x, data=%x, size=%d", addr, data, size);
		//aica_writeram(addr, data, size);
		WriteMemArrRet(aica_ram.data, addr&ARAM_MASK, data, size);
		return;
	}
	//map 0x0100 to 0x01FF
	else if ((base >=0x0100) && (base <=0x01FF) /*&& (addr>= 0x01000000) && (addr<= 0x01FFFFFF)*/) // Ext. Device
	{
		//EMUERROR4("Write to area0_32 not implemented [Ext. Device] - addr=%x, data=%x, size=%d", addr, data, size);
		libExtDevice_WriteMem_A0_010(addr, data, size);
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
