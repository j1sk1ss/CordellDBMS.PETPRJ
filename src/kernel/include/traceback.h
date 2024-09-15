/*
Took from: https://stackoverflow.com/questions/77005/how-to-automatically-generate-a-stacktrace-when-my-program-crashes
*/

#ifndef TRACEBACK_H_
#define TRACEBACK_H_

#include <stdio.h>
#include <execinfo.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>


int TB_enable(void);
void handler(int sig);

#endif