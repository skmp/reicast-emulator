
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

using namespace winrt::Windows::System;

u16 kcode[4];
u32 vks[4];
s8 joyx[4], joyy[4];
u8 rt[4], lt[4];
LARGE_INTEGER qpf;
double  qpfd;
double speed_load_mspdf;



#if 0

	ReserveBottomMemory();
	tick_thd.Start();
	SetupPath();

	//SetUnhandledExceptionFilter(&ExeptionHandler);
	__try
	{
		int dc_init(int argc,wchar_t* argv[]);
		void dc_run();
		void dc_term();
		if (0 == dc_init(0,NULL))//*FIXME* wchar_t (argc, argv))
		{
#ifdef _WIN64
			setup_seh();
#endif
			dc_run();
			dc_term();
		}
	}
	__except( ExeptionHandler(GetExceptionInformation()) )
	{
		wprintf(L"Unhandled exception - Emulation thread halted...\n");
	}
	SetUnhandledExceptionFilter(0);


#endif
	
		int dc_init(int argc,wchar_t* argv[]);
		void dc_run();
		void dc_term();


// Wut happen to header?  ;shrug;
struct App
	: implements<App, IFrameworkViewSource, IFrameworkView>
{
	bool m_init = false;



	IFrameworkView CreateView()
	{
		return *this;
	}

	void Initialize(CoreApplicationView const &)
	{
	//	m_init = (0==dc_init(0,NULL));
	}

	void Load(hstring const&)
	{
		// I should probably check what these do, i'm assuming this is loadUI.elem@hstring path
	}

	void Uninitialize()
	{
		if(m_init) dc_term();
	}

	void Run()	// heres where it gets fun,  dc_run will thread jack on single thread impl. , so we do this stuff in os_DoShit() or w.e it's named...?
	{
		CoreWindow window = CoreWindow::GetForCurrentThread();
		window.Activate();
		
		CoreDispatcher dispatcher = CoreWindow::GetForCurrentThread().Dispatcher();
		dispatcher.ProcessEvents(CoreProcessEventsOption::ProcessUntilQuit);
		
	std::wcout << "::Run(); finished ProcessEvents" << std::endl;
		//if(m_init)	dc_run();
#ifndef TARGET_NO_THREADS
#error I hope you added the dc_*() to a thread before you enable threads!

		CoreWindow window = CoreWindow::GetForCurrentThread();
		window.Activate();

		CoreDispatcher dispatcher = window.Dispatcher();
		dispatcher.ProcessEvents(CoreProcessEventsOption::ProcessUntilQuit);
#endif
	}

	void SetWindow(CoreWindow const & window)
	{

	}
};


void os_DoEvents()
{
	//CoreDispatcher dispatcher = CoreWindow::GetForCurrentThread().Dispatcher();
	//dispatcher.ProcessEvents(CoreProcessEventsOption::ProcessUntilQuit);
}

void os_SetWindowText(const wchar_t * text) {
	CoreWindow window = CoreWindow::GetForCurrentThread();
	//puts(text);
}



void* libPvr_GetRenderTarget()
{
//	CoreWindow window = CoreWindow::GetForCurrentThread();
	return nullptr; //&render_window;
}

void* libPvr_GetRenderSurface()
{
//	CoreWindow window = CoreWindow::GetForCurrentThread();
	return nullptr; //==(void*)EGL_DEFAULT_DISPLAY;
}




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







#define key_CONT_C            (1 << 0)
#define key_CONT_B            (1 << 1)
#define key_CONT_A            (1 << 2)
#define key_CONT_START        (1 << 3)
#define key_CONT_DPAD_UP      (1 << 4)
#define key_CONT_DPAD_DOWN    (1 << 5)
#define key_CONT_DPAD_LEFT    (1 << 6)
#define key_CONT_DPAD_RIGHT   (1 << 7)
#define key_CONT_Z            (1 << 8)
#define key_CONT_Y            (1 << 9)
#define key_CONT_X            (1 << 10)
#define key_CONT_D            (1 << 11)
#define key_CONT_DPAD2_UP     (1 << 12)
#define key_CONT_DPAD2_DOWN   (1 << 13)
#define key_CONT_DPAD2_LEFT   (1 << 14)
#define key_CONT_DPAD2_RIGHT  (1 << 15)


