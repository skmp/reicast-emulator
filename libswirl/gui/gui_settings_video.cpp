#include "types.h"
#include "gui.h"
#include "stdclass.h"
#include "imgui/imgui.h"
#include "gui_partials.h"
#include "gui_util.h"
#include "hw/pvr/Renderer_if.h"

void gui_settings_video()
{
	if (ImGui::BeginTabItem("Video"))
	{
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);

		if (ImGui::BeginCombo("Rendering Backend", settings.pvr.backend.c_str(), ImGuiComboFlags_None))
		{
			{
				bool is_selected = (settings.pvr.backend == "auto");
				if (ImGui::Selectable("auto", &is_selected))
					settings.pvr.backend = "auto";
				ImGui::SameLine(); ImGui::Text("-");
				ImGui::SameLine(); ImGui::Text("Autoselect rendering backend");
			}

			auto backends = rend_get_backends();

			for (auto backend: backends)
			{
				bool is_selected = (settings.pvr.backend == backend.slug);
				if (ImGui::Selectable(backend.slug.c_str(), &is_selected))
					settings.pvr.backend = backend.slug;
				ImGui::SameLine(); ImGui::Text("-");
				ImGui::SameLine(); ImGui::Text(backend.desc.c_str());
			}

			ImGui::EndCombo();
		}
	    if (ImGui::CollapsingHeader("Rendering Options", ImGuiTreeNodeFlags_DefaultOpen))
	    {
			ImGui::Checkbox("Force GLESv2 Context", &settings.pvr.ForceGLES2);
			ImGui::SameLine();
			gui_ShowHelpMarker("Avoid using GLESv3. Works around old Adreno driver bugs");
	    	ImGui::Checkbox("Synchronous Rendering", &settings.pvr.SynchronousRender);
            ImGui::SameLine();
            gui_ShowHelpMarker("Reduce frame skipping by pausing the CPU when possible. Recommended for powerful devices");
	    	ImGui::Checkbox("Clipping", &settings.rend.Clipping);
            ImGui::SameLine();
            gui_ShowHelpMarker("Enable clipping. May produce graphical errors when disabled");
	    	ImGui::Checkbox("Shadows", &settings.rend.ModifierVolumes);
            ImGui::SameLine();
            gui_ShowHelpMarker("Enable modifier volumes, usually used for shadows");
	    	ImGui::Checkbox("Fog", &settings.rend.Fog);
            ImGui::SameLine();
            gui_ShowHelpMarker("Enable fog effects");
	    	ImGui::Checkbox("Widescreen", &settings.rend.WideScreen);
            ImGui::SameLine();
            gui_ShowHelpMarker("Draw geometry outside of the normal 4:3 aspect ratio. May produce graphical glitches in the revealed areas");
	    	ImGui::Checkbox("Show FPS Counter", &settings.rend.ShowFPS);
            ImGui::SameLine();
            gui_ShowHelpMarker("Show on-screen frame/sec counter");
	    	ImGui::Checkbox("Show VMU in game", &settings.rend.FloatVMUs);
            ImGui::SameLine();
            gui_ShowHelpMarker("Show the VMU LCD screens while in game");
	    	ImGui::Checkbox("Rotate screen 90°", &settings.rend.Rotate90);
            ImGui::SameLine();
            gui_ShowHelpMarker("Rotate the screen 90° counterclockwise");
	    	ImGui::SliderInt("Scaling", (int *)&settings.rend.ScreenScaling, 1, 100);
            ImGui::SameLine();
            gui_ShowHelpMarker("Downscaling factor relative to native screen resolution. Higher is better");
	    	ImGui::SliderInt("Horizontal Stretching", (int *)&settings.rend.ScreenStretching, 100, 150);
            ImGui::SameLine();
            gui_ShowHelpMarker("Stretch the screen horizontally");
	    	ImGui::SliderInt("Frame Skipping", (int *)&settings.pvr.ta_skip, 0, 6);
            ImGui::SameLine();
            gui_ShowHelpMarker("Number of frames to skip between two actually rendered frames");
	    }
	    if (ImGui::CollapsingHeader("Render to Texture", ImGuiTreeNodeFlags_DefaultOpen))
	    {
	    	ImGui::Checkbox("Copy to VRAM", &settings.rend.RenderToTextureBuffer);
            ImGui::SameLine();
            gui_ShowHelpMarker("Copy rendered-to textures back to VRAM. Slower but accurate");
	    	ImGui::SliderInt("Render to Texture Upscaling", (int *)&settings.rend.RenderToTextureUpscale, 1, 8);
            ImGui::SameLine();
            gui_ShowHelpMarker("Upscale rendered-to textures. Should be the same as the screen or window upscale ratio, or lower for slow platforms");
	    }
	    if (ImGui::CollapsingHeader("Texture Upscaling", ImGuiTreeNodeFlags_DefaultOpen))
	    {
	    	ImGui::SliderInt("Texture Upscaling", (int *)&settings.rend.TextureUpscale, 1, 8);
            ImGui::SameLine();
            gui_ShowHelpMarker("Upscale textures with the xBRZ algorithm. Only on fast platforms and for certain games");
	    	ImGui::SliderInt("Upscaled Texture Max Size", (int *)&settings.rend.MaxFilteredTextureSize, 8, 1024);
            ImGui::SameLine();
            gui_ShowHelpMarker("Textures larger than this dimension squared will not be upscaled");
	    	ImGui::SliderInt("Max Threads", (int *)&settings.pvr.MaxThreads, 1, 8);
            ImGui::SameLine();
            gui_ShowHelpMarker("Maximum number of threads to use for texture upscaling. Recommended: number of physical cores minus one");
	    	ImGui::Checkbox("Load Custom Textures", &settings.rend.CustomTextures);
            ImGui::SameLine();
            gui_ShowHelpMarker("Load custom/high-res textures from data/textures/<game id>");
	    }
		ImGui::PopStyleVar();
		ImGui::EndTabItem();
	}
}