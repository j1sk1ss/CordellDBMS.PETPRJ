/*
 *  CordellDBMS source code: https://github.com/j1sk1ss/CordellDBMS.EXMPL
 *  Credits: j1sk1ss
 */


#ifndef COMMON_H_
#define COMMON_H_

#include <stdlib.h>
#include <string.h>
#include <ctype.h>


#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))


// Create rundom string 
// Took from: https://stackoverflow.com/questions/15767691/whats-the-c-library-function-to-generate-random-string
//
// dest - pointer to place, where mamory for string allocated
// length - length of string that will be generated bu function
void rand_str(char *dest, size_t length);

int is_integer(const char* str);

int is_float(const char* str);

char* get_next_token(char** input, char delimiter);

#endif