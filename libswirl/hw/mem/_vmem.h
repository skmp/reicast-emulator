/*
	This file is part of libswirl
*/
#include "license/bsd"


#pragma once
#include "types.h"

enum VMemType {
	MemType4GB,
	MemType512MB,
	MemTypeError
};

struct vmem_mapping {
	u32 start_address, end_address;
	unsigned memoffset, memsize;
	bool allow_writes;
};

// Platform specific vmemory API
// To initialize (maybe) the vmem subsystem
VMemType vmem_platform_init(void **vmem_base_addr, void **sh4rcb_addr);
// To reset the on-demand allocated pages.
void vmem_platform_reset_mem(void *ptr, unsigned size_bytes);
// To handle a fault&allocate an ondemand page.
void vmem_platform_ondemand_page(void *address, unsigned size_bytes);
// To create the mappings in the address space.
void vmem_platform_create_mappings(const vmem_mapping *vmem_maps, unsigned nummaps);
// Just tries to wipe as much as possible in the relevant area.
void vmem_platform_destroy();
// Given a block of data in the .text section, prepares it for JIT action.
// both code_area and size are page aligned. Returns success.
bool vmem_platform_prepare_jit_block(void *code_area, unsigned size, void **code_area_rwx);
// Same as above but uses two address spaces one with RX and RW protections.
// Note: this function doesnt have to be implemented, it's a fallback for the above one.
bool vmem_platform_prepare_jit_block(void *code_area, unsigned size, void **code_area_rw, uintptr_t *rx_offset);
// This might not need an implementation (ie x86/64 cpus).
void vmem_platform_flush_cache(void *icache_start, void *icache_end, void *dcache_start, void *dcache_end);

// Note: if you want to disable vmem magic in any given platform, implement the
// above functions as empty functions and make vmem_platform_init return MemTypeError.


//Typedef's

//ReadMem 
typedef u8 DYNACALL _vmem_ReadMem8FP(void* ctx, u32 Address);
typedef u16 DYNACALL _vmem_ReadMem16FP(void* ctx, u32 Address);
typedef u32 DYNACALL _vmem_ReadMem32FP(void* ctx, u32 Address);
//WriteMem
typedef void DYNACALL _vmem_WriteMem8FP(void* ctx, u32 Address, u8 data);
typedef void DYNACALL _vmem_WriteMem16FP(void* ctx, u32 Address, u16 data);
typedef void DYNACALL _vmem_WriteMem32FP(void* ctx, u32 Address, u32 data);

//our own handle type :)
typedef u32 _vmem_handler;



//handler tables
#define HANDLER_MAX 0x1F
#define HANDLER_COUNT (HANDLER_MAX+1)

extern _vmem_ReadMem8FP* _vmem_RF8[HANDLER_COUNT];
extern _vmem_WriteMem8FP* _vmem_WF8[HANDLER_COUNT];

extern _vmem_ReadMem16FP* _vmem_RF16[HANDLER_COUNT];
extern _vmem_WriteMem16FP* _vmem_WF16[HANDLER_COUNT];

extern _vmem_ReadMem32FP* _vmem_RF32[HANDLER_COUNT];
extern _vmem_WriteMem32FP* _vmem_WF32[HANDLER_COUNT];

extern void* _vmem_CTX[HANDLER_COUNT];

extern void* _vmem_MemInfo_ptr[0x100];

void dump_sh4_registers();

template<typename T, typename Trv>
INLINE Trv DYNACALL _vmem_readt(u32 addr)
{
	const u32 sz = sizeof(T);

	u32   page = addr >> 24;	//1 op, shift/extract
	unat  iirf = (unat)_vmem_MemInfo_ptr[page]; //2 ops, insert + read [vmem table will be on reg ]
	void* ptr = (void*)(iirf & ~HANDLER_MAX);     //2 ops, and // 1 op insert

	if (likely(ptr != 0))
	{
		addr <<= iirf;
		addr >>= iirf;

		T data = (*((T*)&(((u8*)ptr)[addr])));

		/* use this as needed */
		dump_sh4_registers();
		return data;
	}
	else
	{
		const u32 id = (u32)iirf;
		if (sz == 1)
		{
			return (T)_vmem_RF8[id / 4](_vmem_CTX[id / 4], addr);
		}
		else if (sz == 2)
		{
			return (T)_vmem_RF16[id / 4](_vmem_CTX[id / 4], addr);
		}
		else if (sz == 4)
		{
			return _vmem_RF32[id / 4](_vmem_CTX[id / 4], addr);
		}
		else if (sz == 8)
		{
			T rv = _vmem_RF32[id / 4](_vmem_CTX[id / 4], addr);
			rv |= (T)((u64)_vmem_RF32[id / 4](_vmem_CTX[id / 4], addr + 4) << 32);

			return rv;
		}
		else
		{
			die("Invalid size");
		}
	}
}
template<typename T>
INLINE void DYNACALL _vmem_writet(u32 addr, T data)
{
	const u32 sz = sizeof(T);

	u32 page = addr >> 24;
	unat  iirf = (unat)_vmem_MemInfo_ptr[page];
	void* ptr = (void*)(iirf & ~HANDLER_MAX);

	if (likely(ptr != 0))
	{
		addr <<= iirf;
		addr >>= iirf;

		*((T*)&(((u8*)ptr)[addr])) = data;
	}
	else
	{
		const u32 id = (u32)iirf;
		if (sz == 1)
		{
			_vmem_WF8[id / 4](_vmem_CTX[id / 4], addr, data);
		}
		else if (sz == 2)
		{
			_vmem_WF16[id / 4](_vmem_CTX[id / 4], addr, data);
		}
		else if (sz == 4)
		{
			_vmem_WF32[id / 4](_vmem_CTX[id / 4], addr, data);
		}
		else if (sz == 8)
		{
			_vmem_WF32[id / 4](_vmem_CTX[id / 4], addr, (u32)data);
			_vmem_WF32[id / 4](_vmem_CTX[id / 4], addr + 4, (u32)((u64)data >> 32));
		}
		else
		{
			die("Invalid size");
		}
	}
}


