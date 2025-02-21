#ifndef MM_H_
#define MM_H_

#include <stddef.h>
#include "common.h"

#define BUFFER_SIZE 81920
#define ALIGNMENT 8  


typedef struct mm_block {
    size_t size;
    int free;
    struct mm_block* next;
} mm_block_t;


/*
Init first memory block in memory manager.

Return -1 if something goes wrong.
Return 1 if success init.
*/
int init_memory();

/*
Allocate memory block.

Params:
    - size - Memory block size.

Return NULL if can't allocate memory.
Return pointer to allocated memory.
*/
void* malloc_s(size_t size);

/*
Realloc pointer to new location with new size.

Params:
    - ptr - Pointer to old place.
    - elem - Size of new allocated area.

Return NULL if can't allocate data.
Return pointer to new allocated area.
*/
void* realloc_s(void* ptr, size_t elem);

/*
Free allocated memory.

Params:
    - ptr - Pointer to allocated data.

Return -1 if something goes wrong.
Return 1 if free success.
*/
int free_s(void* ptr);

#endif