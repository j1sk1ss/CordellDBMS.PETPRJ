#include "../../include/pageman.h"


#pragma region [Content]

    int PGM_insert_value(page_t* page, int offset, uint8_t value) {
        if (offset < PAGE_CONTENT_SIZE) {
            page->content[offset] = value;
            return 1;
        } else return -1;
    }

    int PGM_insert_content(page_t* page, int offset, uint8_t* data, size_t data_length) {
        int end_index = MIN(PAGE_CONTENT_SIZE, (int)data_length + offset);
        for (int i = offset, j = 0; i < end_index && j < (int)data_length; i++, j++) page->content[i] = data[j];
        return (int)data_length - (end_index - offset);
    }

    int PGM_delete_content(page_t* page, int offset, size_t length) {
        int end_index = MIN(PAGE_CONTENT_SIZE - offset, (int)length + offset);
        for (int i = offset; i < end_index; i++) page->content[i] = PAGE_EMPTY;
        return 1;
    }

    int PGM_find_content(page_t* page, int offset, uint8_t* data, size_t data_size) {
        if (offset >= PAGE_CONTENT_SIZE) return -2;
        for (int i = offset; i <= PAGE_CONTENT_SIZE - (int)data_size; i++) {
            if (memcmp(&page->content[i], data, data_size) == 0) return i;
        }

        return -1;
    }

    int PGM_find_value(page_t* page, int offset, uint8_t value) {
        int index = offset;
        if (index >= PAGE_CONTENT_SIZE) return -2;
        while (1) {
            if (page->content[index] == value) return index;
            if (index++ >= PAGE_CONTENT_SIZE) return -1;
        }

        return -1;
    }

    int PGM_set_pe_symbol(page_t* page, int offset) {
        int eof = -1;
        for (int i = offset; i < PAGE_CONTENT_SIZE; i++) {
            if (page->content[i] != PAGE_EMPTY) continue;
            int current_eof = i;
            for (int j = current_eof; j < PAGE_CONTENT_SIZE - current_eof; j++) {
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

        if (eof >= 0 && eof < PAGE_CONTENT_SIZE) {
            page->content[eof] = PAGE_END;
            return eof;
        }

        return 0;
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
        int index = 0;
        while (page->content[index] != PAGE_EMPTY) {
            if (++index > PAGE_CONTENT_SIZE) return -1;
        }

        if (offset == -1) return index;

        int is_reading   = 0;
        int free_index   = index;
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

#pragma region [Page file]

    page_t* PGM_create_page(char* name, uint8_t* buffer, size_t data_size) {
        page_t* page = (page_t*)malloc(sizeof(page_t));
        page_header_t* header = (page_header_t*)malloc(sizeof(page_header_t));

        header->magic = PAGE_MAGIC;
        memcpy(header->name, name, PAGE_NAME_SIZE);

        page->header = header;
        if (buffer != NULL)
            memcpy(page->content, buffer, data_size);

        for (int i = data_size + 1; i < PAGE_CONTENT_SIZE; i++) page->content[i] = PAGE_EMPTY;
        return page;
    }

    page_t* PGM_create_empty_page() {
        char page_name[PAGE_NAME_SIZE]    = { '\0' };
        char save_path[DEFAULT_PATH_SIZE] = { '\0' };

        int delay = 1000;
        while (1) {
            rand_str(page_name, PAGE_NAME_SIZE);
            sprintf(save_path, "%s%.8s.%s", PAGE_BASE_PATH, page_name, PAGE_EXTENSION);

            FILE* file;
            if ((file = fopen(save_path, "r")) != NULL) {
                fclose(file);
                if (delay-- <= 0) return NULL;
            }
            else {
                // File not found, no memory leak since 'file' == NULL
                // fclose(file) would cause an error
                break;
            }
        }

        return PGM_create_page(page_name, NULL, 0);
    }

    int PGM_save_page(page_t* page, char* path) {
        int status = 1;
        #pragma omp critical (page_save)
        {
            #ifndef NO_PAGE_SAVE_OPTIMIZATION
            if (PGM_get_checksum(page) != page->header->checksum)
            #endif
            {
                // We generate default path
                char save_path[DEFAULT_PATH_SIZE];
                if (path == NULL) {
                    sprintf(save_path, "%s%.8s.%s", PAGE_BASE_PATH, page->header->name, PAGE_EXTENSION);
                } else strcpy(save_path, path);

                // Open or create file
                FILE* file = fopen(save_path, "wb");
                if (file == NULL) {
                    status = -1;
                    print_error("Can't save or create [%s] file", save_path);
                } else {
                    // Write data to disk
                    int eof = PGM_set_pe_symbol(page, PAGE_START);
                    int page_size = PGM_find_value(page, 0, PAGE_END);
                    if (page_size <= 0) page_size = PAGE_CONTENT_SIZE;

                    page->header->checksum = PGM_get_checksum(page);
                    if (fwrite(page->header, sizeof(page_header_t), 1, file) != 1) status = -2;
                    if (fwrite(page->content, sizeof(uint8_t), page_size, file) != (size_t)page_size) status = -3;

                    // Close file
                    #ifndef _WIN32
                    fsync(fileno(file));
                    #else
                    fflush(file);
                    #endif

                    fclose(file);
                    page->content[eof] = PAGE_EMPTY;
                }
            }
        }

        return status;
    }

    page_t* PGM_load_page(char* path, char* name) {
        char buffer[512];
        char file_name[PAGE_NAME_SIZE];
        char load_path[DEFAULT_PATH_SIZE];

        if (path == NULL && name != NULL) sprintf(load_path, "%s%.8s.%s", PAGE_BASE_PATH, name, PAGE_EXTENSION);
        else strcpy(load_path, path);

        if (path != NULL) {
            char temp_path[DEFAULT_PATH_SIZE];
            strcpy(temp_path, path);
            get_file_path_parts(temp_path, buffer, file_name, buffer);
        }
        else if (name != NULL) {
            strncpy(file_name, name, PAGE_NAME_SIZE);
        }
        else {
            print_error("No path or name provided!");
            return NULL;
        }

        page_t* loaded_page = PGM_PDT_find_page(file_name);
        if (loaded_page != NULL) return loaded_page;

        #pragma omp critical (page_load)
        {
            // Open file page
            FILE* file = fopen(load_path, "rb");
            if (file == NULL) {
                loaded_page = NULL;
                print_error("Page not found! Path: [%s]", load_path);
            } else {
                // Read header from file
                uint8_t* header_data = (uint8_t*)malloc(sizeof(page_header_t));
                fread(header_data, sizeof(page_header_t), 1, file);
                page_header_t* header = (page_header_t*)header_data;

                // Check page magic
                if (header->magic != PAGE_MAGIC) {
                    loaded_page = NULL;

                    free(header);
                    fclose(file);
                } else {
                    // Allocate memory for page structure
                    page_t* page = (page_t*)malloc(sizeof(page_t));
                    memset(page->content, PAGE_EMPTY, PAGE_CONTENT_SIZE);
                    fread(page->content, sizeof(uint8_t), PAGE_CONTENT_SIZE, file);

                    fclose(file);

                    page->header = header;
                    PGM_PDT_add_page(page);
                    loaded_page = page;
                }
            }
        }

        return loaded_page;
    }

    int PGM_free_page(page_t* page) {
        if (page == NULL) return -1;
        SOFT_FREE(page->header);
        SOFT_FREE(page);

        return 1;
    }

    uint32_t PGM_get_checksum(page_t* page) {
        uint32_t checksum = 0;
        for (int i = 0; i < PAGE_NAME_SIZE; i++) checksum += page->header->name[i];
        checksum += strlen((char*)page->header->name);

        for (int i = 0; i < PAGE_CONTENT_SIZE; i++) checksum += page->content[i];
        return checksum;
    }

#pragma endregion