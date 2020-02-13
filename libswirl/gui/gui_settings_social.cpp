/*
	This file is part of libswirl
*/
#include "license/bsd"


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

		#if !defined(_ANDROID) // Google doesn't like us having donate links
			if (ImGui::Button("Donate / Support Reicast (via emudev.org)")) {
		    	os_LaunchFromURL("https://donate.emudev.org");
		    }
	    #endif


        if (ImGui::Button("Reicast Homepage")) {
            os_LaunchFromURL("https://reicast.com");
        }
		ImGui::NextColumn();
		if (ImGui::Button("Official Reicast Forum")) {
		    os_LaunchFromURL("https://forum.reicast.com");
		}
		if (ImGui::Button("Reicast Guide")) {
		    os_LaunchFromURL("https://reicast.com/guide/");
		}
		ImGui::SameLine();
        if (ImGui::Button("Patreon (emudev.org)")) {
            os_LaunchFromURL("https://patreon.emudev.org");
        }
        ImGui::NextColumn();
        ImGui::Separator();
        if (ImGui::Button("Discord")) {
	    	os_LaunchFromURL("https://chat.reicast.com");
	    }
        ImGui::SameLine();
		if (ImGui::Button("Facebook")) {
	    	os_LaunchFromURL("https://facebook.com/reicastdc");
	    }
		ImGui::NextColumn();
        ImGui::SameLine();
		if (ImGui::Button("Twitter")) {
	    	os_LaunchFromURL("https://twitter.com/reicastdc");
	    }
        ImGui::NextColumn();
		ImGui::Separator();

		ImGui::EndTabItem();
	}
}
