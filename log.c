#include "log.h"

int debug = 0;

int Log(int level, const char *format, ...)
{
    if (!debug && level == LOG_DEBUG)
        return 0;

    int written;
	va_list args;
	va_start(args, format);

	switch (level) {
    case LOG_FATEL:
        fprintf(stderr, "\033[1;31m[FATEL] ");  // rad, bright
        break;
    case LOG_ERROR:
        fprintf(stderr, "\033[0;31m[ERROR] ");  // rad
        break;
    case LOG_WARN:
        fprintf(stderr, "\033[0;33m[WARN] ");  // yellow
        break;
    case LOG_INFO:
        fprintf(stderr, "\033[0m[INFO] ");  // white
        break;
    case LOG_DEBUG:
        fprintf(stderr, "\033[0;32m[DEBUG] "); // color green for debug
        break;
    default:
        break;
	}
    written = vfprintf(stderr, format, args);
    fprintf(stderr, "\033[0m");

	return written;
}
