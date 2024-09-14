#include "../include/pageman.h"


#pragma region [Content]

    int PGM_append_content(page_t* page, uint8_t* data, size_t data_lenght) {
        int eof = PGM_find_value(page, PAGE_START, PAGE_END);
        for (int i = eof, j = 0; i < (int)data_lenght + eof, j < (int)data_lenght; i++, j++) page->content[i] = data[j];
        page->content[eof + (int)data_lenght] = PAGE_END;
        return 1;
    }

    int PGM_insert_content(page_t* page, uint8_t offset, uint8_t* data, size_t data_lenght) {
        for (int i = offset; i < MIN(PAGE_CONTENT_SIZE - offset, (int)data_lenght); i++) page->content[i] = data[i];
        if (data_lenght + offset > PAGE_CONTENT_SIZE) return 2;

        return 1;
    }

    int PGM_delete_content(page_t* page, int offset, size_t length) {
        for (int i = offset; i < MIN(PAGE_CONTENT_SIZE - offset, (int)length); i++) page->content[i] = PAGE_EMPTY;
        return 1;
    }

    int PGM_find_value(page_t* page, int offset, uint8_t value) {
        int index = offset;
        while (page->content[index++] != value) {}
        return MAX(index - 1, 0);
    }

    int PGM_get_free_space(page_t* page, int offset) {
        // We skip any data that not empty
        int index = 0;
        while (page->content[index++] != PAGE_EMPTY);
        
        // We count empty symbols
        int count = 0;
        for (int i = MAX(offset, index); i < PAGE_CONTENT_SIZE; i++) {
            if (page->content[i] == PAGE_EMPTY) count++;
            else if (offset != -1) break;
        }

        return count;
    }

    int PGM_get_fit_free_space(page_t* page, int offset, int size) {
        int index = 0;
        while (page->content[index++] != PAGE_EMPTY);

        if (offset == -1) return index;

        int is_reading = 0;
        int free_index = index;
        int current_size = 0;
        for (int i = MAX(offset, index); i < PAGE_CONTENT_SIZE; i++) {
            if (page->content[i] == PAGE_EMPTY) {
                if (is_reading == 0) free_index = i;
                is_reading = 1;

                if (++current_size >= size) return free_index;
            }
            else {
                is_reading = 0;
                current_size = 0;
            }
        }

        return -2;
    }

#pragma endregion

#pragma region [Page]

    page_t* PGM_create_page(char* name, uint8_t* buffer, size_t data_size) {
        page_t* page = (page_t*)malloc(sizeof(page_t));
        page_header_t* header = (page_header_t*)malloc(sizeof(page_header_t));

        header->magic = PAGE_MAGIC;
        strncpy((char*)header->name, name, PAGE_NAME_SIZE);

        page->header = header;
        memcpy((char*)page->content, (char*)buffer, data_size);

        int data_end = MIN(PAGE_CONTENT_SIZE, strlen((char*)buffer));
        page->content[data_end] = PAGE_END;
        for (int i = data_end + 1; i < PAGE_CONTENT_SIZE; i++) page->content[i] = PAGE_EMPTY;

        return page;
    }

    int PGM_save_page(page_t* page, char* path) {
        // Open or create file
        FILE* file = fopen(path, "wb");
        if (file == NULL) {
            return -1;
        }
        
        // Write data to disk
        fwrite(page->header, sizeof(page_header_t), SEEK_CUR, file);
        fwrite(page->content, PAGE_CONTENT_SIZE, SEEK_CUR, file);

        // Close file
        fclose(file);

        return 1;
    }

    page_t* PGM_load_page(char* name) {
        // Open file page
        FILE* file = fopen(name, "rb");
        if (file == NULL) {
            return NULL;
        }

        // Read header from file
        uint8_t* header_data = (uint8_t*)malloc(sizeof(page_header_t));
        fread(header_data, sizeof(page_header_t), SEEK_CUR, file);
        page_header_t* header = (page_header_t*)header_data;

        // Check page magic
        if (header->magic != PAGE_MAGIC) {
            free(header);
            return NULL;
        }

        // Allocate memory for page structure
        page_t* page = (page_t*)malloc(sizeof(page_t));
        fread(page->content, PAGE_CONTENT_SIZE, SEEK_CUR, file);
        page->header = header;

        // Close file page
        fclose(file);

        return page;
    }

    int PGM_free_page(page_t* page) {
        free(page->header);
        free(page);

        return 1;
    }

#pragma endregion