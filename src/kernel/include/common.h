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


#ifndef COMMON_H_
#define COMMON_H_

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <stdio.h>

#ifdef _WIN32
  #include <windows.h>
#else
  #include <sys/stat.h>
#endif

#include "cache.h"

#ifdef NO_ENV
  #define getenv(key) NULL
#endif

#define ENV_GET(key, default) getenv(key) == NULL ? default : getenv(key)

#define DEFAULT_BUFFER_SIZE 256
#define DEFAULT_PATH_SIZE   128
#define DEFAULT_DELAY       999999999

#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#define SOFT_FREE(ptr) do {  \
    if (ptr != NULL) {       \
      free((ptr));           \
      (ptr) = NULL;          \
    }                        \
  } while(0)


/*
Create rundom string.
Reference from: https://stackoverflow.com/questions/15767691/whats-the-c-library-function-to-generate-random-string
Note: This function not random. It generates next name by offset.
Note 2: Offset should not be grater then 65^length.

Params:
- dest - Pointer to place, where mamory for string allocated.
- length - Length of string that will be generated bu function.
- offset - Offset of rand string.

Return NULL.
*/
void strrand(char* dest, size_t length, int offset);

/*
Check if provided string is integer.

Params:
- str - pointer to string.

Return 1 is integer.
Return 0 is not integer.
*/
int is_integer(const char* str);

/*
Get current time from time.h libraryю
!! Note: Output should be freed after usage. !!

Return char* of current time in format: "%Y-%m-%d %H:%M:%S".
*/
char* get_current_time();

/*
Get load path by name or path.

Params:
- name - Name of file or NULL.
- name_size - Name size.
- buffer - Buffer, where will be stored load path.
- base_path - Base path.
- extension - File extension.

Return 1 if load path generated.
Return -1 if something goes wrong.
*/
int get_load_path(char* name, int name_size, char* buffer, char* base_path, char* extension);

/*
Generate unique filename.
Took from: https://stackoverflow.com/questions/230062/whats-the-best-way-to-check-if-a-file-exists-in-c

Params:
- base_path - Path, where will be placed file.
- name_size - Name size of future file.
- extension - File extension.

Return allocated unique name or NULL, if name can't be create.
*/
char* generate_unique_filename(char* base_path, int name_size, char* extension);

/*
Check if file exists.

Params:
- path - Path to file with filename and extension.

Return 1 if exist, 0 if not.
*/
int file_exists(const char* path, const char* filename);

/*
Delete file by provided filename, basepath and extension.

Params:
- filename - File name.
- basepath - Path, where placed file.
- extension - File extension.

Return remove status code.
*/
int delete_file(const char* filename, const char* basepath, const char* extension);

// TODO: Create wrappers for file_read, file_write and file_close function for future migrations.
// size_t file_read(void* __restrict __ptr, size_t __size, size_t __nitems, FILE* __restrict __stream);
// size_t	file_write(const void* __restrict __ptr, size_t __size, size_t __nitems, FILE* __restrict __stream);
// int fclose(FILE* __restrict __stream);

/*
Replacing sub-string in string by new string.
Took from: https://stackoverflow.com/questions/779875/what-function-is-to-replace-a-substring-from-a-string-in-c
Note: Pointers shouldn't overlap each other!

Params:
- string - Source string.
- source - Sub memory for replace.
- target - New sub string.

Return new string with replaced sub-memory.
*/
char* strrep(char* __restrict string, char* __restrict source, char* __restrict target);

/*
Checksum generator. For avoiding of usage big sha lib, we use one little func instead.
Took from: https://github.com/gcc-mirror/gcc/blob/master/libiberty/crc32.c
Return checksum.
*/
unsigned int crc32(unsigned int init, const unsigned char* buf, int len);

#endif
