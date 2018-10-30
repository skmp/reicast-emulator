#include <stdio.h>
#include <cstdarg>
#include "oslib/logging.h"
#if ANDROID
#include <android/log.h>
#endif


/* NOTE:
 * https://github.com/fmtlib/fmt --> This project could solve possible formating issues
 */

#define LOG_ERROR   0
#define LOG_WARNING 1
#define LOG_INFO    2
#define LOG_DEBUG   3 // Keeping this 'un
#define LOG_VERBOSE 4

#if ANDROID
static void log_msg_android(int loglevel, const char *tag, const char *format, va_list args)
{
    /* This function sends messages to the native Android logging API.
     * See http://mobilepearls.com/labs/native-android-api/#logging for details.
     */


	/* Another Approach:
	 *
	 * #define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, tag,__VA_ARGS__)
	 * #define  LOGW(...)  __android_log_print(ANDROID_LOG_WARN, tag,__VA_ARGS__)
	 * #define  LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, tag,__VA_ARGS__)
	 * #define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO, tag,__VA_ARGS__)
	 *
	 * i.e: LOGD
	*/

    /* Map our custom loglevel to the respective ANDROID_LOG_* level */
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

	/* Send the message to the Android log */
    __android_log_vprint(android_loglevel, tag, format, args);
}
#endif

static void log_msg_printf(int loglevel, const char *tag, const char *format, va_list args)
{
	/* Imitate the Android logging levels for *nix systems
	 * and log through the printf family
	 */
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

	/* Print the tag and loglevel */
    printf("[%s][%s] ", loglevel_str, tag);

    /* Print the actual message */
    vprintf(format, args);
}

void log_msg(int loglevel, const char *tag, const char *format, ...) {
    va_list args;
    va_start(args, format);
#if ANDROID
    log_msg_android(loglevel, tag, format, args);
#else //LINUX
    log_msg_printf(loglevel, tag, format, args);
#endif // OOPS
    va_end(args);
}
