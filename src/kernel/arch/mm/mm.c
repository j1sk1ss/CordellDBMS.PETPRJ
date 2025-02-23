#include "../../include/mm.h"


static unsigned char _buffer[ALLOC_BUFFER_SIZE];
static mm_block_t* _mm_head = (mm_block_t*)_buffer;
static int _allocated = 0;


int init_memory() {
    _mm_head->magic = MM_BLOCK_MAGIC;
    _mm_head->size  = ALLOC_BUFFER_SIZE - sizeof(mm_block_t);
    _mm_head->free  = 1;
    _mm_head->next  = NULL;
    return 1;
}

static int __coalesce_memory() {
    int merged = 0;
    mm_block_t* current = _mm_head;
    
    do {
        merged = 0;
        current = _mm_head;

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
    if (size == 0) return NULL;
    if (prepare_mem) __coalesce_memory();

    size = (size + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1);
    mm_block_t* current = _mm_head;
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
            _allocated += current->size + sizeof(mm_block_t);
            print_mm("Allocated node with [%i] size / [%i]", current->size, _allocated);
            return (unsigned char*)current + sizeof(mm_block_t);
        }

        current = current->next;
    }

    print_mm("Allocation error! I can't allocate [%i]!", size);
    return prepare_mem ? NULL : __malloc_s(size, 1);
}

void* malloc_s(size_t size) {
    void* ptr = __malloc_s(size, 0);
    if (!ptr) print_mm("Allocation error! I can't allocate [%i]!", size);
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
    if (!ptr || ptr < (void*)_buffer || ptr >= (void*)(_buffer + ALLOC_BUFFER_SIZE)) return -1;
    
    mm_block_t* block = (mm_block_t*)((unsigned char*)ptr - sizeof(mm_block_t));
    if (block->magic != MM_BLOCK_MAGIC) return -1;
    if (block->free) return -1;

    block->free = 1;
    _allocated -= block->size + sizeof(mm_block_t);
    print_mm("Free [%p] with [%i] size / [%i]", ptr, block->size, _allocated);
    
    return 1;
}
