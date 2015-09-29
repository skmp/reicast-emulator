#pragma once
#include "handler.h"

class LinuxJoystickInputHandler : public InputHandler
{
	private:
		int m_linuxjs_fd;
		std::string m_linuxjs_devname;
		void get_axis_limits(const InputAxisCode code, InputAxisLimits& limits);
	public:
		void handle();
		std::string get_api_name();
		std::string get_default_mapping_filename();
		bool setup_device(std::string device);
};