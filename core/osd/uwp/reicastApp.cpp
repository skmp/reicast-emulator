
#include <iostream>

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.UI.Core.h>

#include "types.h"

using namespace winrt;
using namespace Windows;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::Foundation;
using namespace Windows::UI;
using namespace Windows::UI::Core;

u16 kcode[4];
u32 vks[4];
s8 joyx[4], joyy[4];
u8 rt[4], lt[4];
LARGE_INTEGER qpf;
double  qpfd;
double speed_load_mspdf;

struct App : implements<App, IFrameworkViewSource, IFrameworkView>
{
	IFrameworkView CreateView()
	{
		return *this;
	}

	void Initialize(CoreApplicationView const &)
	{
	}

	void Load(hstring const&)
	{
	}

	void Uninitialize()
	{
	}

	void Run()
	{
		CoreWindow window = CoreWindow::GetForCurrentThread();
		window.Activate();

		CoreDispatcher dispatcher = window.Dispatcher();
		dispatcher.ProcessEvents(CoreProcessEventsOption::ProcessUntilQuit);
	}

	void SetWindow(CoreWindow const & window)
	{
	}
};

int __stdcall main(HINSTANCE, HINSTANCE, PWSTR, int)
{
	std::wcout << "CoreApplication::Run(nullptr);" << std::endl;
	CoreApplication::Run(make<App>());
}

int get_mic_data(u8* buffer) { return 0; }
int push_vmu_screen(u8* buffer) { return 0; }

void os_DebugBreak() {
}

double os_GetSeconds()
{
	static bool initme = (QueryPerformanceFrequency(&qpf), qpfd = 1 / (double)qpf.QuadPart);
	LARGE_INTEGER time_now;

	QueryPerformanceCounter(&time_now);
	return time_now.QuadPart*qpfd;
}

void os_SetWindowText(const char * text) {
	puts(text);
}

void os_DoEvents() {
}

void os_SetupInput() {
}

void UpdateInputState(u32 port) {
}

void UpdateVibration(u32 port, u32 value) {
}

void os_CreateWindow() {
}

int __cdecl msgboxf(const wchar_t* text, unsigned int type, ...)
{
	va_list args;

	wchar_t temp[2048];
	va_start(args, type);
	vswprintf(temp, text, args);
	va_end(args);

	wprintf(L"----------------------------------------------\n");
	wprintf(L"msgbox(\" %s \" ); \n", temp);
	wprintf(L"----------------------------------------------\n\n");
	//	QMessageBox::information(0, "", temp, QMessageBox::Ok);
	return 0;
}
void* libPvr_GetRenderTarget()
{
	return nullptr;
}

void* libPvr_GetRenderSurface()
{
	return nullptr;
}

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

//cResetEvent Calss
cResetEvent::cResetEvent(bool State, bool Auto)
{
	hEvent = CreateEvent(
		NULL,             // default security attributes
		Auto ? FALSE : TRUE,  // auto-reset event?
		State ? TRUE : FALSE, // initial state is State
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
	return WaitForSingleObject(hEvent, msec) == WAIT_OBJECT_0;
}
void cResetEvent::Wait()//Wait for signal , then reset
{
#if defined(DEBUG_THREADS)
	Sleep(rand() % 10);
#endif
	WaitForSingleObject(hEvent, (u32)-1);
}

void os_SetWindowText(const wchar_t* text)
{
	/*
	if (GetWindowLong((HWND)libPvr_GetRenderTarget(), GWL_STYLE)&WS_BORDER)
	{
		SetWindowText((HWND)libPvr_GetRenderTarget(), text);
	}
	*/
}

void os_MakeExecutable(void* ptr, u32 sz)
{
	DWORD old;
	VirtualProtect(ptr, sizeof(sz), PAGE_EXECUTE_READWRITE, &old);
}
