#include "../include/tabman.h"


/*
Table destriptor table, is an a static array of table indexes. Main idea in
saving tables temporary in static table somewhere in memory. Max size of this 
table equals 2072 * 10 = 20.72Kb.

For working with table we have table struct, that have index of table in TDT. If 
we access to tables with full TBM_TDT, we also unload old tables and load new tables.
(Stack)

Main problem in parallel work. If we have threads, they can try to access this
table at one time. If you use OMP parallel libs, or something like this, please,
define NO_TDT flag (For avoiding deadlocks), or use locks to tables.

Why we need TDT? - https://stackoverflow.com/questions/26250744/efficiency-of-fopen-fclose
*/
table_t* TBM_TDT[TDT_SIZE] = { NULL };

#pragma region [Directory files]

    uint8_t* TBM_get_content(table_t* table, int offset, size_t size) {
        int global_page_offset      = offset / PAGE_CONTENT_SIZE;
        int directory_index         = -1;
        int current_page            = 0;

        directory_t* directory = NULL;

        // Find directory that has first page of data (first page).
        for (int i = 0; i < table->header->dir_count; i++) {
            // Load directory from disk to memory
            char directory_save_path[DEFAULT_PATH_SIZE];
            sprintf(directory_save_path, "%s%.8s.%s", DIRECTORY_BASE_PATH, table->dir_names[i], DIRECTORY_EXTENSION);
            directory = DRM_load_directory(directory_save_path);

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
            char directory_save_path[DEFAULT_PATH_SIZE];
            sprintf(directory_save_path, "%s%.8s.%s", DIRECTORY_BASE_PATH, table->dir_names[i], DIRECTORY_EXTENSION);
            directory = DRM_load_directory(directory_save_path);

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
            char directory_save_path[DEFAULT_PATH_SIZE];
            sprintf(directory_save_path, "%s%.8s.%s", DIRECTORY_BASE_PATH, table->dir_names[i], DIRECTORY_EXTENSION);
            directory_t* directory = DRM_load_directory(directory_save_path);
            if (directory == NULL) return -1;
            if (directory->header->page_count + size4append > PAGES_PER_DIRECTORY) continue; 

            int result = DRM_append_content(directory, data_pointer, size4append);
            if (result == -2) return -2;
            else if (result == 0 || result == 1 || result == 2) {
                return 1;
            }

            // Move append data pointer.
            // And create new directories. Why new instead iterating?
            // It will break end structure of data and create defragmentation.
            int loaded_data = size4append - result;
            data_pointer += loaded_data;
            size4append = result;
        }
        
        // Create new directories while we don`t store all provided data.
        while (size4append > 0) {
            // If we reach table limit, return error code.
            if (table->header->dir_count + 1 > DIRECTORIES_PER_TABLE) return -1;

            // Create new empty directory and append data.
            // If we overfill directory by data, save size of data,
            // that we should save.
            directory_t* new_directory = DRM_create_empty_directory();
            if (new_directory == NULL) return -1;

            size4append = DRM_append_content(new_directory, data_pointer, size4append);
            TBM_link_dir2table(table, new_directory);
            
            // Save directory to disk
            char save_path[DEFAULT_PATH_SIZE];

            // TODO:
            // sprintf(save_path, "%s%s.%s", DIRECTORY_BASE_PATH, new_directory->header->name, DIRECTORY_EXTENSION);
            // Directory name don`t have \0 symbol at the end. In summary nothing bad, but in future
            // we can take many problems with it.
            sprintf(save_path, "%s%.8s.%s", DIRECTORY_BASE_PATH, new_directory->header->name, DIRECTORY_EXTENSION);
            int save_result = DRM_save_directory(new_directory, save_path);

            // Realise directory
            DRM_free_directory(new_directory);

            if (save_result != 1) return -1;
        }

        return 1;
    }

    int TBM_insert_content(table_t* table, int offset, uint8_t* data, size_t data_size) {
        if (table->header->dir_count == 0) return -3;

        uint8_t* data_pointer = data;
        int size4insert = (int)data_size;
        
        // Iterate existed directories. Maybe we can insert data here?
        for (int i = 0; i < table->header->dir_count; i++) {
            // Load directory to memory
            char directory_save_path[DEFAULT_PATH_SIZE];
            sprintf(directory_save_path, "%s%.8s.%s", DIRECTORY_BASE_PATH, table->dir_names[i], DIRECTORY_EXTENSION);
            directory_t* directory = DRM_load_directory(directory_save_path);
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
        if (table->header->dir_count == 0) return -3;
        int size4delete = (int)size;
        
        // Iterate existed directories. Maybe we can insert data here?
        for (int i = 0; i < table->header->dir_count; i++) {
            // Load directory to memory
            char directory_save_path[DEFAULT_PATH_SIZE];
            sprintf(directory_save_path, "%s%.8s.%s", DIRECTORY_BASE_PATH, table->dir_names[i], DIRECTORY_EXTENSION);
            directory_t* directory = DRM_load_directory(directory_save_path);
            if (DRM_lock_directory(directory, omp_get_thread_num()) != 1) return -1;

            size4delete = DRM_delete_content(directory, offset, size4delete);
            // If directory, after delete operation, full empty, we delete directory.
            // Also we realise directory pointer in RAM.
            if (directory->header->page_count == 0) {
                TBM_unlink_dir_from_table(table, (char*)directory->header->name);
                DRM_DDT_flush_directory(directory);
                remove(directory_save_path);
                i--;
            }
            // In other hand, if directory not empty after this operation,
            // we just update it on the disk.
            else DRM_save_directory(directory, directory_save_path);

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
            char save_path[DEFAULT_PATH_SIZE];
            sprintf(save_path, "%s%.8s.%s", DIRECTORY_BASE_PATH, table->dir_names[i], DIRECTORY_EXTENSION);
            directory_t* directory = DRM_load_directory(save_path);
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
            char save_path[DEFAULT_PATH_SIZE];
            sprintf(save_path, "%s%.8s.%s", DIRECTORY_BASE_PATH, table->dir_names[i], DIRECTORY_EXTENSION);
            directory_t* directory = DRM_load_directory(save_path);
            if (directory == NULL) return -2;

            global_directory_offset += directory->header->page_count * PAGE_CONTENT_SIZE;
        }

        return global_directory_offset + target_global_index;
    }

    int TBM_find_value(table_t* table, int offset, uint8_t value) {
        if (table->header->dir_count == 0) return -3;
        int final_result = -1;
        
        #pragma omp parallel for shared(final_result)
        for (int i = 0; i < table->header->dir_count; i++) {
            // Load directory to memory
            char directory_save_path[DEFAULT_PATH_SIZE];
            sprintf(directory_save_path, "%s%.8s.%s", DIRECTORY_BASE_PATH, table->dir_names[i], DIRECTORY_EXTENSION);
            directory_t* directory = DRM_load_directory(directory_save_path);
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
            if (memcmp(master->column_links[i]->slave_column_name, slave_column_name, COLUMN_NAME_SIZE) == 0) {
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

#pragma endregion

#pragma region [Table file]

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

    int TBM_link_dir2table(table_t* table, directory_t* directory) {
        if (table->header->dir_count + 1 >= DIRECTORIES_PER_TABLE) 
            return -1;

        memcpy(table->dir_names[table->header->dir_count++], directory->header->name, DIRECTORY_NAME_SIZE);
        return 1;
    }

    int TBM_unlink_dir_from_table(table_t* table, const char* dir_name) {
        for (int i = 0; i < table->header->dir_count; i++) {
            if (strncmp((char*)table->dir_names[i], dir_name, DIRECTORY_NAME_SIZE) == 0) {
                for (int j = i; j < table->header->dir_count - 1; j++) {
                    memcpy(table->dir_names[j], table->dir_names[j + 1], DIRECTORY_NAME_SIZE);
                }

                table->header->dir_count--;
                return 1;
            }
        }

        return 0;
    }

    table_t* TBM_create_table(char* name, table_column_t** columns, int col_count, uint8_t access) {
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
            // Open or create file
            FILE* file = fopen(path, "wb");
            if (file == NULL) {
                status = -1;
            } else {
                // Write header
                if (fwrite(table->header, sizeof(table_header_t), SEEK_CUR, file) != 1) status = -2;

                // Write table data to open file
                for (int i = 0; i < table->header->column_count; i++) 
                    if (fwrite(table->columns[i], sizeof(table_column_t), SEEK_CUR, file) != 1) {
                        status = -3;
                        break;
                    }

                for (int i = 0; i < table->header->column_link_count; i++) 
                    if (fwrite(table->column_links[i], sizeof(table_column_link_t), SEEK_CUR, file) != 1) {
                        status = -4;
                        break;
                    }

                for (int i = 0; i < table->header->dir_count; i++) 
                    if (fwrite(table->dir_names[i], DIRECTORY_NAME_SIZE, SEEK_CUR, file) != 1) {
                        status = -5;
                        break;
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

        return status;
    }

    table_t* TBM_load_table(char* path) {
        char temp_path[DEFAULT_PATH_SIZE];
        strncpy(temp_path, path, DEFAULT_PATH_SIZE);

        char file_path[DEFAULT_PATH_SIZE];
        char file_name[DIRECTORY_NAME_SIZE];
        char file_ext[8];

        get_file_path_parts(temp_path, file_path, file_name, file_ext);

        table_t* loaded_table = TBM_TDT_find_table(file_name);
        if (loaded_table != NULL) return loaded_table;

        #pragma omp critical (table_load)
        {
            FILE* file = fopen(path, "rb");
            if (file == NULL) {
                loaded_table = NULL;
            } else {
                // Read header of table from file.
                // Note: If magic is wrong, we can say, that this file isn`t table.
                //       We just return error code.
                table_header_t* header = (table_header_t*)malloc(sizeof(table_header_t));
                fread(header, sizeof(table_header_t), SEEK_CUR, file);
                if (header->magic != TABLE_MAGIC) {
                    free(header);
                    loaded_table = NULL;
                } else {
                    // Read columns from file.
                    table_t* table = (table_t*)malloc(sizeof(table_t));
                    table->columns = (table_column_t**)malloc(header->column_count * sizeof(table_column_t*));
                    for (int i = 0; i < header->column_count; i++) {
                        table->columns[i] = (table_column_t*)malloc(sizeof(table_column_t));
                        fread(table->columns[i], sizeof(table_column_t), SEEK_CUR, file);
                    }

                    // Read column links from file.
                    table->column_links = (table_column_link_t**)malloc(header->column_link_count * sizeof(table_column_link_t*));
                    for (int i = 0; i < header->column_link_count; i++) {
                        table->column_links[i] = (table_column_link_t*)malloc(sizeof(table_column_link_t));
                        fread(table->column_links[i], sizeof(table_column_link_t), SEEK_CUR, file);
                    }

                    // Read directory names from file, that linked to this directory.
                    for (int i = 0; i < header->dir_count; i++) 
                        fread(table->dir_names[i], DIRECTORY_NAME_SIZE, SEEK_CUR, file);

                    fclose(file);
                    table->header = header;

                    TBM_TDT_add_table(table);
                    loaded_table = table;
                }
            }
        }

        return loaded_table;
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

    #pragma region [TDT]

        int TBM_TDT_add_table(table_t* table) {
            #ifndef NO_TDT
                int current = 0;
                for (int i = 0; i < TDT_SIZE; i++) {
                    if (TBM_TDT[i] == NULL) {
                        current = i;
                        break;
                    }

                    if (TBM_TDT[i]->lock == LOCKED) continue;
                    current = i;
                    break;
                }
                
                if (TBM_lock_table(TBM_TDT[current], omp_get_thread_num()) != -1) {
                    TBM_TDT_flush_index(current);
                    TBM_TDT[current] = table;
                }
            #endif

            return 1;
        }

        table_t* TBM_TDT_find_table(char* name) {
            #ifndef NO_TDT
                if (name == NULL) return NULL;
                for (int i = 0; i < TDT_SIZE; i++) {
                    if (TBM_TDT[i] == NULL) continue;
                    if (strncmp((char*)TBM_TDT[i]->header->name, name, TABLE_NAME_SIZE) == 0) {
                        return TBM_TDT[i];
                    }
                }
            #endif

            return NULL;
        }

        int TBM_TDT_sync() {
            #ifndef NO_TDT
                for (int i = 0; i < TDT_SIZE; i++) {
                    if (TBM_TDT[i] == NULL) continue;
                    char save_path[DEFAULT_PATH_SIZE];
                    sprintf(save_path, "%s%.8s.%s", TABLE_BASE_PATH, TBM_TDT[i]->header->name, TABLE_EXTENSION);

                    // TODO: Get thread ID
                    if (TBM_lock_table(TBM_TDT[i], omp_get_thread_num()) == 1) {
                        TBM_TDT_flush_index(i);
                        TBM_TDT[i] = TBM_load_table(save_path);
                    } else return -1;
                }
            #endif

            return 1;
        }

        int TBM_TDT_flush_table(table_t* table) {
            #ifndef NO_TDT
                if (table == NULL) return -1;

                int index = -1;
                for (int i = 0; i < TDT_SIZE; i++) {
                    if (TBM_TDT[i] == NULL) continue;
                    if (table == TBM_TDT[i]) {
                        index = i;
                        break;
                    }
                }
                
                if (index != -1) TBM_TDT_flush_index(index);
                else TBM_free_table(table);
            #endif

            return 1;
        }

        int TBM_TDT_flush_index(int index) {
            #ifndef NO_TDT
                if (TBM_TDT[index] == NULL) return -1;
                
                char save_path[DEFAULT_PATH_SIZE];
                sprintf(save_path, "%s%.8s.%s", TABLE_BASE_PATH, TBM_TDT[index]->header->name, TABLE_EXTENSION);

                TBM_save_table(TBM_TDT[index], save_path);
                TBM_free_table(TBM_TDT[index]);

                TBM_TDT[index] = NULL;
            #endif

            return 1;
        }

    #pragma endregion

    #pragma region [Lock]

        int TBM_lock_table(table_t* table, uint8_t owner) {
            #ifndef NO_TDT
                if (table == NULL) return -2;

                int delay = 99999;
                while (table->lock == LOCKED && (table->lock_owner != owner || table->lock_owner != -1)) {
                    if (--delay <= 0) return -1;
                }

                table->lock = LOCKED;
                table->lock_owner = owner;
            #endif

            return 1;
        }

        int TBM_lock_test(table_t* table, uint8_t owner) {
            #ifndef NO_TDT
                if (table->lock_owner != owner) return 0;
                return table->lock;
            #endif

            return UNLOCKED;
        }

        int TBM_release_table(table_t* table, uint8_t owner) {
            #ifndef NO_TDT
                if (table->lock == UNLOCKED) return -1;
                if (table->lock_owner != owner && table->lock_owner != -1) return -2;

                table->lock = UNLOCKED;
                table->lock = -1;
            #endif

            return 1;
        }

    #pragma endregion

#pragma endregion
