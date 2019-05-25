#include <string.h>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#include "types.h"
#include "cfg/cfg.h"
#include "stdclass.h"
#if HOST_OS == OS_DARWIN
#include <mach/clock.h>
#include <mach/mach.h>
#endif


#if COMPILER_VC_OR_CLANG_WIN32
	#include <io.h>
	#include <direct.h>
	#define access _access
	#define R_OK   4
#else
	#include <unistd.h>
#endif

#include "hw/mem/_vmem.h"

string user_config_dir;
string user_data_dir;
std::vector<string> system_config_dirs;
std::vector<string> system_data_dirs;

bool file_exists(const string& filename)
{
	return (access(filename.c_str(), R_OK) == 0);
}

void set_user_config_dir(const string& dir)
{
	user_config_dir = dir;
}

void set_user_data_dir(const string& dir)
{
	user_data_dir = dir;
}

void add_system_config_dir(const string& dir)
{
	system_config_dirs.push_back(dir);
}

void add_system_data_dir(const string& dir)
{
	system_data_dirs.push_back(dir);
}

string get_writable_config_path(const string& filename)
{
	/* Only stuff in the user_config_dir is supposed to be writable,
	 * so we always return that.
	 */
	return (user_config_dir + filename);
}

string get_readonly_config_path(const string& filename)
{
	string user_filepath = get_writable_config_path(filename);
	if(file_exists(user_filepath))
	{
		return user_filepath;
	}

	string filepath;
	for (unsigned int i = 0; i < system_config_dirs.size(); i++) {
		filepath = system_config_dirs[i] + filename;
		if (file_exists(filepath))
		{
			return filepath;
		}
	}

	// Not found, so we return the user variant
	return user_filepath;
}

string get_writable_data_path(const string& filename)
{
	/* Only stuff in the user_data_dir is supposed to be writable,
	 * so we always return that.
	 */
	return (user_data_dir + filename);
}

string get_readonly_data_path(const string& filename)
{
	string user_filepath = get_writable_data_path(filename);
	if(file_exists(user_filepath))
	{
		return user_filepath;
	}

	string filepath;
	for (unsigned int i = 0; i < system_data_dirs.size(); i++) {
		filepath = system_data_dirs[i] + filename;
		if (file_exists(filepath))
		{
			return filepath;
		}
	}

	// Not found, so we return the user variant
	return user_filepath;
}

string get_game_save_prefix()
{
	string save_file = cfgLoadStr("config", "image", "");
	size_t lastindex = save_file.find_last_of("/");
#ifdef _WIN32
	size_t lastindex2 = save_file.find_last_of("\\");
	lastindex = std::max(lastindex, lastindex2);
#endif
	if (lastindex != -1)
		save_file = save_file.substr(lastindex + 1);
	return get_writable_data_path("/data/") + save_file;
}

string get_game_basename()
{
	string game_dir = cfgLoadStr("config", "image", "");
	size_t lastindex = game_dir.find_last_of(".");
	if (lastindex != -1)
		game_dir = game_dir.substr(0, lastindex);
	return game_dir;
}

string get_game_dir()
{
	string game_dir = cfgLoadStr("config", "image", "");
	size_t lastindex = game_dir.find_last_of("/");
#ifdef _WIN32
	size_t lastindex2 = game_dir.find_last_of("\\");
	lastindex = std::max(lastindex, lastindex2);
#endif
	if (lastindex != -1)
		game_dir = game_dir.substr(0, lastindex + 1);
	return game_dir;
}

bool make_directory(const string& path)
{
#if COMPILER_VC_OR_CLANG_WIN32
#define mkdir _mkdir
#endif

#ifdef _WIN32
	return mkdir(path.c_str()) == 0;
#else
	return mkdir(path.c_str(), 0755) == 0;
#endif
}


