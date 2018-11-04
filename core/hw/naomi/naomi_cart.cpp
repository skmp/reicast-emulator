#include "naomi_cart.h"
#include "deps/libzip/zip.h"
#include "decrypt.h"
#include "naomi_roms.h"
#include "hw/flashrom/flashrom.h"

u8* RomPtr;
u32 RomSize;
u8 naomi_cart_ram[64 * 1024];

#ifdef _WIN32
#include <windows.h>
typedef HANDLE fd_t;
#define INVALID_FD INVALID_HANDLE_VALUE
#ifndef FILE_READ_ACCESS
#define FILE_READ_ACCESS        0x0001
#endif

#ifndef FILE_WRITE_ACCESS
#define FILE_WRITE_ACCESS       0x0002
#endif
#else
typedef int fd_t;
#define INVALID_FD -1

#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#endif
#include <unistd.h>

fd_t*	RomCacheMap;
u32		RomCacheMapCount;
char naomi_game_id[33];
static CartridgeType cartridge_type;
u32 cartridge_key;

extern RomChip sys_rom;
extern char game_dir_no_slash[1024];

static bool naomi_LoadBios(const char *filename)
{
	int biosid = 0;
	for (; BIOS[biosid].name != NULL; biosid++)
		if (!stricmp(BIOS[biosid].name, filename))
			break;
	if (BIOS[biosid].name == NULL)
	{
		printf("Unknown BIOS %s\n", filename);
		return false;
	}

	struct BIOS_t *bios = &BIOS[biosid];

	std::string basepath(game_dir_no_slash);
	basepath += "/";

	zip *zip_archive = zip_open((basepath + filename).c_str(), 0, NULL);
	if (zip_archive == NULL)
	{
		printf("Cannot find BIOS %s\n", filename);
		return false;
	}

	int romid = 0;
	while (bios->blobs[romid].filename != NULL)
	{
		if (bios->blobs[romid].blob_type == Copy)
		{
			verify(bios->blobs[romid].offset + bios->blobs[romid].length <= BIOS_SIZE);
			verify(bios->blobs[romid].src_offset + bios->blobs[romid].length <= BIOS_SIZE);
			memcpy(sys_rom.data + bios->blobs[romid].offset, sys_rom.data + bios->blobs[romid].src_offset, bios->blobs[romid].length);
		}
		else
		{
			zip_file* file = zip_fopen(zip_archive, bios->blobs[romid].filename, 0);
			if (!file) {
				printf("%s: Cannot open %s\n", filename, bios->blobs[romid].filename);
				goto error;
			}
			if (bios->blobs[romid].blob_type == Normal)
			{
				verify(bios->blobs[romid].offset + bios->blobs[romid].length <= BIOS_SIZE);
				size_t read = zip_fread(file, sys_rom.data + bios->blobs[romid].offset, bios->blobs[romid].length);
			}
			else if (bios->blobs[romid].blob_type == InterleavedWord)
			{
				u8 *buf = (u8 *)malloc(bios->blobs[romid].length);
				if (buf == NULL)
				{
					printf("malloc failed\n");
					zip_fclose(file);
					goto error;
				}
				verify(bios->blobs[romid].offset + bios->blobs[romid].length <= BIOS_SIZE);
				size_t read = zip_fread(file, buf, bios->blobs[romid].length);
				u16 *to = (u16 *)(RomPtr + bios->blobs[romid].offset);
				u16 *from = (u16 *)buf;
				for (int i = bios->blobs[romid].length / 2; --i >= 0; to++)
					*to++ = *from++;
				free(buf);
			}
			else
				die("Unknown blob type\n");
			zip_fclose(file);
		}
		romid++;
	}

	zip_close(zip_archive);

	return true;

error:
	zip_close(zip_archive);
	return false;
}