void UpdateInputState(u32 port)
{
	CoreWindow window = CoreWindow::GetForCurrentThread();

	//	CoreVirtualKeyStates s; // 0:none, 1:pressed, 2:locked?
	//window.GetAsyncKeyState(VirtualKey::A);
		/// looks like global GetAsyncKeyState works here too,  this was prob not necessary but oh well ?
#define Btn(b)		(CoreVirtualKeyStates::Down == window.GetAsyncKeyState(VirtualKey::b))
#define Pressed(vk) (CoreVirtualKeyStates::Down == window.GetAsyncKeyState(VirtualKey((vk))))
	
		lt[port] = Pressed('A')||Btn(GamepadLeftShoulder) ? 255 : 0;	// {Left,Right}Trigger also *
		rt[port] = Pressed('S')||Btn(GamepadRightShoulder)? 255 : 0;
		
		joyx[port]=joyy[port]=0;
		
		if (Pressed('I')||Btn(GamepadLeftThumbstickUp))		joyy[port]-=126;	// These are already max for type, why set zero then add?  -Z
		if (Pressed('K')||Btn(GamepadLeftThumbstickDown))	joyy[port]+=126;
		if (Pressed('J')||Btn(GamepadLeftThumbstickLeft))	joyx[port]-=126;
		if (Pressed('L')||Btn(GamepadLeftThumbstickRight))	joyx[port]+=126;

		kcode[port]=0xFFFF;
		if (Pressed('V')||Btn(GamepadA))	kcode[port]&=~key_CONT_A;
		if (Pressed('C')||Btn(GamepadB))	kcode[port]&=~key_CONT_B;
		if (Pressed('X')||Btn(GamepadY))	kcode[port]&=~key_CONT_Y;
		if (Pressed('Z')||Btn(GamepadX))	kcode[port]&=~key_CONT_X;
		
		// VK_ matches ofc, these are ok
		if (Pressed(VK_SHIFT)||Btn(GamepadMenu))		kcode[port]&=~key_CONT_START;	// GamePadView also *

		if (Pressed(VK_UP)   ||Btn(GamepadDPadUp))		kcode[port]&=~key_CONT_DPAD_UP;
		if (Pressed(VK_DOWN) ||Btn(GamepadDPadDown))	kcode[port]&=~key_CONT_DPAD_DOWN;
		if (Pressed(VK_LEFT) ||Btn(GamepadDPadLeft))	kcode[port]&=~key_CONT_DPAD_LEFT;
		if (Pressed(VK_RIGHT)||Btn(GamepadDPadRight))	kcode[port]&=~key_CONT_DPAD_RIGHT;


		if (Pressed(VK_F1))	settings.pvr.ta_skip = 100;
		if (Pressed(VK_F2))	settings.pvr.ta_skip = 0;
	//	if (GetAsyncKeyState(VK_F10))	DiscSwap();
	//	if (GetAsyncKeyState(VK_ESCAPE))dc_stop();
		

#if USE_XINPUT
	{
		XINPUT_STATE state;

		if (XInputGetState(port, &state) == 0)
		{
			WORD xbutton = state.Gamepad.wButtons;

			if (xbutton & XINPUT_GAMEPAD_A)			kcode[port] &= ~key_CONT_A;
			if (xbutton & XINPUT_GAMEPAD_B)			kcode[port] &= ~key_CONT_B;
			if (xbutton & XINPUT_GAMEPAD_Y)			kcode[port] &= ~key_CONT_Y;
			if (xbutton & XINPUT_GAMEPAD_X)			kcode[port] &= ~key_CONT_X;

			if (xbutton & XINPUT_GAMEPAD_START)		kcode[port] &= ~key_CONT_START;

			if (xbutton & XINPUT_GAMEPAD_DPAD_UP)	kcode[port] &= ~key_CONT_DPAD_UP;
			if (xbutton & XINPUT_GAMEPAD_DPAD_DOWN)	kcode[port] &= ~key_CONT_DPAD_DOWN;
			if (xbutton & XINPUT_GAMEPAD_DPAD_LEFT)	kcode[port] &= ~key_CONT_DPAD_LEFT;
			if (xbutton & XINPUT_GAMEPAD_DPAD_RIGHT)kcode[port] &= ~key_CONT_DPAD_RIGHT;

			lt[port] |= state.Gamepad.bLeftTrigger;
			rt[port] |= state.Gamepad.bRightTrigger;

			joyx[port] |=  state.Gamepad.sThumbLX / 257;
			joyy[port] |= -state.Gamepad.sThumbLY / 257;
		}
	}
#endif //USE_XINPUT
}


void os_SetupInput()
{
	// mooo? , srsly if we need xinput initialized maybe ... see if those fancy CoreVirtualKeyStates::Gamepad* enums work
}


void UpdateVibration(u32 port, u32 value)
{
#if USE_XINPUT
	// no
#endif
}

void os_CreateWindow()
{
	msgboxf(__FUNCTIONW__ L"()", 0);
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


void os_MakeExecutable(void* ptr, u32 sz)
{
	DWORD old;
	VirtualProtect(ptr, sizeof(sz), PAGE_EXECUTE_READWRITE, &old);
}










#if 0	// The exercise was for you to add win_osd.cpp,  lulz,  added, delete below
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
#endif

