#pragma once

#include <string>
#include <memory>
#include <wrl.h>

namespace Reicast
{
	namespace App
	{
		ref class ReicastApplication sealed : public Windows::ApplicationModel::Core::IFrameworkView
		{
		public:
			ReicastApplication();

			// IFrameworkView Methods.
			virtual void Initialize(Windows::ApplicationModel::Core::CoreApplicationView^ applicationView);
			virtual void SetWindow(Windows::UI::Core::CoreWindow^ window);
			virtual void Load(Platform::String^ entryPoint);
			virtual void Run();
			virtual void Uninitialize();

		private:
			// Application lifecycle event handlers.
			void OnActivated(Windows::ApplicationModel::Core::CoreApplicationView^ applicationView, Windows::ApplicationModel::Activation::IActivatedEventArgs^ args);

			// Window event handlers.
			void OnVisibilityChanged(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::VisibilityChangedEventArgs^ args);
			void OnWindowClosed(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::CoreWindowEventArgs^ args);

			bool mWindowClosed;
			bool mWindowVisible;
		};
	}
}