#pragma once
#include <linux/input.h>
#include "types.h"
#include "linux-dist/mapping.h"

struct EvdevAxisData
{
	s32 range; // smaller size than 32 bit might cause integer overflows
	s32 min;
	void init(int fd, InputAxisCode code, bool inverted);
	s8 convert(s32 value);
};

struct EvdevController
{
	int fd;
	InputMapping* mapping;
	EvdevAxisData data_x;
	EvdevAxisData data_y;
	EvdevAxisData data_trigger_left;
	EvdevAxisData data_trigger_right;
	void init();
};

#define EVDEV_DEVICE_CONFIG_KEY "evdev_device_id_%d"
#define EVDEV_MAPPING_CONFIG_KEY "evdev_mapping_%d"
#define EVDEV_DEVICE_STRING "/dev/input/event%d"
#define EVDEV_MAPPING_PATH "/mappings/%s"

#ifdef TARGET_PANDORA
	#define EVDEV_DEFAULT_DEVICE_ID_1 4
#else
	#define EVDEV_DEFAULT_DEVICE_ID_1 0
#endif

#define EVDEV_DEFAULT_DEVICE_ID(port) (port == 1 ? EVDEV_DEFAULT_DEVICE_ID_1 : -1)

extern int input_evdev_init(EvdevController* controller, const char* device, const char* mapping_fname);
extern bool input_evdev_handle(EvdevController* controller, u32 port);
