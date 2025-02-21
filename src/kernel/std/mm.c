#include "../include/mm.h"


static unsigned char buffer[BUFFER_SIZE] = { 0 };
static mm_block_t* free_list = (mm_block_t*)buffer;


int init_memory() {
    free_list->size = BUFFER_SIZE - sizeof(mm_block_t);
    free_list->free = 1;
    free_list->next = NULL;
    return 1;
}

void* malloc_s(size_t size) {
    if (size == 0) return NULL;
    size = (size + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1);
    mm_block_t* current = free_list;
    while (current) {
        if (current->free && current->size >= size) {
            if (current->size >= size + sizeof(mm_block_t)) {
                mm_block_t* new_block = (mm_block_t*)((unsigned char*)current + sizeof(mm_block_t) + size);
                new_block->size = current->size - size - sizeof(mm_block_t);
                new_block->free = 1;
                new_block->next = current->next;
                current->next = new_block;
                current->size = size;
            }

            current->free = 0;
            return (unsigned char*)current + sizeof(mm_block_t);
        }

        current = current->next;
    }

    return NULL;
}

void* realloc_s(void* ptr, size_t elem) {
    if (ptr == NULL) return malloc_s(elem);
    if (elem == 0) {
        free_s(ptr);
        return NULL;
    }

    mm_block_t* block = (mm_block_t*)((unsigned char*)ptr - sizeof(mm_block_t));
    size_t old_size = block->size;
    if (elem <= old_size) {
        return ptr;
    }

    mm_block_t* next_block = (mm_block_t*)((unsigned char*)block + sizeof(mm_block_t) + block->size);
    if (next_block && next_block->free && (block->size + sizeof(mm_block_t) + next_block->size) >= elem) {
        block->size += sizeof(mm_block_t) + next_block->size;
        block->free = 0;
        next_block->next = next_block->next;
        return ptr;
    }

    void* new_ptr = malloc_s(elem);
    if (new_ptr == NULL) {
        return NULL;
    }

    memcpy_s(new_ptr, ptr, old_size);
    free_s(ptr);
    return new_ptr;
}


int free_s(void* ptr) {
    if (!ptr || ptr < (void*)buffer || ptr >= (void*)(buffer + BUFFER_SIZE)) {
        return -1;
    }
    
    mm_block_t* block = (mm_block_t*)((unsigned char*)ptr - sizeof(mm_block_t));
    block->free = 1;
    
    mm_block_t* current = free_list;
    while (current && current->next) {
        if (current->free && current->next->free) {
            current->size += sizeof(mm_block_t) + current->next->size;
            current->next = current->next->next;
        }
        current = current->next;
    }

    return 1;
}