#pragma once
#include "types.h"
#include <stdlib.h>
#include <vector>
#include <algorithm>
#include <string.h>

#ifdef _ANDROID
#include <sys/mman.h>
#undef PAGE_MASK
#define PAGE_MASK (PAGE_SIZE-1)
#else
#define PAGE_SIZE 4096
#define PAGE_MASK (PAGE_SIZE-1)
#endif

//Commonly used classes across the project
//Simple Array class for helping me out ;P
template<class T>
class Array
{
public:
	T* data;
	u32 Size;

	Array(T* Source,u32 ellements)
	{
		//initialise array
		data=Source;
		Size=ellements;
	}

	Array(u32 ellements)
	{
		//initialise array
		data=0;
		Resize(ellements,false);
		Size=ellements;
	}

	Array(u32 ellements,bool zero)
	{
		//initialise array
		data=0;
		Resize(ellements,zero);
		Size=ellements;
	}

	Array()
	{
		//initialise array
		data=0;
		Size=0;
	}

	~Array()
	{
		if  (data)
		{
			#ifdef MEM_ALLOC_TRACE
			printf("WARNING : DESTRUCTOR WITH NON FREED ARRAY [arrayid:%d]\n",id);
			#endif
			Free();
		}
	}

	void SetPtr(T* Source,u32 ellements)
	{
		//initialise array
		Free();
		data=Source;
		Size=ellements;
	}

	T* Resize(u32 size,bool bZero)
	{
		if (size==0)
		{
			if (data)
			{
				#ifdef MEM_ALLOC_TRACE
				printf("Freeing data -> resize to zero[Array:%d]\n",id);
				#endif
				Free();
			}

		}
		
		if (!data)
			data=(T*)malloc(size*sizeof(T));
		else
			data=(T*)realloc(data,size*sizeof(T));

		//TODO : Optimise this
		//if we allocated more , Zero it out
		if (bZero)
		{
			if (size>Size)
			{
				for (u32 i=Size;i<size;i++)
				{
					u8*p =(u8*)&data[i];
					for (size_t j=0;j<sizeof(T);j++)
					{
						p[j]=0;
					}
				}
			}
		}
		Size=size;

		return data;
	}

	void Zero()
	{
		memset(data,0,sizeof(T)*Size);
	}

	void Free()
	{
		if (Size != 0)
		{
			if (data)
				free(data);

			data = NULL;
		}
	}


	INLINE T& operator [](const u32 i)
	{
#ifdef MEM_BOUND_CHECK
		if (i>=Size)
		{
			printf("Error: Array %d , index out of range (%d>%d)\n",id,i,Size-1);
			MEM_DO_BREAK;
		}
#endif
		return data[i];
	}

	INLINE T& operator [](const s32 i)
	{
#ifdef MEM_BOUND_CHECK
		if (!(i>=0 && i<(s32)Size))
		{
			printf("Error: Array %d , index out of range (%d > %d)\n",id,i,Size-1);
			MEM_DO_BREAK;
		}
#endif
		return data[i];
	}
};

#if HOST_OS != OS_DARWIN
	#define DATA_PATH "/data/"
#else
	#define DATA_PATH "/"
#endif

//Set the path !
void set_user_config_dir(const string& dir);
void set_user_data_dir(const string& dir);
void add_system_config_dir(const string& dir);
void add_system_data_dir(const string& dir);

//subpath format: /data/fsca-table.bit
string get_writable_config_path(const string& filename);
string get_writable_data_path(const string& filename);
string get_readonly_config_path(const string& filename);
string get_readonly_data_path(const string& filename);
bool file_exists(const string& filename);
bool make_directory(const string& path);

string get_game_save_prefix();
string get_game_basename();
string get_game_dir();


// Locked memory class, used for texture invalidation purposes.
class VLockedMemory {
public:
	u8* data;
	unsigned size;

	void SetRegion(void* ptr, unsigned size) {
		this->data = (u8*)ptr;
		this->size = size;
	}
	void *getPtr() const { return data; }
	unsigned getSize() const { return size; }

	void LockRegion(unsigned offset, unsigned size_bytes);
	void UnLockRegion(unsigned offset, unsigned size_bytes);

	void Zero() {
		UnLockRegion(0, size);
		memset(data, 0, size);
	}

	INLINE u8& operator [](unsigned i) {
#ifdef MEM_BOUND_CHECK
        if (i >= size)
		{
			printf("Error: VLockedMemory , index out of range (%d > %d)\n", i, size-1);
			MEM_DO_BREAK;
		}
#endif
		return data[i];
    }
};


int msgboxf(const wchar* text,unsigned int type,...);


#define MBX_OK                       0x00000000L

#define MBX_ICONERROR                0x00000010L
#define MBX_ICONEXCLAMATION          0x00000030L

#define MBX_RV_OK                1