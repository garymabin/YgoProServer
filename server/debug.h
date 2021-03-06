#ifndef _DEBUG_H_
#define _DEBUG_H_
#include <stdio.h>
#include <stdarg.h>
#include <ctime>
#define DEBUG
#define DEBUG_LEVEL INFO

enum log_type {INFO,WARN,BUG};
void log(log_type lt, const char *format, ...);

#endif
