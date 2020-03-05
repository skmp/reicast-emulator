/*
	This file is part of libswirl
*/
#include "license/bsd"


#pragma once

// Thread/Mutex/Cond wrappers for pthreads/windows-threads

#include "build.h"
#if HOST_OS == OS_WINDOWS
	#ifndef NOMINMAX
	#define NOMINMAX
	#endif
	#include <windows.h>
#else
	#include <pthread.h>
#endif

void SleepMs(unsigned count);

//Wait Events
class cResetEvent {
private:
#if HOST_OS == OS_WINDOWS
	HANDLE hEvent;
#else
	pthread_mutex_t mutx;
	pthread_cond_t cond;
	bool state;
#endif

public :
	cResetEvent();
	~cResetEvent();
	void Set();		//Set state to signaled
	void Reset();	//Set state to non signaled
	bool Wait(unsigned msec);//Wait for signal , then reset[if auto]. Returns false if timed out
	void Wait();	//Wait for signal , then reset[if auto]
};

class cMutex {
private:
#if HOST_OS == OS_WINDOWS
	CRITICAL_SECTION cs;
#else
	pthread_mutex_t mutx;
#endif

public :
	bool state;
	cMutex();
	~cMutex();
	void Lock();
	bool TryLock();
	void Unlock();
};


#if !defined(HOST_NO_THREADS)
typedef  void* ThreadEntryFP(void* param);

class cThread {
private:
	ThreadEntryFP* entry;
	void* param;
public :
	#if HOST_OS == OS_WINDOWS
		HANDLE hThread;
	#else
		pthread_t *hThread;
	#endif

	cThread(ThreadEntryFP* function, void* param)
		:entry(function), param(param), hThread(NULL) {}
	~cThread() { WaitToEnd(); }
	void Start();
	void Start(void* param) { this->param = param; Start(); }
	void WaitToEnd();
};
#endif

