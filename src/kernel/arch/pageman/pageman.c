#include "../../include/pageman.h"


#pragma region [CRUD]

int PGM_get_content(page_t* __restrict page, int offset, unsigned char* __restrict buffer, size_t data_length) {
    int end_index = MIN(PAGE_CONTENT_SIZE, (int)data_length + offset);
    for (int i = offset, j = 0; i < end_index && j < (int)data_length; i++, j++) {
        buffer[j] = (unsigned char)decode_hamming_15_11(page->content[i]);
    }

    return end_index - offset;
}

int PGM_insert_content(page_t* __restrict page, int offset, unsigned char* __restrict data, size_t data_length) {
    int end_index = MIN(PAGE_CONTENT_SIZE, (int)data_length + offset);
    for (int i = offset, j = 0; i < end_index && j < (int)data_length; i++, j++) {
        page->content[i] = encode_hamming_15_11((unsigned short)data[j]);
    }

    return end_index - offset;
}

int PGM_delete_content(page_t* page, int offset, size_t length) {
#ifndef NO_DELETE_COMMAND
    int end_index = MIN(PAGE_CONTENT_SIZE, (int)length + offset);
    for (int i = offset; i < end_index; i++) page->content[i] = encode_hamming_15_11((unsigned short)PAGE_EMPTY);
    return end_index - offset;
#endif
    return 1;
}

#pragma endregion

int PGM_find_content(page_t* __restrict page, int offset, unsigned char* __restrict data, size_t data_size) {
    if (offset >= PAGE_CONTENT_SIZE) return -2;
    for (int i = offset; i <= PAGE_CONTENT_SIZE - (int)data_size; i++) {
        int found = 1;
        for (int j = i, k = 0; j < PAGE_CONTENT_SIZE && k < (int)data_size; j++, k++) {
            if (decode_hamming_15_11(page->content[j]) != data[k]) {
                found = -1;
                break;
            }
        } 
        
        if (found) return 1;
    }

    return -1;
}

int PGM_get_free_space(page_t* page, int offset) {
    int count = 0;
    for (int i = offset; i < PAGE_CONTENT_SIZE; i++) {
        if (decode_hamming_15_11(page->content[i]) == PAGE_EMPTY) count++;
        else if (offset != -1) break;
    }

    return count;
}

int PGM_get_fit_free_space(page_t* page, int offset, int size) {
    int index = 0;
    while (decode_hamming_15_11(page->content[index]) != PAGE_EMPTY) {
        if (++index > PAGE_CONTENT_SIZE) return -1;
    }

    if (offset == -1) return index;
    for (int i = MAX(offset, index); i < PAGE_CONTENT_SIZE; i++) {
        if (decode_hamming_15_11(page->content[i]) == PAGE_EMPTY) {
            int free_index = i;
            for (int current_size = 0; i < PAGE_CONTENT_SIZE && decode_hamming_15_11(page->content[i]) == PAGE_EMPTY; i++, current_size++) {
                if (current_size >= size) return free_index;
            }
        }
    }

    return -2;
}
