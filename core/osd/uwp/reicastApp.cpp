
#include <iostream>

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.UI.Core.h>

#include "types.h"

using namespace winrt;
using namespace Windows::Foundation;

u16 kcode[4];
u32 vks[4];
s8 joyx[4], joyy[4];
u8 rt[4], lt[4];

int get_mic_data(u8* buffer) { return 0; }
int push_vmu_screen(u8* buffer) { return 0; }

void os_DebugBreak() {}


void os_SetWindowText(const char * text) {
	puts(text);
}

void os_DoEvents() {

}

void os_SetupInput() {
	//mcfg_CreateDevicesFromConfig();
}

//void os_DebugBreak() { return; } // SIGNAL()


void UpdateInputState(u32 port) {

}

void UpdateVibration(u32 port, u32 value) {

}

void os_CreateWindow() { }


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

using namespace Windows;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::UI;
using namespace Windows::UI::Core;

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
		std::wcout << "Activating CoreWindow..." << std::endl;
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
	CoreApplication::Run(make<App>());
}