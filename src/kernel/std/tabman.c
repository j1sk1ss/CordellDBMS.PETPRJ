#include "../include/tabman.h"


#pragma region [Directory]

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

            break;
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
        return 1; // TODO
    }

    int TBM_delete_content(table_t* table, int offset, size_t size) {
        return 1; // TODO
    }

    int TBM_find_content(table_t* table, int offset, uint8_t* data, size_t data_size) {
        return 1; // TODO
    }

    int TBM_find_value(table_t* table, int offset, uint8_t value) {
        return 1; // TODO
    }

#pragma endregion

#pragma region [Column]

    int TBM_link_column2column(table_t* master, char* master_column_name, table_column_t* slave_column) {
        return 1; // TODO
    }

    int TBM_unlink_column2column(table_t* master, char* master_column_name, table_column_t* slave_column) {
        return 1; // TODO
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
            if (strcmp(table->columns[i]->name, column->name) == 0) {
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

#pragma region [Table]

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

    // TODO: TBM_unlink_dir2table

    table_t* TBM_create_table(char* name, table_column_t** columns, int col_count, uint8_t access) {
        table_header_t* header = (table_header_t*)malloc(sizeof(table_header_t));

        header->access  = access;
        header->magic   = TABLE_MAGIC;
        strncpy((char*)header->name, name, TABLE_NAME_SIZE);

        header->dir_count           = 0;
        header->column_link_count   = 0;
        header->column_count        = col_count;

        table_t* table = (table_t*)malloc(sizeof(table_t));

        table->header  = header;
        table->columns = columns;

        table->columns      = NULL;
        table->column_links = NULL;

        return table;
    }

    int TBM_save_table(table_t* table, char* path) {
        // Open or create file
        FILE* file = fopen(path, "wb");
        if (file == NULL) {
            return -1;
        }

        // Write header
        fwrite(table->header, sizeof(table_header_t), SEEK_CUR, file);

        // Write table data to open file
        for (int i = 0; i < table->header->column_count; i++) fwrite(table->columns[i], sizeof(table_column_t), SEEK_CUR, file);
        for (int i = 0; i < table->header->column_link_count; i++) fwrite(table->column_links[i], sizeof(table_column_link_t), SEEK_CUR, file);
        for (int i = 0; i < table->header->dir_count; i++) fwrite(table->dir_names[i], DIRECTORY_NAME_SIZE, SEEK_CUR, file);

        // Close file and clear buffers
        #ifndef _WIN32
            fsync(fileno(file));
        #else
            fflush(file);
        #endif
        
        fclose(file);

        return 1;
    }

    table_t* TBM_load_table(char* name) {
        FILE* file = fopen(name, "rb");
        if (file == NULL) {
            return NULL;
        }

        // Read header of table from file.
        // Note: If magic is wrong, we can say, that this file isn`t table.
        //       We just return error code.
        table_header_t* header = (table_header_t*)malloc(sizeof(table_header_t));
        fread(header, sizeof(table_header_t), SEEK_CUR, file);
        if (header->magic != TABLE_MAGIC) {
            free(header);
            return NULL;
        }
        
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
        return table;
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

#pragma endregion
