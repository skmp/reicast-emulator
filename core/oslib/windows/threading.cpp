
#include "oslib/threading.h"
#include "build.h"

// Thread & related platform dependant code
#if !defined(HOST_NO_THREADS)

void cThread::Start() {
	hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)entry, param, 0, NULL);
	ResumeThread(hThread);
}
void cThread::WaitToEnd() {
	WaitForSingleObject(hThread,INFINITE);
	CloseHandle(hThread);
	hThread = NULL;
}
#endif

cMutex::cMutex() {
	InitializeCriticalSection(&cs);
}
cMutex::~cMutex() {
	DeleteCriticalSection(&cs);
}
void cMutex::Lock() {
	EnterCriticalSection(&cs);
}
bool cMutex::TryLock() {
	return TryEnterCriticalSection(&cs);
}
void cMutex::Unlock() {
	LeaveCriticalSection(&cs);
}


void SleepMs(unsigned count) {
	Sleep(count);
}

cResetEvent::cResetEvent() {
		hEvent = CreateEvent(
		NULL,             // default security attributes
		FALSE,            // auto-reset event?
		FALSE,            // initial state is State
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
bool cResetEvent::Wait(unsigned msec)//Wait for signal , then reset
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
	WaitForSingleObject(hEvent,INFINITE);
}

