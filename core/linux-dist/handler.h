#pragma once
#include "types.h"
#include "mapping.h"
#include "mappingstore.h"

struct InputAxisLimits
{
	s32 minimum;
	s32 maximum;
	s32 deadzone;
};

class InputAxisConverter
{
	private:
		s32 m_minimum;
		s32 m_range;
		s32 m_deadzone;
	public:
		InputAxisConverter(bool inverted, InputAxisLimits limits);
		s8 convert(s32 value);
};

class InputHandler
{
	typedef std::map<InputAxisID, InputAxisConverter*> InputAxisConverterStore;
	private:
		InputAxisConverterStore m_axis_converters;
		void enable_axis_converter(InputAxisID axis, bool inverted, InputAxisLimits limits);
		void disable_axis_converter(InputAxisID axis);
		InputAxisConverter* get_axis_converter(InputAxisID axis);
		bool m_initialized;
	protected:
		static InputMappingStore s_mappingstore;
		u32 m_port;
		InputMapping* m_mapping;
		bool is_initialized();
		void handle_button(InputButtonCode code, int value);
		void handle_axis(InputAxisCode code, int value);
		virtual void get_axis_limits(const InputAxisCode code, InputAxisLimits& limits);
	public:
		InputHandler();
		~InputHandler();
		bool initialize(u32 port, std::string device, std::string custom_mapping);
		virtual std::string get_default_mapping_filename();
		virtual void handle() = 0;
		virtual std::string get_api_name() = 0;
		virtual bool setup_device(std::string device) = 0;
};