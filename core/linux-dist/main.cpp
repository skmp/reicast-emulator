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
#include "proxy.h"

#if defined(TARGET_EMSCRIPTEN)
	#include <emscripten.h>
#endif

#if defined(SUPPORT_X11)
	#include "linux-dist/x11.h"
#endif

#if defined(USE_SDL)
	#include "sdl/sdl.h"
#endif

#if defined(USES_HOMEDIR)
	#include <sys/stat.h>
#endif

#if defined(USE_EVDEV)
	#include "linux-dist/handler_evdev.h"
#endif

#if defined(USE_JOYSTICK)
	#include "linux-dist/handler_linuxjs.h"
#endif

#ifdef TARGET_PANDORA
	#include <signal.h>
	#include <execinfo.h>
	#include <sys/soundcard.h>
#endif

#if FEAT_HAS_NIXPROF
#include "profiler/profiler.h"
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

static InputHandlerProxy input_handlers;

void SetupInput()
{
	#if defined(USE_EVDEV)
		#define EVDEV_DEVICE_CONFIG_KEY "evdev_device_id_%d"
		#define EVDEV_MAPPING_CONFIG_KEY "evdev_mapping_%d"

		#ifdef TARGET_PANDORA
			#define EVDEV_DEFAULT_DEVICE_ID_1 "4"
		#else
			#define EVDEV_DEFAULT_DEVICE_ID_1 "0"
		#endif
		#define EVDEV_DEFAULT_DEVICE_ID(port) (port == 1 ? EVDEV_DEFAULT_DEVICE_ID_1 : "-1")

		for (int port = 0; port < 4; port++)
		{
			size_t size_needed;
			char* evdev_config_key;
			std::string evdev_device;
			std::string custom_mapping_filename;

			size_needed = snprintf(NULL, 0, EVDEV_DEVICE_CONFIG_KEY, port+1) + 1;
			evdev_config_key = (char*)malloc(size_needed);
			sprintf(evdev_config_key, EVDEV_DEVICE_CONFIG_KEY, port+1);
			evdev_device = cfgLoadStr("input", evdev_config_key, EVDEV_DEFAULT_DEVICE_ID(port+1));
			free(evdev_config_key);

			if(evdev_device.c_str()[0] == '-')
			{
				printf("evdev: Controller %d disabled by config.\n", port + 1);
				continue;
			}

			size_needed = snprintf(NULL, 0, EVDEV_MAPPING_CONFIG_KEY, port+1) + 1;
			evdev_config_key = (char*)malloc(size_needed);
			sprintf(evdev_config_key, EVDEV_MAPPING_CONFIG_KEY, port+1);
			custom_mapping_filename = (cfgExists("input", evdev_config_key) == 2 ? cfgLoadStr("input", evdev_config_key, "") : "");
			free(evdev_config_key);

			EvdevInputHandler* handler = new EvdevInputHandler();
			handler->initialize(port, evdev_device, custom_mapping_filename);
			input_handlers.add(port, handler);
		}
	#endif

	#if defined(USE_JOYSTICK)
		#define JOYSTICK_DEFAULT_DEVICE_ID "-1"

		std::string linuxjs_device;
		std::string custom_mapping_filename = "";

		linuxjs_device = cfgLoadStr("input", "joystick_device_id", JOYSTICK_DEFAULT_DEVICE_ID);
		if (linuxjs_device.c_str()[0] == '-')
		{
			puts("LinuxJoystick input disabled by config.\n");
		}
		else
		{
			LinuxJoystickInputHandler* handler = new LinuxJoystickInputHandler();
			handler->initialize(0, linuxjs_device, custom_mapping_filename);
			input_handlers.add(0, handler);
		}
	#endif

	#if defined(SUPPORT_X11)
		input_x11_init();
	#endif

	#if defined(USE_SDL)
		input_sdl_init();
	#endif
}

void UpdateInputState(u32 port)
{
	#if defined(TARGET_EMSCRIPTEN)
		return;
	#endif

	input_handlers.handle(port);

	#if defined(USE_SDL)
		input_sdl_handle(port);
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
	#if defined(USE_SDL)
		sdl_window_set_text(text);
	#endif
}

void os_CreateWindow()
{
	#if defined(SUPPORT_X11)
		x11_window_create();
	#endif
	#if defined(USE_SDL)
		sdl_window_create();
	#endif
}

void common_linux_setup();
int dc_init(int argc,wchar* argv[]);
void dc_run();

#ifdef TARGET_PANDORA
	void gl_term();

	void clean_exit(int sig_num)
	{
		void* array[10];
		size_t size;

		if (joystick_fd >= 0) { close(joystick_fd); }
		for (int port = 0; port < 4 ; port++)
		{
			if (evdev_controllers[port]->fd >= 0)
			{
				close(evdev_controllers[port]->fd);
			}
		}

		// Close EGL context ???
		if (sig_num!=0)
		{
			gl_term();
		}

		x11_window_destroy():

		// finish cleaning
		if (sig_num!=0)
		{
			write(2, "\nSignal received\n", sizeof("\nSignal received\n"));

			size = backtrace(array, 10);
			backtrace_symbols_fd(array, size, STDERR_FILENO);
			exit(1);
		}
	}
