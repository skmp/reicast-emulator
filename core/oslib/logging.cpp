#include <stdio.h>
#include <cstdarg>
#include "oslib/logging.h"
#if ANDROID
#include <android/log.h>
#endif

#define LOG_ERROR   0
#define LOG_WARNING 1
#define LOG_INFO    2
#define LOG_DEBUG   3
#define LOG_VERBOSE 4

#if ANDROID
static void log_msg_android(int loglevel, const char *tag, const char *fmt, va_list args)
{
    /* This function sends messages to the native Android logging API.
     * See http://mobilepearls.com/labs/native-android-api/#logging for details.
     */

    // Map our custom loglevel to the respective ANDROID_LOG_* level
    int android_loglevel;
    switch(loglevel)
    {
        case LOG_ERROR:
            android_loglevel = ANDROID_LOG_ERROR;
            break;
        case LOG_WARNING:
            android_loglevel = ANDROID_LOG_WARNING;
            break;
        case LOG_INFO:
            android_loglevel = ANDROID_LOG_INFO;
            break;
        case LOG_DEBUG:
            android_loglevel = ANDROID_LOG_DEBUG;
            break;
        case LOG_VERBOSE:
            android_loglevel = ANDROID_LOG_VERBOSE;
            break;
        default:
            android_loglevel = ANDROID_LOG_UNKNOWN;
    }
    // Then, send the message to the Android log
    __android_log_vprint(android_loglevel, tag, fmt, args);
}
#endif

static void log_msg_printf(int loglevel, const char *tag, const char *fmt, va_list args)
{
    const char *loglevel_str;
    switch(loglevel)
    {
        case LOG_ERROR:
            loglevel_str = "ERROR";
            break;
        case LOG_WARNING:
            loglevel_str = "WARNING";
            break;
        case LOG_INFO:
            loglevel_str = "INFO";
            break;
        case LOG_DEBUG:
            loglevel_str = "DEBUG";
            break;
        case LOG_VERBOSE:
            loglevel_str = "VERBOSE";
            break;
        default:
            loglevel_str = "UNKNOWN";
    }
    // Print the tag and loglevel
    printf("[%s][%s] ", loglevel_str, tag);

    // Print the actual message
    vprintf(fmt, args);

    // Print newline
    printf("\n");
}

void log_msg(int loglevel, const char *tag, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
#if ANDROID
    log_msg_android(loglevel, tag, fmt, args);
#else
    log_msg_printf(loglevel, tag, fmt, args);
#endif
    va_end(args);
}