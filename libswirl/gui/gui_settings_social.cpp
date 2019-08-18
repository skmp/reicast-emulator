#include "types.h"
#include "gui.h"
#include "stdclass.h"
#include "imgui/imgui.h"
#include "gui_partials.h"
#include "gui_util.h"
#include "oslib/oslib.h"

void gui_settings_social()
{
	if (ImGui::BeginTabItem("Social"))
	{
		ImGui::Checkbox("Hide Social links from games list", &settings.social.HideCallToAction);
	
		ImGui::Separator();

		#if !defined(__ANDROID__) // Google doesn't like us having donate links
			if (ImGui::Button("Donate / Support Reicast (via emudev.org)")) {
		    	os_LaunchFromURL("http://donate.emudev.org");
		    }
	    #endif

		if (ImGui::Button("Patreon (emudev.org)")) {
	    	os_LaunchFromURL("http://patreon.emudev.org");
	    }

		ImGui::Separator();

		if (ImGui::Button("Discord")) {
	    	os_LaunchFromURL("http://chat.reicast.com");
	    }

		if (ImGui::Button("Facebook")) {
	    	os_LaunchFromURL("http://facebook.com/reicastdc");
	    }

		if (ImGui::Button("Twitter")) {
	    	os_LaunchFromURL("https://twitter.com/reicastdc");
	    }

		if (ImGui::Button("Official Forum")) {
	    	os_LaunchFromURL("http://forum.reicast.com");
	    }


		if (ImGui::Button("Homepage")) {
	    	os_LaunchFromURL("http://reicast.com");
	    }

		ImGui::EndTabItem();
	}
}