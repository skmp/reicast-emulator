#include <cstdio>
#include <cstring>

#ifndef _WIN32
#include <unistd.h>
#endif

#include "ConsoleListener.h"
#include "Log.h"

ConsoleListener::ConsoleListener(void *log_cb)
{
	m_use_color = false;
	retro_printf = (retro_log_printf_t)log_cb;
}

ConsoleListener::~ConsoleListener()
{
}

void ConsoleListener::Log(LogTypes::LOG_LEVELS level, const char* text)
{
	retro_log_level retro_level;
	switch (level)
	{
	case LogTypes::LNOTICE:
	case LogTypes::LINFO:
		retro_level = RETRO_LOG_INFO;
		break;
	case LogTypes::LERROR:
		retro_level = RETRO_LOG_ERROR;
		break;
	case LogTypes::LWARNING:
		retro_level = RETRO_LOG_WARN;
		break;
	case LogTypes::LDEBUG:
		retro_level = RETRO_LOG_DEBUG;
		break;

	}
	if (retro_printf != nullptr)
		retro_printf(retro_level, "%s", text);
	else
		printf("%s\n", text);
}
