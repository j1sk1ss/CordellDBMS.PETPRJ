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


#ifndef THREADING_H_
#define THREADING_H_

#include <stdlib.h>


#define LOCKED    1
#define UNLOCKED  0
#define NO_OWNER  0xFF

#ifndef _OPENMP
  #define omp_get_thread_num() 0
  #define omp_set_num_threads(num)
#else
  #include <omp.h>
#endif

#ifdef _WIN32
  #include <windows.h>
  #define __thread
#else
  #include <pthread.h>
#endif


/*
Cross-platform thread creation tool.
Note: entry should had next sighnature: void* <name>(void* args)

Params:
- entry - Thread function entry-point.
- args - Entry args for thread.

Return 1 if thread create.
Return -1 if thread can't be created.
*/
int THR_create_thread(void* entry, void* args);

#endif
