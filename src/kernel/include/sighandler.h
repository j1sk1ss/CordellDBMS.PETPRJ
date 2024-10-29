/*
 *  Took from: https://stackoverflow.com/questions/77005/how-to-automatically-generate-a-stacktrace-when-my-program-crashes
*/

#ifndef TRACEBACK_H_
#define TRACEBACK_H_

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>

#ifndef _WIN32
    #include <execinfo.h>
    #include <unistd.h>
#endif

#include "kentry.h"

/*
Enable trace back
*/
int TB_enable(void);
int CL_enable(void);

/*
Signal handler for trace back
*/
void traceback_handler(int sig);
void cleanup_handler();

#endif
