#include "../../include/dirman.h"


#pragma region [Page files]

    uint8_t* DRM_get_content(directory_t* directory, int offset, size_t size) {
        uint8_t* content = (uint8_t*)malloc(size);
        uint8_t* content_pointer = content;
        memset(content, '\0', size);

        int pages4work    = (int)size / PAGE_CONTENT_SIZE;
        int page_offset   = offset / PAGE_CONTENT_SIZE;
        int current_index = offset % PAGE_CONTENT_SIZE;
        int size2get      = (int)size;

        for (int i = page_offset; i < page_offset + pages4work + 1 && size2get > 0; i++) {
            if (i > directory->header->page_count) {
                // To  many pages. We reach directory end.
                return content;
            }

            // We load current page
            page_t* page = PGM_load_page(NULL, (char*)directory->names[i]);
            if (page == NULL) continue;

            // We check, that we don't return Page Empty, because
            // PE symbols != Content symbols.
            uint8_t* page_content_pointer = page->content;
            while (page->content[current_index] == PAGE_EMPTY) {
                if (++current_index >= PAGE_CONTENT_SIZE) {
                    page_content_pointer = NULL;
                    break;
                }
            }

            // Also we don't check full content body. I mean, that
            // if situation CS ... CS, CS, PE, PE occur, we don't care,
            // because this is not our fault. This is problem of higher
            // abstraction levels.
            if (page_content_pointer != NULL) {
                page_content_pointer += current_index;

                // We work with page
                int current_size = MIN(PAGE_CONTENT_SIZE - current_index, size2get);
                memcpy(content_pointer, page_content_pointer, current_size);

                // We reload local index and update size2get
                // Also we move content pointer to next location
                current_index   = 0;
                size2get        -= current_size;
                content_pointer += current_size;
            }
        }

        return content;
    }

    int DRM_append_content(directory_t* directory, uint8_t* data, size_t data_lenght) {
        // First we try to find fit empty place somewhere in linked pages
        // We skip this part if data_lenght larger then PAGE_CONTENT_SIZE
        if (data_lenght < PAGE_CONTENT_SIZE) {
            for (int i = 0; i < directory->header->page_count; i++) {
                page_t* page = PGM_load_page(NULL, (char*)directory->names[i]);
                if (page == NULL) return -1;

                int index = PGM_get_fit_free_space(page, PAGE_START, data_lenght);
                if (index < 0) continue;

                PGM_insert_content(page, index, data, data_lenght);
                return 1;
            }

            if (directory->header->page_count + 1 >= PAGES_PER_DIRECTORY) return (int)data_lenght;

            // We allocate memory for page structure with all needed data
            page_t* new_page = PGM_create_empty_page();
            if (new_page == NULL) return -2;

            // Insert new content to page and mark end
            PGM_insert_content(new_page, 0, data, data_lenght);

            // We link page to directory
            DRM_link_page2dir(directory, new_page);
            PGM_PDT_add_page(new_page);

            return 2;
        }
        else return -3;
    }

    int DRM_insert_content(directory_t* directory, uint8_t offset, uint8_t* data, size_t data_lenght) {
        int pages4work      = data_lenght / PAGE_CONTENT_SIZE;
        int page_offset     = offset / PAGE_CONTENT_SIZE;
        int current_index   = offset % PAGE_CONTENT_SIZE;
        int current_page    = page_offset;
        int size2insert     = data_lenght;

        uint8_t* data_pointer = data;
        for (int i = 0; i < pages4work + 1 && size2insert > 0; i++) {
            // If we reach pages count in current directory, we return error code
            // We return error instead creationg a new directory, because this is not our abstraction level
            if (current_page > directory->header->page_count) {
                // To  many pages. We reach directory end.
                // Return size, that we can't insert.
                return size2insert;
            }

            // We load current page to memory
            page_t* page = PGM_load_page(NULL, (char*)directory->names[current_page++]);
            if (page == NULL) return -1;

            // We insert current part of content with local offset
            int current_size = MIN(PAGE_CONTENT_SIZE - current_index, size2insert);
            PGM_insert_content(page, current_index, data_pointer, current_size);

            // We reload local index and update size2delete
            current_index = 0;
            size2insert -= current_size;
            data_pointer += current_size;
        }

        if (size2insert > 0) return 2;
        else return 1;
    }

    int DRM_delete_content(directory_t* directory, int offset, size_t length) {
        int page_offset     = offset / PAGE_CONTENT_SIZE;
        int current_index   = offset % PAGE_CONTENT_SIZE;
        int pages4work      = (int)length / PAGE_CONTENT_SIZE;
        int current_page    = page_offset;
        int size2delete     = (int)length;

        for (int i = 0; i < pages4work + 1 && size2delete > 0; i++) {
            if (current_page > directory->header->page_count) {
                // Too  many pages. We reach directory end.
                return size2delete;
            }

            // We load current page
            page_t* page = PGM_load_page(NULL, (char*)directory->names[current_page++]);
            if (PGM_lock_page(page, omp_get_thread_num()) != 1) return -1;

            // We check, that we don't return Page Empty, because
            // PE symbols != Content symbols.
            while (page->content[current_index] == PAGE_EMPTY) {
                if (++current_index >= PAGE_CONTENT_SIZE) {
                    return -1;
                }
            }

            // We work with page
            int current_size = MIN(PAGE_CONTENT_SIZE - current_index, size2delete);
            PGM_delete_content(page, current_index, current_size);

            // We reload local index and update size2delete
            current_index = 0;
            size2delete -= current_size;

            PGM_release_page(page, omp_get_thread_num());
        }

        return 1;
    }

    int DRM_cleanup_pages(directory_t* directory) {
        for (int i = 0; i < directory->header->page_count; i++) {
            char page_path[DEFAULT_PATH_SIZE];
            sprintf(page_path, "%s%.8s.%s", PAGE_BASE_PATH, directory->names[i], PAGE_EXTENSION);
            page_t* page = PGM_load_page(NULL, directory->names[i]);

            // If page, after delete operation, full empty, we delete page.
            // Also we realise page pointer in RAM.
            if (PGM_get_free_space(page, PAGE_START) == PAGE_CONTENT_SIZE) {
                DRM_unlink_page_from_directory(directory, (char*)page->header->name);
                PGM_PDT_flush_page(page);
                print_log("Page [%s] was deleted with result [%i]", page_path, remove(page_path));
            }
        }

        return 1;
    }

    int DRM_find_content(directory_t* directory, int offset, uint8_t* data, size_t data_size) {
        int page_offset         = offset / PAGE_CONTENT_SIZE;
        int current_index       = offset % PAGE_CONTENT_SIZE;
        int size4seach          = (int)data_size;
        int pages4search        = directory->header->page_count - page_offset;
        int current_page        = page_offset;
        int target_global_index = -1;

        uint8_t* data_pointer = data;
        for (int i = 0; i < pages4search + 1 && size4seach > 0; i++) {
            // If we reach pages count in current directory, we return error code.
            // We return error instead creationg a new directory, because this is not our abstraction level.
            if (current_page >= directory->header->page_count) {
                // To  many pages. We reach directory end.
                // That mean, we don't find any entry of target data.
                return -1;
            }

            // We load current page to memory
            page_t* page = PGM_load_page(NULL, (char*)directory->names[current_page]);
            if (page == NULL) return -2;

            // We search part of data in this page, save index and unload page.
            int current_size = MIN(PAGE_CONTENT_SIZE - current_index, size4seach);
            int result = PGM_find_content(page, current_index, data_pointer, current_size);

            // If TGI is -1, we know that we start seacrhing from start.
            // Save current TGI of find part of data.
            if (target_global_index == -1) {
                target_global_index = result + current_page * PAGE_CONTENT_SIZE;
            }

            if (result == -1) {
                // We don`t find any entry of data part.
                // This indicates, that we don`t find any data.
                // Restore size4search and datapointer, we go to start
                size4seach          = (int)data_size;
                data_pointer        = data;
                target_global_index = -1;
            } else {
                // Move pointer to next position
                size4seach      -= current_size;
                data_pointer    += current_size;
            }

            // Set local index to 0. We don`t need offset now.
            current_index = 0;
            current_page++;
        }

        return target_global_index;
    }

    int DRM_find_value(directory_t* directory, int offset, uint8_t value) {
        int page_offset   = offset / PAGE_CONTENT_SIZE;
        int current_index = offset % PAGE_CONTENT_SIZE;
        int result = -1; // Shared result across threads
        int break_flag = 0;

        #pragma omp parallel for shared(result)
        for (int i = page_offset; i < directory->header->page_count; i++) {
            if (result != -1) continue; // Skip search if result is already found
            if (break_flag == -1) continue;

            // Load current page
            page_t* page = PGM_load_page(NULL, (char*)directory->names[i]);
            if (page == NULL) {
                break_flag = -1;
                continue;
            }

            // Find the value in the page content
            int local_result = PGM_find_value(page, i == page_offset ? current_index : 0, value);
            if (local_result != -1) {
                #pragma omp critical (local_result2global)  // Ensure only one thread updates the result
                {
                    if (result == -1) result = local_result + i * PAGE_CONTENT_SIZE;
                }
            }
        }

        return result;
    }

    int DRM_link_page2dir(directory_t* directory, page_t* page) {
        if (directory->header->page_count + 1 >= PAGES_PER_DIRECTORY)
            return -1;

        #pragma omp critical (link_page2dir)
        memcpy(directory->names[directory->header->page_count++], page->header->name, PAGE_NAME_SIZE);
        return 1;
    }

    int DRM_unlink_page_from_directory(directory_t* directory, char* page_name) {
        int status = 0;
        #pragma omp critical (unlink_page_from_directory)
        {
            for (int i = 0; i < directory->header->page_count; i++) {
                if (memcmp(directory->names[i], page_name, PAGE_NAME_SIZE) == 0) {
                    for (int j = i; j < directory->header->page_count - 1; j++) {
                        memcpy(directory->names[j], directory->names[j + 1], PAGE_NAME_SIZE);
                    }

                    directory->header->page_count--;
                    status = 1;
                    break;
                }
            }
        }

        return status;
    }

