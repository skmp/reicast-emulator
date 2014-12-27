#include "types.h"
#include "JitMagic.h"

int msgboxf(const wchar* text, unsigned int type, ...)
{
	va_list args;

	wchar temp[2048];
	va_start(args, type);
	vsprintf(temp, text, args);
	va_end(args);


	//return MessageBox(NULL, temp, VER_SHORTNAME, type | MB_TASKMODAL);

	return MBX_OK;
}

LARGE_INTEGER qpf;
double  qpfd;
//Helper functions
double os_GetSeconds()
{
	static bool initme = (QueryPerformanceFrequency(&qpf), qpfd = 1 / (double)qpf.QuadPart);
	LARGE_INTEGER time_now;

	QueryPerformanceCounter(&time_now);
	return time_now.QuadPart*qpfd;
}

void os_DebugBreak()
{
	__debugbreak();
}



//Windoze Code implementation of commong classes from here and after ..

//Thread class
cThread::cThread(ThreadEntryFP* function, void* prm)
{
	Entry = function;
	param = prm;
}


void cThread::Start()
{
	hThread = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)Entry, param, 0, NULL);
	ResumeThread(hThread);
}

void cThread::WaitToEnd()
{
	WaitForSingleObjectEx(hThread, INFINITE, FALSE);
}
//End thread class

//cResetEvent Calss
cResetEvent::cResetEvent(bool State, bool Auto)
{
	hEvent = CreateEventEx(
		NULL,             // default security attributes
		NULL,
		Auto ? FALSE : TRUE,  // auto-reset event?
		State ? TRUE : FALSE // initial state is State
		);
}
cResetEvent::~cResetEvent()
{
	//Destroy the event object ?
	CloseHandle(hEvent);
}
void cResetEvent::Set()//Signal
{
	SetEvent(hEvent);
}
void cResetEvent::Reset()//reset
{
	ResetEvent(hEvent);
}
void cResetEvent::Wait(u32 msec)//Wait for signal , then reset
{
	WaitForSingleObjectEx(hEvent, msec, FALSE);
}
void cResetEvent::Wait()//Wait for signal , then reset
{
	WaitForSingleObjectEx(hEvent, 0xFFFFFFFF, FALSE);
}
//End AutoResetEvent

void VArray2::LockRegion(u32 offset, u32 size)
{
	//verify(offset+size<this->size);
	verify(size != 0);
	DWORD old;
	VirtualProtect(((u8*)data) + offset, size, PAGE_READONLY, &old);
}
void VArray2::UnLockRegion(u32 offset, u32 size)
{
	//verify(offset+size<=this->size);
	verify(size != 0);
	DWORD old;
	VirtualProtect(((u8*)data) + offset, size, PAGE_READWRITE, &old);
}

int get_mic_data(u8* buffer) { return 0; }
int push_vmu_screen(u8* buffer) { return 0; }


u16 kcode[4];
u32 vks[4];
s8 joyx[4], joyy[4];
u8 rt[4], lt[4];

void UpdateInputState(u32 port) {

}

void os_CreateWindow() {

}

u32 os_Push(void* ptr, u32 sz, bool p) {
	return 0;
}

void os_DoEvents() {

}

void os_SetWindowText(char const*) {

}

double speed_load_mspdf;

void* libPvr_GetRenderTarget()
{
	return 0;
}

void* libPvr_GetRenderSurface()
{
	return 0;
}


void SetupPath()
{
	char fname[512];
	//GetModuleFileName(0, fname, 512);

	wcstombs(fname, Windows::Storage::ApplicationData::Current->RoamingFolder->Path->Data(), 512);
	
	//strcpy(fname, "ms-appdata");
	string fn = string(fname);
	//fn = fn.substr(0, fn.find_last_of('\\'));
	SetHomeDir(fn);
}

bool VramLockedWrite(u8* address);
bool ngen_Rewrite(unat& addr, unat retadr, unat acc);
bool BM_LockedWrite(u8* address);

int ExeptionHandler(u32 dwCode, void* pExceptionPointers)
{
	EXCEPTION_POINTERS* ep = (EXCEPTION_POINTERS*)pExceptionPointers;

	EXCEPTION_RECORD* pExceptionRecord = ep->ExceptionRecord;

	if (dwCode != EXCEPTION_ACCESS_VIOLATION)
		return EXCEPTION_CONTINUE_SEARCH;

	u8* address = (u8*)pExceptionRecord->ExceptionInformation[1];

	if (VramLockedWrite(address))
	{
		return EXCEPTION_CONTINUE_EXECUTION;
	}
	else if (BM_LockedWrite(address))
	{
		return EXCEPTION_CONTINUE_EXECUTION;
	}
#ifndef HOST_NO_REC
	else if (ngen_Rewrite((unat&)ep->ContextRecord->Eip, *(unat*)ep->ContextRecord->Esp, ep->ContextRecord->Eax))
	{
		//remove the call from call stack
		ep->ContextRecord->Esp += 4;
		//restore the addr from eax to ecx so its valid again
		ep->ContextRecord->Ecx = ep->ContextRecord->Eax;
		return EXCEPTION_CONTINUE_EXECUTION;
	}
#endif
	else
	{
		printf("[GPF]Unhandled access to : 0x%X\n", address);
	}

	return EXCEPTION_CONTINUE_SEARCH;
}

void* main_th(void*) {
	
	SetupPath();

	__try
	{
		int dc_init(int argc, wchar* argv[]);
		void dc_run();
		void dc_term();
		if (dc_init(0, 0) < 0)
			die("init failed");
		dc_run();
		dc_term();
	}
	__except ((EXCEPTION_CONTINUE_EXECUTION == ExeptionHandler(GetExceptionCode(), (GetExceptionInformation()))) ? EXCEPTION_CONTINUE_EXECUTION : EXCEPTION_CONTINUE_SEARCH)
	{
		printf("Unhandled exception - Emulation thread halted...\n");
	}

	return 0;
}

cThread main_thd(&main_th, 0);

#include "JitMagic.h"

void run_main_thd() {
	JitMagicInit();
	main_thd.Start();
}