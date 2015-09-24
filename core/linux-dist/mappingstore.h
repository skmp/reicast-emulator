#pragma once
#include "mapping.h"
#include <string>

class InputMappingStore
{
	private:
		std::map<std::string, InputMapping*> loaded_mappings;
	public:
		InputMapping* get(std::string filepath, std::string subdir = "");
};
