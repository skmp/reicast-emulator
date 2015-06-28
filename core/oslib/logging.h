#pragma once

#define LOG_ERROR   0
#define LOG_WARNING 1
#define LOG_INFO    2
#define LOG_DEBUG   3
#define LOG_VERBOSE 4

extern void log_msg(int loglevel, const char *tag, const char *fmt, ...);