//Functions

//init/reset/term
void _vmem_init();
void _vmem_reset();
void _vmem_term();

//functions to register and map handlers/memory
_vmem_handler _vmem_register_handler(void* ctx, _vmem_ReadMem8FP* read8,_vmem_ReadMem16FP* read16,_vmem_ReadMem32FP* read32, _vmem_WriteMem8FP* write8,_vmem_WriteMem16FP* write16,_vmem_WriteMem32FP* write32);

#define  _vmem_register_handler_Template(ctx, read,write) _vmem_register_handler \
									(ctx, read<1,u8>,read<2,u16>,read<4,u32>,	\
									write<1,u8>,write<2,u16>,write<4,u32>)	

#define  _vmem_register_handler_Template1(ctx, read,write,extra_Tparam) _vmem_register_handler \
									(ctx, read<1,u8,extra_Tparam>,read<2,u16,extra_Tparam>,read<4,u32,extra_Tparam>,	\
									write<1,u8,extra_Tparam>,write<2,u16,extra_Tparam>,write<4,u32,extra_Tparam>)	

#define  _vmem_register_handler_Template2(ctx, read,write,etp1,etp2) _vmem_register_handler \
									(ctx, read<1,u8,etp1,etp2>,read<2,u16,etp1,etp2>,read<4,u32,etp1,etp2>,	\
									write<1,u8,etp1,etp2>,write<2,u16,etp1,etp2>,write<4,u32,etp1,etp2>)	

void _vmem_map_handler(_vmem_handler Handler,u32 start,u32 end);
void _vmem_map_block(void* base,u32 start,u32 end,u32 mask);
void _vmem_mirror_mapping(u32 new_region,u32 start,u32 size);

#define _vmem_map_block_mirror(base,start,end,blck_size) {u32 block_size=(blck_size)>>24;u32 map_sz=(end)-(start)+1;/*verify((map_sz%block_size)==0);u32 map_times=map_sz/(block_size);*/ for (u32 _maip=(start);_maip<(end);_maip+=block_size) _vmem_map_block((base),_maip,_maip+block_size-1,blck_size-1);}

#define VMEM_MAP_FORWARDER(klass, name) \
template <u32 sz, class T>  \
T DYNACALL Read##name(void* ctx, u32 addr) {  \
	auto that = reinterpret_cast<klass*>(ctx);  \
	return that->Read##name<sz, T>(addr); \
} \
 \
template <u32 sz, class T> \
void DYNACALL Write##name(void* ctx, u32 addr, T data) { \
	auto that = reinterpret_cast<SuperH4Mmr_impl*>(ctx); \
	that->Write##name<sz, T>(addr, data); \
}

//ReadMem/WriteMem functions
//ReadMem
INLINE u32 DYNACALL _vmem_ReadMem8SX32(u32 Address) { return _vmem_readt<s8, s32>(Address); }
INLINE u32 DYNACALL _vmem_ReadMem16SX32(u32 Address) { return _vmem_readt<s16, s32>(Address); }

INLINE u8 DYNACALL _vmem_ReadMem8(u32 Address) { return _vmem_readt<u8, u8>(Address); }
INLINE u16 DYNACALL _vmem_ReadMem16(u32 Address) { return _vmem_readt<u16, u16>(Address); }
INLINE u32 DYNACALL _vmem_ReadMem32(u32 Address) { return _vmem_readt<u32, u32>(Address); }
INLINE u64 DYNACALL _vmem_ReadMem64(u32 Address) { return _vmem_readt<u64, u64>(Address); }

//WriteMem
INLINE void DYNACALL _vmem_WriteMem8(u32 Address, u8 data) { _vmem_writet<u8>(Address, data); }
INLINE void DYNACALL _vmem_WriteMem16(u32 Address, u16 data) { _vmem_writet<u16>(Address, data); }
INLINE void DYNACALL _vmem_WriteMem32(u32 Address, u32 data) { _vmem_writet<u32>(Address, data); }
INLINE void DYNACALL _vmem_WriteMem64(u32 Address, u64 data) { _vmem_writet<u64>(Address, data); }

//should be called at start up to ensure it will succeed :)
bool _vmem_reserve(VLockedMemory* mram, VLockedMemory* vram, VLockedMemory* aica_ram, u32 aram_size);
void _vmem_release(VLockedMemory* mram, VLockedMemory* vram, VLockedMemory* aica_ram);

//dynarec helpers
void _vmem_get_ptrs(u32 sz,bool write,void*** vmap,void*** func);
void* _vmem_get_ptr2(u32 addr,u32& mask);
void* _vmem_read_const(u32 addr,bool& ismem,u32 sz);

extern u8* virt_ram_base;

static inline bool _nvmem_enabled() {
	return virt_ram_base != 0;
}

void _vmem_bm_reset();
bool _vmem_bm_LockedWrite(u8* address);
