#ifndef _WIN32
// Implementation of the vmem related function for POSIX-like platforms.
// There's some minimal amount of platform specific hacks to support
// Android and OSX since they are slightly different in some areas.

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include "hw/mem/_vmem.h"
#include "stdclass.h"

#ifndef MAP_NOSYNC
#define MAP_NOSYNC       0 //missing from linux :/ -- could be the cause of android slowness ?
#endif

#if defined(HAVE_LIBNX)
#include <switch.h>
FILE *	fmemopen (void *__restrict, size_t, const char *__restrict);
#endif

#ifdef _ANDROID
	#include <linux/ashmem.h>
	#ifndef ASHMEM_DEVICE
		#define ASHMEM_DEVICE "/dev/ashmem"
		#undef PAGE_MASK
		#define PAGE_MASK (PAGE_SIZE-1)
	#else
		#define PAGE_SIZE 4096
		#define PAGE_MASK (PAGE_SIZE-1)
	#endif

// Android specific ashmem-device stuff for creating shared memory regions
int ashmem_create_region(const char *name, size_t size) {
	int fd = open(ASHMEM_DEVICE, O_RDWR);
	if (fd < 0)
		return -1;

	if (ioctl(fd, ASHMEM_SET_SIZE, size) < 0) {
		close(fd);
		return -1;
	}

	return fd;
}
#endif  // #ifdef _ANDROID

#ifdef HAVE_LIBNX
using mem_handle_t = uintptr_t;
mem_handle_t vmem_fd = -1;
mem_handle_t vmem_fd_page = -1;
mem_handle_t vmem_fd_codememory = -1;

static mem_handle_t shmem_fd2 = -1;
#else
using mem_handle_t = int;
mem_handle_t vmem_fd = -1;
static mem_handle_t shmem_fd2 = -1;
#endif // HAVE_LIBNX

static void *reserved_base;
static size_t reserved_size;

bool mem_region_lock(void *start, size_t len)
{
	size_t inpage = (uintptr_t)start & PAGE_MASK;

#ifdef HAVE_LIBNX
    len += inpage;

    size_t inlen = len & PAGE_MASK;

    if(inlen)
        len = (len + PAGE_SIZE) & (~(PAGE_SIZE-1));

	Result rc;
	uintptr_t start_addr = ((uintptr_t)start - inpage);
	for(uintptr_t addr = start_addr; addr < (start_addr + len); addr += PAGE_SIZE)
	{
		rc = svcSetMemoryPermission((void*)addr, PAGE_SIZE, Perm_R);
		if(R_FAILED(rc))
		{
			printf("Failed to SetPerm Perm_R on %p len 0x%x rc 0x%x\n", (void*)addr, PAGE_SIZE, rc);
		}
	}
#else
	if (mprotect((u8*)start - inpage, len + inpage, PROT_READ))
		die("mprotect failed...");
#endif // HAVE_LIBNX

	return true;
}

bool mem_region_unlock(void *start, size_t len)
{
	size_t inpage = (uintptr_t)start & PAGE_MASK;

#ifdef HAVE_LIBNX
    len += inpage;

    size_t inlen = len & PAGE_MASK;

    if(inlen)
        len = (len + PAGE_SIZE) & (~(PAGE_SIZE-1));
        
	Result rc;
	uintptr_t start_addr = ((uintptr_t)start - inpage);
	for(uintptr_t addr = start_addr; addr < (start_addr + len); addr += PAGE_SIZE)
	{
		rc = svcSetMemoryPermission((void*)addr, PAGE_SIZE, Perm_Rw);
		if(R_FAILED(rc))
		{
			printf("Failed to SetPerm Perm_Rw on %p len 0x%x rc 0x%x\n", (void*)addr, PAGE_SIZE, rc);
		}
	}

#else
	if (mprotect((u8*)start - inpage, len + inpage, PROT_READ | PROT_WRITE))
		// Add some way to see why it failed? gdb> info proc mappings
		die("mprotect  failed...");
#endif // HAVE_LIBNX

	return true;
}

