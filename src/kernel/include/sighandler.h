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
*/

#ifndef TRACEBACK_H_
#define TRACEBACK_H_

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>

#ifndef NO_TRACE
#ifndef _WIN32
    #include <execinfo.h>
    #include <unistd.h>
#endif
#endif

#include "kentry.h"


/*
Enable trace back.
Took from: https://stackoverflow.com/questions/77005/how-to-automatically-generate-a-stacktrace-when-my-program-crashes
*/
int TB_enable(void);

/*
Enable auto memory cleanup at SIGTERM.
*/
int CL_enable(void);

/*
Signal handler for trace back.
*/
void traceback_handler(int sig);

/*
Cleanup handler.
*/
void cleanup_handler();

#endif
