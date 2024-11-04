#include "../include/cache.h"

/*
Global Cache Table used for caching results of I/O operations.
*/
static __thread cache_t GCT[ENTRY_COUNT];


int CHC_init() {
    for (int i = 0; i < ENTRY_COUNT; i++) {
        GCT[i].free = NULL;
        GCT[i].save = NULL;
        GCT[i].type = -1;
        GCT[i].pointer = NULL;
    }

    return 1;
}

int CHC_add_entry(void* entry, char* name, uint8_t type, void* free, void* save) {
    if (entry == NULL) return -2;
    print_debug("Adding to GCT [%s] entry with type [%i]", name, type);

    int current = -1;
    int free_current = -1;
    int occup_current = -1;

    for (int i = 0; i < ENTRY_COUNT; i++) {
        if (GCT[i].pointer != NULL) {
            if (THR_test_lock(&((cache_body_t*)GCT[i].pointer)->lock, omp_get_thread_num()) == UNLOCKED) {
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

    if (GCT[current].pointer != NULL) {
        if (THR_require_lock(&((cache_body_t*)GCT[current].pointer)->lock, omp_get_thread_num()) != -1) {
            GCT[current].save(GCT[current].pointer, NULL);
            CHC_flush_index(current);
        }
        else {
            print_error("Can't lock selected entry [%s] entry with type [%i] during CHC_add_entry()", GCT[current].name, GCT[current].type);
            return -1;
        }
    }

    GCT[current].pointer = entry;
    strncpy(GCT[current].name, name, ENTRY_NAME_SIZE);
    GCT[current].type = type;
    GCT[current].free = free;
    GCT[current].save = save;
    return 1;
}

void* CHC_find_entry(char* name, uint8_t type) {
    for (int i = 0; i < ENTRY_COUNT; i++) {
        if (GCT[i].pointer == NULL) continue;
        if (strncmp(GCT[i].name, name, ENTRY_NAME_SIZE) == 0 && GCT[i].type == type) {
            print_debug("Found entry in GCT [%s] entry with type [%i]", GCT[i].name, type);
            return GCT[i].pointer;
        }
    }

    return NULL;
}

int CHC_sync() {
    for (int i = 0; i < ENTRY_COUNT; i++) {
        if (GCT[i].pointer == NULL) continue;
        if (THR_require_lock(&((cache_body_t*)GCT[i].pointer)->lock, omp_get_thread_num()) == 1) {
            GCT[i].save(GCT[i].pointer, NULL);
            THR_release_lock(&((cache_body_t*)GCT[i].pointer)->lock, omp_get_thread_num());
        }
        else {
            print_error("Can't lock entry [%s] entry with type [%i] during CHC_sync()", GCT[i].name, GCT[i].type);
            return -1;
        }
    }

    return 1;
}

int CHC_free() {
    for (int i = 0; i < ENTRY_COUNT; i++) {
        if (GCT[i].pointer == NULL) continue;
        if (THR_require_lock(&((cache_body_t*)GCT[i].pointer)->lock, omp_get_thread_num()) != -1) CHC_flush_index(i);
        else {
            print_error("Can't lock entry [%s] entry with type [%i] during CHC_free()", GCT[i].name, GCT[i].type);
            return -1;
        }
    }

    return 1;
}

int CHC_flush_entry(void* entry, uint8_t type) {
    if (entry == NULL) return -1;

    int index = -1;
    for (int i = 0; i < ENTRY_COUNT; i++) {
        if (GCT[i].pointer == NULL) continue;
        if (entry == GCT[i].pointer && type == GCT[i].type) {
            index = i;
            break;
        }
    }

    if (index != -1) CHC_flush_index(index);
    else return -2;
    return 1;
}

int CHC_flush_index(int index) {
    if (GCT[index].pointer == NULL) return -1;
    GCT[index].free(GCT[index].pointer);
    GCT[index].pointer = NULL;
    return 1;
}
