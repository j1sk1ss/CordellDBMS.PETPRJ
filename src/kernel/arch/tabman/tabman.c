#include "../../include/tabman.h"


#pragma region [Directory files]

    uint8_t* TBM_get_content(table_t* table, int offset, size_t size) {
        int global_page_offset      = offset / PAGE_CONTENT_SIZE;
        int directory_index         = -1;
        int current_page            = 0;

        directory_t* directory = NULL;

        // Find directory that has first page of data (first page).
        for (int i = 0; i < table->header->dir_count; i++) {
            // Load directory from disk to memory
            directory = DRM_load_directory(NULL, (char*)table->dir_names[i]);
            if (directory == NULL) continue;

            // Check:
            // If current directory include page, that we want, we save this directory.
            // Else we unload directory and go to the next dir. name.
            if (current_page + directory->header->page_count < global_page_offset) {
                current_page += directory->header->page_count;
                continue;
            }
            else {
                // Get offset in pages for local directory and save directory index
                directory_index = i;
                break;
            }

            // Page not found. Offset to large.
            return NULL;
        }

        // Data for DRM
        int content2get_size = (int)size;

        // Allocate data for output
        uint8_t* output_content = (uint8_t*)malloc(size);
        uint8_t* output_content_pointer = output_content;

        // Iterate from all directories in table
        for (int i = directory_index; i < table->header->dir_count && content2get_size > 0; i++) {
            // Load directory to memory
            directory = DRM_load_directory(NULL, (char*)table->dir_names[i]);
            if (directory == NULL) continue;

            // Get data from directory
            // After getting data, copy it to allocated output
            int current_size = MIN(directory->header->page_count * PAGE_CONTENT_SIZE, content2get_size);
            uint8_t* directory_content = DRM_get_content(directory, offset, current_size);
            memcpy(output_content_pointer, directory_content, current_size);

            // Realise data
            free(directory_content);

            // Set offset to 0, because we go to next directory
            // Update size of getcontent
            offset = 0;
            content2get_size        -= current_size;
            output_content_pointer  += current_size;
        }

        return output_content;
    }

    int TBM_append_content(table_t* table, uint8_t* data, size_t data_size) {
        uint8_t* data_pointer = data;
        int size4append = (int)data_size;

        // Iterate existed directories. Maybe we can store data here?
        for (int i = 0; i < table->header->dir_count; i++) {
            // Load directory to memory
            directory_t* directory = DRM_load_directory(NULL, (char*)table->dir_names[i]);
            if (directory == NULL) return -1;
            if (directory->header->page_count + size4append > PAGES_PER_DIRECTORY) continue;

            int result = DRM_append_content(directory, data_pointer, size4append);
            if (result < 0) return result - 10;
            else if (result == 0 || result == 1 || result == 2) {
                DRM_save_directory(directory, NULL);
                return 1;
            }
        }

        if (table->header->dir_count + 1 > DIRECTORIES_PER_TABLE) return -1;

        // Create new empty directory and append data.
        // If we overfill directory by data, save size of data,
        // that we should save.
        directory_t* new_directory = DRM_create_empty_directory();
        if (new_directory == NULL) return -1;

        int append_result = DRM_append_content(new_directory, data_pointer, size4append);
        if (append_result < 0) return append_result - 10;

        TBM_link_dir2table(table, new_directory);

        // Save directory to disk
        DRM_save_directory(new_directory, NULL);

        // Realise directory
        DRM_free_directory(new_directory);
        return 1;
    }

    int TBM_insert_content(table_t* table, int offset, uint8_t* data, size_t data_size) {
        if (table->header->dir_count == 0) return -3;

        uint8_t* data_pointer = data;
        int size4insert = (int)data_size;

        // Iterate existed directories. Maybe we can insert data here?
        for (int i = 0; i < table->header->dir_count; i++) {
            // Load directory to memory
            directory_t* directory = DRM_load_directory(NULL, (char*)table->dir_names[i]);
            if (directory == NULL) return -1;

            int result = DRM_insert_content(directory, offset, data_pointer, size4insert);
            if (result == -1) return -1;
            else if (result == 1 || result == 2) {
                return result;
            }

            // Move append data pointer.
            int loaded_data = size4insert - result;
            data_pointer += loaded_data;
            size4insert = result;
        }

        // If we reach end, return error code.
        // Check docs why we return error, instead dir creation.
        return -2;
    }

    int TBM_delete_content(table_t* table, int offset, size_t size) {
        int size4delete = (int)size;

        // Iterate existed directories. Maybe we can insert data here?
        for (int i = 0; i < table->header->dir_count; i++) {
            // Load directory to memory
            directory_t* directory = DRM_load_directory(NULL, (char*)table->dir_names[i]);
            if (DRM_lock_directory(directory, omp_get_thread_num()) != 1) return -1;

            size4delete = DRM_delete_content(directory, offset, size4delete);
            // If directory, after delete operation, full empty, we delete directory.
            // Also we realise directory pointer in RAM.
            if (directory->header->page_count == 0) {
                DRM_delete_directory(directory, 1);
                i--;
            }
            // In other hand, if directory not empty after this operation,
            // we just update it on the disk.
            else DRM_save_directory(directory, NULL);

            DRM_release_directory(directory, omp_get_thread_num());
            if (size4delete == -1) return -1;
            else if (size4delete == 1 || size4delete == 2) {
                return size4delete;
            }
        }

        // If we reach end, return error code.
        return -2;
    }

    int TBM_find_content(table_t* table, int offset, uint8_t* data, size_t data_size) {
        int directory_offset    = 0;
        int size4seach          = (int)data_size;
        int target_global_index = -1;

        uint8_t* data_pointer = data;
        for (int i = 0; i < table->header->dir_count && size4seach > 0; i++) {
            // We load current page to memory
            directory_t* directory = DRM_load_directory(NULL, (char*)table->dir_names[i]);
            if (directory == NULL) return -2;

            directory_offset = i;

            // We search part of data in this page, save index and unload page.
            int result = DRM_find_content(directory, offset, data_pointer, size4seach);

            // If TGI is -1, we know that we start seacrhing from start.
            // Save current TGI of find part of data.
            if (target_global_index == -1) {
                target_global_index = result;
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
                size4seach      -= directory->header->page_count * PAGE_CONTENT_SIZE - result;
                data_pointer    += directory->header->page_count * PAGE_CONTENT_SIZE - result;
            }
        }

        if (target_global_index == -1) return -1;

        int global_directory_offset = 0;
        for (int i = 0; i < MAX(directory_offset - 1, 0); i++) {
            directory_t* directory = DRM_load_directory(NULL, (char*)table->dir_names[i]);
            if (directory == NULL) return -2;

            global_directory_offset += directory->header->page_count * PAGE_CONTENT_SIZE;
        }

        return global_directory_offset + target_global_index;
    }

    int TBM_find_value(table_t* table, int offset, uint8_t value) {
        int final_result = -1;

        #pragma omp parallel for shared(final_result)
        for (int i = 0; i < table->header->dir_count; i++) {
            // Load directory to memory
            directory_t* directory = DRM_load_directory(NULL, (char*)table->dir_names[i]);
            if (DRM_lock_directory(directory, omp_get_thread_num()) != 1) {
                #pragma omp critical (final_result2error)
                {
                    final_result = -1;
                }

                continue;
            }

            int result = DRM_find_value(directory, offset, value);
            DRM_release_directory(directory, omp_get_thread_num());
            if (result != -1) {
                #pragma omp critical (final_result2result)
                {
                    if (final_result == -1)
                        final_result = result;
                }
            }

            offset = 0;
        }

        return final_result;
    }

    int TBM_link_dir2table(table_t* table, directory_t* directory) {
        if (table->header->dir_count + 1 >= DIRECTORIES_PER_TABLE)
            return -1;

        #pragma omp critical (link_dir2table)
        memcpy(table->dir_names[table->header->dir_count++], directory->header->name, DIRECTORY_NAME_SIZE);
        return 1;
    }

    int TBM_unlink_dir_from_table(table_t* table, const char* dir_name) {
        int status = 0;
        #pragma omp critical (unlink_dir_from_table)
        {
            for (int i = 0; i < table->header->dir_count; i++) {
                if (strncmp((char*)table->dir_names[i], dir_name, DIRECTORY_NAME_SIZE) == 0) {
                    for (int j = i; j < table->header->dir_count - 1; j++) {
                        memcpy(table->dir_names[j], table->dir_names[j + 1], DIRECTORY_NAME_SIZE);
                    }

                    table->header->dir_count--;
                    status = 1;
                    break;
                }
            }
        }

        return status;
    }

