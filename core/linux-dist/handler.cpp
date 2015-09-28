#include "handler.h"

#define SET_FLAG(field, mask, expr) field =((expr) ? (field & ~mask) : (field | mask))
static 	InputAxisID axis_ids[] = { DC_AXIS_X, DC_AXIS_Y, DC_AXIS_TRIGGER_LEFT, DC_AXIS_TRIGGER_RIGHT, EMU_AXIS_DPAD1_X, EMU_AXIS_DPAD1_Y, EMU_AXIS_DPAD2_X, EMU_AXIS_DPAD2_Y };

InputAxisConverter::InputAxisConverter(bool inverted, InputAxisLimits limits)
{
	this->m_deadzone = limits.deadzone;
	if(inverted)
	{
		this->m_range = (limits.minimum - limits.maximum);
		this->m_minimum = limits.maximum;
	}
	else
	{
		this->m_range = (limits.maximum - limits.minimum);
		this->m_minimum = limits.minimum;
	}
}

s8 InputAxisConverter::convert(s32 value)
{
	// If value is in deadzone, return 0
	if (this->m_deadzone && ((value >= 0 && value <= this->m_deadzone) || (value < 0 && value >= -this->m_deadzone)))
	{
		return 0;
	}
	if (this->m_range)
	{
		return (((value - this->m_minimum) * 255) / this->m_range);
	}
	return value;
}

InputMappingStore InputHandler::s_mappingstore;


InputHandler::InputHandler()
{
	this->m_initialized = false;
}

InputHandler::~InputHandler()
{
	//TODO;
}

bool InputHandler::initialize(u32 port, std::string device, std::string custom_mapping_filename)
{
	if(this->m_initialized)
	{
		printf("%s: Handler is already initialized!\n", this->get_api_name().c_str());
		return true;
	}

	this->m_port = port;

	bool success = this->setup_device(device);

	if(!success)
	{
		printf("%s: Initialization of device '%s' failed!\n", this->get_api_name().c_str(), device.c_str());
		return false;
	}

	if(custom_mapping_filename.empty())
	{
		this->m_mapping = NULL;
	}
	else
	{
		this->m_mapping = InputHandler::s_mappingstore.get(custom_mapping_filename, this->get_api_name());
	}

	if(this->m_mapping == NULL)
	{
		if(!custom_mapping_filename.empty())
		{
			printf("%s: Loading custom mapping '%s' failed!\n", this->get_api_name().c_str(), custom_mapping_filename.c_str());
		}
		std::string default_mapping_filename = this->get_default_mapping_filename();
		if(default_mapping_filename.empty())
		{
			printf("%s: No default mapping available!\n", this->get_api_name().c_str());
		}
		else
		{
			printf("%s: Using default mapping '%s'.\n", this->get_api_name().c_str(), default_mapping_filename.c_str());	
			this->m_mapping = InputHandler::s_mappingstore.get(default_mapping_filename, this->get_api_name());
		}
	}
	

	if(this->m_mapping == NULL)
	{
		printf("%s: Couldn't load a mapping!\n", this->get_api_name().c_str());
		return false;
	}

	for(int i = 0; i < 8; i++)
	{
		InputAxisID id = axis_ids[i];
		const InputAxisCode* code = this->m_mapping->get_axis_code(id);
		if(code != NULL)
		{
			InputAxisLimits limits;
			this->get_axis_limits(*code, limits);
			if(limits.minimum != limits.maximum)
			{
				bool inverted = this->m_mapping->get_axis_inverted(*code);
				this->enable_axis_converter(id, inverted, limits);
			}
		}
	}
	this->m_initialized = true;
	return true;
}

bool InputHandler::is_initialized()
{
	return this->m_initialized;
}

std::string InputHandler::get_default_mapping_filename()
{
	return "default.cfg";
}

void InputHandler::get_axis_limits(const InputAxisCode code, InputAxisLimits& limits)
{
	// Default stub, can be overridden in subclasses
	limits.minimum = 0;
	limits.maximum = 0;
	limits.deadzone = 0;
}

