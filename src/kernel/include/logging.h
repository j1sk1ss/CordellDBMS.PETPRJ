/*
 *  License:
 *  Copyright (C) 2024 Nikolaj Fot
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of 
 *  the GNU General Public License as published by the Free Software Foundation, version 3.
 *  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  See the GNU General Public License for more details.
 *  You should have received a copy of the GNU General Public License along with this program. 
 *  If not, see https://www.gnu.org/licenses/.
 * 
 *  CordellDBMS source code: https://github.com/j1sk1ss/CordellDBMS.EXMPL
 *  Credits: j1sk1ss
 */


#ifndef LOGGING_H_
#define LOGGING_H_


#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

#include "threading.h"
#include "common.h"


#define DEBUG
#define LOGGING
#define WARNINGS
#define ERRORS
#define INFORMING

// Note: LOG_TO_FILE option very heavy function. Prefere console logging.
// #define LOG_TO_FILE
#define LOG_FILE_PATH       ENV_GET("LOG_FILE_PATH", "")
#define LOG_FILE_EXTENSION  ENV_GET("LOG_FILE_EXTENSION", "log")

#define LOG_FILE_NAME_SIZE  16
#define LOG_FILE_SIZE       100

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


/*
Write log to file descriptor.

Params:
- level - Log level.
- file - File descriptor.
- line - Code line number.
- message - Additional info message.
- args - Args.
*/
void write_log(const char* level, const char* file, int line, const char* message, va_list args);

/*
Create log message.

- level - Log level.
- file - File name.
- line - Code line number.
- message - Additional info message.
*/
void log_message(const char* level, const char* file, int line, const char* message, ...);

#endif
