#ifndef COMMON_H_
#define COMMON_H_

#include <stdlib.h>


#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))


// Create rundom string 
void rand_str(char *dest, size_t length);

#endif