#pragma endregion

#pragma region [Directory file]

    directory_t* DRM_create_directory(char* name) {
        directory_t* dir = (directory_t*)malloc(sizeof(directory_t));

        dir->header = (directory_header_t*)malloc(sizeof(directory_header_t));
        strncpy((char*)dir->header->name, name, DIRECTORY_NAME_SIZE);
        dir->header->magic = DIRECTORY_MAGIC;
        dir->header->page_count = 0;

        return dir;
    }

    directory_t* DRM_create_empty_directory() {
        char directory_name[DIRECTORY_NAME_SIZE];
        char save_path[DEFAULT_PATH_SIZE];

        int delay = 1000;
        while (1) {
            rand_str(directory_name, DIRECTORY_NAME_SIZE);
            sprintf(save_path, "%s%.8s.%s", DIRECTORY_BASE_PATH, directory_name, DIRECTORY_EXTENSION);

            FILE* file;
            if ((file = fopen(save_path,"r")) != NULL) {
                fclose(file);
                if (--delay <= 0) return NULL;
            }
            else {
                // File not found, no memory leak since 'file' == NULL
                // fclose(file) would cause an error
                break;
            }
        }

        return DRM_create_directory(directory_name);
    }

    int DRM_save_directory(directory_t* directory, char* path) {
        int status = 1;
        #pragma omp critical (directory_save)
        {
            #ifndef NO_DIRECTORY_SAVE_OPTIMIZATION
            if (DRM_get_checksum(directory) != directory->header->checksum)
            #endif
            {
                char save_path[DEFAULT_PATH_SIZE];
                if (path == NULL) {
                    sprintf(save_path, "%s%.8s.%s", DIRECTORY_BASE_PATH, directory->header->name, DIRECTORY_EXTENSION);
                }
                else strcpy(save_path, path);

                FILE* file = fopen(save_path, "wb");
                if (file == NULL) {
                    status = -1;
                    print_error("Can`t create file: [%s]", save_path);
                } else {
                    directory->header->checksum = DRM_get_checksum(directory);
                    if (fwrite(directory->header, sizeof(directory_header_t), 1, file) != 1)
                        status = -1;

                    for (int i = 0; i < directory->header->page_count; i++) {
                        if (fwrite(directory->names[i], sizeof(uint8_t), PAGE_NAME_SIZE, file) != 1) {
                            status = -1;
                        }
                    }

                    #ifndef _WIN32
                    fsync(fileno(file));
                    #else
                    fflush(file);
                    #endif

                    fclose(file);
                }
            }
        }

        return status;
    }

    directory_t* DRM_load_directory(char* path, char* name) {
        char buffer[512];
        char file_name[DIRECTORY_NAME_SIZE];
        char load_path[DEFAULT_PATH_SIZE];

        if (path == NULL && name != NULL) sprintf(load_path, "%s%.8s.%s", DIRECTORY_BASE_PATH, name, DIRECTORY_EXTENSION);
        else strcpy(load_path, path);

        if (path != NULL) {
            char temp_path[DEFAULT_PATH_SIZE];
            strcpy(temp_path, path);
            get_file_path_parts(temp_path, buffer, file_name, buffer);
        }
        else if (name != NULL) {
            strncpy(file_name, name, DIRECTORY_NAME_SIZE);
        }
        else {
            print_error("No path or name provided!");
            return NULL;
        }

        directory_t* loaded_directory = DRM_DDT_find_directory(file_name);
        if (loaded_directory != NULL) return loaded_directory;

        #pragma omp critical (directory_load)
        {
            // Open file directory
            FILE* file = fopen(load_path, "rb");
            if (file == NULL) {
                loaded_directory = NULL;
                print_error("Directory not found! Path: [%s]\n", load_path);
            } else {
                // Read header from file
                directory_header_t* header = (directory_header_t*)malloc(sizeof(directory_header_t));
                fread(header, sizeof(directory_header_t), 1, file);

                // Check directory magic
                if (header->magic != DIRECTORY_MAGIC) {
                    loaded_directory = NULL;

                    free(header);
                    fclose(file);
                } else {
                    // First we allocate memory for directory struct
                    // Then we read page names
                    directory_t* directory = (directory_t*)malloc(sizeof(directory_t));
                    for (int i = 0; i < MIN(header->page_count, PAGES_PER_DIRECTORY); i++)
                        fread(directory->names[i], sizeof(uint8_t), PAGE_NAME_SIZE, file);

                    // Close file directory
                    fclose(file);

                    directory->header = header;
                    DRM_DDT_add_directory(directory);
                    loaded_directory = directory;
                }
            }
        }

        return loaded_directory;
    }

    int DRM_delete_directory(directory_t* directory, int full) {
        if (DRM_lock_directory(directory, omp_get_thread_num()) == 1) {
            #pragma omp parallel
            for (int i = 0; i < directory->header->page_count && full == 1; i++) {
                char page_path[DEFAULT_PATH_SIZE];
                sprintf(page_path, "%s%.8s.%s", PAGE_BASE_PATH, directory->names[i], PAGE_EXTENSION);
                page_t* page = PGM_load_page(page_path, NULL);
                if (PGM_lock_page(page, omp_get_thread_num()) == 1) {
                    PGM_PDT_flush_page(page);
                    print_log("Page [%s] was deleted with result [%i]", page_path, remove(page_path));
                }
                else {
                    print_error("Can't lock page [%s]", page_path);
                }
            }

            char delete_path[DEFAULT_PATH_SIZE];
            sprintf(delete_path, "%s%.8s.%s", DIRECTORY_BASE_PATH, directory->header->name, DIRECTORY_EXTENSION);
            DRM_DDT_flush_directory(directory);
            print_log("Directory [%s] was deleted with result [%i]", delete_path, remove(delete_path));
        }
        else {
            print_error("Can't lock directory [%s]", directory->header->name);
        }

        return 1;
    }

    int DRM_free_directory(directory_t* directory) {
        if (directory == NULL) return -1;
        SOFT_FREE(directory->header);
        SOFT_FREE(directory);

        return 1;
    }

    uint32_t DRM_get_checksum(directory_t* directory) {
        uint32_t checksum = 0;
        for (int i = 0; i < DIRECTORY_NAME_SIZE; i++) checksum += directory->header->name[i];
        checksum += strlen((char*)directory->header->name);

        for (int i = 0; i < directory->header->page_count; i++) {
            for (int j = 0; j < PAGE_NAME_SIZE; j++) {
                checksum += directory->names[i][j];
            }

            checksum += strlen((char*)directory->names[i]);
        }

        return checksum;
    }

#pragma endregion