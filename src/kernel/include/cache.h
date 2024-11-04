#ifndef CACHE_H_
#define CACHE_H_

#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "common.h"
#include "threading.h"


#define ENTRY_COUNT     20
#define ENTRY_NAME_SIZE 8

#define TABLE_CACHE     2
#define DIRECTORY_CACHE 1
#define PAGE_CACHE      0


typedef struct cache_body {
    uint16_t lock;
    void* body;
} cache_body_t;

typedef struct cache_entry {
    char name[ENTRY_NAME_SIZE];
    uint8_t type;
    void* pointer;

    void (*free)(void* p);
    void (*save)(void* p, char* path);
} cache_t;


/*

*/
int CHC_init();

/*

*/
int CHC_add_entry(void* entry, char* name, uint8_t type, void* free, void* save);

/*

*/
void* CHC_find_entry(char* name, uint8_t type);

/*

*/
int CHC_sync();

/*

*/
int CHC_free();

/*

*/
int CHC_flush_entry(void* entry, uint8_t type);

/*

*/
int CHC_flush_index(int index);

#endif