#pragma endregion

#pragma region [Column]

    int TBM_link_column2column(table_t* master, char* master_column_name, table_t* slave, char* slave_column_name, uint8_t type) {
        master->column_links = (table_column_link_t**)realloc(master->column_links, (master->header->column_link_count + 1) * sizeof(table_column_link_t*));
        master->column_links[master->header->column_link_count] = (table_column_link_t*)malloc(sizeof(table_column_link_t));
        memcpy(
            master->column_links[master->header->column_link_count]->master_column_name,
            master_column_name, COLUMN_NAME_SIZE
        );

        memcpy(
            master->column_links[master->header->column_link_count]->slave_table_name,
            slave->header->name, TABLE_NAME_SIZE
        );

        memcpy(
            master->column_links[master->header->column_link_count]->slave_column_name,
            slave_column_name, COLUMN_NAME_SIZE
        );

        master->column_links[master->header->column_link_count]->type = type;
        master->header->column_link_count++;
        return 1;
    }

    int TBM_unlink_column_from_column(table_t* master, char* master_column_name, table_t* slave, char* slave_column_name) {
        for (int i = 0; i < master->header->column_link_count; i++) {
            if (
                memcmp(master->column_links[i]->master_column_name, master_column_name, COLUMN_NAME_SIZE) == 0 &&
                memcmp(master->column_links[i]->slave_column_name, slave_column_name, COLUMN_NAME_SIZE) == 0 &&
                memcmp(master->column_links[i]->slave_table_name, slave->header->name, TABLE_NAME_SIZE) == 0
            ) {
                free(master->column_links[i]);

                for (int j = i; j < master->header->column_link_count - 1; j++) {
                    master->column_links[j] = master->column_links[j + 1];
                }

                table_column_link_t** new_links = (table_column_link_t**)realloc(
                    master->column_links, (master->header->column_link_count - 1) * sizeof(table_column_link_t*)
                );

                if (new_links || master->header->column_link_count - 1 == 0) {
                    master->column_links = new_links;
                } else return -1;

                master->header->column_link_count--;
                return 1;
            }
        }

        return 0;
    }

    int TBM_update_column_in_table(table_t* table, table_column_t* column, int by_index) {
        if (by_index != -1) {
            if (by_index > table->header->column_count) return -1;
            if (table->columns[by_index]->size != column->size) return -2;

            SOFT_FREE(table->columns[by_index]);
            table->columns[by_index] = column;

            return 1;
        }

        for (int i = 0; i < table->header->column_count; i++) {
            if (memcmp(table->columns[i]->name, column->name, COLUMN_NAME_SIZE) == 0) {
                if (table->columns[i]->size != column->size) return -2;

                SOFT_FREE(table->columns[i]);
                table->columns[i] = column;
                return 1;
            }
        }

        return -1;
    }

    table_column_t* TBM_create_column(uint8_t type, uint8_t size, char* name) {
        table_column_t* column = (table_column_t*)malloc(sizeof(table_column_t));

        column->magic = COLUMN_MAGIC;
        strncpy((char*)column->name, name, COLUMN_NAME_SIZE);
        column->type = type;
        column->size = size;

        return column;
    }

    int TBM_check_signature(table_t* table, uint8_t* data) {
        uint8_t* data_pointer = data;
        for (int i = 0; i < table->header->column_count; i++) {
            char value[COLUMN_MAX_SIZE] = { '\0' };
            memcpy(value, data_pointer, table->columns[i]->size);
            data_pointer += table->columns[i]->size;

            uint8_t data_type = GET_COLUMN_DATA_TYPE(table->columns[i]->type);
            switch (data_type) {
                case COLUMN_TYPE_STRING:
                    break;
                case COLUMN_TYPE_INT:
                    if (!is_integer(value)) return -2;
                    break;
                case COLUMN_TYPE_FLOAT:
                    if (!is_float(value)) return -3;
                    break;
                case COLUMN_TYPE_ANY:
                    break;
                default:
                    return -4;
            }
        }

        return 1;
    }

