#pragma once
#include "types.h"

//main system mem
extern VArray2 mem_b;

#include "hw/mem/_vmem.h"
#include "modules/mmu.h"

u8 DYNACALL ReadMem8(u32 Address);
u16 DYNACALL ReadMem16(u32 Address);
u16 DYNACALL IReadMem16(u32 Address);
u32 DYNACALL ReadMem32(u32 Address);
u64 DYNACALL ReadMem64(u32 Address);

void DYNACALL WriteMem8(u32 addr,u8 data);
void DYNACALL WriteMem16(u32 addr,u16 data);
void DYNACALL WriteMem32(u32 addr,u32 data);
void DYNACALL WriteMem64(u32 addr,u64 data);

#define ReadMem8_nommu _vmem_ReadMem8
#define ReadMem16_nommu _vmem_ReadMem16
#define IReadMem16_nommu _vmem_IReadMem16
#define ReadMem32_nommu _vmem_ReadMem32

#define WriteMem8_nommu _vmem_WriteMem8
#define WriteMem16_nommu _vmem_WriteMem16
#define WriteMem32_nommu _vmem_WriteMem32

void WriteMemBlock_ptr(u32 dst,u32* src,u32 size);
void WriteMemBlock_nommu_ptr(u32 dst,u32* src,u32 size);
void WriteMemBlock_nommu_sq(u32 dst,u32* src);
void WriteMemBlock_nommu_dma(u32 dst,u32 src,u32 size);
//Init/Res/Term
void mem_Init();
void mem_Term();
void mem_Reset(bool Manual);
void mem_map_default();

//Generic read/write functions for debugger
bool ReadMem_DB(u32 addr,u32& data,u32 size );
bool WriteMem_DB(u32 addr,u32 data,u32 size );

//Get pointer to ram area , 0 if error
//For debugger(gdb) - dynarec
u8* GetMemPtr(u32 Addr,u32 size);

//Get infomation about an area , eg ram /size /anything
//For dynarec - needs to be done
struct MemInfo
{
	//MemType:
	//Direct ptr   , just read/write to the ptr
	//Direct call  , just call for read , ecx=data on write (no address)
	//Generic call , ecx=addr , call for read , edx=data for write
	u32 MemType;
	
	//todo
	u32 Flags;

	void* read_ptr;
	void* write_ptr;
};

void GetMemInfo(u32 addr,u32 size,MemInfo* meminfo);

bool IsOnRam(u32 addr);


u32 GetRamPageFromAddress(u32 RamAddress);


bool LoadRomFiles(const string& root);
void SaveRomFiles(const string& root);
bool LoadHle(const string& root);
