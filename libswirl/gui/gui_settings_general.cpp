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

void directory_selected_callback(bool cancelled, std::string selection)
{
	if (!cancelled)
	{
		settings.dreamcast.ContentPath.push_back(selection);
		game_list_done = false;
	}
}

void gui_settings_general()
{
	if (ImGui::BeginTabItem("General"))
	{
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
		const char *languages[] = { "Japanese", "English", "German", "French", "Spanish", "Italian", "Default" };
		if (ImGui::BeginCombo("Language", languages[settings.dreamcast.language], ImGuiComboFlags_None))
		{
			for (int i = 0; i < IM_ARRAYSIZE(languages); i++)
			{
				bool is_selected = settings.dreamcast.language == i;
				if (ImGui::Selectable(languages[i], &is_selected))
					settings.dreamcast.language = i;
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
        ImGui::SameLine();
        gui_ShowHelpMarker("The language as configured in the Dreamcast BIOS");

#ifdef _ANDROID
		const char *orientation[] = { "Auto", "Force Portrait", "Force Landscape" };
		if (ImGui::BeginCombo("Orientation", orientation[settings.rend.ScreenOrientation], ImGuiComboFlags_None))
		{
			for (int i = 0; i < IM_ARRAYSIZE(orientation); i++)
			{
				bool is_selected = settings.rend.ScreenOrientation == i;
				if (ImGui::Selectable(orientation[i], &is_selected))
					settings.rend.ScreenOrientation = i;
				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		gui_ShowHelpMarker("Select type of orientation");
#endif
		const char *broadcast[] = { "NTSC", "PAL", "PAL/M", "PAL/N", "Default" };
		if (ImGui::BeginCombo("Broadcast", broadcast[settings.dreamcast.broadcast], ImGuiComboFlags_None))
		{
			for (int i = 0; i < IM_ARRAYSIZE(broadcast); i++)
			{
				bool is_selected = settings.dreamcast.broadcast == i;
				if (ImGui::Selectable(broadcast[i], &is_selected))
					settings.dreamcast.broadcast = i;
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
        ImGui::SameLine();
        gui_ShowHelpMarker("TV broadcasting standard for non-VGA modes");

		const char *region[] = { "Japan", "USA", "Europe", "Default" };
		if (ImGui::BeginCombo("Region", region[settings.dreamcast.region], ImGuiComboFlags_None))
		{
			for (int i = 0; i < IM_ARRAYSIZE(region); i++)
			{
				bool is_selected = settings.dreamcast.region == i;
				if (ImGui::Selectable(region[i], &is_selected))
					settings.dreamcast.region = i;
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
        ImGui::SameLine();
        gui_ShowHelpMarker("BIOS region");

		const char *cable[] = { "VGA", "RGB Component", "TV Composite" };
		if (ImGui::BeginCombo("Cable", cable[settings.dreamcast.cable == 0 ? 0 : settings.dreamcast.cable - 1], ImGuiComboFlags_None))
		{
			for (int i = 0; i < IM_ARRAYSIZE(cable); i++)
			{
				bool is_selected = i == 0 ? settings.dreamcast.cable <= 1 : settings.dreamcast.cable - 1 == i;
				if (ImGui::Selectable(cable[i], &is_selected))
					settings.dreamcast.cable = i == 0 ? 0 : i + 1;
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
        ImGui::SameLine();
        gui_ShowHelpMarker("Video connection type");

        std::vector<const char *> paths;
        for (auto path : settings.dreamcast.ContentPath)
        	paths.push_back(path.c_str());

        ImVec2 size;
        size.x = 0.0f;
        size.y = (ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().FramePadding.y * 2.f)
        				* (settings.dreamcast.ContentPath.size() + 1) ;//+ ImGui::GetStyle().FramePadding.y * 2.f;

        if (ImGui::ListBoxHeader("Content Location", size))
        {
        	int to_delete = -1;
            for (int i = 0; i < settings.dreamcast.ContentPath.size(); i++)
            {
            	ImGui::PushID(settings.dreamcast.ContentPath[i].c_str());
                ImGui::AlignTextToFramePadding();
            	ImGui::Text("%s", settings.dreamcast.ContentPath[i].c_str());
            	ImGui::SameLine(ImGui::GetContentRegionAvailWidth() - ImGui::CalcTextSize("X").x - ImGui::GetStyle().FramePadding.x);
            	if (ImGui::Button("X"))
            		to_delete = i;
            	ImGui::PopID();
            }
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(24 * scaling, 3 * scaling));
        	if (ImGui::Button("Add"))
        		ImGui::OpenPopup("Select Directory");
    		select_directory_popup("Select Directory", scaling, &directory_selected_callback);
    		ImGui::PopStyleVar();

    		ImGui::ListBoxFooter();
        	if (to_delete >= 0)
        	{
        		settings.dreamcast.ContentPath.erase(settings.dreamcast.ContentPath.begin() + to_delete);
    			game_list_done = false;
        	}
        }
        ImGui::SameLine();
        gui_ShowHelpMarker("The directories where your games are stored");

        if (ImGui::ListBoxHeader("Home Directory", 1))
        {
        	ImGui::AlignTextToFramePadding();
            ImGui::Text("%s", get_writable_config_path("").c_str());
		}
#ifdef _ANDROID
        ImGui::SameLine(ImGui::GetContentRegionAvailWidth() - ImGui::CalcTextSize("Change").x - ImGui::GetStyle().FramePadding.x);
        if (ImGui::Button("Change"))
        {
        	gui_state = Onboarding;
        }
#endif
        ImGui::ListBoxFooter();
        
        ImGui::SameLine();
        gui_ShowHelpMarker("The directory where reicast saves configuration files and VMUs. BIOS files should be in the data subdirectory");

    	ImGui::PopStyleVar();
    	ImGui::EndTabItem();
    }
}
