/*
 *  CordellDBMS source code: https://github.com/j1sk1ss/CordellDBMS.EXMPL
 *  Credits: j1sk1ss
 */


#ifndef LOGGING_H_
#define LOGGING_H_


#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

#include "common.h"

#define DEBUG
#define LOGGING
#define WARNINGS
#define ERRORS
#define INFORMING

// Note: LOG_TO_FILE option very heavy function. Prefere console logging.
// #define LOG_TO_FILE
#define LOG_FILE_PATH getenv("LOG_FILE_PATH") == NULL ? "cdbms.log" : getenv("LOG_FILE_PATH")

#ifdef DEBUG
    #define print_debug(message, ...)    log_message("DEBUG", __FILE__, __LINE__, message, ##__VA_ARGS__)
#else
    #define print_debug(message, ...)
#endif

#ifdef LOGGING
    #define print_log(message, ...)    log_message("LOG", __FILE__, __LINE__, message, ##__VA_ARGS__)
#else
    #define print_log(message, ...)
#endif

#ifdef WARNINGS
    #define print_warn(message, ...)   log_message("WARN", __FILE__, __LINE__, message, ##__VA_ARGS__)
#else
    #define print_warn(message, ...)
#endif

#ifdef ERRORS
    #define print_error(message, ...)  log_message("ERROR", __FILE__, __LINE__, message, ##__VA_ARGS__)
#else
    #define print_error(message, ...)
#endif

#ifdef INFORMING
    #define print_info(message, ...)   log_message("INFO", __FILE__, __LINE__, message, ##__VA_ARGS__)
#else
    #define print_info(message, ...)
#endif


void write_log(const char* level, const char* file, int line, const char* message, va_list args);
void log_message(const char* level, const char* file, int line, const char* message, ...);

#endif