#pragma endregion

#pragma region [Table file]

    table_t* TBM_create_table(char* name, table_column_t** columns, int col_count, uint8_t access) {
        int row_size = 0;
        for (int i = 0; i < col_count; i++) {
            row_size += columns[i]->size;
        }

        if (row_size >= PAGE_CONTENT_SIZE) return NULL;
        table_header_t* header = (table_header_t*)malloc(sizeof(table_header_t));

        header->access  = access;
        header->magic   = TABLE_MAGIC;
        strncpy((char*)header->name, name, TABLE_NAME_SIZE);

        header->dir_count           = 0;
        header->column_count        = col_count;
        header->column_link_count   = 0;

        table_t* table = (table_t*)malloc(sizeof(table_t));

        table->header  = header;
        table->columns = columns;

        table->column_links = NULL;

        return table;
    }

    int TBM_save_table(table_t* table, char* path) {
        int status = 1;
        #pragma omp critical (table_save)
        {
            #ifndef NO_PAGE_SAVE_OPTIMIZATION
            if (TBM_get_checksum(table) != table->header->checksum)
            #endif
            {
                // We generate default path
                char save_path[DEFAULT_PATH_SIZE];
                if (path == NULL) {
                    sprintf(save_path, "%s%.8s.%s", TABLE_BASE_PATH, table->header->name, TABLE_EXTENSION);
                }
                else strcpy(save_path, path);

                // Open or create file
                FILE* file = fopen(save_path, "wb");
                if (file == NULL) {
                    status = -1;
                    print_error("Can't save or create table [%s] file", save_path);
                } else {
                    // Write header
                    table->header->checksum = TBM_get_checksum(table);
                    if (fwrite(table->header, sizeof(table_header_t), 1, file) != 1) status = -2;

                    // Write table data to open file
                    for (int i = 0; i < table->header->column_count; i++)
                        if (fwrite(table->columns[i], sizeof(table_column_t), 1, file) != 1) {
                            status = -3;
                        }

                    for (int i = 0; i < table->header->column_link_count; i++)
                        if (fwrite(table->column_links[i], sizeof(table_column_link_t), 1, file) != 1) {
                            status = -4;
                        }

                    for (int i = 0; i < table->header->dir_count; i++)
                        if (fwrite(table->dir_names[i], sizeof(uint8_t), DIRECTORY_NAME_SIZE, file) != DIRECTORY_NAME_SIZE) {
                            status = -5;
                        }

                    // Close file and clear buffers
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

    table_t* TBM_load_table(char* path, char* name) {
        char buffer[512];
        char file_name[TABLE_NAME_SIZE];
        char load_path[DEFAULT_PATH_SIZE];

        if (path == NULL && name != NULL) sprintf(load_path, "%s%.8s.%s", TABLE_BASE_PATH, name, TABLE_EXTENSION);
        else strcpy(load_path, path);

        if (path != NULL) {
            char temp_path[DEFAULT_PATH_SIZE];
            strcpy(temp_path, path);
            get_file_path_parts(temp_path, buffer, file_name, buffer);
        }
        else if (name != NULL) {
            strncpy(file_name, name, TABLE_NAME_SIZE);
        }
        else {
            print_error("No path or name provided!");
            return NULL;
        }

        table_t* loaded_table = TBM_TDT_find_table(file_name);
        if (loaded_table != NULL) return loaded_table;

        #pragma omp critical (table_load)
        {
            FILE* file = fopen(load_path, "rb");
            if (file == NULL) {
                loaded_table = NULL;
                print_error("Can't open table [%s]", load_path);
            } else {
                // Read header of table from file.
                // Note: If magic is wrong, we can say, that this file isn`t table.
                //       We just return error code.
                table_header_t* header = (table_header_t*)malloc(sizeof(table_header_t));
                fread(header, sizeof(table_header_t), 1, file);
                if (header->magic != TABLE_MAGIC) {
                    loaded_table = NULL;

                    free(header);
                    fclose(file);
                } else {
                    // Read columns from file.
                    table_t* table = (table_t*)malloc(sizeof(table_t));
                    table->columns = (table_column_t**)malloc(header->column_count * sizeof(table_column_t*));
                    for (int i = 0; i < header->column_count; i++) {
                        table->columns[i] = (table_column_t*)malloc(sizeof(table_column_t));
                        fread(table->columns[i], sizeof(table_column_t), 1, file);
                    }

                    // Read column links from file.
                    table->column_links = (table_column_link_t**)malloc(header->column_link_count * sizeof(table_column_link_t*));
                    for (int i = 0; i < header->column_link_count; i++) {
                        table->column_links[i] = (table_column_link_t*)malloc(sizeof(table_column_link_t));
                        fread(table->column_links[i], sizeof(table_column_link_t), 1, file);
                    }

                    // Read directory names from file, that linked to this directory.
                    for (int i = 0; i < header->dir_count; i++)
                        fread(table->dir_names[i], sizeof(uint8_t), DIRECTORY_NAME_SIZE, file);

                    fclose(file);

                    table->header = header;
                    TBM_TDT_add_table(table);
                    loaded_table = table;
                }
            }
        }

        return loaded_table;
    }

    int TBM_delete_table(table_t* table, int full) {
        if (TBM_lock_table(table, omp_get_thread_num()) == 1) {
            #pragma omp parallel
            for (int i = 0; i < table->header->dir_count; i++) {
                directory_t* directory = DRM_load_directory(NULL, (char*)table->dir_names[i]);
                if (directory == NULL) continue;

                TBM_unlink_dir_from_table(table, (char*)table->dir_names[i]);
                DRM_delete_directory(directory, full);
            }

            char delete_path[DEFAULT_PATH_SIZE];
            sprintf(delete_path, "%s%.8s.%s", TABLE_BASE_PATH, table->header->name, TABLE_EXTENSION);
            remove(delete_path);
            TBM_TDT_flush_table(table);
        }

        return 1;
    }

    int TBM_free_table(table_t* table) {
        if (table == NULL) return -1;
        for (int i = 0; i < table->header->column_count; i++)
            SOFT_FREE(table->columns[i]);

        for (int i = 0; i < table->header->column_link_count; i++)
            SOFT_FREE(table->column_links[i]);

        SOFT_FREE(table->column_links);
        SOFT_FREE(table->columns);
        SOFT_FREE(table);

        return 1;
    }

    uint32_t TBM_get_checksum(table_t* table) {
        uint32_t checksum = 0;
        for (int i = 0; i < TABLE_NAME_SIZE; i++) checksum += table->header->name[i];
        checksum += strlen((char*)table->header->name);

        for (int i = 0; i < table->header->dir_count; i++)
            for (int j = 0; j < DIRECTORY_NAME_SIZE; j++) {
                checksum += table->dir_names[i][j];
                checksum += strlen((char*)table->dir_names[i]);
            }

        for (int i = 0; i < table->header->column_count; i++) {
            for (int j = 0; j < COLUMN_NAME_SIZE; j++)
                checksum += table->columns[i]->name[j];

            checksum += strlen((char*)table->columns[i]->name);
        }

        for (int i = 0; i < table->header->column_link_count; i++) {
            for (int j = 0; j < COLUMN_NAME_SIZE; j++) {
                checksum += table->column_links[i]->master_column_name[j];
                checksum += table->column_links[i]->slave_column_name[j];
            }

            for (int j = 0; j < TABLE_NAME_SIZE; j++)
                checksum += table->column_links[i]->slave_table_name[j];

            checksum += strlen((char*)table->column_links[i]->master_column_name);
            checksum += strlen((char*)table->column_links[i]->slave_column_name);
            checksum += strlen((char*)table->column_links[i]->slave_table_name);
        }

        return checksum;
    }

#pragma endregion
