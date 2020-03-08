/*
	This file is part of libswirl
        original code by flyinghead
*/
#include "license/bsd"



#pragma once

#include <string>

typedef void (*StringCallback)(bool cancelled, std::string selection);

void select_directory_popup(const char *prompt, float scaling, StringCallback callback);
void gui_ShowHelpMarker(const char* desc);