void InputHandler::handle_button(InputButtonCode code, int value)
{
	if(this->m_mapping == NULL)
	{
		return;
	}
	const InputButtonID* button_id = this->m_mapping->get_button_id(code);
	if(button_id == NULL)
	{
		printf("Ignoring %d (%d)\n", code, button_id);
		return;
	}
	switch(*button_id)
	{
		case EMU_BTN_ESCAPE:
			if(value)
			{
				die("death by escape key");
			}
			break;
		case EMU_BTN_TRIGGER_LEFT:
			lt[this->m_port] = (value ? 255 : 0);
			break;
		case EMU_BTN_TRIGGER_RIGHT:
			rt[this->m_port] = (value ? 255 : 0);
			break;
		default:
			SET_FLAG(kcode[this->m_port], *button_id, value);
	};
}

void InputHandler::handle_axis(InputAxisCode code, int value)
{
	if(this->m_mapping == NULL)
	{
		return;
	}
	const InputAxisID* axis_id = this->m_mapping->get_axis_id(code);
	if(axis_id == NULL)
	{
		printf("Ignoring %d\n", code);
		return;
	}
	switch(*axis_id)
	{
		case EMU_AXIS_DPAD1_X:
		case EMU_AXIS_DPAD1_Y:
		case EMU_AXIS_DPAD2_X:
		case EMU_AXIS_DPAD2_Y:
		{
			InputButtonID axis_button_id[2];
			switch(*axis_id)
			{
				case EMU_AXIS_DPAD1_X:
					axis_button_id[0] = DC_BTN_DPAD1_LEFT;
					axis_button_id[1] = DC_BTN_DPAD1_RIGHT;
					break;
				case EMU_AXIS_DPAD1_Y:
					axis_button_id[0] = DC_BTN_DPAD1_UP;
					axis_button_id[1] = DC_BTN_DPAD1_DOWN;
					break;
				case EMU_AXIS_DPAD2_X:
					axis_button_id[0] = DC_BTN_DPAD2_LEFT;
					axis_button_id[1] = DC_BTN_DPAD2_RIGHT;
					break;
				case EMU_AXIS_DPAD2_Y:
					axis_button_id[0] = DC_BTN_DPAD2_UP;
					axis_button_id[1] = DC_BTN_DPAD2_DOWN;
			}
			bool axis_button_value[2];
			axis_button_value[0] = (value < 0);
			axis_button_value[1] = (value > 0);
			SET_FLAG(kcode[this->m_port], axis_button_id[0], axis_button_value[0]);
			SET_FLAG(kcode[this->m_port], axis_button_id[1], axis_button_value[1]);
			break;
		}
		case DC_AXIS_X:
		case DC_AXIS_Y:
		case DC_AXIS_TRIGGER_LEFT:
		case DC_AXIS_TRIGGER_RIGHT:
		{
			InputAxisConverter* converter = this->get_axis_converter(*axis_id);
			s8 converted_value = ((converter == NULL) ? value : converter->convert(value));
			switch(*axis_id)
			{
				case DC_AXIS_X:
					joyx[this->m_port] = (converted_value + 128);
					break;
				case DC_AXIS_Y:
					joyy[this->m_port] = (converted_value + 128);
					break;
				case DC_AXIS_TRIGGER_LEFT:
					lt[this->m_port] = converted_value;
					break;
				case DC_AXIS_TRIGGER_RIGHT:
					rt[this->m_port] = converted_value;
					break;
			}
		}
	}
}

void InputHandler::enable_axis_converter(InputAxisID axis, bool inverted, InputAxisLimits limits)
{
	this->disable_axis_converter(axis); // Delete old axis converter
	this->m_axis_converters[axis] = new InputAxisConverter(inverted, limits);
}

void InputHandler::disable_axis_converter(InputAxisID axis)
{
	InputAxisConverterStore::iterator iter = this->m_axis_converters.find(axis);
	if(iter != this->m_axis_converters.end())
	{
		delete iter->second;
		this->m_axis_converters.erase(iter);
	}
}

InputAxisConverter* InputHandler::get_axis_converter(InputAxisID axis)
{
	InputAxisConverterStore::iterator iter = this->m_axis_converters.find(axis);
	if(iter == this->m_axis_converters.end())
	{
		return NULL;
	}
	return iter->second;
}