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

void gui_settings_advanced()
{
	if (ImGui::BeginTabItem("Advanced"))
	{
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
	    if (ImGui::CollapsingHeader("MCPU Mode", ImGuiTreeNodeFlags_DefaultOpen))
	    {
			ImGui::Columns(2, "sh4_modes", false);
			ImGui::RadioButton("MCPU Dynarec", &dynarec_enabled, 1);
            ImGui::SameLine();
            gui_ShowHelpMarker("Use the dynamic recompiler. Recommended in most cases");
			ImGui::NextColumn();
			ImGui::RadioButton("MCPU Interpreter", &dynarec_enabled, 0);
            ImGui::SameLine();
            gui_ShowHelpMarker("Use the interpreter. Very slow but may help in case of a dynarec problem");
			ImGui::Columns(1, NULL, false);
	    }
	    if (ImGui::CollapsingHeader("SH4 Dynarec Options", dynarec_enabled ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None))
	    {
	    	ImGui::Checkbox("Safe Mode", &settings.dynarec.safemode);
            ImGui::SameLine();
            gui_ShowHelpMarker("Do not optimize integer division. Recommended");
	    	ImGui::Checkbox("Unstable Optimizations", &settings.dynarec.unstable_opt);
            ImGui::SameLine();
            gui_ShowHelpMarker("Enable unsafe optimizations. Will cause crash or environmental disaster");
	    	ImGui::Checkbox("Idle Skip", &settings.dynarec.idleskip);
            ImGui::SameLine();
            gui_ShowHelpMarker("Skip wait loops. Recommended");
			ImGui::PushItemWidth(ImGui::CalcTextSize("Largeenough").x);
            const char *preview = settings.dynarec.SmcCheckLevel == NoCheck ? "Faster" : settings.dynarec.SmcCheckLevel == FastCheck ? "Fast" : "Full";
			if (ImGui::BeginCombo("SMC Checks", preview	, ImGuiComboFlags_None))
			{
				bool is_selected = settings.dynarec.SmcCheckLevel == NoCheck;
				if (ImGui::Selectable("Faster", &is_selected))
					settings.dynarec.SmcCheckLevel = NoCheck;
				if (is_selected)
					ImGui::SetItemDefaultFocus();
				is_selected = settings.dynarec.SmcCheckLevel == FastCheck;
				if (ImGui::Selectable("Fast", &is_selected))
					settings.dynarec.SmcCheckLevel = FastCheck;
				if (is_selected)
					ImGui::SetItemDefaultFocus();
				is_selected = settings.dynarec.SmcCheckLevel == FullCheck;
				if (ImGui::Selectable("Full", &is_selected))
					settings.dynarec.SmcCheckLevel = FullCheck;
				if (is_selected)
					ImGui::SetItemDefaultFocus();
				ImGui::EndCombo();
			}
            ImGui::SameLine();
            gui_ShowHelpMarker("How to detect self-modifying code. Full check recommended");
	    }
		if (ImGui::CollapsingHeader("SCPU Mode", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Columns(2, "arm7_modes", false);
			ImGui::RadioButton("SCPU Dynarec", &settings.dynarec.ScpuEnable, 1);
			ImGui::SameLine();
			gui_ShowHelpMarker("Use the ARM7 dynamic recompiler. Recommended in most cases");
			ImGui::NextColumn();
			ImGui::RadioButton("SCPU Interpreter", &settings.dynarec.ScpuEnable, 0);
			ImGui::SameLine();
			gui_ShowHelpMarker("Use the ARM7 interpreter. Very slow but may help in case of a dynarec problem");
			ImGui::Columns(1, NULL, false);
		}
		if (ImGui::CollapsingHeader("DSP Mode", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Columns(2, "dsp_modes", false);
			ImGui::RadioButton("DSP Dynarec", &settings.dynarec.DspEnable, 1);
			ImGui::SameLine();
			gui_ShowHelpMarker("Use the DSP dynamic recompiler. Recommended in most cases");
			ImGui::NextColumn();
			ImGui::RadioButton("DSP Interpreter", &settings.dynarec.DspEnable, 0);
			ImGui::SameLine();
			gui_ShowHelpMarker("Use the DSP interpreter. Very slow but may help in case of a DSP dynarec problem");
			ImGui::Columns(1, NULL, false);
		}
		if (ImGui::CollapsingHeader("Other", ImGuiTreeNodeFlags_DefaultOpen))
	    {
#ifndef _ANDROID
			ImGui::Checkbox("Serial Console", &settings.debug.SerialConsole);
            ImGui::SameLine();
            gui_ShowHelpMarker("Dump the Dreamcast serial console to stdout");
#endif

#if FEAT_HAS_SERIAL_TTY
			ImGui::Checkbox("Create Virtual Serial Port", &settings.debug.VirtualSerialPort);
            ImGui::SameLine();
            gui_ShowHelpMarker("Create a PTY for use with dc-load. Requires restart.");
#endif

			ImGui::Checkbox("Dump Textures", &settings.rend.DumpTextures);
            ImGui::SameLine();
            gui_ShowHelpMarker("Dump all textures into data/texdump/<game id>");
	    }
		ImGui::PopStyleVar();
		ImGui::EndTabItem();
	}
}