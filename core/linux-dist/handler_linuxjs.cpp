#if defined(USE_JOYSTICK)
#include "handler_linuxjs.h"
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <linux/joystick.h>

#define LINUXJS_DEVICE_STRING "/dev/input/js%s"

void LinuxJoystickInputHandler::get_axis_limits(const InputAxisCode code, InputAxisLimits& limits)
{
	// The Linux Joystick API's axes always normalized to this minimum/maximum
	limits.minimum = -32767;
	limits.maximum = 32767;
	limits.deadzone = 0;
}

std::string LinuxJoystickInputHandler::get_api_name()
{
	return "linuxjs";
}

bool LinuxJoystickInputHandler::setup_device(std::string device)
{
	size_t size_needed = snprintf(NULL, 0, LINUXJS_DEVICE_STRING, device.c_str()) + 1;
	char* linuxjs_fname = (char*)malloc(size_needed);
	sprintf(linuxjs_fname, LINUXJS_DEVICE_STRING, device.c_str());

	printf("linuxjs: Trying to open device '%s'\n", linuxjs_fname);

	this->m_linuxjs_fd = open(linuxjs_fname, O_RDONLY);

	if (this->m_linuxjs_fd < 0)
	{
		printf("linuxjs: Opening device '%s' failed - %s", linuxjs_fname, strerror(errno));
		free(linuxjs_fname);
		return false;
	}

	fcntl(this->m_linuxjs_fd, F_SETFL, O_NONBLOCK);

	char device_name[256] = "Unknown";
	int button_count = 0;
	int axis_count = 0;

	// Get device name
	if(ioctl(this->m_linuxjs_fd, JSIOCGNAME(sizeof(device_name)), device_name) < 0)
	{
		printf("linuxjs: Getting name of '%s' (ioctl) failed - %s", linuxjs_fname, strerror(errno));
		free(linuxjs_fname);
		return false;
	}

	// Get number of buttons
	if(ioctl(this->m_linuxjs_fd, JSIOCGBUTTONS, &button_count) < 0)
	{
		printf("linuxjs: Getting button count of '%s' (ioctl) failed - %s", linuxjs_fname, strerror(errno));
		free(linuxjs_fname);
		return false;
	}

	// Get number of axes
	if(ioctl(this->m_linuxjs_fd, JSIOCGAXES, &axis_count) < 0)
	{
		printf("linuxjs: Getting axis count of '%s' (ioctl) failed - %s", linuxjs_fname, strerror(errno));
		free(linuxjs_fname);
		return false;
	}

	printf("linuxjs: Found '%s' with %d axes and %d buttons at '%s'\n", device_name, axis_count, button_count, linuxjs_fname);
	this->m_linuxjs_devname = std::string(device_name);

	free(linuxjs_fname);

	return true;
}

std::string LinuxJoystickInputHandler::get_default_mapping_filename()
{
	if (this->m_linuxjs_devname.empty())
	{
		return "";
	}

	std::string mapping_filename;
	if (strstr(this->m_linuxjs_devname.c_str(), "Microsoft X-Box 360 pad") != NULL ||
		strstr(this->m_linuxjs_devname.c_str(), "Xbox Gamepad (userspace driver)") != NULL ||
		strstr(this->m_linuxjs_devname.c_str(), "Xbox 360 Wireless Receiver") != NULL)
	{
		mapping_filename = "controller_xbox360.cfg";
	}
	else
	{
		mapping_filename = "controller_generic.cfg";
	}

	return mapping_filename;
}

void LinuxJoystickInputHandler::handle()
{
	if (!this->is_initialized())
	{
		return;
	}

	struct js_event je;
	while(read(this->m_linuxjs_fd, &je, sizeof(je)) == sizeof(je))
	{
		printf("linuxjs: type = %d  - code = %d - value = %d\n", je.type, je.number, je.value);
		switch(je.type)
		{
			case JS_EVENT_BUTTON:
			{
				this->handle_button(je.number, je.value);
				break;
			}
			case JS_EVENT_AXIS:
			{
				this->handle_axis(je.number, je.value);
				break;
			}
		}
	}
}
#endif