static bool naomi_cart_LoadZip(char *filename)
{
	char *p = strrchr(filename, '/');
#ifdef _WIN32
	p = strrchr(p == NULL ? filename : p, '\\');
#endif
	if (p == NULL)
		p = filename;
	else
		p++;

	int gameid = 0;
	for (; Games[gameid].name != NULL; gameid++)
		if (!stricmp(Games[gameid].name, p))
			break;
	if (Games[gameid].name == NULL)
	{
		printf("Unknown game %s\n", filename);
		return false;
	}

	struct Game *game = &Games[gameid];

	if (game->bios != NULL)
	{
		if (!naomi_LoadBios(game->bios))
			return false;
	}

	zip *zip_archive = zip_open(filename, 0, NULL);
	if (zip_archive == NULL)
	{
		printf("Cannot open %s\n", filename);
		return false;
	}

	RomSize = game->size;
	RomPtr = (u8 *)malloc(RomSize);
	memset(RomPtr, 0xFF, RomSize);
	cartridge_type = game->cart_type;
	cartridge_key = game->key;

	int romid = 0;
	while (game->blobs[romid].filename != NULL)
	{
		if (game->blobs[romid].blob_type == Copy)
		{
			verify(game->blobs[romid].offset + game->blobs[romid].length <= RomSize);
			verify(game->blobs[romid].src_offset + game->blobs[romid].length <= RomSize);
			memcpy(RomPtr + game->blobs[romid].offset, RomPtr + game->blobs[romid].src_offset, game->blobs[romid].length);
		}
		else
		{
			zip_file* file = zip_fopen(zip_archive, game->blobs[romid].filename, 0);
			if (!file) {
				printf("%s: Cannot open %s\n", filename, game->blobs[romid].filename);
				goto error;
			}
			if (game->blobs[romid].blob_type == Normal)
			{
				verify(game->blobs[romid].offset + game->blobs[romid].length <= RomSize);
				size_t read = zip_fread(file, RomPtr + game->blobs[romid].offset, game->blobs[romid].length);
			}
			else if (game->blobs[romid].blob_type == InterleavedWord)
			{
				u8 *buf = (u8 *)malloc(game->blobs[romid].length);
				if (buf == NULL)
				{
					printf("malloc failed\n");
					zip_fclose(file);
					goto error;
				}
				verify(game->blobs[romid].offset + game->blobs[romid].length <= RomSize);
				size_t read = zip_fread(file, buf, game->blobs[romid].length);
				u16 *to = (u16 *)(RomPtr + game->blobs[romid].offset);
				u16 *from = (u16 *)buf;
				for (int i = game->blobs[romid].length / 2; --i >= 0; to++)
					*to++ = *from++;
				free(buf);
			}
			else
				die("Unknown blob type\n");
			zip_fclose(file);
		}
		romid++;
	}
	zip_close(zip_archive);
	cyptoSetKey(game->key);

	memcpy(naomi_game_id, RomPtr + 0x30, sizeof(naomi_game_id) - 1);
	naomi_game_id[sizeof(naomi_game_id) - 1] = '\0';
	for (char *p = naomi_game_id + sizeof(naomi_game_id) - 2; *p == ' ' && p >= naomi_game_id; *p-- = '\0');
	printf("NAOMI GAME ID [%s]\n", naomi_game_id);

	return true;

error:
	zip_close(zip_archive);
	free(RomPtr);
	return false;
}

