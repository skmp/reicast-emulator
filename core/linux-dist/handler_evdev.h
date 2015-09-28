#pragma once
#include "handler.h"

class EvdevInputHandler : public InputHandler
{
	private:
		int m_evdev_fd;
		std::string m_evdev_devname;
		void get_axis_limits(const InputAxisCode code, InputAxisLimits& limits);
	public:
		void handle();
		std::string get_api_name();
		std::string get_default_mapping_filename();
		bool setup_device(std::string device);
};