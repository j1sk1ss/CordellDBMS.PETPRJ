#include "../include/database.h"


#pragma region [Table files]

    uint8_t* DB_get_row(
        database_t* database, char* table_name,
        int row, uint8_t access
    ) {
        table_t* table = DB_get_table(database, table_name);
        if (table == NULL) return NULL;
        if (CHECK_WRITE_ACCESS(access, table->header->access) == -1) {
            return NULL;
        }

        int row_size = 0;
        for (int i = 0; i < table->header->column_count; i++) row_size += table->columns[i]->size;

        int rows_per_page = PAGE_CONTENT_SIZE / row_size;
        int pages_offset  = row / rows_per_page;
        int page_offset   = row % rows_per_page;
        int global_offset = pages_offset * PAGE_CONTENT_SIZE + page_offset * row_size;

        uint8_t* row_data = TBM_get_content(table, global_offset, row_size);
        return row_data;
    }

    int DB_append_row(
        database_t* database, char* table_name,
        uint8_t* data, size_t data_size, uint8_t access
    ) {
        table_t* table = DB_get_table(database, table_name);
        if (table == NULL) return -4;
        if (CHECK_WRITE_ACCESS(access, table->header->access) == -1) {
            return -3;
        }

        int result = TBM_check_signature(table, data);
        if (result != 1) return result - 10;

        #ifndef NO_PRIMARY_CHECK
            // Get primary column and column offset
            int column_offset = 0;
            table_column_t* primary_column_name = NULL;
            for (int i = 0; i < table->header->column_count; i++) {
                if (GET_COLUMN_PRIMARY(table->columns[i]->type) == COLUMN_PRIMARY) {
                    primary_column_name = table->columns[i];
                    break;
                }

                column_offset += table->columns[i]->size;
            }

            // If in provided table presented primary column
            if (primary_column_name != NULL) {
                uint8_t* data_pointer = data;
                data_pointer += column_offset;

                int row = DB_find_data_row(
                    database, table_name, (char*)primary_column_name->name,
                    0, data_pointer, primary_column_name->size, access
                );

                // If in table already presented this value.
                // That means, that this data not uniqe.
                if (row >= 0) {
                    return -1;
                }
            }
        #endif

        int append_result = TBM_append_content(table, data, data_size);
        TBM_save_table(table, NULL);

        return append_result;
    }

    int DB_insert_row(
        database_t* database, char* table_name,
        int row, uint8_t* data, size_t data_size,
        uint8_t access
    ) {
        table_t* table = DB_get_table(database, table_name);
        if (table == NULL) return -1;
        if (CHECK_WRITE_ACCESS(access, table->header->access) == -1) {
            return -3;
        }

        int row_size = 0;
        for (int i = 0; i < table->header->column_count; i++) row_size += table->columns[i]->size;

        int rows_per_page = PAGE_CONTENT_SIZE / row_size;
        int pages_offset  = row / rows_per_page;
        int page_offset   = row % rows_per_page;
        int global_offset = pages_offset * PAGE_CONTENT_SIZE + page_offset * row_size;

        int result = TBM_check_signature(table, data);
        if (result != 1) return result - 10;
        return TBM_insert_content(table, global_offset, data, data_size);
    }

    int DB_update_row(
        database_t* database, char* table_name,
        int row, char* column_name, uint8_t* data,
        size_t data_size, uint8_t access
    ) { // TODO: Cascade Update (If updated linked column)
        table_t* table = DB_get_table(database, table_name);
        if (table == NULL) return -1;
        if (CHECK_WRITE_ACCESS(access, table->header->access) == -1) {
            return -3;
        }

        int row_size        = 0;
        int column_offset   = -1;
        int column_size     = -1;
        for (int i = 0; i < table->header->column_count; i++) {
            if (column_name != NULL) {
                if (strcmp((char*)table->columns[i]->name, column_name) == 0) {
                    column_offset = row_size;
                    column_size = table->columns[i]->size;
                }
            }

            row_size += table->columns[i]->size;
        }

        if (column_size == -1 && column_offset == -1) return TBM_insert_content(table, row_size * row, data, row_size);
        if ((int)data_size > column_size) return -1;

        return TBM_insert_content(table, row_size * row + column_offset, data, column_size);
    }

    int DB_delete_row(
        database_t* database, char* table_name,
        int row, uint8_t access
    ) {
        table_t* table = DB_get_table(database, table_name);
        if (table == NULL) return -1;
        if (CHECK_DELETE_ACCESS(access, table->header->access) == -1) {
            return -3;
        }

        int row_size = 0;
        for (int i = 0; i < table->header->column_count; i++) row_size += table->columns[i]->size;

        int rows_per_page = PAGE_CONTENT_SIZE / row_size;
        int pages_offset  = row / rows_per_page;
        int page_offset   = row % rows_per_page;
        int global_offset = pages_offset * PAGE_CONTENT_SIZE + page_offset * row_size;

        #ifndef NO_CASCADE_DELETE
            #pragma omp parallel
            for (int i = 0; i < table->header->column_link_count; i++) {
                // Load slave table from disk
                table_t* slave_table = DB_get_table(database, (char*)table->column_links[i]->slave_table_name);
                if (slave_table == NULL) continue;
                if (CHECK_DELETE_ACCESS(access, slave_table->header->access) == -1) continue;

                // Get master column data and offset
                int master_column_offset = 0;
                table_column_t* master_column = NULL;
                for (int j = 0; j < table->header->column_count; j++) {
                    master_column_offset += table->columns[j]->size;
                    if (memcmp(table->columns[j]->name, table->column_links[i]->master_column_name, COLUMN_NAME_SIZE) == 0) {
                        master_column = table->columns[j];
                        break;
                    }
                }

                // Get slave column
                table_column_t* slave_column = NULL;
                for (int j = 0; j < table->header->column_count; j++) {
                    if (memcmp(slave_table->columns[j]->name, table->column_links[i]->slave_column_name, COLUMN_NAME_SIZE) == 0) {
                        slave_column = table->columns[j];
                        break;
                    }
                }

                // Get row by provided index
                if (master_column == NULL || slave_column == NULL) continue;
                char* master_row = (char*)DB_get_row(database, table_name, row, access);
                char* master_row_pointer = master_row;
                master_row_pointer += master_column_offset;

                // Find row in slave table with same key on linked column
                int slave_row = DB_find_data_row(database, (char*)slave_table->header->name, (char*)slave_column->name, 0, (uint8_t*)master_row_pointer, master_column->size, access);
                free(master_row);
                DB_delete_row(database, (char*)slave_table->header->name, slave_row, access);
            }
        #endif

        return TBM_delete_content(table, global_offset, row_size);
    }

    int DB_find_data_row(
        database_t* database, char* table_name,
        char* column, int offset, uint8_t* data,
        size_t data_size, uint8_t access
    ) {
        table_t* table = DB_get_table(database, table_name);
        if (table == NULL) return -1;
        if (CHECK_READ_ACCESS(access, table->header->access) == -1) {
            return -3;
        }

        int row_size        = 0;
        int column_offset   = -1;
        int column_size     = -1;
        for (int i = 0; i < table->header->column_count; i++) {
            if (column != NULL) {
                if (strcmp((char*)table->columns[i]->name, column) == 0) {
                    column_offset = row_size;
                    column_size = table->columns[i]->size;
                }
            }

            row_size += table->columns[i]->size;
        }

        while (1) {
            int global_offset = TBM_find_content(table, offset, data, data_size);
            if (global_offset == -1 || global_offset == -2) return -1;

            int row = global_offset / row_size;
            int position_in_row = global_offset % row_size;
            if (column_offset == -1 && column_size == -1) return row;

            if (position_in_row >= column_offset && position_in_row < column_offset + column_size) return row;
            else offset = global_offset + data_size;
        }
    }

    int DB_find_value_row(
        database_t* database, char* table_name,
        char* column, int offset, uint8_t value,
        uint8_t access
    ) {
        table_t* table = DB_get_table(database, table_name);
        if (table == NULL) return -1;
        if (CHECK_READ_ACCESS(access, table->header->access) == -1) {
            return -3;
        }

        int row_size        = 0;
        int column_offset   = -1;
        int column_size     = -1;
        for (int i = 0; i < table->header->column_count; i++) {
            if (column != NULL) {
                if (strcmp((char*)table->columns[i]->name, column) == 0) {
                    column_offset = row_size;
                    column_size = table->columns[i]->size;
                }
            }

            row_size += table->columns[i]->size;
        }

        while (1) {
            int global_offset = TBM_find_value(table, offset, value);
            if (global_offset == -1) return -1;

            int row = global_offset / row_size;
            int position_in_row = global_offset % row_size;
            if (column_offset == -1 && column_size == -1) return row;

            if (position_in_row >= column_offset && position_in_row < column_offset + column_size) return row;
            else offset = global_offset + 1;
        }
    }

