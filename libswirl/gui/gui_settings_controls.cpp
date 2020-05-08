/*
	This file is part of libswirl
*/
#include "license/bsd"


#include "types.h"
#include "gui.h"
#include "stdclass.h"
#include "oslib/oslib.h"
#include "imgui/imgui.h"
#include "gui_partials.h"
#include "gui_util.h"

#include "input/gamepad_device.h"
#include "input/keyboard_device.h"
#include "hw/maple/maple_if.h"

#include "gui_android.h"

const char *maple_device_types[] = { "None", "Sega Controller", "Light Gun", "Keyboard", "Mouse" };
const char *maple_expansion_device_types[] = { "None", "Sega VMU", "Purupuru", "Microphone" };

static const char *maple_device_name(MapleDeviceType type)
{
	switch (type)
	{
	case MDT_SegaController:
		return maple_device_types[1];
	case MDT_LightGun:
		return maple_device_types[2];
	case MDT_Keyboard:
		return maple_device_types[3];
	case MDT_Mouse:
		return maple_device_types[4];
	case MDT_None:
	default:
		return maple_device_types[0];
	}
}

static MapleDeviceType maple_device_type_from_index(int idx)
{
	switch (idx)
	{
	case 1:
		return MDT_SegaController;
	case 2:
		return MDT_LightGun;
	case 3:
		return MDT_Keyboard;
	case 4:
		return MDT_Mouse;
	case 0:
	default:
		return MDT_None;
	}
}

static const char *maple_expansion_device_name(MapleDeviceType type)
{
	switch (type)
	{
	case MDT_SegaVMU:
		return maple_expansion_device_types[1];
	case MDT_PurupuruPack:
		return maple_expansion_device_types[2];
	case MDT_Microphone:
		return maple_expansion_device_types[3];
	case MDT_None:
	default:
		return maple_expansion_device_types[0];
	}
}

const char *maple_ports[] = { "None", "A", "B", "C", "D" };
const DreamcastKey button_keys[] = { DC_BTN_START, DC_BTN_A, DC_BTN_B, DC_BTN_X, DC_BTN_Y, DC_DPAD_UP, DC_DPAD_DOWN, DC_DPAD_LEFT, DC_DPAD_RIGHT,
		EMU_BTN_MENU, EMU_BTN_ESCAPE, EMU_BTN_TRIGGER_LEFT, EMU_BTN_TRIGGER_RIGHT,
		DC_BTN_C, DC_BTN_D, DC_BTN_Z, DC_DPAD2_UP, DC_DPAD2_DOWN, DC_DPAD2_LEFT, DC_DPAD2_RIGHT };
const char *button_names[] = { "Start", "A", "B", "X", "Y", "DPad Up", "DPad Down", "DPad Left", "DPad Right",
		"Menu", "Exit", "Left Trigger", "Right Trigger",
		"C", "D", "Z", "Right Dpad Up", "Right DPad Down", "Right DPad Left", "Right DPad Right" };
const DreamcastKey axis_keys[] = { DC_AXIS_X, DC_AXIS_Y, DC_AXIS_LT, DC_AXIS_RT, EMU_AXIS_DPAD1_X, EMU_AXIS_DPAD1_Y, EMU_AXIS_DPAD2_X, EMU_AXIS_DPAD2_Y };
const char *axis_names[] = { "Stick X", "Stick Y", "Left Trigger", "Right Trigger", "DPad X", "DPad Y", "Right DPad X", "Right DPad Y" };

static MapleDeviceType maple_expansion_device_type_from_index(int idx)
{
	switch (idx)
	{
	case 1:
		return MDT_SegaVMU;
	case 2:
		return MDT_PurupuruPack;
	case 3:
		return MDT_Microphone;
	case 0:
	default:
		return MDT_None;
	}
}

static std::shared_ptr<GamepadDevice> mapped_device;
static u32 mapped_code;
static double map_start_time;

static void input_detected(u32 code)
{
	mapped_code = code;
}

