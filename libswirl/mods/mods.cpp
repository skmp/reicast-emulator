#include "mods.h"
#include "reios/reios.h"
#ifdef _MSC_VER
#include "dirent/dirent.h"
#else
#include <dirent.h>
#endif

std::string mods_path;
std::vector<mod_handlers>* modhandlers;

std::string get_game_id()
{
	std::string game_id = reios_product_number;
	const size_t str_end = game_id.find_last_not_of(" ");
	if (str_end == std::string::npos)
		return "";
	game_id = game_id.substr(0, str_end + 1);
	std::replace(game_id.begin(), game_id.end(), ' ', '_');

	return game_id;
}

bool mod_handler_register(mod_handlers& mod)
{
	if (modhandlers == nullptr)
		modhandlers = new std::vector<mod_handlers>();
	modhandlers->push_back(mod);
	return true;
}

void modpack_detect_mods(std::string path) 
{
	for (auto& mh : *modhandlers)
	{
		mh.active = mh.detect(path);
	}
}

void modpack_gamestart()
{
	std::string game_id = get_game_id();
	if (game_id.length() > 0)
	{
		mods_path = get_readonly_data_path(DATA_PATH) + "mods/" + game_id + "/";

		DIR* dir = opendir(mods_path.c_str());
		if (dir != NULL)
		{
			printf("Found modpack directory: %s\n", mods_path.c_str());

			closedir(dir);

			modpack_detect_mods(mods_path);
		}
	}
}

void modpack_close()
{
	for (auto& mh : *modhandlers)
	{
		if (mh.active) mh.close();
		mh.active = false;
	}
}
void modpack_onframe()
{
	for (auto& mh : *modhandlers)
	{
		if (mh.active && mh.onframe) mh.onframe();
	}
}
void modpack_onstart()
{
	for (auto& mh : *modhandlers)
	{
		if (mh.active && mh.onstart) mh.onstart();
	}
}
void modpack_onstop()
{
	for (auto& mh : *modhandlers)
	{
		if (mh.active && mh.onstop) mh.onstop();
	}
}
void modpack_onreset()
{
	for (auto& mh : *modhandlers)
	{
		if (mh.active && mh.onreset) mh.onreset();
	}
}
