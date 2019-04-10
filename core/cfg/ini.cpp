#include "ini.h"
#include <sstream>

wchar_t* trim_ws(wchar_t* str);

/* ConfigEntry */

wstring ConfigEntry::get_string()
{
	return this->value;
}

int ConfigEntry::get_int()
{
	int base = (wcsstr(this->value.c_str(), L"0x") != NULL) ? 16 : 10 ;
	return wcstol(this->value.c_str(), NULL, base);
}

bool ConfigEntry::get_bool()
{
	if (wcsicmp(this->value.c_str(), L"yes") == 0 ||
		wcsicmp(this->value.c_str(), L"true") == 0 ||
		wcsicmp(this->value.c_str(), L"on") == 0 ||
		wcsicmp(this->value.c_str(), L"1") == 0)
	{
		return true;
	}
	else
	{
		return false;
	}
}

/* ConfigSection */

bool ConfigSection::has_entry(wstring name)
{
	return (this->entries.count(name) == 1);
};

ConfigEntry* ConfigSection::get_entry(wstring name)
{
	if(this->has_entry(name))
	{
		return &this->entries[name];
	}
	return NULL;
};

void ConfigSection::set(wstring name, wstring value)
{
	ConfigEntry new_entry = { value };
	this->entries[name] = new_entry;
};

/* ConfigFile */

ConfigSection* ConfigFile::add_section(wstring name, bool is_virtual)
{
	const size_t maxSectionSize = 128;
	wchar_t sectionName[maxSectionSize] = { '\0' };
	size_t nameLength = wcsnlen(name.c_str(), maxSectionSize - 1);
	wcsncpy(sectionName, name.c_str(), nameLength);
	sectionName[nameLength] = '\0';

	ConfigSection new_section;
	if (is_virtual)
	{
		this->virtual_sections.insert(std::make_pair(trim_ws(sectionName), new_section));
		return &this->virtual_sections[name];
	}
	this->sections.insert(std::make_pair(trim_ws(sectionName), new_section));
	return &this->sections[name];
};

bool ConfigFile::has_section(wstring name)
{
	return (this->virtual_sections.count(name) == 1 || this->sections.count(name) == 1);
}

bool ConfigFile::has_entry(wstring section_name, wstring entry_name)
{
	ConfigSection* section = this->get_section(section_name, true);
	if ((section != NULL) && section->has_entry(entry_name))
	{
		return true;
	}
	section = this->get_section(section_name, false);
	return ((section != NULL) && section->has_entry(entry_name));
}

ConfigSection* ConfigFile::get_section(wstring name, bool is_virtual)
{
	if(is_virtual)
	{
		if (this->virtual_sections.count(name) == 1)
		{
			return &this->virtual_sections[name];
		}
	}
	else
	{
		if (this->sections.count(name) == 1)
		{
			return &this->sections[name];
		}
	}
	return NULL;
};

ConfigEntry* ConfigFile::get_entry(wstring section_name, wstring entry_name)
{
	ConfigSection* section = this->get_section(section_name, true);
	if(section != NULL && section->has_entry(entry_name))
	{
		return section->get_entry(entry_name);
	}

		section = this->get_section(section_name, false);
	if(section != NULL)
	{
		return section->get_entry(entry_name);
	}
	return NULL;

}

wstring ConfigFile::get(wstring section_name, wstring entry_name, wstring default_value)
{
	ConfigEntry* entry = this->get_entry(section_name, entry_name);
	if (entry == NULL)
	{
		return default_value;
	}
	else
	{
		return entry->get_string();
	}
}

int ConfigFile::get_int(wstring section_name, wstring entry_name, int default_value)
{
	ConfigEntry* entry = this->get_entry(section_name, entry_name);
	if (entry == NULL)
	{
		return default_value;
	}
	else
	{
		return entry->get_int();
	}
}

bool ConfigFile::get_bool(wstring section_name, wstring entry_name, bool default_value)
{
	ConfigEntry* entry = this->get_entry(section_name, entry_name);
	if (entry == NULL)
	{
		return default_value;
	}
	else
	{
		return entry->get_bool();
	}
}

void ConfigFile::set(wstring section_name, wstring entry_name, wstring value, bool is_virtual)
{
	ConfigSection* section = this->get_section(section_name, is_virtual);
	if(section == NULL)
	{
		section = this->add_section(section_name, is_virtual);
	}
	section->set(entry_name, value);
};

void ConfigFile::set_int(wstring section_name, wstring entry_name, int value, bool is_virtual)
{
	std::wstringstream str_value;
	str_value << value;
	this->set(section_name, entry_name, str_value.str(), is_virtual);
}

void ConfigFile::set_bool(wstring section_name, wstring entry_name, bool value, bool is_virtual)
{
	wstring str_value = (value ? L"yes" : L"no");
	this->set(section_name, entry_name, str_value, is_virtual);
}

void ConfigFile::parse(FILE* file)
{
	const size_t maxlineSize = 512;
	if(file == NULL)
	{
		return;
	}
	wchar_t line[maxlineSize] = { '\0' };
	wchar_t current_section[maxlineSize] = { '\0' };
	int cline = 0;
	while(file && !feof(file))
	{
		if (fgetws(line, maxlineSize, file) == NULL || feof(file))
		{
			break;
		}

		cline++;

		if (wcslen(line) < 3)
		{
			continue;
		}

		if (line[wcslen(line)-1] == '\r' ||
			  line[wcslen(line)-1] == '\n')
		{
			line[wcslen(line)-1] = '\0';
		}

		wchar_t* tl = trim_ws(line);

		if (tl[0] == '[' && tl[wcslen(tl)-1] == ']')
		{
			tl[wcslen(tl)-1] = '\0';

			// FIXME: Data loss if buffer is too small
			size_t sectionLength = wcsnlen(tl + 1, maxlineSize - 1);
			wcsncpy(current_section, tl+1, sectionLength);
			current_section[sectionLength] = '\0';

			trim_ws(current_section);
		}
		else
		{
			if (wcslen(current_section) == 0)
			{
				continue; //no open section
			}

			wchar_t* separator = wcsstr(tl, L"=");

			if (!separator)
			{
				wprintf(L"Malformed entry on config - ignoring @ %d(%s)\n",cline, tl);
				continue;
			}

			*separator = '\0';

			wchar_t* name = trim_ws(tl);
			wchar_t* value = trim_ws(separator + 1);
			if (name == NULL || value == NULL)
			{
				wprintf(L"Malformed entry on config - ignoring @ %d(%s)\n",cline, tl);
				continue;
			}
			else
			{
				this->set(wstring(current_section), wstring(name), wstring(value));
			}
		}
	}
}

void ConfigFile::save(FILE* file)
{
	for(std::map<wstring, ConfigSection>::iterator section_it = this->sections.begin();
		  section_it != this->sections.end(); section_it++)
	{
		wstring section_name = section_it->first;
		ConfigSection section = section_it->second;

		fwprintf(file, L"[%s]\n", section_name.c_str());

		for(std::map<wstring, ConfigEntry>::iterator entry_it = section.entries.begin();
			  entry_it != section.entries.end(); entry_it++)
		{
			wstring entry_name = entry_it->first;
			ConfigEntry entry = entry_it->second;
			fwprintf(file, L"%s = %s\n", entry_name.c_str(), entry.get_string().c_str());
		}

		fputws(L"\n", file);
	}
}
