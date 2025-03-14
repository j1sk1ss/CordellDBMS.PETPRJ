#include "../../include/pageman.h"


#pragma region [CRUD]

int PGM_get_content(page_t* __restrict page, int offset, unsigned char* __restrict buffer, size_t data_length) {
    int end_index = MIN(PAGE_CONTENT_SIZE, (int)data_length + offset);
    for (int i = offset, j = 0; i < end_index && j < (int)data_length; i++, j++) buffer[j] = page->content[i];
    return end_index - offset;
}

int PGM_insert_content(page_t* __restrict page, int offset, unsigned char* __restrict data, size_t data_length) {
    int end_index = MIN(PAGE_CONTENT_SIZE, (int)data_length + offset);
    for (int i = offset, j = 0; i < end_index && j < (int)data_length; i++, j++) page->content[i] = data[j];
    return end_index - offset;
}

int PGM_delete_content(page_t* page, int offset, size_t length) {
#ifndef NO_DELETE_COMMAND
    int end_index = MIN(PAGE_CONTENT_SIZE, offset + (int)length);
    for (int i = offset; i < end_index; i++) page->content[i] = PAGE_EMPTY;
    return end_index - offset;
#endif
    return 1;
}

#pragma endregion

int PGM_find_content(page_t* __restrict page, int offset, unsigned char* __restrict data, size_t data_size) {
    if (offset >= PAGE_CONTENT_SIZE) return -2;
    for (int i = offset; i <= PAGE_CONTENT_SIZE - (int)data_size; i++) {
        if (memcmp(page->content + i, data, data_size) == 0) return i;
    }

    return -1;
}

int PGM_get_free_space(page_t* page, int offset) {
    int count = 0;
    for (int i = offset; i < PAGE_CONTENT_SIZE; i++) {
        if (page->content[i] == PAGE_EMPTY) count++;
        else if (offset != -1) break;
    }

    return count;
}

int PGM_get_fit_free_space(page_t* page, int offset, int size) {
    if (!page) return -1;
    unsigned char* first_empty = (unsigned char*)memchr(page->content, PAGE_EMPTY, PAGE_CONTENT_SIZE);
    if (!first_empty) return -1;

    int index = first_empty - page->content;
    if (offset == -1) return index;

    int start_index = MAX(offset, index);
    int free_index = -2, current_size = 0;
    for (int i = start_index; i < PAGE_CONTENT_SIZE; i++) {
        if (page->content[i] == PAGE_EMPTY) {
            if (free_index == -2) free_index = i;
            if (++current_size >= size) return free_index;
        } 
        else {
            free_index = -2;
            current_size = 0;
        }
    }

    return -2;
}

int PGM_get_page_occupie_size(page_t* page, int offset) {
    int eof = 0;
    for (int i = offset; i < PAGE_CONTENT_SIZE; i++) {
        if (page->content[i] != PAGE_EMPTY) continue;
        int current_eof = i;
        for (int j = current_eof; j < PAGE_CONTENT_SIZE; j++) {
            if (page->content[j] != PAGE_EMPTY) {
                current_eof = -1;
                i = j;
                break;
            }
        }

        if (current_eof >= 0) {
            eof = current_eof;
            break;
        }
    }

    return MAX(MIN(eof, PAGE_CONTENT_SIZE), 0);
}
