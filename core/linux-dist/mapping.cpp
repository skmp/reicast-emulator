#include "linux-dist/mapping.h"
#include "cfg/ini.h"

struct ButtonMap
{
	InputButtonID id;
	string section;
	string option;
};

struct AxisMap
{
	InputAxisID id;
	string section;
	string option;
	string section_inverted;
	string option_inverted;
};

static ButtonMap button_list[19] = {
	{ DC_BTN_A, "dreamcast", "btn_a" },
	{ DC_BTN_B, "dreamcast", "btn_b" },
	{ DC_BTN_C, "dreamcast", "btn_c" },
	{ DC_BTN_D, "dreamcast", "btn_d" },
	{ DC_BTN_X, "dreamcast", "btn_x" },
	{ DC_BTN_Y, "dreamcast", "btn_y" },
	{ DC_BTN_Z, "dreamcast", "btn_z" },
	{ DC_BTN_START, "dreamcast", "btn_start" },
	{ DC_BTN_DPAD1_LEFT, "dreamcast", "btn_dpad1_left" },
	{ DC_BTN_DPAD1_RIGHT, "dreamcast", "btn_dpad1_right" },
	{ DC_BTN_DPAD1_UP, "dreamcast", "btn_dpad1_up" },
	{ DC_BTN_DPAD1_DOWN, "dreamcast", "btn_dpad1_down" },
	{ DC_BTN_DPAD2_LEFT, "dreamcast", "btn_dpad2_left" },
	{ DC_BTN_DPAD2_RIGHT, "dreamcast", "btn_dpad2_right" },
	{ DC_BTN_DPAD2_UP, "dreamcast", "btn_dpad2_up" },
	{ DC_BTN_DPAD2_DOWN, "dreamcast", "btn_dpad2_down" },
	{ EMU_BTN_ESCAPE, "emulator", "btn_escape" },
	{ EMU_BTN_TRIGGER_LEFT, "compat", "btn_trigger_left" },
	{ EMU_BTN_TRIGGER_RIGHT, "compat", "btn_trigger_right" }
};

static AxisMap axis_list[8] = {
	{ DC_AXIS_X, "dreamcast", "axis_x", "compat", "axis_x_inverted" },
	{ DC_AXIS_Y, "dreamcast", "axis_y", "compat", "axis_y_inverted" },
	{ DC_AXIS_TRIGGER_LEFT,  "dreamcast", "axis_trigger_left",  "compat", "axis_trigger_left_inverted" },
	{ DC_AXIS_TRIGGER_RIGHT, "dreamcast", "axis_trigger_right", "compat", "axis_trigger_right_inverted" },
	{ EMU_AXIS_DPAD1_X, "compat", "axis_dpad1_x", "compat", "axis_dpad1_x_inverted" },
	{ EMU_AXIS_DPAD1_Y, "compat", "axis_dpad1_y", "compat", "axis_dpad1_y_inverted" },
	{ EMU_AXIS_DPAD2_X, "compat", "axis_dpad2_x", "compat", "axis_dpad2_x_inverted" },
	{ EMU_AXIS_DPAD2_Y, "compat", "axis_dpad2_y", "compat", "axis_dpad2_y_inverted" }
};

const InputButtonID* InputMapping::get_button_id(InputButtonCode code)
{
	return this->buttons.get_by_value(code);
}

const InputButtonCode* InputMapping::get_button_code(InputButtonID id)
{
	return this->buttons.get_by_key(id);
}

void InputMapping::set_button(InputButtonID id, InputButtonCode code)
{
	if(id != EMU_BTN_NONE)
	{
		this->buttons.insert(id, code);
	}
}

const InputAxisID* InputMapping::get_axis_id(InputAxisCode code)
{
	return this->axes.get_by_value(code);
}

const InputAxisCode* InputMapping::get_axis_code(InputAxisID id)
{
	return this->axes.get_by_key(id);
}

bool InputMapping::get_axis_inverted(InputAxisCode code)
{
	std::map<InputAxisCode, bool>::iterator it = this->axes_inverted.find('b');
	if (it != this->axes_inverted.end())
	{
		return it->second;
	}
	return false;
}

void InputMapping::set_axis(InputAxisID id, InputAxisCode code, bool is_inverted)
{
	if(id != EMU_AXIS_NONE)
	{
		this->axes.insert(id, code);
		this->axes_inverted.insert(std::pair<InputAxisCode,bool>(code, is_inverted));
	}
}

void InputMapping::load(FILE* fd)
{
	ConfigFile mf;
	mf.parse(fd);

	this->name = mf.get("emulator", "mapping_name", "<Unknown>").c_str();

	int i;
	for(i = 0; i < 19; i++)
	{
		InputButtonCode button_code = mf.get_int(button_list[i].section, button_list[i].option, -1);
		if (button_code >= 0)
		{
			this->set_button(button_list[i].id, button_code);
		}
	}

	for(i = 0; i < 8; i++)
	{
		InputAxisCode axis_code = mf.get_int(axis_list[i].section, axis_list[i].option, -1);
		if(axis_code >= 0)
		{
			this->set_axis(axis_list[i].id, axis_code, mf.get_bool(axis_list[i].section_inverted, axis_list[i].option_inverted, false));
		}
	}
}

void InputMapping::print()
{
	int i;

	printf("\nMAPPING NAME: %s\n", this->name);

	puts("MAPPED BUTTONS:");
	for(i = 0; i < 19; i++)
	{
		const InputButtonCode* button_code = this->get_button_code(button_list[i].id);
		if(button_code)
		{
			printf("%-18s = %d\n", button_list[i].option.c_str(), *button_code);
		}
	}

	puts("MAPPED AXES:");
	for(i = 0; i < 8; i++)
	{
		const InputAxisCode* axis_code = this->get_axis_code(axis_list[i].id);
		if(axis_code)
		{
			printf("%-18s = %d", axis_list[i].option.c_str(), *axis_code);
			if(this->get_axis_inverted(*axis_code))
			{
				printf("%s", "(inverted)");
			}
			printf("%s", "\n");
		}
	}
	puts("");
}

