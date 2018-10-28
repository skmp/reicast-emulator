#include "naomi_cart.h"

u8* RomPtr;
u32 RomSize;

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

	memcpy(dst, naomi_cart_GetPtr(offset, size), size);
	return true;
}

void* naomi_cart_GetPtr(u32 offset, u32 size) {

	offset &= 0x0FFFffff;

	verify(offset < RomSize);
	verify((offset + size) <= RomSize);

	return &RomPtr[offset];
}
