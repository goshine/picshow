#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdarg.h>

// log level
#define LOG_FATEL   2
#define LOG_ERROR	3
#define LOG_WARN	4
#define LOG_INFO	6
#define LOG_DEBUG	7

int Log(int level, const char *format, ...);

#endif /* LOG_H */
