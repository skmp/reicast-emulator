#include "linux-dist/mapping.h"
#include "cfg/ini.h"

DreamcastController ControllerMapping::get_button(int code)
{
	if(this->buttons.count(code) != 1)
	{
		return EMU_BTN_NONE;
	}
	return this->buttons[code];
}

void ControllerMapping::set_button(int code, DreamcastController button_id)
{
	if(button_id != EMU_BTN_NONE)
	{
		this->buttons[code] = button_id;
	}
}

ControllerAxis* ControllerMapping::get_axis_by_code(int code)
{
	if(this->axis_codes.count(code) != 1)
	{
		return NULL;
	}
	return &this->axis_codes[code];
}

ControllerAxis* ControllerMapping::get_axis_by_id(DreamcastController axis_id)
{
	if(this->axis_ids.count(axis_id) != 1)
	{
		return NULL;
	}
	return this->axis_ids[axis_id];
}

void ControllerMapping::set_axis(int code, ControllerAxis axis)
{
	if(axis.id != EMU_AXIS_NONE)
	{
		this->axis_codes[code] = axis;
		this->axis_ids[axis.id] = &this->axis_codes[code];
	}
}
void ControllerMapping::load(FILE* fd)
{
	ConfigFile mf;
	mf.parse(fd);

	this->name = mf.get("emulator", "mapping_name", "<Unknown>").c_str();
	puts(this->name);
	this->set_button(mf.get_int("dreamcast", "btn_a", -1),             DC_BTN_A);
	this->set_button(mf.get_int("dreamcast", "btn_b", -1),             DC_BTN_B);
	this->set_button(mf.get_int("dreamcast", "btn_c", -1),             DC_BTN_C);
	this->set_button(mf.get_int("dreamcast", "btn_d", -1),             DC_BTN_D);
	this->set_button(mf.get_int("dreamcast", "btn_x", -1),             DC_BTN_X);
	this->set_button(mf.get_int("dreamcast", "btn_y", -1),             DC_BTN_Y);
	this->set_button(mf.get_int("dreamcast", "btn_z", -1),             DC_BTN_Z);
	this->set_button(mf.get_int("dreamcast", "btn_start", -1),         DC_BTN_START);
	this->set_button(mf.get_int("dreamcast", "btn_dpad1_left", -1),    DC_BTN_DPAD1_LEFT);
	this->set_button(mf.get_int("dreamcast", "btn_dpad1_right", -1),   DC_BTN_DPAD1_RIGHT);
	this->set_button(mf.get_int("dreamcast", "btn_dpad1_up", -1),      DC_BTN_DPAD1_UP);
	this->set_button(mf.get_int("dreamcast", "btn_dpad1_down", -1),    DC_BTN_DPAD1_DOWN);
	this->set_button(mf.get_int("dreamcast", "btn_dpad2_left", -1),    DC_BTN_DPAD2_LEFT);
	this->set_button(mf.get_int("dreamcast", "btn_dpad2_right", -1),   DC_BTN_DPAD2_RIGHT);
	this->set_button(mf.get_int("dreamcast", "btn_dpad2_up", -1),      DC_BTN_DPAD2_UP);
	this->set_button(mf.get_int("dreamcast", "btn_dpad2_down", -1),    DC_BTN_DPAD2_DOWN);
	this->set_button(mf.get_int("emulator",  "btn_escape", -1),        EMU_BTN_ESCAPE);
	this->set_button(mf.get_int("compat",    "btn_trigger_left", -1),  EMU_BTN_TRIGGER_LEFT);
	this->set_button(mf.get_int("compat",    "btn_trigger_right", -1), EMU_BTN_TRIGGER_RIGHT);
	this->set_axis(mf.get_int("dreamcast", "axis_x", -1), {
		DC_AXIS_X,
		mf.get_int("dreamcast", "axis_x", -1),
		mf.get_bool("compat", "axis_x_inverted", false)
	});
	this->set_axis(mf.get_int("dreamcast", "axis_y", -1), {
		DC_AXIS_Y,
		mf.get_int("dreamcast", "axis_y", -1),
		mf.get_bool("compat", "axis_y_inverted", false)
	});
	this->set_axis(mf.get_int("dreamcast", "axis_trigger_left", -1), {
		DC_AXIS_TRIGGER_LEFT,
		mf.get_int("dreamcast", "axis_trigger_left", -1),
		mf.get_bool("compat", "axis_trigger_left_inverted", false)
	});
	this->set_axis(mf.get_int("dreamcast", "axis_trigger_right", -1), {
		DC_AXIS_TRIGGER_RIGHT,
		mf.get_int("dreamcast", "axis_trigger_right", -1),
		mf.get_bool("compat", "axis_trigger_right_inverted", false)
	});
	this->set_axis(mf.get_int("compat", "axis_dpad1_x", -1), { EMU_AXIS_DPAD1_X, mf.get_int("compat", "axis_dpad1_x", -1), false });
	this->set_axis(mf.get_int("compat", "axis_dpad1_y", -1), { EMU_AXIS_DPAD1_Y, mf.get_int("compat", "axis_dpad1_y", -1), false });
	this->set_axis(mf.get_int("compat", "axis_dpad2_x", -1), { EMU_AXIS_DPAD2_X, mf.get_int("compat", "axis_dpad2_x", -1), false });
	this->set_axis(mf.get_int("compat", "axis_dpad2_y", -1), { EMU_AXIS_DPAD2_Y, mf.get_int("compat", "axis_dpad2_y", -1), false });
}

