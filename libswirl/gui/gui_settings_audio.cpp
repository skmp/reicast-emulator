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
#include "oslib/audiostream.h"
#include "cfg/cfg.h"

void gui_settings_audio()
{
	if (ImGui::BeginTabItem("Audio"))
	{
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
		ImGui::Checkbox("Disable Sound", &settings.aica.NoSound);
        ImGui::SameLine();
        gui_ShowHelpMarker("Disable the emulator sound output");

		ImGui::Checkbox("Enable DSP", &settings.aica.NoBatch);
        ImGui::SameLine();
        gui_ShowHelpMarker("Enable the Dreamcast Digital Sound Processor. Only recommended on fast and arm64 platforms");
		ImGui::Checkbox("Limit FPS", &settings.aica.LimitFPS);
        ImGui::SameLine();
        gui_ShowHelpMarker("Use the sound output to limit the speed of the emulator. Recommended in most cases");

		audiobackend_t* backend = NULL;;
		std::string backend_name = settings.audio.backend;
		if (backend_name != "auto" && backend_name != "none")
		{
			backend = GetAudioBackend(settings.audio.backend);
			if (backend != NULL)
				backend_name = backend->slug;
		}

		SortAudioBackends();

		audiobackend_t* current_backend = backend;
		if (ImGui::BeginCombo("Audio Backend", backend_name.c_str(), ImGuiComboFlags_None))
		{
			bool is_selected = (settings.audio.backend == "auto");
			if (ImGui::Selectable("auto", &is_selected))
				settings.audio.backend = "auto";
			ImGui::SameLine(); ImGui::Text("-");
			ImGui::SameLine(); ImGui::Text("Autoselect audio backend");

			is_selected = (settings.audio.backend == "none");
			if (ImGui::Selectable("none", &is_selected))
				settings.audio.backend = "none";
			ImGui::SameLine(); ImGui::Text("-");
			ImGui::SameLine(); ImGui::Text("No audio backend");

			for (int i = 0; i < GetAudioBackendCount(); i++)
			{
				backend = GetAudioBackend(i);
				is_selected = (settings.audio.backend == backend->slug);

				if (is_selected)
					current_backend = backend;

				if (ImGui::Selectable(backend->slug.c_str(), &is_selected))
					settings.audio.backend = backend->slug;
				ImGui::SameLine(); ImGui::Text("-");
				ImGui::SameLine(); ImGui::Text("%s", backend->name.c_str());
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
        ImGui::SameLine();
        gui_ShowHelpMarker("The audio backend to use");

		if (current_backend != NULL && current_backend->get_options != NULL)
		{
			// get backend specific options
			int option_count;
			audio_option_t* options = current_backend->get_options(&option_count);

			// initialize options if not already done
			std::map<std::string, std::string>* cfg_entries = &settings.audio.options[current_backend->slug];
			bool populate_entries = (cfg_entries->size() == 0);

			for (int o = 0; o < option_count; o++)
			{
				std::string value;
				if (populate_entries)
				{
					value = cfgLoadStr(current_backend->slug.c_str(), options->cfg_name.c_str(), "");
					(*cfg_entries)[options->cfg_name] = value;
				}
				value = (*cfg_entries)[options->cfg_name];

				if (options->type == integer)
				{
					int val = stoi(value);
					ImGui::SliderInt(options->caption.c_str(), &val, options->min_value, options->max_value);
					(*cfg_entries)[options->cfg_name] = to_string(val);
				}
				else if (options->type == checkbox)
				{
					bool check = (value == "1");
					ImGui::Checkbox(options->caption.c_str(), &check);
					std::string cur = check ? "1" : "0";
					(*cfg_entries)[options->cfg_name] = cur;
				}
				else if (options->type == ::list)
				{
					if (ImGui::BeginCombo(options->caption.c_str(), value.c_str(), ImGuiComboFlags_None))
					{
						bool is_selected = false;
						std::vector<std::string> list_items = options->list_callback();
						for (std::vector<std::string>::iterator it = list_items.begin() ; it != list_items.end(); ++it)
						{
							std::string cur = (std::string)*it;
							is_selected = (value == cur);
							if (ImGui::Selectable(cur.c_str(), &is_selected))
							{
								(*cfg_entries)[options->cfg_name] = cur;
							}

							if (is_selected)
								ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}
				}
				else {
					printf("Unknown option\n");
				}

				options++;
			}
		}

		ImGui::PopStyleVar();
		ImGui::EndTabItem();
	}
}