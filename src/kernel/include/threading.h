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

#include "common.h"

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
  #ifndef NO_THREADS
  #include <pthread.h>
  #endif
#endif


/*
Main idea, that lock status of object can be stored in one unsigned short object.
In first 14 bits we store info about lock owner (or NO_OWNER) thread number:
0b00000000000000XX
In second 2 bits, lock state (LOCKED/UNLOCKED)
0bXXXXXXXXXXXXXX00
*/
#define PACK_LOCK(status, owner) (((status) & 0x3) << 0) | (((owner) & (0xFFFC >> 2)) << 2)

#define UNPACK_STATUS(lock) (((lock) >> 0) & 0x3)
#define UNPACK_OWNER(lock)  (((lock) >> 2) & (0xFFFC >> 2))


/*
Cross-platform thread creation tool.
Note: entry should had next sighnature: void* <name>(void* args)

Params:
- entry - Thread function entry-point.
- args - Entry args for thread.

Return 1 if thread create.
Return -1 if thread can't be created.
*/
int THR_create_thread(void* (*entry)(void*), void* args);

/*
Kill thread by file descriptor.

Params:
- fd - Descriptor.

Return 1 if kill success.
*/
int THR_kill_thread(int fd);

/*
Create empty lock with next params:
Lock owner: NO_ONWER
Lock status: UNLOCKED

Return lock
*/
int THR_create_lock();

/*
Lock for working.
Note: If we earn delay of lock try, we return -1.
Note 2: Be sure, that lock not NULL.

Params:
- lock - pointer to object lock.
- owner - thread, that want lock this table.

Return -1 if we can`t require lock (for some reason)
Return 1 if lock now locked.
*/
int THR_require_lock(unsigned short* lock, unsigned char owner);

/*
Check lock status of table.

Params:
- table - pointer to table.
- owner - thread, that want test this table.

Return lock status (LOCKED and UNLOCKED).
*/
int THR_test_lock(unsigned short* lock, unsigned char owner);

/*
Realise table for working.

Params:
- table - pointer to table.
- owner - thread, that want release this table.

Return -3 if table is NULL.
Return -2 if this table has another owner.
Return -1 if table was unlocked. (Nothing changed)
Return 1 if table now unlocked.
*/
int THR_release_lock(unsigned short* lock, unsigned char owner);

#endif
