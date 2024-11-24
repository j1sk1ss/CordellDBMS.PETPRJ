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
 *  Description:
 *  This file is global cache manager for caching results of IO operations with disk.
 *  For working with this manager, object should have unsigned short field at top of struct.
 * 
 *  CordellDBMS source code: https://github.com/j1sk1ss/CordellDBMS.EXMPL
 *  Credits: j1sk1ss
 */

#ifndef CACHE_H_
#define CACHE_H_

#include <string.h>
#include <stdlib.h>

#include "common.h"
#include "logging.h"
#include "threading.h"


#define ENTRY_COUNT     10
#define ENTRY_NAME_SIZE 8

#define CACHE_TYPES_COUNT   3
#define ANY_CACHE       0xFF
#define TABLE_CACHE     2
#define DIRECTORY_CACHE 1
#define PAGE_CACHE      0


typedef struct cache_body {
    unsigned short lock;
    unsigned char is_cached;
    void* body;
} cache_body_t;

typedef struct cache_entry {
    char name[ENTRY_NAME_SIZE];
    unsigned char type;
    void* pointer;

    void (*free)(void* p);
    void (*save)(void* p, char* path);
} cache_t;


/*
Cache init fill GCT by empty entrie.

Return 1 if init was success.
*/
int CHC_init();

/*
Cache add entry add to GCT a new entry.

Params:
- entry - Pointer to object, that should be saved in GCT.
- name - Object name.
- type - Object type.
- free - Pointer to object free function | free(void* entry).
- save - Pointer to object save file function | save(void* entry, char* path).

Return -3 if entry type reach limit in GCT.
Return -2 if entry is NULL.
Return -1 if by some reason, function can't lock entry.
Return 1 if add was success.
*/
int CHC_add_entry(void* entry, char* name, unsigned char type, void* free, void* save);

/*
Cache find entry find entry in GCT by provided name and type.

Params:
- name - Object name.
- type - Object type.

Return NULL if entry wasn't found.
Return pointer to entry, if entry was found.
*/
void* CHC_find_entry(char* name, unsigned char type);

/*
Save and load entries from GCT.

Return -1 if something goes wrong.
Return 1 if sync success.
*/
int CHC_sync();

/*
Free GCT entries. In difference with CHC_sync() function, this will avoid
working with disk. That's why this function used in DB rollback.

Return 1 if free was correct.
Return -1 if function can't lock any entry.
*/
int CHC_free();

/*
Hard cleanup of GCT. Really not recomment for use!
Note: It will just unload data from GCT to disk by provided index.
Note 2: Empty space will be marked by NULL.

Params:
- entry - Pointer to entry for flushing.
- type - Object type for flushing.

Return -1 if something goes wrong.
Return 1 if cleanup success.
*/
int CHC_flush_entry(void* entry, unsigned char type);

/*
Hard cleanup of GCT. Really not recomment for use!
Note: It will just unload data from GCT to disk by provided index.
Note 2: Empty space will be marked by NULL.

Params:
- index - index of entry for flushing.

Return -1 if something goes wrong.
Return 1 if cleanup success.
*/
int CHC_flush_index(int index);

#endif