bool naomi_cart_LoadRom(char* file, char *s, size_t len)
{
	printf("nullDC-Naomi rom loader v1.2\n");
	printf("File: %s\n", file);

	size_t folder_pos = strlen(file) - 1;
	while (folder_pos>1 && (file[folder_pos] != '\\' && file[folder_pos] != '/'))
		folder_pos--;

	folder_pos++;

	vector<string> files;
	vector<u32> fstart;
	vector<u32> fsize;

	u32 setsize = 0;
	bool raw_bin_file = false;

	char t[512];
	strcpy(t, file);

	char *pdot = strrchr(file, '.');

	if (pdot != NULL && (!strcmp(pdot, ".zip") || !strcmp(pdot, ".ZIP")))
		return naomi_cart_LoadZip(file);

	if (pdot != NULL && (!strcmp(pdot, ".lst") || !strcmp(pdot, ".LST")))
	{
	   FILE* fl = fopen(t, "r");
	   if (!fl)
		   return false;

	   char* line = fgets(t, 512, fl);
	   if (!line)
	   {
		   fclose(fl);
		   return false;
	   }

	   char* eon = strstr(line, "\n");
	   if (!eon)
	   {
		   printf("+Parsing was unsuccessful, there is something wrong with your lst file\n");
		   fclose(fl);
		   return false;
	   }
	   else
	   {
		   *eon = 0;
	   }
	   eon = strstr(line, "\r");
	   if (eon)
		   *eon = 0;

	   printf("+Loading naomi rom : %s\n", line);

	   strcpy(s, line);

	   line = fgets(t, 512, fl);
	   if (!line)
	   {
		   fclose(fl);
		   return false;
	   }

	   RomSize = 0;

	   while (line)
	   {
		   char filename[512];
		   u32 addr, sz;
		   if (sscanf(line, "\"%[^\"]\",%x,%x", filename, &addr, &sz) == 3)
		   {
			  files.push_back(filename);
			  fstart.push_back(addr);
			  fsize.push_back(sz);
			  setsize += sz;
			  RomSize = max(RomSize, (addr + sz));
		   }
		   else if (line[0] != 0 && line[0] != '\n' && line[0] != '\r')
			   printf("Warning: invalid line in .lst file: %s\n", line);
		   line = fgets(t, 512, fl);
	   }
	   fclose(fl);
	}
	else
	{
	   // BIN loading
	   FILE* fp = fopen(t, "rb");
	   if (fp == NULL)
		  return false;
	   fseek(fp, 0, SEEK_END);
	   u32 file_size = ftell(fp);
	   fclose(fp);
	   files.push_back(t);
	   fstart.push_back(0);
	   fsize.push_back(file_size);
	   setsize = file_size;
	   RomSize = file_size;
	   raw_bin_file = true;
	}

	printf("+%lu romfiles, %.2f MB set size, %.2f MB set address space\n", files.size(), setsize / 1024.f / 1024.f, RomSize / 1024.f / 1024.f);

	if (RomCacheMap)
	{
		RomCacheMapCount = 0;
		delete[] RomCacheMap;
	}

	RomCacheMapCount = (u32)files.size();
	RomCacheMap = new fd_t[files.size()]();

	//Allocate space for the ram, so we are sure we have a segment of continius ram
#ifdef _WIN32
	RomPtr = (u8*)VirtualAlloc(0, RomSize, MEM_RESERVE, PAGE_NOACCESS);
#else
	RomPtr = (u8*)mmap(0, RomSize, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
#endif

	verify(RomPtr != 0);
	verify(RomPtr != (void*)-1);

	strcpy(t, file);

	bool load_error = false;

	//Create File Mapping Objects
	for (size_t i = 0; i<files.size(); i++)
	{
		if (!raw_bin_file)
		{
		   strncpy(t, file, sizeof(t));
		   t[sizeof(t) - 1] = '\0';
		   t[folder_pos] = 0;
		   strcat(t, files[i].c_str());
		}
		else
		{
		   strncpy(t, files[i].c_str(), sizeof(t));
		   t[sizeof(t) - 1] = '\0';
		}
		fd_t RomCache;

		if (strcmp(files[i].c_str(), "null") == 0)
		{
			RomCacheMap[i] = INVALID_FD;
			continue;
		}
#ifdef _WIN32
		RomCache = CreateFile(t, FILE_READ_ACCESS, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
#else
		RomCache = open(t, O_RDONLY);
#endif
		if (RomCache == INVALID_FD)
		{
		   printf("-Unable to read file %s: error %d\n", t, errno);
		   RomCacheMap[i] = INVALID_FD;
		   load_error = true;
		   break;
		}

#ifdef _WIN32
		RomCacheMap[i] = CreateFileMapping(RomCache, 0, PAGE_READONLY, 0, fsize[i], 0);
		verify(CloseHandle(RomCache));
#else
		RomCacheMap[i] = RomCache;
#endif

		verify(RomCacheMap[i] != INVALID_FD);
		//printf("-Preparing \"%s\" at 0x%08X, size 0x%08X\n", files[i].c_str(), fstart[i], fsize[i]);
	}

	//Release the segment we reserved so we can map the files there
#ifdef _WIN32
	verify(VirtualFree(RomPtr, 0, MEM_RELEASE));
#else
	munmap(RomPtr, RomSize);
#endif

	if (load_error)
	{
	   for (size_t i = 0; i < files.size(); i++)
		  if (RomCacheMap[i] != INVALID_FD)
			 close(RomCacheMap[i]);
	   return false;
	}
	//We have all file mapping objects, we start to map the ram

	//Map the files into the segment of the ram that was reserved
	for (size_t i = 0; i<RomCacheMapCount; i++)
	{
		u8* RomDest = RomPtr + fstart[i];

		if (RomCacheMap[i] == INVALID_FD)
		{
			//printf("-Reserving ram at 0x%08X, size 0x%08X\n", fstart[i], fsize[i]);
			
#ifdef _WIN32
			bool mapped = RomDest == VirtualAlloc(RomDest, fsize[i], MEM_RESERVE, PAGE_NOACCESS);
#else
			bool mapped = RomDest == (u8*)mmap(RomDest, RomSize, PROT_NONE, MAP_PRIVATE, 0, 0);
#endif

			if (!mapped)
			{
			   printf("-Mapping RAM FAILED @ %08x size %x\n", fstart[i], fsize[i]);
			   return false;
			}
		}
		else
		{
			//printf("-Mapping \"%s\" at 0x%08X, size 0x%08X\n", files[i].c_str(), fstart[i], fsize[i]);
#ifdef _WIN32
			bool mapped = RomDest == MapViewOfFileEx(RomCacheMap[i], FILE_MAP_READ, 0, 0, fsize[i], RomDest);
#else
			bool mapped = RomDest == mmap(RomDest, fsize[i], PROT_READ, MAP_PRIVATE, RomCacheMap[i], 0 );
#endif
			if (!mapped)
			{
			   printf("-Mapping ROM FAILED: %s @ %08x size %x\n", files[i].c_str(), fstart[i], fsize[i]);
			   return false;
			}
			if (fstart[i] == 0 && fsize[i] >= 0x50)
			{
			   memcpy(naomi_game_id, RomDest + 0x30, sizeof(naomi_game_id) - 1);
			   naomi_game_id[sizeof(naomi_game_id) - 1] = '\0';
			   if (!strcmp("AWNAOMI                         ", naomi_game_id) && fsize[i] >= 0xFF50)
			   {
				  memcpy(naomi_game_id, RomDest + 0xFF30, sizeof(naomi_game_id) - 1);
			   }
			   for (char *p = naomi_game_id + sizeof(naomi_game_id) - 2; *p == ' ' && p >= naomi_game_id; *p-- = '\0');
			   printf("NAOMI GAME ID [%s]\n", naomi_game_id);
			}
		}
	}

	//done :)
	printf("\nMapped ROM Successfully !\n\n");


	return true;
}

extern char *game_data;
extern char eeprom_file[PATH_MAX];

bool naomi_cart_SelectFile(char *s, size_t len)
{
	if (!naomi_cart_LoadRom(game_data, s, len))
	{
	   printf("Cannot load %s: error %d\n", game_data, errno);
	   return false;
	}

	printf("EEPROM file : %s\n", eeprom_file);

	return true;
}

bool naomi_cart_Read(u32 offset, u32 size, void* dst) {
	if (!RomPtr)
		return false;

	if (cartridge_type == M2 && (offset & 0x40000000) != 0)
	{
		if (offset == 0x4001fffe)
		{
			u16 data = cryptoDecrypt();
			data = ((data & 0xff00) >> 8) | ((data & 0x00ff) << 8);
			*(u16 *)dst = data;
			return true;
		}
		EMUERROR("naomi_cart_Read: Invalid read @ %08x\n", offset);
		return false;
	}
	offset &= 0x1FFFFFFF;
	if (offset >= RomSize || (offset + size) > RomSize)
	{
		static u32 ones = 0xffffffff;

		// Makes Outtrigger boot
		EMUERROR("naomi_cart_Read: offset %d > %d\n", offset, RomSize);
		memcpy(dst, &ones, size);
	}
	else
	{
		memcpy(dst, &RomPtr[offset], size);
	}

	return true;
}

bool naomi_cart_Write(u32 offset, u32 size, u32 data)
{
	if (!RomPtr)
		return false;

	if (cartridge_type == M2 && (offset & 0x40000000) != 0)
	{
		if (offset & 0x00020000)
		{
			offset &= sizeof(naomi_cart_ram) - 1;
			naomi_cart_ram[offset] = data;
			naomi_cart_ram[offset + 1] = data >> 8;
			return true;
		}
		switch (offset & 0x1ffff)
		{
			case 0x1fff8:
				cyptoSetLowAddr(data);
				return true;
			case 0x1fffa:
				cyptoSetHighAddr(data);
				return true;
			case 0x1fffc:
				cyptoSetSubkey(data);
				return true;
		}
	}
	EMUERROR("naomi_cart_Write: Invalid write @ %08x data %x\n", offset, data);
	return false;
}

void* naomi_cart_GetPtr(u32 offset, u32 size) {

	offset &= 0x0FFFffff;

	verify(offset < RomSize);
	verify((offset + size) <= RomSize);

	return &RomPtr[offset];
}
