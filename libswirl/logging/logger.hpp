#ifndef __logger__hpp___

enum log_verbosity_level_e {
	default_level = 0,
	lowt_level,
	hight_level,
};

enum log_output_mode_e {
	default_mode = 0,
	stdio_mode,
	stream_mode
};

#define log_printf


#endif
