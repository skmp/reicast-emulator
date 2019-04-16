#include "types.h"
#include "cfg/cfg.h"

//Windoze Code implementation of commong classes from here and after ..

#if !defined(TARGET_NO_THREADS)
//Thread class
cThread::cThread(ThreadEntryFP* function,void* prm)
{
	Entry=function;
	param=prm;
}


void cThread::Start()
{
	hThread=CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)Entry,param,0,NULL);
	ResumeThread(hThread);
}

void cThread::WaitToEnd()
{
	WaitForSingleObject(hThread,INFINITE);
}
//End thread class
#endif //TARGET_NO_THREADS


//cResetEvent Calss
cResetEvent::cResetEvent(bool State,bool Auto)
{
		hEvent = CreateEvent(
		NULL,             // default security attributes
		Auto?FALSE:TRUE,  // auto-reset event?
		State?TRUE:FALSE, // initial state is State
		NULL			  // unnamed object
		);
}
cResetEvent::~cResetEvent()
{
	//Destroy the event object ?
	 CloseHandle(hEvent);
}
void cResetEvent::Set()//Signal
{
	#if defined(DEBUG_THREADS)
		Sleep(rand() % 10);
	#endif
	SetEvent(hEvent);
}
void cResetEvent::Reset()//reset
{
	#if defined(DEBUG_THREADS)
		Sleep(rand() % 10);
	#endif
	ResetEvent(hEvent);
}
bool cResetEvent::Wait(u32 msec)//Wait for signal , then reset
{
	#if defined(DEBUG_THREADS)
		Sleep(rand() % 10);
	#endif
	return WaitForSingleObject(hEvent,msec) == WAIT_OBJECT_0;
}
void cResetEvent::Wait()//Wait for signal , then reset
{
	#if defined(DEBUG_THREADS)
		Sleep(rand() % 10);
	#endif
	WaitForSingleObject(hEvent,(u32)-1);
}
//End AutoResetEvent

void VArray2::LockRegion(u32 offset,u32 size)
{
	//verify(offset+size<this->size);
	verify(size!=0);
	DWORD old;
	VirtualProtect(((u8*)data)+offset , size, PAGE_READONLY,&old);
}
void VArray2::UnLockRegion(u32 offset,u32 size)
{
	//verify(offset+size<=this->size);
	verify(size!=0);
	DWORD old;
	VirtualProtect(((u8*)data)+offset , size, PAGE_READWRITE,&old);
}
