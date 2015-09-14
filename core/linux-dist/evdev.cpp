#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>
#include "linux-dist/evdev.h"
#include "linux-dist/main.h"
#include "cfg/ini.h"
#include <vector>
#include <map>
#include <dlfcn.h>

#if defined(USE_EVDEV)
	bool libevdev_tried = false;
	bool libevdev_available = false;
	typedef int (*libevdev_func1_t)(int, const char*);
	typedef const char* (*libevdev_func2_t)(int, int);
	libevdev_func1_t libevdev_event_code_from_name;
	libevdev_func2_t libevdev_event_code_get_name;

	void load_libevdev()
	{
		if (libevdev_tried)
		{
			return;
		}

		libevdev_tried = true;
		void* lib_handle = dlopen("libevdev.so", RTLD_NOW);

		bool failed = false;

		if (!lib_handle)
		{
			fprintf(stderr, "%s\n", dlerror());
			failed = true;
		}
		else
		{
			libevdev_event_code_from_name = reinterpret_cast<libevdev_func1_t>(dlsym(lib_handle, "libevdev_event_code_from_name"));

			const char* error1 = dlerror();
			if (error1 != NULL)
			{
				fprintf(stderr, "%s\n", error1);
				failed = true;
			}

			libevdev_event_code_get_name = reinterpret_cast<libevdev_func2_t>(dlsym(lib_handle, "libevdev_event_code_get_name"));

			const char* error2 = dlerror();
			if (error2 != NULL)
			{
				fprintf(stderr, "%s\n", error2);
				failed = true;
			}
		}

		if(failed)
		{
			puts("WARNING: libevdev is not available. You'll not be able to use button names instead of numeric codes in your controller mappings!\n");
			return;
		}

		libevdev_available = true;
	}

	s8 EvdevAxisData::convert(s32 value)
	{
		return (((value - min) * 255) / range);
	}

	void EvdevAxisData::init(int fd, int code, bool inverted)
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
		ControllerAxis* axis_x  = this->mapping->get_axis_by_id(DC_AXIS_X);
		ControllerAxis* axis_y  = this->mapping->get_axis_by_id(DC_AXIS_Y);
		ControllerAxis* axis_lt = this->mapping->get_axis_by_id(DC_AXIS_TRIGGER_LEFT);
		ControllerAxis* axis_rt = this->mapping->get_axis_by_id(DC_AXIS_TRIGGER_RIGHT);
		this->data_x.init(this->fd, axis_x->code, axis_x->inverted);
		this->data_y.init(this->fd, axis_y->code, axis_y->inverted);
		this->data_trigger_left.init(this->fd, axis_lt->code, axis_lt->inverted);
		this->data_trigger_right.init(this->fd, axis_rt->code, axis_rt->inverted);
	}

	std::map<std::string, ControllerMapping> loaded_mappings;

	int input_evdev_init(EvdevController* controller, const char* device, const char* custom_mapping_fname = NULL)
	{
		load_libevdev();

		char name[256] = "Unknown";

		printf("evdev: Trying to open device at '%s'\n", device);

		int fd = open(device, O_RDONLY);

		if (fd >= 0)
		{
			fcntl(fd, F_SETFL, O_NONBLOCK);
			if(ioctl(fd, EVIOCGNAME(sizeof(name)), name) < 0)
			{
				perror("evdev: ioctl");
				return -2;
			}
			else
			{
				printf("evdev: Found '%s' at '%s'\n", name, device);

				controller->fd = fd;

				const char* mapping_fname;

				if(custom_mapping_fname != NULL)
				{
					mapping_fname = custom_mapping_fname;
				}
				else
				{
					#if defined(TARGET_PANDORA)
						mapping_fname = "controller_pandora.cfg";
					#elif defined(TARGET_GCW0)
						mapping_fname = "controller_gcwz.cfg";
					#else
						if (strcmp(name, "Microsoft X-Box 360 pad") == 0 ||
							strcmp(name, "Xbox 360 Wireless Receiver") == 0 ||
							strcmp(name, "Xbox 360 Wireless Receiver (XBOX)") == 0)
						{
							mapping_fname = "controller_xpad.cfg";
						}
						else if (strstr(name, "Xbox Gamepad (userspace driver)") != NULL)
						{
							mapping_fname = "controller_xboxdrv.cfg";
						}
						else if (strstr(name, "keyboard") != NULL ||
								 strstr(name, "Keyboard") != NULL)
						{
							mapping_fname = "keyboard.cfg";
						}
						else
						{
							mapping_fname = "controller_generic.cfg";
						}
					#endif
				}
				if(loaded_mappings.count(string(mapping_fname)) == 0)
				{
					FILE* mapping_fd = NULL;
					if(mapping_fname[0] == '/')
					{
						// Absolute mapping
						mapping_fd = fopen(mapping_fname, "r");
					}
					else
					{
						// Mapping from ~/.reicast/mappings/
						size_t size_needed = snprintf(NULL, 0, EVDEV_MAPPING_PATH, mapping_fname) + 1;
						char* mapping_path = (char*)malloc(size_needed);
						sprintf(mapping_path, EVDEV_MAPPING_PATH, mapping_fname);
						printf("evdev: reading mapping path: '%s'\n", get_readonly_data_path(mapping_path).c_str());
						mapping_fd = fopen(get_readonly_data_path(mapping_path).c_str(), "r");
						free(mapping_path);
					}
					
					if(mapping_fd != NULL)
					{
						printf("evdev: reading mapping file: '%s'\n", mapping_fname);
						ControllerMapping mapping;
						mapping.load(mapping_fd);
						puts(mapping.name);
						loaded_mappings.insert(std::make_pair(string(mapping_fname), mapping));
						fclose(mapping_fd);
					}
					else
					{
						printf("evdev: unable to open mapping file '%s'\n", mapping_fname);
						perror("evdev");
						return -3;
					}
				}
				controller->mapping = &loaded_mappings[string(mapping_fname)];
				printf("evdev: Using '%s' mapping\n", controller->mapping->name);
				controller->init();

				return 0;
			}
		}
		else
		{
			perror("evdev: open");
			return -1;
		}
	}

	bool input_evdev_handle(EvdevController* controller, u32 port)
	{
		#define SET_FLAG(field, mask, expr) field =((expr) ? (field & ~mask) : (field | mask))
		if (controller->fd < 0 || controller->mapping == NULL)
		{
			return false;
		}

		input_event ie;
		DreamcastController button;
		ControllerAxis* axis;

		while(read(controller->fd, &ie, sizeof(ie)) == sizeof(ie))
		{
			switch(ie.type)
			{
				case EV_KEY:
					button = controller->mapping->get_button(ie.code);
					printf("BTN: %d, %d\n", ie.code, controller->mapping->get_button(ie.code));
					switch(button)
					{
						case EMU_BTN_NONE:
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
							SET_FLAG(kcode[port], button, ie.value);
					}
					break;
				case EV_ABS:
					axis = controller->mapping->get_axis_by_code(ie.code);
					printf("ABS: %d, %d\n", ie.code, axis->id);
					switch(axis->id)
					{
						case EMU_AXIS_DPAD1_X:
						case EMU_AXIS_DPAD1_Y:
						case EMU_AXIS_DPAD2_X:
						case EMU_AXIS_DPAD2_Y:
							DreamcastController axis_buttons[2];
							switch(axis->id)
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
			}
		}
	}
#endif

