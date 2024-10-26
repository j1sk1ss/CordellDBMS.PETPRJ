/*
 *  CordellDBMS source code: https://github.com/j1sk1ss/CordellDBMS.EXMPL
 *  Credits: j1sk1ss
 */


#ifndef COMMON_H_
#define COMMON_H_

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <time.h>
#include <stdio.h>
#ifdef _WIN32
  #include <windows.h>
#else
  #include <sys/stat.h>
#endif

#include "threading.h"


#define DEFAULT_PATH_SIZE 100
#define DEFAULT_DELAY     99999999

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#define SOFT_FREE(ptr) do { \
    if (ptr != NULL) {      \
      free((ptr));          \
      (ptr) = NULL;         \
    }                       \
  } while(0)


/*
Create rundom string
Took from: https://stackoverflow.com/questions/15767691/whats-the-c-library-function-to-generate-random-string

dest - pointer to place, where mamory for string allocated
length - length of string that will be generated bu function
*/
void strrand(char* dest, size_t length);

/*
Check if provided string is integer

Params:
str - pointer to string

Return 1 is integer
Return 0 is not integer
*/
int is_integer(const char* str);

/*
Check if provided string is number with floating point

Params:
str - pointer to string

Return 1 is float / double
Return 0 is not float / double
*/
int is_float(const char* str);

/*
strtok alternative

Params:
- input - pointer to string
- delimiter - delimiter symbol

Return token
*/
char* get_next_token(char** input, char delimiter);

/*
Example:
Given path == "C:\\dir1\\dir2\\dir3\\file.exe"
will return path_ as   "C:\\dir1\\dir2\\dir3"
Will return base_ as   "file"
Will return ext_ as    "exe"

Important Note: path, that you provided into function, will be broken. It
happens, because in function used strtok.

Took from: https://stackoverflow.com/questions/24975928/extract-the-file-name-and-its-extension-in-c
*/
void get_file_path_parts(char* path, char* path_, char* base_, char* ext_);

/*
Get current time from time.h library

Return char* of current time in format: "%Y-%m-%d %H:%M:%S"
*/
char* get_current_time();

/*
Get load path by name or path.

Params:
- name - Name of file or NULL.
- path - Path for save or NULL.
- buffer - Buffer, where will be stored load path.
- base_path - Base path.
- extension - File extension.

Return 1 if load path generated.
Return -1 if something goes wrong.
*/
int get_load_path(char* name, char* path, char* buffer, char* base_path, char* extension);

/*
Get filename by name or path.

Params:
- name - Name of file or NULL.
- path - Path for save or NULL.
- buffer - Buffer, where will be stored load path.
- name_size - Size of name.

Return 1 if name generated.
Return -1 if something goes wrong.
*/
int get_filename(char* name, char* path, char* buffer, size_t name_size);

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
int file_exists(const char* path);

/*
Replacing sub-memory in memory by new memory.
Reference from: https://stackoverflow.com/questions/779875/what-function-is-to-replace-a-substring-from-a-string-in-c

Params:
- source - Source memory.
- source_size - Size of source memory.
- sub_string - Sub memory for replace.
- sub_string_size - Sub memory size.
- new_string - New sub string.
- new_string_size - New sub memory size.

Return new mamory with replaced sub-memory.
*/
uint8_t* memrep(
    uint8_t* source, int source_size, uint8_t* sub, int sub_size, uint8_t* new, int new_size, size_t *result_len
);

#endif
