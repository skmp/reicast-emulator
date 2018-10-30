#pragma once
#include <cstdarg>

#define LOG_ERROR   0
#define LOG_WARNING 1
#define LOG_INFO    2
#define LOG_DEBUG   3
#define LOG_VERBOSE 4

extern void log_msg(int loglevel, const char *tag, const char *fmt, ...);

/* Automation of logging practises */
#define LOG_E(tag, ...) log_msg(LOG_ERROR, tag, __VA_ARGS__)
#define LOG_W(tag, ...) log_msg(LOG_WARNING, tag, __VA_ARGS__)
#define LOG_I(tag, ...) log_msg(LOG_INFO, tag, __VA_ARGS__)
#define LOG_D(tag, ...) log_msg(LOG_DEBUG, tag, __VA_ARGS__)
#define LOG_V(tag, ...) log_msg(LOG_VERBOSE, tag, __VA_ARGS__)
