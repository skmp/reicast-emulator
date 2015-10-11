#include <cstddef>
#include "libreicast.h"
#include "linux.h"

extern void sdl_set_hwnd(void* hwnd); // FIXME: This and everything related to it is hacky as hell

int libreicast_init(int argc, char* argv[])
{
	return libreicast_init(argc, argv, NULL);
}

int libreicast_init(int argc, char* argv[], void* hwnd)
{
	if(hwnd != NULL)
	{
		sdl_set_hwnd(hwnd);
	}
	return reicast_init(argc, argv);
}

int libreicast_run()
{
	return reicast_run();
}

int libreicast_term()
{
	return reicast_term();
}