#endif

string find_user_config_dir()
{
	#ifdef USES_HOMEDIR
		struct stat info;
		string home = "";
		if(getenv("HOME") != NULL)
		{
			// Support for the legacy config dir at "$HOME/.reicast"
			string legacy_home = (string)getenv("HOME") + "/.reicast";
			if((stat(legacy_home.c_str(), &info) == 0) && (info.st_mode & S_IFDIR))
			{
				// "$HOME/.reicast" already exists, let's use it!
				return legacy_home;
			}

			/* If $XDG_CONFIG_HOME is not set, we're supposed to use "$HOME/.config" instead.
			 * Consult the XDG Base Directory Specification for details:
			 *   http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html#variables
			 */
			home = (string)getenv("HOME") + "/.config/reicast";
		}
		if(getenv("XDG_CONFIG_HOME") != NULL)
		{
			// If XDG_CONFIG_HOME is set explicitly, we'll use that instead of $HOME/.config
			home = (string)getenv("XDG_CONFIG_HOME") + "/reicast";
		}

		if(!home.empty())
		{
			if((stat(home.c_str(), &info) != 0) || !(info.st_mode & S_IFDIR))
			{
				// If the directory doesn't exist yet, create it!
				mkdir(home.c_str(), 0755);
			}
			return home;
		}
	#endif

	// Unable to detect config dir, use the current folder
	return ".";
}

string find_user_data_dir()
{
	#ifdef USES_HOMEDIR
		struct stat info;
		string data = "";
		if(getenv("HOME") != NULL)
		{
			// Support for the legacy config dir at "$HOME/.reicast"
			string legacy_data = (string)getenv("HOME") + "/.reicast";
			if((stat(legacy_data.c_str(), &info) == 0) && (info.st_mode & S_IFDIR))
			{
				// "$HOME/.reicast" already exists, let's use it!
				return legacy_data;
			}

			/* If $XDG_DATA_HOME is not set, we're supposed to use "$HOME/.local/share" instead.
			 * Consult the XDG Base Directory Specification for details:
			 *   http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html#variables
			 */
			data = (string)getenv("HOME") + "/.local/share/reicast";
		}
		if(getenv("XDG_DATA_HOME") != NULL)
		{
			// If XDG_DATA_HOME is set explicitly, we'll use that instead of $HOME/.config
			data = (string)getenv("XDG_DATA_HOME") + "/reicast";
		}
		
		if(!data.empty())
		{
			if((stat(data.c_str(), &info) != 0) || !(info.st_mode & S_IFDIR))
			{
				// If the directory doesn't exist yet, create it!
				mkdir(data.c_str(), 0755);
			}
			return data;
		}
	#endif

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


int main(int argc, wchar* argv[])
{
	#ifdef TARGET_PANDORA
		signal(SIGSEGV, clean_exit);
		signal(SIGKILL, clean_exit);
	#endif

	/* Set directories */
	set_user_config_dir(find_user_config_dir());
	set_user_data_dir(find_user_data_dir());
	std::vector<string> dirs;
	dirs = find_system_config_dirs();
	for(unsigned int i = 0; i < dirs.size(); i++)
	{
		add_system_data_dir(dirs[i]);
	}
	dirs = find_system_data_dirs();
	for(unsigned int i = 0; i < dirs.size(); i++)
	{
		add_system_data_dir(dirs[i]);
	}
	printf("Config dir is: %s\n", get_writable_config_path("/").c_str());
	printf("Data dir is:   %s\n", get_writable_data_path("/").c_str());

	#if defined(USE_SDL)
		if (SDL_Init(0) != 0)
		{
			die("SDL: Initialization failed!");
		}
	#endif

	common_linux_setup();

	settings.profile.run_counts=0;

	dc_init(argc,argv);

	SetupInput();

	#if !defined(TARGET_EMSCRIPTEN)
		#if FEAT_HAS_NIXPROF
		install_prof_handler(0);
		#endif
		dc_run();
	#else
		emscripten_set_main_loop(&dc_run, 100, false);
	#endif


	#ifdef TARGET_PANDORA
		clean_exit(0);
	#endif

	return 0;
}
#endif

int get_mic_data(u8* buffer) { return 0; }
int push_vmu_screen(u8* buffer) { return 0; }

void os_DebugBreak()
{
	#if !defined(TARGET_EMSCRIPTEN)
		raise(SIGTRAP);
	#else
		printf("DEBUGBREAK!\n");
		exit(-1);
	#endif
}



