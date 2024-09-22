#include "../include/pageman.h"

// TODO: Maybe we can`t avoid saving full page to disk? 
//       We can fill EP and EDP symbols when we load page into RAM.

/*
Page destriptor table, is an a static array of pages indexes. Main idea in
saving pages temporary in static table somewhere in memory. Max size of this 
table equals 1024 * 10 = 10Kb.

For working with table we have PAGE struct, that have index of table in PDT. If 
we access to pages with full PGM_PDT, we also unload old page and load new page.
(Stack)

Main problem in parallel work. If we have threads, they can try to access this
table at one time. If you use OMP parallel libs, or something like this, please,
define NO_PDT flag (For avoiding deadlocks), or use locks to pages.

Why we need PDT? - https://stackoverflow.com/questions/26250744/efficiency-of-fopen-fclose
*/
page_t* PGM_PDT[PDT_SIZE] = { NULL };


#pragma region [Content]

    int PGM_append_content(page_t* page, uint8_t* data, size_t data_length) {
        int eof = PGM_find_value(page, PAGE_START, PAGE_END);
        if (eof == -1) return -1;

        int start_index = eof;
        int end_index = MIN(PAGE_CONTENT_SIZE, eof + (int)data_length);

        for (int i = start_index, j = 0; i < end_index && j < (int)data_length; i++, j++) {
            page->content[i] = data[j];
        }

        if (end_index < PAGE_CONTENT_SIZE) {
            page->content[end_index] = PAGE_END;
        }

        return (int)data_length - (end_index - start_index);
    }

    int PGM_insert_content(page_t* page, int offset, uint8_t* data, size_t data_length) {
        int end_index = MIN(PAGE_CONTENT_SIZE - offset, (int)data_length + offset);
        for (int i = offset, j = 0; i < end_index && j < (int)data_length; i++, j++) {
            page->content[i] = data[j];
        }

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
            if (memcmp(&page->content[i], data, data_size) == 0) 
                return i;
        }

        return -1;
    }

    int PGM_find_value(page_t* page, int offset, uint8_t value) {
        if (offset >= PAGE_CONTENT_SIZE) return -2;
        
        int index = offset;
        while (1) {
            if (page->content[index] == value) return index;
            if (index >= PAGE_CONTENT_SIZE) return -1;
            index++;
        }

        return -1;
    }

    int PGM_get_free_space(page_t* page, int offset) {
        // We count empty symbols
        int count = 0;
        for (int i = offset; i < PAGE_CONTENT_SIZE; i++) {
            if (page->content[i] == PAGE_EMPTY) count++;
            else if (offset != -1) break;
        }

        return count;
    }

    int PGM_get_fit_free_space(page_t* page, int offset, int size) {
        int index = 0;
        while (page->content[index] != PAGE_EMPTY) { index++; }
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

#pragma region [Page file]

    page_t* PGM_create_page(char* name, uint8_t* buffer, size_t data_size) {
        page_t* page = (page_t*)malloc(sizeof(page_t));
        page_header_t* header = (page_header_t*)malloc(sizeof(page_header_t));

        header->magic = PAGE_MAGIC;
        memcpy(header->name, name, PAGE_NAME_SIZE);

        page->header = header;
        memcpy(page->content, buffer, data_size);

        if (data_size < PAGE_CONTENT_SIZE) {
            page->content[data_size] = PAGE_END;
            for (int i = data_size + 1; i < PAGE_CONTENT_SIZE; i++) page->content[i] = PAGE_EMPTY;
        }

        return page;
    }

    page_t* PGM_create_empty_page() {
        char page_name[PAGE_NAME_SIZE];
        char save_path[DEFAULT_PATH_SIZE];

        rand_str(page_name, PAGE_NAME_SIZE);
        sprintf(save_path, "%s%.8s.%s", PAGE_BASE_PATH, page_name, PAGE_EXTENSION);

        int delay = 1000;
        while (1) {
            FILE* file;
            if ((file = fopen(save_path,"r")) != NULL) {
                fclose(file);

                rand_str(page_name, PAGE_NAME_SIZE);
                sprintf(save_path, "%s%.8s.%s", PAGE_BASE_PATH, page_name, PAGE_EXTENSION);

                delay--;
                if (delay <= 0) return NULL;
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
            // Open or create file
            FILE* file = fopen(path, "wb");
            if (file == NULL) {
                status = -1;
            } else {
                // Write data to disk
                if (fwrite(page->header, sizeof(page_header_t), SEEK_CUR, file) != 1) status = -2;
                if (fwrite(page->content, PAGE_CONTENT_SIZE, SEEK_CUR, file) != 1) status = -3;

                // Close file
                #ifndef _WIN32
                    fsync(fileno(file));
                #else
                    fflush(file);
                #endif
                
                fclose(file);
            }
        }

        return status;
    }

    page_t* PGM_load_page(char* path) {
        char temp_path[DEFAULT_PATH_SIZE];
        strncpy(temp_path, path, DEFAULT_PATH_SIZE);

        char file_path[DEFAULT_PATH_SIZE];
        char file_name[PAGE_NAME_SIZE];
        char file_ext[8];
        
        get_file_path_parts(temp_path, file_path, file_name, file_ext);

        page_t* loaded_page = PGM_PDT_find_page(file_name);
        if (loaded_page != NULL) return loaded_page;

        #pragma omp critical (page_load)
        {
            // Open file page
            FILE* file = fopen(path, "rb");
            if (file == NULL) {
                printf("Page not found! Path: [%s]\n", path);
                loaded_page = NULL;
            } else {
                // Read header from file
                uint8_t* header_data = (uint8_t*)malloc(sizeof(page_header_t));
                fread(header_data, sizeof(page_header_t), SEEK_CUR, file);
                page_header_t* header = (page_header_t*)header_data;

                // Check page magic
                if (header->magic != PAGE_MAGIC) {
                    free(header);
                    loaded_page = NULL;
                } else {
                    // Allocate memory for page structure
                    page_t* page = (page_t*)malloc(sizeof(page_t));
                    fread(page->content, PAGE_CONTENT_SIZE, SEEK_CUR, file);
                    page->header = header;

                    // Close file page
                    #ifndef _WIN32
                        fsync(fileno(file));
                    #else
                        fflush(file);
                    #endif

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

    #pragma region [PDT]

        int PGM_PDT_add_page(page_t* page) {
            #ifndef NO_PDT
                int current = 0;
                for (int i = 0; i < PDT_SIZE; i++) {
                    if (PGM_PDT[i] == NULL) {
                        current = i;
                        break;
                    }

                    if (PGM_PDT[i]->lock == LOCKED) continue;
                    current = i;
                    break;
                }
                
                // TODO: get thread ID
                if (PGM_lock_page(PGM_PDT[current], omp_get_thread_num()) != -1) {
                    PGM_PDT_flush_index(current);
                    PGM_PDT[current] = page;
                }
            #endif

            return 1;
        }

        page_t* PGM_PDT_find_page(char* name) {
            #ifndef NO_PDT
                for (int i = 0; i < PDT_SIZE; i++) {
                    if (PGM_PDT[i] == NULL) continue;
                    if (strncmp((char*)PGM_PDT[i]->header->name, name, PAGE_NAME_SIZE) == 0) {
                        return PGM_PDT[i];
                    }
                }
            #endif

            return NULL;
        }

        int PGM_PDT_sync() {
            #ifndef NO_PDT
                for (int i = 0; i < PDT_SIZE; i++) {
                    if (PGM_PDT[i] == NULL) continue;
                    char save_path[DEFAULT_PATH_SIZE];
                    sprintf(save_path, "%s%.8s.%s", PAGE_BASE_PATH, PGM_PDT[i]->header->name, PAGE_EXTENSION);

                    if (PGM_lock_page(PGM_PDT[i], omp_get_thread_num()) == 1) {
                        PGM_PDT_flush_index(i);
                        PGM_PDT[i] = PGM_load_page(save_path);
                    } else return -1;
                }
            #endif

            return 1;
        }

        int PGM_PDT_flush_page(page_t* page) {
            #ifndef NO_PDT
                if (page == NULL) return -1;

                int index = -1;
                for (int i = 0; i < PDT_SIZE; i++) {
                    if (PGM_PDT[i] == NULL) continue;
                    if (page == PGM_PDT[i]) {
                        index = i;
                        break;
                    }
                }
                
                if (index != -1) PGM_PDT_flush_index(index);
                else PGM_free_page(page);
            #endif

            return 1;
        }

        int PGM_PDT_flush_index(int index) {
            #ifndef NO_PDT
                if (PGM_PDT[index] == NULL) return -1;

                char save_path[DEFAULT_PATH_SIZE];
                sprintf(save_path, "%s%.8s.%s", PAGE_BASE_PATH, PGM_PDT[index]->header->name, PAGE_EXTENSION);

                PGM_save_page(PGM_PDT[index], save_path);
                PGM_free_page(PGM_PDT[index]);

                PGM_PDT[index] = NULL;
            #endif

            return 1;
        }

    #pragma endregion

    #pragma region [Lock]

        int PGM_lock_page(page_t* page, uint8_t owner) {
            #ifndef NO_PDT
                if (page == NULL) return -2;

                int delay = 99999;
                while (page->lock == LOCKED && (page->lock_owner != owner || page->lock_owner != -1)) {
                    if (--delay <= 0) return -1;
                }

                page->lock = LOCKED;
                page->lock_owner = owner;
            #endif

            return 1;
        }

        int PGM_lock_test(page_t* page, uint8_t owner) {
            #ifndef NO_PDT
                if (page->lock_owner != owner) return 0;
                return page->lock;
            #endif

            return UNLOCKED;
        }

        int PGM_release_page(page_t* page, uint8_t owner) {
            #ifndef NO_PDT
                if (page == NULL) return -3;
                if (page->lock == UNLOCKED) return -1;
                if (page->lock_owner != owner && page->lock_owner != -1) return -2;

                page->lock = UNLOCKED;
                page->lock = -1;
            #endif

            return 1;
        }

    #pragma endregion

#pragma endregion