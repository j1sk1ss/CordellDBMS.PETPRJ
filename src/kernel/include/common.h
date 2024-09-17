/*
 *  CordellDBMS source code: https://github.com/j1sk1ss/CordellDBMS.EXMPL
 *  Credits: j1sk1ss
 */


#ifndef COMMON_H_
#define COMMON_H_

#include <stdlib.h>
#include <string.h>
#include <ctype.h>


#define LOCKED    1
#define UNLOCKED  0

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#define SOFT_FREE(ptr) do { \
    free((ptr));      \
    (ptr) = NULL;     \
  } while(0)

/*
Create rundom string 
Took from: https://stackoverflow.com/questions/15767691/whats-the-c-library-function-to-generate-random-string

dest - pointer to place, where mamory for string allocated
length - length of string that will be generated bu function
*/
void rand_str(char *dest, size_t length);

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

Took from: https://stackoverflow.com/questions/24975928/extract-the-file-name-and-its-extension-in-c
*/
void get_file_path_parts(char *path, char *path_, char *base_, char *ext_);

#endif