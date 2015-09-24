#include "mappingstore.h"
#include <errno.h>

static std::string get_mapping_path(std::string filepath, std::string subdir)
{
	if (filepath.empty())
	{
		return "";
	}

	std::string new_filepath;
	if (filepath.at(0) != '/')
	{
		// It's not an absolute path
		std::string mapping_dir = "/mappings/";
		if(!subdir.empty())
		{
			mapping_dir.append(subdir);
			mapping_dir.append("/");
		}
		filepath.insert(0, mapping_dir);
		new_filepath = get_readonly_data_path(filepath);
	}
	else
	{
		new_filepath = filepath;
	}
	
	// TODO: Some realpath magic? How portable is it?

	return new_filepath;
}

InputMapping* InputMappingStore::get(std::string filepath, std::string subdir)
{
	std::string full_filepath = get_mapping_path(filepath, subdir);
	if(!full_filepath.empty())
	{
		if(this->loaded_mappings.count(full_filepath) == 0)
		{
			// Mapping has not been loaded yet
			FILE* fp = fopen(full_filepath.c_str(), "r");
			if(fp == NULL)
			{
				printf("Unable to open mapping file '%s': %s\n", full_filepath.c_str(), strerror(errno));
			}
			else
			{
				InputMapping* mapping = new InputMapping();
				mapping->load(fp);
				this->loaded_mappings.insert(std::pair<std::string, InputMapping*>(full_filepath, mapping));
			}
		}
		std::map<std::string, InputMapping*>::iterator it = loaded_mappings.find(full_filepath);
		if(it != loaded_mappings.end())
		{
			return it->second;
		}
	}
	return NULL;
}