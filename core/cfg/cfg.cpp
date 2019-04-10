/*
	Config file crap
	Supports various things, as virtual config entries and such crap
	Works surprisingly well considering how old it is ...
*/

#define _CRT_SECURE_NO_DEPRECATE (1)
#include <errno.h>
#include "cfg.h"
#include "ini.h"

wstring cfgPath;
bool save_config = true;

ConfigFile cfgdb;

void savecfgf()
{
	FILE* cfgfile = fopen(cfgPath.c_str(),"wt");
	if (!cfgfile)
	{
		wprintf(L"Error : Unable to open file for saving \n");
	}
	else
	{
		cfgdb.save(cfgfile);
		fclose(cfgfile);
	}
}
void  cfgSaveStr(const wchar_t * Section, const wchar_t * Key, const wchar_t * String)
{
	cfgdb.set(wstring(Section), wstring(Key), wstring(String));
	if(save_config)
	{
		savecfgf();
	}
	//WritePrivateProfileString(Section,Key,String,cfgPath);
}
//New config code

/*
	I want config to be really flexible .. so , here is the new implementation :

	Functions :
	cfgLoadInt  : Load an int , if it does not exist save the default value to it and return it
	cfgSaveInt  : Save an int
	cfgLoadStr  : Load a str , if it does not exist save the default value to it and return it
	cfgSaveStr  : Save a str
	cfgExists   : Returns true if the Section:Key exists. If Key is null , it retuns true if Section exists

	Config parameters can be read from the config file , and can be given at the command line
	-cfg section:key=value -> defines a value at command line
	If a cfgSave* is made on a value defined by command line , then the command line value is replaced by it

	cfg values set by command line are not written to the cfg file , unless a cfgSave* is used

	There are some special values , all of em are on the emu namespace :)

	These are readonly :

	emu:AppPath		: Returns the path where the emulator is stored
	emu:PluginPath	: Returns the path where the plugins are loaded from
	emu:DataPath	: Returns the path where the bios/data files are

	emu:FullName	: str,returns the emulator's name + version wstring (ex."nullDC v1.0.0 Private Beta 2 built on {datetime}")
	emu:ShortName	: str,returns the emulator's name + version wstring , short form (ex."nullDC 1.0.0pb2")
	emu:Name		: str,returns the emulator's name (ex."nullDC")

	These are read/write
	emu:Caption		: str , get/set the window caption
*/

///////////////////////////////
/*
**	This will verify there is a working file @ ./szIniFn
**	- if not present, it will write defaults
*/

bool cfgOpen()
{
	const wchar_t* filename = L"/emu.cfg";
	wstring config_path_read = get_readonly_config_path(filename);
	cfgPath = get_writable_config_path(filename);

	FILE* cfgfile = fopen(config_path_read.c_str(),"r");
	if(cfgfile != NULL) {
		cfgdb.parse(cfgfile);
		fclose(cfgfile);
	}
	else
	{
		// Config file can't be opened
		int error_code = errno;
		wprintf(L"Warning: Unable to open the config file '%s' for reading (%s)\n", config_path_read.c_str(), strerror(error_code));
		if (error_code == ENOENT || cfgPath != config_path_read)
		{
			// Config file didn't exist
			wprintf(L"Creating new empty config file at '%s'\n", cfgPath.c_str());
			savecfgf();
		}
		else
		{
			// There was some other error (may be a permissions problem or something like that)
			save_config = false;
		}
	}

	return true;
}

//Implementations of the interface :)
//Section must be set
//If key is 0 , it looks for the section
//0 : not found
//1 : found section , key was 0
//2 : found section & key
s32  cfgExists(const wchar_t * Section, const wchar_t * Key)
{
	if(cfgdb.has_entry(wstring(Section), wstring(Key)))
	{
		return 2;
	}
	else
	{
		return (cfgdb.has_section(wstring(Section)) ? 1 : 0);
	}
}
void  cfgLoadStr(const wchar_t * Section, const wchar_t * Key, wchar_t * Return,const wchar_t* Default)
{
	wstring value = cfgdb.get(Section, Key, Default);
	// FIXME: Buffer overflow possible
	wcscpy(Return, value.c_str());
}

wstring  cfgLoadStr(const wchar_t * Section, const wchar_t * Key, const wchar_t* Default)
{
	if(!cfgdb.has_entry(wstring(Section), wstring(Key)))
	{
		cfgSaveStr(Section, Key, Default);
	}
	return cfgdb.get(wstring(Section), wstring(Key), wstring(Default));
}

//These are helpers , mainly :)
void  cfgSaveInt(const wchar_t * Section, const wchar_t * Key, s32 Int)
{
	cfgdb.set_int(wstring(Section), wstring(Key), Int);
	if(save_config)
	{
		savecfgf();
	}
}

s32  cfgLoadInt(const wchar_t * Section, const wchar_t * Key,s32 Default)
{
	if(!cfgdb.has_entry(wstring(Section), wstring(Key)))
	{
		cfgSaveInt(Section, Key, Default);
	}
	return cfgdb.get_int(wstring(Section), wstring(Key), Default);
}

s32  cfgGameInt(const wchar_t * Section, const wchar_t * Key,s32 Default)
{
    if(cfgdb.has_entry(wstring(Section), wstring(Key)))
    {
        return cfgdb.get_int(wstring(Section), wstring(Key), Default);
    }
    return Default;
}

void  cfgSaveBool(const wchar_t * Section, const wchar_t * Key, bool BoolValue)
{
	cfgdb.set_bool(wstring(Section), wstring(Key), BoolValue);
	if(save_config)
	{
		savecfgf();
	}
}

bool  cfgLoadBool(const wchar_t * Section, const wchar_t * Key,bool Default)
{
	if(!cfgdb.has_entry(wstring(Section), wstring(Key)))
	{
		cfgSaveBool(Section, Key, Default);
	}
	return cfgdb.get_bool(wstring(Section), wstring(Key), Default);
}

void cfgSetVirtual(const wchar_t * Section, const wchar_t * Key, const wchar_t * String)
{
	cfgdb.set(wstring(Section), wstring(Key), wstring(String), true);
}
