/*
	bios & nvmem related code
*/

#pragma once
#include "types.h"

struct MemChip
{
	u8* data;
	u32 size;
	u32 mask;

	MemChip(u32 size)
	{
		this->data=new u8[size];
		this->size=size;
		this->mask=size-1;//must be power of 2
	}
	~MemChip() { delete[] data; }

	virtual u8 Read8(u32 addr)
	{
		return data[addr&mask];
	}

	u32 Read(u32 addr,u32 sz) 
	{
		addr&=mask;

		u32 rv=0;

		for (u32 i=0;i<sz;i++)
			rv|=Read8(addr+i)<<(i*8);

		return rv;
	}

	bool Load(const wstring& file)
	{
		FILE* f=fopen(file.c_str(),"rb");
		if (f)
		{
			bool rv=fread(data,1,size,f)==size;
			fclose(f);
			return rv;
		}
		return false;
	}

	void Save(const wstring& file)
	{
		FILE* f=fopen(file.c_str(),"wb");
		if (f)
		{
			fwrite(data,1,size,f);
			fclose(f);
		}
	}

	bool Load(const wstring& root,const wstring& prefix,const wstring& names_ro,const wstring& title)
	{
		const size_t maxSize = 512;
		wchar_t base[maxSize] = { '\0' };
		wchar_t temp[maxSize] = { '\0' };
		wchar_t names[maxSize] = { '\0' };

		// FIXME: Data loss if buffer is too small
		size_t namesLength = wcsnlen(names_ro.c_str(), maxSize - 1);
		wcsncpy(names,names_ro.c_str(), namesLength);
		names[namesLength] = '\0';

		swprintf(base,L"%s",root.c_str());

		wchar_t* curr=names;
		wchar_t* next;
		do
		{
			next=wcsstr(curr,L";");
			if(next) *next=0;
			if (curr[0]=='%')
			{
				swprintf(temp,L"%s%s%s",base,prefix.c_str(),curr+1);
			}
			else
			{
				swprintf(temp,L"%s%s",base,curr);
			}
			
			curr=next+1;

			if (Load(temp))
			{
				wprintf(L"Loaded %s as %s\n\n",temp,title.c_str());
				return true;
			}
		} while(next);


		return false;
	}
	void Save(const wstring& root,const wstring& prefix,const wstring& name_ro,const wstring& title)
	{
		wchar_t path[512];

		swprintf(path,L"%s%s%s",root.c_str(),prefix.c_str(),name_ro.c_str());
		Save(path);

		wprintf(L"Saved %s as %s\n\n",path,title.c_str());
	}
};
struct RomChip : MemChip
{
	RomChip(u32 sz) : MemChip(sz) {}
	void Reset()
	{
		//nothing, its permanent read only ;p
	}
	void Write(u32 addr,u32 data,u32 sz)
	{
		die(L"Write to RomChip is not possible, address=%x, data=%x, size=%d");
	}
};
struct SRamChip : MemChip
{
	SRamChip(u32 sz) : MemChip(sz) {}

	void Reset()
	{
		//nothing, its battery backed up storage
	}
	void Write(u32 addr,u32 val,u32 sz)
	{
		addr&=mask;
		switch (sz)
		{
		case 1:
			data[addr]=(u8)val;
			return;
		case 2:
			*(u16*)&data[addr]=(u16)val;
			return;
		case 4:
			*(u32*)&data[addr]=val;
			return;
		}

		die(L"invalid access size");
	}
};
struct DCFlashChip : MemChip // I think its Micronix :p
{
	DCFlashChip(u32 sz): MemChip(sz) { }

	enum FlashState
	{
		FS_CMD_AA, //Waiting AA
		FS_CMD_55, //Waiting 55
		FS_CMD,    //Waiting command

		FS_Erase_AA,
		FS_Erase_55,
		FS_Erase,

		FS_Write,
		

	};

	FlashState state;
	void Reset()
	{
		//reset the flash chip state
		state=FS_CMD_AA;
	}
	
	virtual u8 Read8(u32 addr)
	{
		u32 rv=MemChip::Read8(addr);

		#if DC_PLATFORM==DC_PLATFORM_DREAMCAST
			if ((addr==0x1A002 || addr==0x1A0A2) && settings.dreamcast.region<=2)
				return '0' + settings.dreamcast.region;
			else if ((addr==0x1A004 || addr==0x1A0A4) && settings.dreamcast.broadcast<=3)
				return '0' + settings.dreamcast.broadcast;
		#endif

		return rv;
	}
	

	void Write(u32 addr,u32 val,u32 sz)
	{
		if (sz != 1)
			die(L"invalid access size");

		addr &= mask;
		
		switch(state)
		{
		case FS_Erase_AA:
		case FS_CMD_AA:
			{
				verify(addr==0x5555 && val==0xAA);
				state = (FlashState)(state + 1);
			}
			break;

		case FS_Erase_55:
		case FS_CMD_55:
			{
				verify((addr==0xAAAA || addr==0x2AAA) && val==0x55);
				state = (FlashState)(state + 1);
			}
			break;

		case FS_CMD:
			{
				switch(val)
				{
				case 0xA0:
					state=FS_Write;
					break;
				case 0x80:
					state=FS_Erase_AA;
					break;
				default:
					wprintf(L"Flash write: address=%06X, value=%08X, size=%d\n",addr,val,sz);
					state=FS_CMD_AA;
					die(L"lolwhut");
				}
			}
			break;

		case FS_Erase:
			{
				switch(val)
				{
				case 0x30:
					wprintf(L"Erase Sector %08X! (%08X)\n",addr,addr&(~0x3FFF));
					memset(&data[addr&(~0x3FFF)],0xFF,0x4000);
					break;
				default:
					wprintf(L"Flash write: address=%06X, value=%08X, size=%d\n",addr,val,sz);
					die(L"erase .. what ?");
				}
				state=FS_CMD_AA;
			}
			break;

		case FS_Write:
			{
				//wprintf(L"flash write\n");
				data[addr]&=val;
				state=FS_CMD_AA;
			}
			break;
		}
	}	
};
