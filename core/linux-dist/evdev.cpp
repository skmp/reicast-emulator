#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>
#include "linux-dist/evdev.h"
#include "linux-dist/main.h"
#include "linux-dist/mappingstore.h"
#include "cfg/ini.h"
#include <vector>
#include <map>
#include <dlfcn.h>

#if defined(USE_EVDEV)
	s8 EvdevAxisData::convert(s32 value)
	{
		return (((value - min) * 255) / range);
	}

	void EvdevAxisData::init(int fd, InputAxisCode code, bool inverted)
	{
		struct input_absinfo abs;
		if(code < 0 || ioctl(fd, EVIOCGABS(code), &abs))
		{
			if(code >= 0)
			{
				perror("evdev ioctl");
			}
			this->range = 255;
			this->min = 0;
			return;
		}
		s32 min = abs.minimum;
		s32 max = abs.maximum;
		printf("evdev: range of axis %d is from %d to %d\n", code, min, max);
		if(inverted)
		{
			this->range = (min - max);
			this->min = max;
		}
		else
		{
			this->range = (max - min);
			this->min = min;
		}
	}

	void EvdevController::init()
	{
		InputAxisID axis_ids[4] = { DC_AXIS_X, DC_AXIS_Y, DC_AXIS_TRIGGER_LEFT, DC_AXIS_TRIGGER_RIGHT };
		EvdevAxisData* axis_data[4] = { &this->data_x, &this->data_y, &this->data_trigger_left, &this->data_trigger_right };
		for(int i = 0; i < 4; i++)
		{
			const InputAxisCode* axis_code = this->mapping->get_axis_code(axis_ids[i]);
			if(axis_code)
			{
				bool axis_inverted = this->mapping->get_axis_inverted(*axis_code);
				if (axis_code != NULL)
				{
					axis_data[i]->init(this->fd, *axis_code, axis_inverted);
				}
			}
		}
	}

	static InputMappingStore mapping_store;

	int input_evdev_init(EvdevController* controller, const char* device, const char* custom_mapping_fname = NULL)
	{
		char device_name[256] = "Unknown";

		printf("evdev: Trying to open device at '%s'\n", device);

		int fd = open(device, O_RDONLY);

		if (fd < 0)
		{
			perror("evdev: open");
			return -1;
		}

		fcntl(fd, F_SETFL, O_NONBLOCK);
		if(ioctl(fd, EVIOCGNAME(sizeof(device_name)), device_name) < 0)
		{
			perror("evdev: ioctl");
			return -2;
		}

		printf("evdev: Found '%s' at '%s'\n", device_name, device);

		controller->fd = fd;
		controller->mapping = NULL;

		if(custom_mapping_fname != NULL)
		{
			controller->mapping = mapping_store.get(std::string(custom_mapping_fname));
		}

		if(controller->mapping == NULL)
		{
			std::string filename;
			#if defined(TARGET_PANDORA)
				filename = "controller_pandora.cfg";
			#elif defined(TARGET_GCW0)
				filename = "controller_gcwz.cfg";
			#else
				if (strstr(device_name, "Microsoft X-Box 360 pad") == NULL || strstr(device_name, "Xbox 360 Wireless Receiver") == NULL)
				{
					filename = "controller_xpad.cfg";
				}
				else if (strstr(device_name, "Xbox Gamepad (userspace driver)") != NULL)
				{
					filename = "controller_xboxdrv.cfg";
				}
				else if (strstr(device_name, "keyboard") != NULL || strstr(device_name, "Keyboard") != NULL)
				{
					filename = "keyboard.cfg";
				}
				else
				{
					filename = "controller_generic.cfg";
				}
			#endif
			controller->mapping = mapping_store.get(filename);
		}
		
		if(controller->mapping == NULL)
		{
			printf("evdev: No mapping available for '%s', disabling controller\n", device_name);
			return -3;
		}

		printf("evdev: Using '%s' mapping\n", controller->mapping->name);
		controller->mapping->print();
		controller->init();

		return 0;
	}

	bool input_evdev_handle(EvdevController* controller, u32 port)
	{
		#define SET_FLAG(field, mask, expr) field =((expr) ? (field & ~mask) : (field | mask))
		if (controller->fd < 0 || controller->mapping == NULL)
		{
			return false;
		}

		input_event ie;
		while(read(controller->fd, &ie, sizeof(ie)) == sizeof(ie))
		{
			switch(ie.type)
			{
				case EV_KEY:
				{
					const InputButtonID* button_id = controller->mapping->get_button_id(ie.code);
					if(button_id == NULL)
					{
						printf("Ignoring %d\n", ie.code);
						break;
					}
					switch(*button_id)
					{
						case NULL:
							printf("Ignoring %d\n", ie.code);
							break;
						case EMU_BTN_ESCAPE:
							die("death by escape key");
							break;
						case EMU_BTN_TRIGGER_LEFT:
							lt[port] = (ie.value ? 255 : 0);
							break;
						case EMU_BTN_TRIGGER_RIGHT:
							rt[port] = (ie.value ? 255 : 0);
							break;
						default:
							SET_FLAG(kcode[port], *button_id, ie.value);
					};
					break;
				}
				case EV_ABS:
				{
					const InputAxisID* axis_id = controller->mapping->get_axis_id(ie.code);
					if(axis_id == NULL)
					{
						printf("Ignoring %d\n", ie.code);
						break;
					}
					switch(*axis_id)
					{
						case EMU_AXIS_DPAD1_X:
						case EMU_AXIS_DPAD1_Y:
						case EMU_AXIS_DPAD2_X:
						case EMU_AXIS_DPAD2_Y:
							InputButtonID axis_buttons[2];
							switch(*axis_id)
							{
								case EMU_AXIS_DPAD1_X:
									axis_buttons[0] = DC_BTN_DPAD1_LEFT;
									axis_buttons[1] = DC_BTN_DPAD1_RIGHT;
									break;
								case EMU_AXIS_DPAD1_Y:
									axis_buttons[0] = DC_BTN_DPAD1_UP;
									axis_buttons[1] = DC_BTN_DPAD1_DOWN;
									break;
								case EMU_AXIS_DPAD2_X:
									axis_buttons[0] = DC_BTN_DPAD2_LEFT;
									axis_buttons[1] = DC_BTN_DPAD2_RIGHT;
									break;
								case EMU_AXIS_DPAD2_Y:
									axis_buttons[0] = DC_BTN_DPAD2_UP;
									axis_buttons[1] = DC_BTN_DPAD2_DOWN;
									break;
								default:
									axis_buttons[0] = EMU_BTN_NONE;
									axis_buttons[1] = EMU_BTN_NONE;
							}
							bool value[2];
							value[0] = (ie.value < 0);
							value[1] = (ie.value > 0);
							SET_FLAG(kcode[port], axis_buttons[0], value[0]);
							SET_FLAG(kcode[port], axis_buttons[1], value[1]);
							break;
						case DC_AXIS_X:
							joyx[port] = (controller->data_x.convert(ie.value) + 128);
							break;
						case DC_AXIS_Y:
							joyy[port] = (controller->data_y.convert(ie.value) + 128);
							break;
						case DC_AXIS_TRIGGER_LEFT:
							lt[port] = controller->data_trigger_left.convert(ie.value);
							break;
						case DC_AXIS_TRIGGER_RIGHT:
							rt[port] = controller->data_trigger_right.convert(ie.value);
					}
					break;
				}
			}
		}
	}
#endif