bool mem_region_set_exec(void *start, size_t len)
{
	size_t inpage = (uintptr_t)start & PAGE_MASK;
	
#ifdef HAVE_LIBNX
	svcSetMemoryPermission((void*)((uintptr_t)start - inpage), len + inpage, Perm_R); // *shrugs*
#else
	if (mprotect((u8*)start - inpage, len + inpage, PROT_READ | PROT_WRITE | PROT_EXEC))
		die("mprotect  failed...");
#endif // HAVE_LIBNX
	return true;
}

void *mem_region_reserve(void *start, size_t len)
{
#ifdef HAVE_LIBNX
	return virtmemReserve(len);
#else
	void *p = mmap(start, len, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
	if (p == MAP_FAILED)
	{
		perror("mmap");
		return NULL;
	}
	else
		return p;
#endif // HAVE_LIBNX
}

bool mem_region_release(void *start, size_t len)
{
#ifdef HAVE_LIBNX
	return true;
#else
	return munmap(start, len) == 0;
#endif // HAVE_LIBNX
}

void *mem_region_map_file(void *file_handle, void *dest, size_t len, size_t offset, bool readwrite)
{
#ifdef HAVE_LIBNX
	Result rc = svcMapProcessMemory(dest, envGetOwnProcessHandle(), (u64)(vmem_fd_codememory + offset), len);
	if(R_FAILED(rc))
	{
		printf("Fatal error creating the view... base: %p offset: 0x%x size: 0x%x src: %p err: 0x%x\n", vmem_fd, offset, len, vmem_fd_codememory + offset, rc);
	} else {
		printf("Created the view... base: %p offset: 0x%x size: 0x%x src: %p err: 0x%x\n", vmem_fd, offset, len, vmem_fd_codememory + offset, rc);
	}

	return dest;
#else
	int flags = MAP_SHARED | MAP_NOSYNC | (dest != NULL ? MAP_FIXED : 0);
	void *p = mmap(dest, len, PROT_READ | (readwrite ? PROT_WRITE : 0), flags, (int)(uintptr_t)file_handle, offset);
	if (p == MAP_FAILED)
	{
		perror("mmap");
		return NULL;
	}
	else
		return p;
#endif // HAVE_LIBNX
}

bool mem_region_unmap_file(void *start, size_t len)
{
	return mem_region_release(start, len);
}

// Allocates memory via a fd on shmem/ahmem or even a file on disk
static mem_handle_t allocate_shared_filemem(unsigned size) {
	int fd = -1;
	#if defined(_ANDROID)
	// Use Android's specific shmem stuff.
	fd = ashmem_create_region(0, size);
	#elif defined(HAVE_LIBNX)
	void* mem = memalign(0x1000, size);
	return (uintptr_t)mem;
	#else
		#if HOST_OS != OS_DARWIN
		fd = shm_open("/dcnzorz_mem", O_CREAT | O_EXCL | O_RDWR, S_IREAD | S_IWRITE);
		shm_unlink("/dcnzorz_mem");
		#endif

		// if shmem does not work (or using OSX) fallback to a regular file on disk
		if (fd < 0) {
			string path = get_writable_data_path("/dcnzorz_mem");
			fd = open(path.c_str(), O_CREAT|O_RDWR|O_TRUNC, S_IRWXU|S_IRWXG|S_IRWXO);
			unlink(path.c_str());
		}
		// If we can't open the file, fallback to slow mem.
		if (fd < 0)
			return -1;

		// Finally make the file as big as we need!
		if (ftruncate(fd, size)) {
			// Can't get as much memory as needed, fallback.
			close(fd);
			return -1;
		}
	#endif

	return fd;
}

// Implement vmem initialization for RAM, ARAM, VRAM and SH4 context, fpcb etc.
// The function supports allocating 512MB or 4GB addr spaces.
// vmem_base_addr points to an address space of 512MB (or 4GB) that can be used for fast memory ops.
// In negative offsets of the pointer (up to FPCB size, usually 65/129MB) the context and jump table
// can be found. If the platform init returns error, the user is responsible for initializing the
// memory using a fallback (that is, regular mallocs and falling back to slow memory JIT).
VMemType vmem_platform_init(void **vmem_base_addr, void **sh4rcb_addr) {
#ifdef HAVE_LIBNX
	const unsigned size_aligned = ((RAM_SIZE_MAX + VRAM_SIZE_MAX + ARAM_SIZE_MAX + PAGE_SIZE) & (~(PAGE_SIZE-1)));
	vmem_fd_page = allocate_shared_filemem(size_aligned);
	if (vmem_fd_page < 0)
		return MemTypeError;

	vmem_fd_codememory = (uintptr_t)virtmemReserve(size_aligned);
	
	if(R_FAILED(svcMapProcessCodeMemory(envGetOwnProcessHandle(), (u64) vmem_fd_codememory, (u64) vmem_fd_page, size_aligned)))
		printf("Failed to Map memory (platform_int)...\n");

	if(R_FAILED(svcSetProcessMemoryPermission(envGetOwnProcessHandle(), vmem_fd_codememory, size_aligned, Perm_Rx)))
		printf("Failed to set perms (platform_int)...\n");
#else
	// Firt let's try to allocate the shm-backed memory
	vmem_fd = allocate_shared_filemem(RAM_SIZE_MAX + VRAM_SIZE_MAX + ARAM_SIZE_MAX);
	if (vmem_fd < 0)
		return MemTypeError;
#endif // HAVE_LIBNX
	// Now try to allocate a contiguous piece of memory.
	VMemType rv;
#if defined(HOST_64BIT_CPU) && !defined(HAVE_LIBNX)
	reserved_size = 0x100000000L + sizeof(Sh4RCB) + 0x10000;	// 4GB + context size + 64K padding
	reserved_base = mem_region_reserve(NULL, reserved_size);
	rv = MemType4GB;
#endif // defined(HOST_64BIT_CPU) && !defined(HAVE_LIBNX)
	if (reserved_base == NULL)
	{
		reserved_size = 512*1024*1024 + sizeof(Sh4RCB) + ARAM_SIZE_MAX + 0x10000;
		reserved_base = mem_region_reserve(NULL, reserved_size);
		if (!reserved_base) {
#ifndef HAVE_LIBNX
			close(vmem_fd);
#endif // HAVE_LIBNX
			return MemTypeError;
		}
		rv = MemType512MB;
	}

	// Align pointer to 64KB too, some Linaro bug (no idea but let's just be safe I guess).
	uintptr_t ptrint = (uintptr_t)reserved_base;
	ptrint = (ptrint + 0x10000 - 1) & (~0xffff);
	*sh4rcb_addr = (void*)ptrint;
	*vmem_base_addr = (void*)(ptrint + sizeof(Sh4RCB));
	const size_t fpcb_size = sizeof(((Sh4RCB *)NULL)->fpcb);
	void *sh4rcb_base_ptr  = (void*)(ptrint + fpcb_size);

	// Now map the memory for the SH4 context, do not include FPCB on purpose (paged on demand).
	mem_region_unlock(sh4rcb_base_ptr, sizeof(Sh4RCB) - fpcb_size);

	return rv;
}

// Just tries to wipe as much as possible in the relevant area.
void vmem_platform_destroy() {
	if (reserved_base != NULL)
		mem_region_release(reserved_base, reserved_size);
}

// Resets a chunk of memory by deleting its data and setting its protection back.
void vmem_platform_reset_mem(void *ptr, unsigned size_bytes) {
#ifndef HAVE_LIBNX
	// Mark them as non accessible.
	mprotect(ptr, size_bytes, PROT_NONE);
	// Tell the kernel to flush'em all (FIXME: perhaps unmap+mmap 'd be better?)
	#if defined(__HAIKU__)
	// Posix function for Haiku
	posix_madvise(ptr, size_bytes, POSIX_MADV_DONTNEED);
	#else
	madvise(ptr, size_bytes, MADV_DONTNEED);
	#endif
	#if defined(MADV_REMOVE)
	madvise(ptr, size_bytes, MADV_REMOVE);
	#elif defined(MADV_FREE)
	madvise(ptr, size_bytes, MADV_FREE);
	#endif
#else
	svcSetMemoryPermission(ptr, size_bytes, Perm_None);
#endif // HAVE_LIBNX
}

// Allocates a bunch of memory (page aligned and page-sized)
void vmem_platform_ondemand_page(void *address, unsigned size_bytes) {
	verify(mem_region_unlock(address, size_bytes));
}

// Creates mappings to the underlying file including mirroring sections
void vmem_platform_create_mappings(const vmem_mapping *vmem_maps, unsigned nummaps) {
	for (unsigned i = 0; i < nummaps; i++) {
		// Ignore unmapped stuff, it is already reserved as PROT_NONE
		if (!vmem_maps[i].memsize)
			continue;

		// Calculate the number of mirrors
		u64 address_range_size = vmem_maps[i].end_address - vmem_maps[i].start_address;
		unsigned num_mirrors = (address_range_size) / vmem_maps[i].memsize;
		verify((address_range_size % vmem_maps[i].memsize) == 0 && num_mirrors >= 1);

		for (unsigned j = 0; j < num_mirrors; j++) {
			u64 offset = vmem_maps[i].start_address + j * vmem_maps[i].memsize;
			verify(mem_region_unmap_file(&virt_ram_base[offset], vmem_maps[i].memsize));
			verify(mem_region_map_file((void*)(uintptr_t)vmem_fd, &virt_ram_base[offset],
					vmem_maps[i].memsize, vmem_maps[i].memoffset, vmem_maps[i].allow_writes) != NULL);
		}
	}
}

// Prepares the code region for JIT operations, thus marking it as RWX
bool vmem_platform_prepare_jit_block(void *code_area, unsigned size, void **code_area_rwx) {
	// Try to map is as RWX, this fails apparently on OSX (and perhaps other systems?)
	if (!mem_region_set_exec(code_area, size))
	{
		// Well it failed, use another approach, unmap the memory area and remap it back.
		// Seems it works well on Darwin according to reicast code :P
		munmap(code_area, size);
		void *ret_ptr = mmap(code_area, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_FIXED | MAP_PRIVATE | MAP_ANON, 0, 0);
		// Ensure it's the area we requested
		if (ret_ptr != code_area)
			return false;   // Couldn't remap it? Perhaps RWX is disabled? This should never happen in any supported Unix platform.
	}

	// Pointer location should be same:
	*code_area_rwx = code_area;
	return true;
}

// Use two addr spaces: need to remap something twice, therefore use allocate_shared_filemem()
bool vmem_platform_prepare_jit_block(void *code_area, unsigned size, void **code_area_rw, uintptr_t *rx_offset) {
#ifndef HAVE_LIBNX
	shmem_fd2 = allocate_shared_filemem(size);
	if (shmem_fd2 < 0)
		return false;

	// Need to unmap the section we are about to use (it might be already unmapped but nevertheless...)
	munmap(code_area, size);

	// Map the RX bits on the code_area, for proximity, as usual.
	void *ptr_rx = mmap(code_area, size, PROT_READ | PROT_EXEC,
	                    MAP_SHARED | MAP_NOSYNC | MAP_FIXED, shmem_fd2, 0);
#else
	const unsigned size_aligned = ((size + PAGE_SIZE) & (~(PAGE_SIZE-1)));
	void *ptr_rx = code_area;
#endif // HAVE_LIBNX
	if (ptr_rx != code_area)
		return false;

#ifndef HAVE_LIBNX
	// Now remap the same memory as RW in some location we don't really care at all.
	void *ptr_rw = mmap(NULL, size, PROT_READ | PROT_WRITE,
	                    MAP_SHARED | MAP_NOSYNC, shmem_fd2, 0);
#else
	void* ptr_rw = virtmemReserve(size_aligned);
	if(R_FAILED(svcMapProcessMemory(ptr_rw, envGetOwnProcessHandle(), (u64)code_area, size_aligned)))
		printf("Failed to map jit rw block...\n");
#endif // HAVE_LIBNX
	*code_area_rw = ptr_rw;
	*rx_offset = (char*)ptr_rx - (char*)ptr_rw;
	INFO_LOG(DYNAREC, "Info: Using NO_RWX mode, rx ptr: %p, rw ptr: %p, offset: %lu\n", ptr_rx, ptr_rw, (unsigned long)*rx_offset);

	return (ptr_rw != MAP_FAILED);
}

// Some OSes restrict cache flushing, cause why not right? :D

#if HOST_CPU == CPU_ARM64

// Code borrowed from Dolphin https://github.com/dolphin-emu/dolphin
static void Arm64_CacheFlush(void* start, void* end) {
	if (start == end)
		return;

#if HOST_OS == OS_DARWIN
	// Header file says this is equivalent to: sys_icache_invalidate(start, end - start);
	sys_cache_control(kCacheFunctionPrepareForExecution, start, end - start);
#else
	// Don't rely on GCC's __clear_cache implementation, as it caches
	// icache/dcache cache line sizes, that can vary between cores on
	// big.LITTLE architectures.
	u64 addr, ctr_el0;
	static size_t icache_line_size = 0xffff, dcache_line_size = 0xffff;
	size_t isize, dsize;

	__asm__ volatile("mrs %0, ctr_el0" : "=r"(ctr_el0));
	isize = 4 << ((ctr_el0 >> 0) & 0xf);
	dsize = 4 << ((ctr_el0 >> 16) & 0xf);

	// use the global minimum cache line size
	icache_line_size = isize = icache_line_size < isize ? icache_line_size : isize;
	dcache_line_size = dsize = dcache_line_size < dsize ? dcache_line_size : dsize;

	addr = (u64)start & ~(u64)(dsize - 1);
	for (; addr < (u64)end; addr += dsize)
		// use "civac" instead of "cvau", as this is the suggested workaround for
		// Cortex-A53 errata 819472, 826319, 827319 and 824069.
		__asm__ volatile("dc civac, %0" : : "r"(addr) : "memory");
	__asm__ volatile("dsb ish" : : : "memory");

	addr = (u64)start & ~(u64)(isize - 1);
	for (; addr < (u64)end; addr += isize)
		__asm__ volatile("ic ivau, %0" : : "r"(addr) : "memory");

	__asm__ volatile("dsb ish" : : : "memory");
	__asm__ volatile("isb" : : : "memory");
#endif
}


void vmem_platform_flush_cache(void *icache_start, void *icache_end, void *dcache_start, void *dcache_end) {
	Arm64_CacheFlush(dcache_start, dcache_end);

	// Dont risk it and flush and invalidate icache&dcache for both ranges just in case.
	if (icache_start != dcache_start)
		Arm64_CacheFlush(icache_start, icache_end);
}

#endif // #if HOST_CPU == CPU_ARM64

#else	// _WIN32


#define _WIN32_WINNT 0x0500
#include <windows.h>
#include <windowsx.h>

#include "hw/mem/_vmem.h"

// Implementation of the vmem related function for Windows platforms.
// For now this probably does some assumptions on the CPU/platform.

bool mem_region_lock(void *start, size_t len)
{
	DWORD old;
	if (!VirtualProtect(start, len, PAGE_READONLY, &old))
		die("VirtualProtect failed ..\n");
	return true;
}

bool mem_region_unlock(void *start, size_t len)
{
	DWORD old;
	if (!VirtualProtect(start, len, PAGE_READWRITE, &old))
		die("VirtualProtect failed ..\n");
	return true;
}

bool mem_region_set_exec(void *start, size_t len)
{
	DWORD old;
	if (!VirtualProtect(start, len, PAGE_EXECUTE_READWRITE, &old))
		die("VirtualProtect failed ..\n");
	return true;
}

void *mem_region_reserve(void *start, size_t len)
{
	return VirtualAlloc(start, len, MEM_RESERVE, PAGE_NOACCESS);
}

bool mem_region_release(void *start, size_t len)
{
	return VirtualFree(start, 0, MEM_RELEASE);
}

void *mem_region_map_file(void *file_handle, void *dest, size_t len, size_t offset, bool readwrite)
{
	return MapViewOfFileEx((HANDLE)file_handle, readwrite ? FILE_MAP_WRITE : FILE_MAP_READ, (DWORD)(offset >> 32), (DWORD)offset, len, dest);
}

bool mem_region_unmap_file(void *start, size_t len)
{
	return UnmapViewOfFile(start);
}

HANDLE mem_handle = INVALID_HANDLE_VALUE;
static HANDLE mem_handle2 = INVALID_HANDLE_VALUE;
static char * base_alloc = NULL;

// Implement vmem initialization for RAM, ARAM, VRAM and SH4 context, fpcb etc.
// The function supports allocating 512MB or 4GB addr spaces.

// Plase read the POSIX implementation for more information. On Windows this is
// rather straightforward.
VMemType vmem_platform_init(void **vmem_base_addr, void **sh4rcb_addr) {
	// Firt let's try to allocate the in-memory file
	mem_handle = CreateFileMapping(INVALID_HANDLE_VALUE, 0, PAGE_READWRITE, 0, RAM_SIZE_MAX + VRAM_SIZE_MAX + ARAM_SIZE_MAX, 0);

	// Now allocate the actual address space (it will be 64KB aligned on windows).
	unsigned memsize = 512*1024*1024 + sizeof(Sh4RCB) + ARAM_SIZE_MAX;
	base_alloc = (char*)mem_region_reserve(NULL, memsize);

	// Calculate pointers now
	*sh4rcb_addr = &base_alloc[0];
	*vmem_base_addr = &base_alloc[sizeof(Sh4RCB)];

	return MemType512MB;
}

// Just tries to wipe as much as possible in the relevant area.
void vmem_platform_destroy() {
	VirtualFree(base_alloc, 0, MEM_RELEASE);
	CloseHandle(mem_handle);
}

// Resets a chunk of memory by deleting its data and setting its protection back.
void vmem_platform_reset_mem(void *ptr, unsigned size_bytes) {
	VirtualFree(ptr, size_bytes, MEM_DECOMMIT);
}

// Allocates a bunch of memory (page aligned and page-sized)
void vmem_platform_ondemand_page(void *address, unsigned size_bytes) {
	verify(VirtualAlloc(address, size_bytes, MEM_COMMIT, PAGE_READWRITE));
}

/// Creates mappings to the underlying file including mirroring sections
void vmem_platform_create_mappings(const vmem_mapping *vmem_maps, unsigned nummaps) {
	// Since this is tricky to get right in Windows (in posix one can just unmap sections and remap later)
	// we unmap the whole thing only to remap it later.

	// Unmap the whole section
	VirtualFree(base_alloc, 0, MEM_RELEASE);

	// Map the SH4CB block too
	void *base_ptr = VirtualAlloc(base_alloc, sizeof(Sh4RCB), MEM_RESERVE, PAGE_NOACCESS);
	verify(base_ptr == base_alloc);
	void *cntx_ptr = VirtualAlloc((u8*)p_sh4rcb + sizeof(p_sh4rcb->fpcb), sizeof(Sh4RCB) - sizeof(p_sh4rcb->fpcb), MEM_COMMIT, PAGE_READWRITE);
	verify(cntx_ptr == (u8*)p_sh4rcb + sizeof(p_sh4rcb->fpcb));

	for (unsigned i = 0; i < nummaps; i++) {
		unsigned address_range_size = vmem_maps[i].end_address - vmem_maps[i].start_address;
		DWORD protection = vmem_maps[i].allow_writes ? (FILE_MAP_READ | FILE_MAP_WRITE) : FILE_MAP_READ;

		if (!vmem_maps[i].memsize) {
			// Unmapped stuff goes with a protected area or memory. Prevent anything from allocating here
			void *ptr = VirtualAlloc(&virt_ram_base[vmem_maps[i].start_address], address_range_size, MEM_RESERVE, PAGE_NOACCESS);
			verify(ptr == &virt_ram_base[vmem_maps[i].start_address]);
		}
		else {
			// Calculate the number of mirrors
			unsigned num_mirrors = (address_range_size) / vmem_maps[i].memsize;
			verify((address_range_size % vmem_maps[i].memsize) == 0 && num_mirrors >= 1);

			// Remap the views one by one
			for (unsigned j = 0; j < num_mirrors; j++) {
				unsigned offset = vmem_maps[i].start_address + j * vmem_maps[i].memsize;

				void *ptr = MapViewOfFileEx(mem_handle, protection, 0, vmem_maps[i].memoffset,
				                    vmem_maps[i].memsize, &virt_ram_base[offset]);
				verify(ptr == &virt_ram_base[offset]);
			}
		}
	}
}

typedef void* (*mapper_fn) (void *addr, unsigned size);

// This is a tempalted function since it's used twice
static void* vmem_platform_prepare_jit_block_template(void *code_area, unsigned size, mapper_fn mapper) {
	// Several issues on Windows: can't protect arbitrary pages due to (I guess) the way
	// kernel tracks mappings, so only stuff that has been allocated with VirtualAlloc can be
	// protected (the entire allocation IIUC).

	// Strategy: ignore code_area and allocate a new one. Protect it properly.
	// More issues: the area should be "close" to the .text stuff so that code gen works.
	// Remember that on x64 we have 4 byte jump/load offset immediates, no issues on x86 :D

	// Take this function addr as reference.
	uintptr_t base_addr = reinterpret_cast<uintptr_t>(&vmem_platform_init) & ~0xFFFFF;

	// Probably safe to assume reicast code is <200MB (today seems to be <16MB on every platform I've seen).
	for (unsigned i = 0; i < 1800*1024*1024; i += 8*1024*1024) {  // Some arbitrary step size.
		uintptr_t try_addr_above = base_addr + i;
		uintptr_t try_addr_below = base_addr - i;

		// We need to make sure there's no address wrap around the end of the addrspace (meaning: int overflow).
		if (try_addr_above > base_addr) {
			void *ptr = mapper((void*)try_addr_above, size);
			if (ptr)
				return ptr;
		}
		if (try_addr_below < base_addr) {
			void *ptr = mapper((void*)try_addr_below, size);
			if (ptr)
				return ptr;
		}
	}
	return NULL;
}

static void* mem_alloc(void *addr, unsigned size) {
	return VirtualAlloc(addr, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
}

// Prepares the code region for JIT operations, thus marking it as RWX
bool vmem_platform_prepare_jit_block(void *code_area, unsigned size, void **code_area_rwx) {
	// Get the RWX page close to the code_area
	void *ptr = vmem_platform_prepare_jit_block_template(code_area, size, &mem_alloc);
	if (!ptr)
		return false;

	*code_area_rwx = ptr;
	INFO_LOG(DYNAREC, "Found code area at %p, not too far away from %p", *code_area_rwx, &vmem_platform_init);

	// We should have found some area in the addrspace, after all size is ~tens of megabytes.
	// Pages are already RWX, all done
	return true;
}


static void* mem_file_map(void *addr, unsigned size) {
	// Maps the entire file at the specified addr.
	void *ptr = VirtualAlloc(addr, size, MEM_RESERVE, PAGE_NOACCESS);
	if (!ptr)
		return NULL;
	VirtualFree(ptr, 0, MEM_RELEASE);
	if (ptr != addr)
		return NULL;

	return MapViewOfFileEx(mem_handle2, FILE_MAP_READ | FILE_MAP_EXECUTE, 0, 0, size, addr);
}

// Use two addr spaces: need to remap something twice, therefore use CreateFileMapping()
bool vmem_platform_prepare_jit_block(void *code_area, unsigned size, void **code_area_rw, uintptr_t *rx_offset) {
	mem_handle2 = CreateFileMapping(INVALID_HANDLE_VALUE, 0, PAGE_EXECUTE_READWRITE, 0, size, 0);

	// Get the RX page close to the code_area
	void *ptr_rx = vmem_platform_prepare_jit_block_template(code_area, size, &mem_file_map);
	if (!ptr_rx)
		return false;

	// Ok now we just remap the RW segment at any position (dont' care).
	void *ptr_rw = MapViewOfFileEx(mem_handle2, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, size, NULL);

	*code_area_rw = ptr_rw;
	*rx_offset = (char*)ptr_rx - (char*)ptr_rw;
	INFO_LOG(DYNAREC, "Info: Using NO_RWX mode, rx ptr: %p, rw ptr: %p, offset: %lu", ptr_rx, ptr_rw, (unsigned long)*rx_offset);

	return (ptr_rw != NULL);
}

#endif // _WIN32
