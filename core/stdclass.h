#pragma once
#include "types.h"
#include <stdlib.h>
#include <vector>
#include <string.h>

#include <rthreads/rthreads.h>

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

	Array(u32 size,bool bZero)
	{
		//initialise array
		data=0;
		Resize(size, bZero);
		Size = size;
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
			Free();
	}

	T* Resize(u32 size,bool bZero)
	{
		if (size==0)
		{
			if (data)
				Free();
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
						p[j]=0;
				}
			}
		}
		Size=size;

		return data;
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
};

//Threads

//Wait Events
struct cResetEvent
{
   slock_t *mutx;
   scond_t *cond;

   bool state;
};

//Set the path !
void set_user_config_dir(const string& dir);
void set_user_data_dir(const string& dir);
void add_system_config_dir(const string& dir);
void add_system_data_dir(const string& dir);

//subpath format: /data/fsca-table.bit
string get_writable_data_path(const string& filename);

struct VArray2
{
	u8* data;
	u32 size;
};

void VArray2_LockRegion(VArray2 *varray2, u32 offset, u32 size);
void VArray2_UnLockRegion(VArray2 *varray2, u32 offset, u32 size);
void VArray2_Zero(VArray2 *varray2);

int ExeptionHandler(u32 dwCode, void* pExceptionPointers);
int msgboxf(const wchar* text,unsigned int type,...);

#define MBX_OK                       0x00000000L
#define MBX_OKCANCEL                 0x00000001L
#define MBX_ABORTRETRYIGNORE         0x00000002L
#define MBX_YESNOCANCEL              0x00000003L
#define MBX_YESNO                    0x00000004L
#define MBX_RETRYCANCEL              0x00000005L


#define MBX_ICONHAND                 0x00000010L
#define MBX_ICONQUESTION             0x00000020L
#define MBX_ICONEXCLAMATION          0x00000030L
#define MBX_ICONASTERISK             0x00000040L


#define MBX_USERICON                 0x00000080L
#define MBX_ICONWARNING              MBX_ICONEXCLAMATION
#define MBX_ICONERROR                MBX_ICONHAND


#define MBX_ICONINFORMATION          MBX_ICONASTERISK
#define MBX_ICONSTOP                 MBX_ICONHAND

#define MBX_DEFBUTTON1               0x00000000L
#define MBX_DEFBUTTON2               0x00000100L
#define MBX_DEFBUTTON3               0x00000200L

#define MBX_DEFBUTTON4               0x00000300L


#define MBX_APPLMODAL                0x00000000L
#define MBX_SYSTEMMODAL              0x00001000L
#define MBX_TASKMODAL                0x00002000L

#define MBX_HELP                     0x00004000L // Help Button


#define MBX_NOFOCUS                  0x00008000L
#define MBX_SETFOREGROUND            0x00010000L
#define MBX_DEFAULT_DESKTOP_ONLY     0x00020000L

#define MBX_TOPMOST                  0x00040000L
#define MBX_RIGHT                    0x00080000L
#define MBX_RTLREADING               0x00100000L

#define MBX_RV_OK                1
#define MBX_RV_CANCEL            2
#define MBX_RV_ABORT             3
#define MBX_RV_RETRY             4
#define MBX_RV_IGNORE            5
#define MBX_RV_YES               6
#define MBX_RV_NO                7
