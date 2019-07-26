#include "types.h"
#include "gui.h"
#include "stdclass.h"
#include "imgui/imgui.h"
#include "gui_partials.h"
#include "gui_util.h"
#include "rend/gles/imgui_impl_opengl3.h"
#include "cfg/cfg.h"
#include "hw/maple/maple_if.h"

void gui_settings()
{

	ImGui_Impl_NewFrame();
    ImGui::NewFrame();

	dynarec_enabled = settings.dynarec.Enable;
	auto pvr_backend = settings.pvr.backend;

    if (!settings_opening)
    	ImGui_ImplOpenGL3_DrawBackground();

	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImVec2(screen_width, screen_height));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

    ImGui::Begin("Settings", NULL, /*ImGuiWindowFlags_AlwaysAutoResize |*/ ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
	normal_padding = ImGui::GetStyle().FramePadding;

    if (ImGui::Button("Done", ImVec2(100 * scaling, 30 * scaling)))
    {
    	if (game_started)
    		gui_state = Commands;
    	else
    		gui_state = Main;
    	if (maple_devices_changed)
    	{
    		maple_devices_changed = false;
#if DC_PLATFORM == DC_PLATFORM_DREAMCAST
    		maple_ReconnectDevices();
    		reset_vmus();
#endif
    	}
       	SaveSettings();
    }
    
	if (game_started)
	{
	    ImGui::SameLine();
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16 * scaling, normal_padding.y));
		if (cfgHasGameSpecificConfig())
		{
			if (ImGui::Button("Delete Game Config", ImVec2(0, 30 * scaling)))
			{
				cfgDeleteGameSpecificConfig();
				InitSettings();
				LoadSettings(false);
			}
		}
		else
		{
			if (ImGui::Button("Make Game Config", ImVec2(0, 30 * scaling)))
				cfgMakeGameSpecificConfig();
		}
	    ImGui::PopStyleVar();
	}

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16 * scaling, 6 * scaling));		// from 4, 3

    if (ImGui::BeginTabBar("settings", ImGuiTabBarFlags_NoTooltip))
    {
		gui_settings_general();

		gui_settings_controls();

		gui_settings_video();
		
		gui_settings_audio();
		
		gui_settings_advanced();
		
		gui_settings_social();

		gui_settings_about();

		ImGui::EndTabBar();
    }

    ImGui::PopStyleVar();


    ImGui::End();
    ImGui::PopStyleVar();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData(), false);

   	if (pvr_backend != settings.pvr.backend)
   		renderer_changed = true;
   	settings.dynarec.Enable = (bool)dynarec_enabled;
}