static void detect_input_popup(int index, bool analog)
{
	ImVec2 padding = ImVec2(20 * scaling, 20 * scaling);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padding);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, padding);
	if (ImGui::BeginPopupModal(analog ? "Map Axis" : "Map Button", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
	{
		ImGui::Text("Waiting for %s '%s'...", analog ? "axis" : "button", analog ? axis_names[index] : button_names[index]);
		double now = os_GetSeconds();
		ImGui::Text("Time out in %d s", (int)(5 - (now - map_start_time)));
		if (mapped_code != -1)
		{
			InputMapping *input_mapping = mapped_device->get_input_mapping();
			if (input_mapping != NULL)
			{
				if (analog)
				{
					u32 previous_mapping = input_mapping->get_axis_code(axis_keys[index]);
					bool inverted = false;
					if (previous_mapping != -1)
						inverted = input_mapping->get_axis_inverted(previous_mapping);
					// FIXME Allow inverted to be set
					input_mapping->set_axis(axis_keys[index], mapped_code, inverted);
				}
				else
					input_mapping->set_button(button_keys[index], mapped_code);
			}
			mapped_device = NULL;
			ImGui::CloseCurrentPopup();
		}
		else if (now - map_start_time >= 5)
		{
			mapped_device = NULL;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	ImGui::PopStyleVar(2);
}

static void controller_mapping_popup(std::shared_ptr<GamepadDevice> gamepad)
{
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImVec2(screen_width, screen_height));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	if (ImGui::BeginPopupModal("Controller Mapping", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
	{
		const float width = screen_width / 2;
		const float col0_width = ImGui::CalcTextSize("Right DPad Downxxx").x + ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetStyle().ItemSpacing.x;
		const float col1_width = width
				- ImGui::GetStyle().GrabMinSize
				- (col0_width + ImGui::GetStyle().ItemSpacing.x)
				- (ImGui::CalcTextSize("Map").x + ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetStyle().ItemSpacing.x);

		InputMapping *input_mapping = gamepad->get_input_mapping();
		if (input_mapping == NULL || ImGui::Button("Done", ImVec2(100 * scaling, 30 * scaling)))
		{
			ImGui::CloseCurrentPopup();
			gamepad->save_mapping();
		}
		ImGui::SetItemDefaultFocus();

		char key_id[32];
		ImGui::BeginGroup();
		ImGui::Text("  Buttons  ");

		ImGui::BeginChildFrame(ImGui::GetID("buttons"), ImVec2(width, 0), ImGuiWindowFlags_None);
		ImGui::Columns(3, "bindings", false);
		ImGui::SetColumnWidth(0, col0_width);
		ImGui::SetColumnWidth(1, col1_width);
		for (int j = 0; j < ARRAY_SIZE(button_keys); j++)
		{
			sprintf(key_id, "key_id%d", j);
			ImGui::PushID(key_id);
			ImGui::Text("%s", button_names[j]);
			ImGui::NextColumn();
			u32 code = input_mapping->get_button_code(button_keys[j]);
			if (code != -1)
				ImGui::Text("%d", code);
			ImGui::NextColumn();
			if (ImGui::Button("Map"))
			{
				map_start_time = os_GetSeconds();
				ImGui::OpenPopup("Map Button");
				mapped_device = gamepad;
				mapped_code = -1;
				gamepad->detect_btn_input(&input_detected);
			}
			detect_input_popup(j, false);
			ImGui::NextColumn();
			ImGui::PopID();
		}
		ImGui::EndChildFrame();
		ImGui::EndGroup();

		ImGui::SameLine();

		ImGui::BeginGroup();
		ImGui::Text("  Analog Axes  ");
		ImGui::BeginChildFrame(ImGui::GetID("analog"), ImVec2(width, 0), ImGuiWindowFlags_None);
		ImGui::Columns(3, "bindings", false);
		ImGui::SetColumnWidth(0, col0_width);
		ImGui::SetColumnWidth(1, col1_width);

		for (int j = 0; j < ARRAY_SIZE(axis_keys); j++)
		{
			sprintf(key_id, "axis_id%d", j);
			ImGui::PushID(key_id);
			ImGui::Text("%s", axis_names[j]);
			ImGui::NextColumn();
			u32 code = input_mapping->get_axis_code(axis_keys[j]);
			if (code != -1)
				ImGui::Text("%d", code);
			ImGui::NextColumn();
			if (ImGui::Button("Map"))
			{
				map_start_time = os_GetSeconds();
				ImGui::OpenPopup("Map Axis");
				mapped_device = gamepad;
				mapped_code = -1;
				gamepad->detect_axis_input(&input_detected);
			}
			detect_input_popup(j, true);
			ImGui::NextColumn();
			ImGui::PopID();
		}
		ImGui::EndChildFrame();
		ImGui::EndGroup();
		ImGui::EndPopup();
	}
	ImGui::PopStyleVar();
}

void gui_settings_controls()
{
	if (ImGui::BeginTabItem("Controls"))
	{
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
#if DC_PLATFORM == DC_PLATFORM_DREAMCAST || DC_PLATFORM == DC_PLATFORM_ATOMISWAVE
	    if (ImGui::CollapsingHeader("Dreamcast Devices", ImGuiTreeNodeFlags_DefaultOpen))
	    {
			for (int bus = 0; bus < MAPLE_PORTS; bus++)
			{
				ImGui::Text("Device %c", bus + 'A');
				ImGui::SameLine();
#if DC_PLATFORM == DC_PLATFORM_DREAMCAST
				char device_name[32];
				sprintf(device_name, "##device%d", bus);
				float w = ImGui::CalcItemWidth() / 3;
				ImGui::PushItemWidth(w);
				if (ImGui::BeginCombo(device_name, maple_device_name((MapleDeviceType)settings.input.maple_devices[bus]), ImGuiComboFlags_None))
				{
					for (int i = 0; i < IM_ARRAYSIZE(maple_device_types); i++)
					{
						bool is_selected = settings.input.maple_devices[bus] == maple_device_type_from_index(i);
						if (ImGui::Selectable(maple_device_types[i], &is_selected))
						{
							settings.input.maple_devices[bus] = maple_device_type_from_index(i);
							maple_devices_changed = true;
						}
						if (is_selected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
				int port_count = settings.input.maple_devices[bus] == MDT_SegaController ? 2 : settings.input.maple_devices[bus] == MDT_LightGun ? 1 : 0;
				for (int port = 0; port < port_count; port++)
				{
					ImGui::SameLine();
					sprintf(device_name, "##device%d.%d", bus, port + 1);
					ImGui::PushID(device_name);
					if (ImGui::BeginCombo(device_name, maple_expansion_device_name((MapleDeviceType)settings.input.maple_expansion_devices[bus][port]), ImGuiComboFlags_None))
					{
						for (int i = 0; i < IM_ARRAYSIZE(maple_expansion_device_types); i++)
						{
							bool is_selected = settings.input.maple_expansion_devices[bus][port] == maple_expansion_device_type_from_index(i);
							if (ImGui::Selectable(maple_expansion_device_types[i], &is_selected))
							{
								settings.input.maple_expansion_devices[bus][port] = maple_expansion_device_type_from_index(i);
								maple_devices_changed = true;
							}
							if (is_selected)
								ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}
					ImGui::PopID();
				}
				ImGui::PopItemWidth();
#elif DC_PLATFORM == DC_PLATFORM_ATOMISWAVE
				if (MapleDevices[bus][5] != NULL)
					ImGui::Text("%s", maple_device_name(MapleDevices[bus][5]->get_device_type()));
#endif
			}
			ImGui::Spacing();
	    }
#endif
	    if (ImGui::CollapsingHeader("Physical Devices", ImGuiTreeNodeFlags_DefaultOpen))
	    {
			ImGui::Columns(4, "renderers", false);
			ImGui::Text("System");
			ImGui::SetColumnWidth(-1, ImGui::CalcTextSize("System").x + ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetStyle().ItemSpacing.x);
			ImGui::NextColumn();
			ImGui::Text("Name");
			ImGui::NextColumn();
			ImGui::Text("Port");
			ImGui::SetColumnWidth(-1, ImGui::CalcTextSize("None").x * 1.6f + ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetFrameHeight()
				+ ImGui::GetStyle().ItemInnerSpacing.x	+ ImGui::GetStyle().ItemSpacing.x);
			ImGui::NextColumn();
			ImGui::NextColumn();
			for (int i = 0; i < GamepadDevice::GetGamepadCount(); i++)
			{
				std::shared_ptr<GamepadDevice> gamepad = GamepadDevice::GetGamepad(i);
				if (!gamepad)
					continue;
				ImGui::Text("%s", gamepad->api_name().c_str());
				ImGui::NextColumn();
				ImGui::Text("%s", gamepad->name().c_str());
				ImGui::NextColumn();
				char port_name[32];
				sprintf(port_name, "##mapleport%d", i);
				ImGui::PushID(port_name);
				if (ImGui::BeginCombo(port_name, maple_ports[gamepad->maple_port() + 1]))
				{
					for (int j = -1; j < MAPLE_PORTS; j++)
					{
						bool is_selected = gamepad->maple_port() == j;
						if (ImGui::Selectable(maple_ports[j + 1], &is_selected))
							gamepad->set_maple_port(j);
						if (is_selected)
							ImGui::SetItemDefaultFocus();
					}

					ImGui::EndCombo();
				}
				ImGui::NextColumn();
				if (gamepad->remappable() && ImGui::Button("Map"))
					ImGui::OpenPopup("Controller Mapping");

				controller_mapping_popup(gamepad);

#ifdef _ANDROID
				if (gamepad->is_virtual_gamepad())
				{
					if (ImGui::Button("Edit"))
					{
						vjoy_start_editing();
						gui_state = VJoyEdit;
					}
					ImGui::SameLine();
					ImGui::SliderInt("Haptic", &settings.input.VirtualGamepadVibration, 0, 60);
				}
#endif
				ImGui::NextColumn();
				ImGui::PopID();
			}
	    }
    	ImGui::Columns(1, NULL, false);

    	ImGui::Spacing();
		ImGui::SliderInt("Mouse sensitivity", (int *)&settings.input.MouseSensitivity, 1, 500);

		ImGui::PopStyleVar();
		ImGui::EndTabItem();
	}
}