#if defined(USE_EVDEV)
#include "handler_evdev.h"
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>

#define EVDEV_DEVICE_STRING "/dev/input/event%s"

void EvdevInputHandler::get_axis_limits(const InputAxisCode code, InputAxisLimits& limits)
{
	struct input_absinfo abs;
	if(!this->m_evdev_fd < 0 || code < 0 || ioctl(this->m_evdev_fd, EVIOCGABS(code), &abs))
	{
		if(this->m_evdev_fd >= 0 && code >= 0)
		{
			perror("evdev ioctl");
		}
		limits.minimum = 0;
		limits.maximum = 0;
		limits.deadzone = 0;
		return;
	}
	limits.minimum = abs.minimum;
	limits.maximum = abs.maximum;
	limits.deadzone = abs.flat;
}

std::string EvdevInputHandler::get_api_name()
{
	return "evdev";
}

bool EvdevInputHandler::setup_device(std::string device)
{
	size_t size_needed = snprintf(NULL, 0, EVDEV_DEVICE_STRING, device.c_str()) + 1;
	char* evdev_fname = (char*)malloc(size_needed);
	sprintf(evdev_fname, EVDEV_DEVICE_STRING, device.c_str());

	printf("evdev: Trying to open device '%s'\n", evdev_fname);

	this->m_evdev_fd = open(evdev_fname, O_RDONLY);

	char device_name[256] = "Unknown";

	if (this->m_evdev_fd < 0)
	{
		printf("evdev: Opening device '%s' failed - %s", evdev_fname, strerror(errno));
		free(evdev_fname);
		return false;
	}

	fcntl(this->m_evdev_fd, F_SETFL, O_NONBLOCK);

	// Get device name
	if(ioctl(this->m_evdev_fd, EVIOCGNAME(sizeof(device_name)), device_name) < 0)
	{
		printf("evdev: Getting name of '%s' (ioctl) failed - %s", evdev_fname, strerror(errno));
		free(evdev_fname);
		return false;
	}

	printf("evdev: Found '%s' at '%s'\n", device_name, evdev_fname);
	this->m_evdev_devname = std::string(device_name);
	
	free(evdev_fname);

	return true;
}

std::string EvdevInputHandler::get_default_mapping_filename()
{
	if (this->m_evdev_devname.empty())
	{
		return "";
	}

	std::string mapping_filename;
	#if defined(TARGET_PANDORA)
		mapping_filename = "controller_pandora.cfg";
	#elif defined(TARGET_GCW0)
		mapping_filename = "controller_gcwz.cfg";
	#else
		if (strstr(this->m_evdev_devname.c_str(), "Microsoft X-Box 360 pad") == NULL || strstr(this->m_evdev_devname.c_str(), "Xbox 360 Wireless Receiver") == NULL)
		{
			mapping_filename = "controller_xpad.cfg";
		}
		else if (strstr(this->m_evdev_devname.c_str(), "Xbox Gamepad (userspace driver)") != NULL)
		{
			mapping_filename = "controller_xboxdrv.cfg";
		}
		else if (strstr(this->m_evdev_devname.c_str(), "keyboard") != NULL || strstr(this->m_evdev_devname.c_str(), "Keyboard") != NULL)
		{
			mapping_filename = "keyboard.cfg";
		}
		else
		{
			mapping_filename = "controller_generic.cfg";
		}
	#endif
		
	return mapping_filename;
}

void EvdevInputHandler::handle()
{
	if (!this->is_initialized())
	{
		return;
	}

	input_event ie;
	while(read(this->m_evdev_fd, &ie, sizeof(ie)) == sizeof(ie))
	{
		//printf("evdev: type = %d  - code = %d - value = %d\n", ie.type, ie.code, ie.value);
		switch(ie.type)
		{
			case EV_KEY:
			{
				this->handle_button(ie.code, ie.value);
				break;
			}
			case EV_ABS:
			{
				this->handle_axis(ie.code, ie.value);
				break;
			}
		}
	}
}
#endif