#pragma endregion

#pragma region [Database file]

    table_t* DB_get_table(database_t* database, char* table_name) {
        // If we already has cached table, we can think, that this is
        // table what we need.
        if (database->cached_table != NULL) {
            if (strncmp((char*)database->cached_table->header->name, table_name, TABLE_NAME_SIZE) == 0) {
                return database->cached_table;
            }
        }

        // Main difference with TBM_load in check, that table in database
        for (int i = 0; i < database->header->table_count; i++) {
            if (strncmp((char*)database->table_names[i], table_name, TABLE_NAME_SIZE) == 0) {
                char save_path[DEFAULT_PATH_SIZE];
                sprintf(save_path, "%s%.8s.%s", TABLE_BASE_PATH, table_name, TABLE_EXTENSION);

                database->cached_table = TBM_load_table(save_path);
                return database->cached_table;
            }
        }

        // If table not in database, we return NULL
        return NULL;
    }

    int DB_delete_table(database_t* database, char* table_name, int full) {
        table_t* table = DB_get_table(database, table_name);
        if (table == NULL) return -1;

        DB_unlink_table_from_database(database, table_name);
        DB_save_database(database, NULL);

        return TBM_delete_table(table, full);
    }

    int DB_link_table2database(database_t* database, table_t* table) {
        if (database->header->table_count + 1 >= TABLES_PER_DATABASE)
            return -1;

        #pragma omp critical (link_table2database)
        memcpy(database->table_names[database->header->table_count++], table->header->name, TABLE_NAME_SIZE);
        return 1;
    }

    int DB_unlink_table_from_database(database_t* database, char* name) {
        int status = 0;
        #pragma omp critical (unlink_table_from_database)
        {
            // If table for unlink in cache, mark it NULL
            if (database->cached_table != NULL) {
                if (strncmp((char*)database->cached_table->header->name, name, TABLE_NAME_SIZE) == 0) {
                    database->cached_table = NULL;
                }
            }

            for (int i = 0; i < database->header->table_count; i++) {
                if (strncmp((char*)database->table_names[i], name, TABLE_NAME_SIZE) == 0) {
                    for (int j = i; j < database->header->table_count - 1; j++) {
                        memcpy(database->table_names[j], database->table_names[j + 1], TABLE_NAME_SIZE);
                    }

                    database->header->table_count--;
                    status = 1;
                    break;
                }
            }
        }

        return status;
    }

    database_t* DB_create_database(char* name) {
        database_header_t* header = (database_header_t*)malloc(sizeof(database_header_t));
        header->magic = DATABASE_MAGIC;
        strncpy((char*)header->name, name, DATABASE_NAME_SIZE);
        header->table_count = 0;

        database_t* database = (database_t*)malloc(sizeof(database_t));
        database->header        = header;
        database->cached_table  = NULL;

        return database;
    }

    int DB_free_database(database_t* database) {
        if (database == NULL) return -1;

        SOFT_FREE(database->header);
        SOFT_FREE(database);

        return 1;
    }

    database_t* DB_load_database(char* path) {
        database_t* loaded_database = NULL;
        #pragma omp critical (load_database)
        {
            FILE* file = fopen(path, "rb");
            if (file == NULL) {
                loaded_database = NULL;
                print_error("Database file not found! [%s]", path);
            } else {
                database_header_t* header = (database_header_t*)malloc(sizeof(database_header_t));
                fread(header, sizeof(database_header_t), 1, file);

                if (header->magic != DATABASE_MAGIC) {
                    free(header);
                    loaded_database = NULL;
                } else {
                    database_t* database = (database_t*)malloc(sizeof(database_t));
                    for (int i = 0; i < header->table_count; i++) {
                        fread(database->table_names[i], sizeof(uint8_t), TABLE_NAME_SIZE, file);
                    }

                    database->header        = header;
                    database->cached_table  = NULL;
                    loaded_database = database;
                }
            }
        }

        return loaded_database;
    }

    int DB_save_database(database_t* database, char* path) {
        int status = 1;
        #pragma omp critical (save_database)
        {
            // We generate default path
            char save_path[DEFAULT_PATH_SIZE];
            if (path == NULL) {
                sprintf(save_path, "%s%.8s.%s", DATABASE_BASE_PATH, database->header->name, DATABASE_EXTENSION);
            }
            else strcpy(save_path, path);

            FILE* file = fopen(save_path, "wb");
            if (file == NULL) {
                status = -1;
                print_error("Can`t create file: [%s]", save_path);
            } else {
                if (fwrite(database->header, sizeof(database_header_t), 1, file) != 1) status = -2;
                for (int i = 0; i < database->header->table_count; i++)
                    if (fwrite(database->table_names[i], sizeof(uint8_t), TABLE_NAME_SIZE, file) != TABLE_NAME_SIZE) {
                        status = -3;
                    }

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

#pragma endregion
