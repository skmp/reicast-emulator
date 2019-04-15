
#include "reicastApp.h"

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