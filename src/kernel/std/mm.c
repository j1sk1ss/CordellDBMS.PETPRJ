#include "../include/mm.h"


static unsigned char buffer[ALLOC_BUFFER_SIZE] = { 0 };
static mm_block_t* free_list = (mm_block_t*)buffer;
static int _allocated = 0;


int init_memory() {
    free_list->magic = MM_BLOCK_MAGIC;
    free_list->size  = ALLOC_BUFFER_SIZE - sizeof(mm_block_t);
    free_list->free  = 1;
    free_list->next  = NULL;
    return 1;
}

static int __coalesce_memory() {
    mm_block_t* current = free_list;
    int merged = 1;
    
    do {
        merged = 0;
        current = free_list;

        while (current && current->next) {
            if (current->free && current->next->free) {
                current->size += sizeof(mm_block_t) + current->next->size;
                current->next = current->next->next;
                merged = 1;
            } else {
                current = current->next;
            }
        }
    } while (merged);
    return 1;
}

static void* __malloc_s(size_t size, int prepare_mem) {
    print_mm("Try to allocate %i / %i", size, _allocated);
    if (size == 0) return NULL;
    if (prepare_mem) __coalesce_memory();

    size = (size + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1);
    mm_block_t* current = free_list;
    while (current) {
        if (current->free && current->size >= size) {
            if (current->size >= size + sizeof(mm_block_t)) {
                mm_block_t* new_block = (mm_block_t*)((unsigned char*)current + sizeof(mm_block_t) + size);
                new_block->magic = MM_BLOCK_MAGIC;
                new_block->size = current->size - size - sizeof(mm_block_t);
                new_block->free = 1;
                new_block->next = current->next;

                current->next = new_block;
                current->size = size;
            }

            current->free = 0;
            _allocated += size + sizeof(mm_block_t);
            return (unsigned char*)current + sizeof(mm_block_t);
        }

        current = current->next;
    }

    print_mm("Allocation error! I can't allocate [%i]!", (int)size);
    return prepare_mem ? NULL : __malloc_s(size, 1);
}

void* malloc_s(size_t size) {
    void* ptr = __malloc_s(size, 0);
    if (!ptr) print_mm("Allocation error! I can't allocate [%i]!", (int)size);
    return ptr;
}

void* realloc_s(void* ptr, size_t elem) {
    void* new_data = NULL;
    if (elem) {
        if(!ptr) return malloc_s(elem);
        new_data = malloc_s(elem);
        if(new_data) {
            memcpy_s(new_data, ptr, elem);
            free_s(ptr);
        }
    }

    return new_data;
}

int free_s(void* ptr) {
    print_mm("Try to free [%p]", ptr);
    if (!ptr || ptr < (void*)buffer || ptr >= (void*)(buffer + ALLOC_BUFFER_SIZE)) return -1;
    
    mm_block_t* block = (mm_block_t*)((unsigned char*)ptr - sizeof(mm_block_t));
    if (block->magic != MM_BLOCK_MAGIC) return -1;
    if (block->free) return -1;

    block->free = 1;
    _allocated -= block->size + sizeof(mm_block_t);
    
    return 1;
}
