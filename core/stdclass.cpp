#include <string.h>
#include <vector>
#include <sys/stat.h>
#include "types.h"

#ifdef _MSC_VER
#include <io.h>
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

string get_writable_data_path(const string& filename)
{
   extern char game_dir_no_slash[1024];
   return std::string(game_dir_no_slash + 
#ifdef _WIN32
         std::string("\\")
#else
         std::string("/")
#endif
         + filename);
}
