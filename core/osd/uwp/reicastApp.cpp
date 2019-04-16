
#include "reicastApp.h"

#include <iostream>

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Web.Syndication.h>

#include "types.h"

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Web::Syndication;



int main()
{
    winrt::init_apartment();

    Uri rssFeedUri{ L"https://blogs.windows.com/feed" };
    SyndicationClient syndicationClient;
    SyndicationFeed syndicationFeed = syndicationClient.RetrieveFeedAsync(rssFeedUri).get();
    for (const SyndicationItem syndicationItem : syndicationFeed.Items())
    {
        winrt::hstring titleAsHstring = syndicationItem.Title().Text();
        std::wcout << titleAsHstring.c_str() << std::endl;
    }
}









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
























#if 0

using namespace Windows::ApplicationModel::Core;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::Foundation;
using namespace Windows::UI::Core;
using namespace Windows::UI::Input;

using namespace Reicast::App;

// Implementation of the IFrameworkViewSource interface, necessary to run our app.
ref class ReicastApplicationSource sealed : Windows::ApplicationModel::Core::IFrameworkViewSource
{
public:
	virtual Windows::ApplicationModel::Core::IFrameworkView^ CreateView()
	{
		return ref new ReicastApplication();
	}
};

// The main function creates an IFrameworkViewSource for our app, and runs the app.
[Platform::MTAThread]
int main(Platform::Array<Platform::String^>^)
{
	auto reicastApplicationSource = ref new ReicastApplicationSource();
	CoreApplication::Run(reicastApplicationSource);
	return 0;
}

ReicastApplication::ReicastApplication() :
	mWindowClosed(false),
	mWindowVisible(true)
{
}

// The first method called when the IFrameworkView is being created.
void ReicastApplication::Initialize(CoreApplicationView^ applicationView)
{
	// Register event handlers for app lifecycle. This example includes Activated, so that we
	// can make the CoreWindow active and start rendering on the window.
	applicationView->Activated +=
		ref new TypedEventHandler<CoreApplicationView^, IActivatedEventArgs^>(this, &ReicastApplication::OnActivated);

	// Logic for other event handlers could go here.
}

// Called when the CoreWindow object is created (or re-created).
void ReicastApplication::SetWindow(CoreWindow^ window)
{
	window->VisibilityChanged +=
		ref new TypedEventHandler<CoreWindow^, VisibilityChangedEventArgs^>(this, &ReicastApplication::OnVisibilityChanged);

	window->Closed +=
		ref new TypedEventHandler<CoreWindow^, CoreWindowEventArgs^>(this, &ReicastApplication::OnWindowClosed);
}

// Initializes scene resources
void ReicastApplication::Load(Platform::String^ entryPoint)
{
}

// This method is called after the window becomes active.
void ReicastApplication::Run()
{
	while (!mWindowClosed)
	{
		if (mWindowVisible)
		{
			CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);
		}
		else
		{
			CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessOneAndAllPending);
		}
	}
}

// Terminate events do not cause Uninitialize to be called. It will be called if your IFrameworkView
// class is torn down while the app is in the foreground.
void ReicastApplication::Uninitialize()
{
}

// Application lifecycle event handler.
void ReicastApplication::OnActivated(CoreApplicationView^ applicationView, IActivatedEventArgs^ args)
{
	// Run() won't start until the CoreWindow is activated.
	CoreWindow::GetForCurrentThread()->Activate();
}

// Window event handlers.
void ReicastApplication::OnVisibilityChanged(CoreWindow^ sender, VisibilityChangedEventArgs^ args)
{
	mWindowVisible = args->Visible;
}

void ReicastApplication::OnWindowClosed(CoreWindow^ sender, CoreWindowEventArgs^ args)
{
	mWindowClosed = true;
}

#endif