#include "../include/cache.h"

/*
Global Cache Table used for caching results of I/O operations.
*/
static cache_t GCT[ENTRY_COUNT];

/*
This defined vars guaranty, that database will take only:
2 * 2080 bytes (for table_t)
4 * 2056 bytes (for directory_t)
4 * 4112 bytes (for page_t)

In summary, whole GCT will take 28KB of RAM.
Reduction of ENTRY_COUNT and MAX_TABLE_ENTRY, MAX_DIRECTORY_ENTRY, MAX_PAGE_ENTRY
will decrease usage of RAM by next formula:
X * 2080 bytes (for table_t)
Y * 2056 bytes (for directory_t)
Z * 4112 bytes (for page_t)

0 index - pages,
1 index - directories,
2 index - tables
*/
static int GCT_TYPES[CACHE_TYPES_COUNT] = { 0 };
static int GCT_TYPES_MAX[CACHE_TYPES_COUNT] = { 4, 2, 2 };


static int _flush_index(int index) {
    if (GCT[index].pointer == NULL) return -1;
    GCT[index].free(GCT[index].pointer);
    GCT[index].pointer = NULL;

    GCT[index].free = NULL;
    GCT[index].save = NULL;
    GCT[index].type = ANY_CACHE;

    return 1;
}

int CHC_init() {
    for (int i = 0; i < ENTRY_COUNT; i++) {
        GCT[i].free = NULL;
        GCT[i].save = NULL;
        GCT[i].type = ANY_CACHE;
        GCT[i].pointer = NULL;
    }

    return 1;
}

int CHC_add_entry(void* entry, char* name, unsigned char type, void* free, void* save) {
    if (entry == NULL) return -2;
    ((cache_body_t*)entry)->is_cached = 0;

    int free_current   = -1;
    int occup_current  = -1;
    int should_replace = 0;

    if (GCT_TYPES[type] >= GCT_TYPES_MAX[type]) should_replace = 1;
    for (int i = 0; i < ENTRY_COUNT; i++) {
        if (GCT[i].pointer != NULL) {
            if (THR_test_lock(&((cache_body_t*)GCT[i].pointer)->lock, omp_get_thread_num()) == UNLOCKED) {
                if (GCT[i].type == type && should_replace == 1) {
                    occup_current = i;
                    break;
                }
            }
        }
        else {
            free_current = i;
        }
    }

    int current = -1;
    if (occup_current != -1) current = occup_current;
    else if (free_current != -1 && should_replace == 0) current = free_current;
    else return -4;

    if (GCT[current].pointer != NULL) {
        if (THR_require_lock(&((cache_body_t*)GCT[current].pointer)->lock, omp_get_thread_num()) != -1) {
            #pragma omp critical (gct_types_decreese)
            GCT_TYPES[GCT[current].type] = MAX(GCT_TYPES[GCT[current].type] - 1, 0);
            GCT[current].save(GCT[current].pointer);
            _flush_index(current);
        }
        else {
            return -1;
        }
    }

    ((cache_body_t*)entry)->is_cached = 1;

    GCT[current].pointer = entry;
    strncpy_s(GCT[current].name, name, ENTRY_NAME_SIZE);
    GCT[current].type = type;
    GCT[current].free = free;
    GCT[current].save = save;

    #pragma omp critical (gct_types_increase)
    GCT_TYPES[type] = MIN(GCT_TYPES[type] + 1, GCT_TYPES_MAX[type]);
    return 1;
}

void* CHC_find_entry(char* name, unsigned char type) {
    for (int i = 0; i < ENTRY_COUNT; i++) {
        if (GCT[i].pointer == NULL) continue;
        if (strncmp_s(GCT[i].name, name, ENTRY_NAME_SIZE) == 0 && (GCT[i].type == type || type == ANY_CACHE)) {
            return GCT[i].pointer;
        }
    }

    return NULL;
}

int CHC_sync() {
    for (int i = 0; i < ENTRY_COUNT; i++) {
        if (GCT[i].pointer == NULL) continue;
        if (THR_require_lock(&((cache_body_t*)GCT[i].pointer)->lock, omp_get_thread_num()) == 1) {
            GCT[i].save(GCT[i].pointer);
            THR_release_lock(&((cache_body_t*)GCT[i].pointer)->lock, omp_get_thread_num());
        }
        else {
            return -1;
        }
    }

    return 1;
}

int CHC_free() {
    for (int i = 0; i < ENTRY_COUNT; i++) {
        if (GCT[i].pointer == NULL) continue;
        if (THR_require_lock(&((cache_body_t*)GCT[i].pointer)->lock, omp_get_thread_num()) != -1) _flush_index(i);
        else {
            return -1;
        }
    }

    return 1;
}

int CHC_flush_entry(void* entry, unsigned char type) {
    if (entry == NULL) return -1;

    int index = -1;
    for (int i = 0; i < ENTRY_COUNT; i++) {
        if (GCT[i].pointer == NULL) continue;
        if (entry == GCT[i].pointer && type == GCT[i].type) {
            index = i;
            break;
        }
    }

    if (index != -1) _flush_index(index);
    else return -2;
    return 1;
}
