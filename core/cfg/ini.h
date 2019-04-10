#pragma once
#include "types.h"
#include <map>

struct ConfigEntry {
	wstring value;
	wstring get_string();
	int get_int();
	bool get_bool();
};

struct ConfigSection {
	std::map<wstring, ConfigEntry> entries;
	bool has_entry(wstring name);
	void set(wstring name, wstring value);
	ConfigEntry* get_entry(wstring name);
};

struct ConfigFile {
	private:
		std::map<wstring, ConfigSection> sections;
		std::map<wstring, ConfigSection> virtual_sections;
		ConfigSection* add_section(wstring name, bool is_virtual);
		ConfigSection* get_section(wstring name, bool is_virtual);
		ConfigEntry* get_entry(wstring section_name, wstring entry_name);


	public:
		bool has_section(wstring name);
		bool has_entry(wstring section_name, wstring entry_name);

		void parse(FILE* fd);
		void save(FILE* fd);

		/* getting values */
		wstring get(wstring section_name, wstring entry_name, wstring default_value = L"");
		int get_int(wstring section_name, wstring entry_name, int default_value = 0);
		bool get_bool(wstring section_name, wstring entry_name, bool default_value = false);
		/* setting values */
		void set(wstring section_name, wstring entry_name, wstring value, bool is_virtual = false);
		void set_int(wstring section_name, wstring entry_name, int value, bool is_virtual = false);
		void set_bool(wstring section_name, wstring entry_name, bool value, bool is_virtual = false);
};
