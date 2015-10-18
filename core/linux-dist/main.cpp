#include "types.h"
#include "cfg/cfg.h"

#if HOST_OS==OS_LINUX
#include <poll.h>
#include <termios.h>
//#include <curses.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/time.h>
#include "hw/sh4/dyna/blockmanager.h"
#include <unistd.h>

#if defined(SUPPORT_X11)
	#include "linux-dist/x11.h"
#endif

#if defined(USE_EVDEV)
	#include "linux-dist/evdev.h"
#endif

int msgboxf(const wchar* text, unsigned int type, ...)
{
	va_list args;

	wchar temp[2048];
	va_start(args, type);
	vsprintf(temp, text, args);
	va_end(args);

	//printf(NULL,temp,VER_SHORTNAME,type | MB_TASKMODAL);
	puts(temp);
	return MBX_OK;
}

void* x11_win = 0;
void* x11_disp = 0;

void* libPvr_GetRenderTarget()
{
	return x11_win;
}

void* libPvr_GetRenderSurface()
{
	return x11_disp;
}

u16 kcode[4] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
u8 rt[4] = {0, 0, 0, 0};
u8 lt[4] = {0, 0, 0, 0};
u32 vks[4];
s8 joyx[4], joyy[4];

void emit_WriteCodeCache();

#if defined(USE_EVDEV)
	/* evdev input */
	static EvdevController evdev_controllers[4] = {
		{ -1, NULL },
		{ -1, NULL },
		{ -1, NULL },
		{ -1, NULL }
	};
#endif

void SetupInput()
{
	#if defined(USE_EVDEV)
		int evdev_device_id[4] = { -1, -1, -1, -1 };
		size_t size_needed;
		int port, i;

		char* evdev_device;

		for (port = 0; port < 4; port++)
		{
			size_needed = snprintf(NULL, 0, EVDEV_DEVICE_CONFIG_KEY, port+1) + 1;
			char* evdev_config_key = (char*)malloc(size_needed);
			sprintf(evdev_config_key, EVDEV_DEVICE_CONFIG_KEY, port+1);
			evdev_device_id[port] = cfgLoadInt("input", evdev_config_key, EVDEV_DEFAULT_DEVICE_ID(port+1));
			free(evdev_config_key);

			// Check if the same device is already in use on another port
			if (evdev_device_id[port] < 0)
			{
				printf("evdev: Controller %d disabled by config.\n", port + 1);
			}
			else
			{
				for (i = 0; i < port; i++)
				{
						if (evdev_device_id[port] == evdev_device_id[i])
						{
								die("You can't assign the same device to multiple ports!\n");
						}
				}

				size_needed = snprintf(NULL, 0, EVDEV_DEVICE_STRING, evdev_device_id[port]) + 1;
				evdev_device = (char*)malloc(size_needed);
				sprintf(evdev_device, EVDEV_DEVICE_STRING, evdev_device_id[port]);

				size_needed = snprintf(NULL, 0, EVDEV_MAPPING_CONFIG_KEY, port+1) + 1;
				evdev_config_key = (char*)malloc(size_needed);
				sprintf(evdev_config_key, EVDEV_MAPPING_CONFIG_KEY, port+1);
				const char* mapping = (cfgExists("input", evdev_config_key) == 2 ? cfgLoadStr("input", evdev_config_key, "").c_str() : NULL);
				free(evdev_config_key);

				input_evdev_init(&evdev_controllers[port], evdev_device, mapping);

				free(evdev_device);
			}
		}
	#endif

	#if defined(SUPPORT_X11)
		input_x11_init();
	#endif
}

void UpdateInputState(u32 port)
{
	#if defined(USE_EVDEV)
		input_evdev_handle(&evdev_controllers[port], port);
	#endif
}

void os_DoEvents()
{
	#if defined(SUPPORT_X11)
		input_x11_handle();
	#endif
}

void os_SetWindowText(const char * text)
{
	printf("%s\n",text);
	#if defined(SUPPORT_X11)
		x11_window_set_text(text);
	#endif
}

void os_CreateWindow()
{
	#if defined(SUPPORT_X11)
		x11_window_create();
	#endif
}

void common_linux_setup();
int dc_init(int argc,wchar* argv[]);
void dc_run();

string find_user_config_dir()
{
	// Unable to detect config dir, use the current folder
	return ".";
}

string find_user_data_dir()
{
	// Unable to detect config dir, use the current folder
	return ".";
}

std::vector<string> find_system_config_dirs()
{
	std::vector<string> dirs;
	if (getenv("XDG_DATA_DIRS") != NULL)
	{
		string s = (string)getenv("XDG_CONFIG_DIRS");

		string::size_type pos = 0;
		string::size_type n = s.find(":", pos);
		while(n != std::string::npos)
		{
			dirs.push_back(s.substr(pos, n-pos) + "/reicast");
			pos = n + 1;
			n = s.find(":", pos);
		}
		// Separator not found
		dirs.push_back(s.substr(pos) + "/reicast");
	}
	else
	{
		dirs.push_back("/etc/reicast"); // This isn't part of the XDG spec, but much more common than /etc/xdg/
		dirs.push_back("/etc/xdg/reicast");
	}
	return dirs;
}

std::vector<string> find_system_data_dirs()
{
	std::vector<string> dirs;
	if (getenv("XDG_DATA_DIRS") != NULL)
	{
		string s = (string)getenv("XDG_DATA_DIRS");

		string::size_type pos = 0;
		string::size_type n = s.find(":", pos);
		while(n != std::string::npos)
		{
			dirs.push_back(s.substr(pos, n-pos) + "/reicast");
			pos = n + 1;
			n = s.find(":", pos);
		}
		// Separator not found
		dirs.push_back(s.substr(pos) + "/reicast");
	}
	else
	{
		dirs.push_back("/usr/local/share/reicast");
		dirs.push_back("/usr/share/reicast");
	}
	return dirs;
}

static void retro_init(int argc, wchar *argv[] )
{
	/* Set directories */
	set_user_config_dir(find_user_config_dir());
	set_user_data_dir(find_user_data_dir());
	std::vector<string> dirs;
	dirs = find_system_config_dirs();
	for(unsigned int i = 0; i < dirs.size(); i++)
		add_system_data_dir(dirs[i]);
	dirs = find_system_data_dirs();
	for(unsigned int i = 0; i < dirs.size(); i++)
		add_system_data_dir(dirs[i]);
	printf("Config dir is: %s\n", get_writable_config_path("/").c_str());
	printf("Data dir is:   %s\n", get_writable_data_path("/").c_str());

	common_linux_setup();

	settings.profile.run_counts=0;

	dc_init(argc,argv);

	SetupInput();
}

int main(int argc, wchar* argv[])
{
   retro_init(argc, argv);

   dc_run();

	return 0;
}
#endif

int get_mic_data(u8* buffer) { return 0; }
int push_vmu_screen(u8* buffer) { return 0; }

void os_DebugBreak()
{
   printf("DEBUGBREAK!\n");
   exit(-1);
}



