#include "../../include/cache.h"


static __thread cache_t TABLE[ENTRY_COUNT];


int CHC_init() {
    for (int i = 0; i < ENTRY_COUNT; i++) {
        TABLE[i].free = NULL;
        TABLE[i].save = NULL;
        TABLE[i].type = -1;
        TABLE[i].pointer = NULL;
    }
}

int CHC_add_entry(void* entry, char* name, uint8_t type, void* free, void* save) {
    if (entry == NULL) return -2;

    int current = -1;
    int free_current = -1;
    int occup_current = -1;

    for (int i = 0; i < ENTRY_COUNT; i++) {
        if (TABLE[i].pointer != NULL) {
            if (THR_test_lock(&((cache_body_t*)TABLE[i].pointer)->lock, omp_get_thread_num()) == UNLOCKED) {
                occup_current = i;
            }
        }
        else {
            free_current = i;
            break;
        }
    }

    if (free_current == -1 && occup_current == -1) return -1;
    else if (free_current != -1) current = free_current;
    else if (occup_current != -1) current = occup_current;

    if (TABLE[current].pointer != NULL) {
        if (THR_require_lock(&((cache_body_t*)TABLE[current].pointer)->lock, omp_get_thread_num()) != -1) {
            TABLE[current].save(TABLE[current].pointer, NULL);
            CHC_flush_index(current);
        }
        else return -1;
    }

    TABLE[current].pointer = entry;
    strncpy(TABLE[current].name, name, ENTRY_NAME_SIZE);
    TABLE[current].type = type;
    TABLE[current].free = free;
    TABLE[current].save = save;
    return 1;
}

void* CHC_find_entry(char* name, uint8_t type) {
    for (int i = 0; i < ENTRY_COUNT; i++) {
        if (TABLE[i].pointer == NULL) continue;
        if (strncmp(TABLE[i].name, name, ENTRY_NAME_SIZE) == 0 && TABLE[i].type == type) return TABLE[i].pointer;
    }

    return NULL;
}

int CHC_sync() {
    for (int i = 0; i < ENTRY_COUNT; i++) {
        if (TABLE[i].pointer == NULL) continue;
        if (THR_require_lock(&((cache_body_t*)TABLE[i].pointer)->lock, omp_get_thread_num()) == 1) {
            TABLE[i].save(TABLE[i].pointer, NULL);
            THR_release_lock(&((cache_body_t*)TABLE[i].pointer)->lock, omp_get_thread_num());
        }

        return -1;
    }

    return 1;
}

int CHC_free() {
    for (int i = 0; i < ENTRY_COUNT; i++) {
        if (TABLE[i].pointer == NULL) continue;
        if (THR_require_lock(&((cache_body_t*)TABLE[i].pointer)->lock, omp_get_thread_num()) != -1) CHC_flush_index(i);
        else return -1;
    }

    return 1;
}

int CHC_flush_entry(void* entry, char* name, uint8_t type) {
    if (entry == NULL) return -1;

    int index = -1;
    for (int i = 0; i < ENTRY_COUNT; i++) {
        if (TABLE[i].pointer == NULL) continue;
        if (entry == TABLE[i].pointer) {
            index = i;
            break;
        }
    }

    if (index != -1) CHC_flush_index(index);
    else return -2;
    return 1;
}

int CHC_flush_index(int index) {
    if (TABLE[index].pointer == NULL) return -1;
    TABLE[index].free(TABLE[index].pointer);
    TABLE[index].pointer = NULL;
    return 1;
}
