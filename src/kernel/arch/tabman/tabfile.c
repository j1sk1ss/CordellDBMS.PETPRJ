#include "../../include/tabman.h"


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
