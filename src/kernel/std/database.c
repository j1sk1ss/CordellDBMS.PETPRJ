#include "../include/database.h"


#pragma region [Table]

    #pragma region [Row]

        uint8_t* DB_get_row(database_t* database, char* table_name, int row, uint8_t access) {
            table_t* table = DB_get_table(database, table_name);
            if (table == NULL) return NULL;

            uint8_t uwrite_access = GET_WRITE_ACCESS(access);
            uint8_t twrite_access = GET_WRITE_ACCESS(table->header->access);
            if (twrite_access < uwrite_access) {
                TBM_free_table(table);
                return -3;
            }

            int current_row = 0;
            int current_global_offset = 0;
            while (current_row++ != row) {
                int result = TBM_find_value(table, current_global_offset, ROW_DELIMITER);
                if (result == -1) break;

                current_global_offset = result;
            }

            int row_end = TBM_find_value(table, current_global_offset, ROW_DELIMITER);
            int row_size = row_end - current_global_offset;

            uint8_t* row_data = TBM_get_content(table, current_global_offset, row_size);
            return row_data;
        }

        int DB_append_row(database_t* database, char* table_name, uint8_t* data, size_t data_size, uint8_t access) {
            table_t* table = DB_get_table(database, table_name);
            if (table == NULL) return -1;

            uint8_t uwrite_access = GET_WRITE_ACCESS(access);
            uint8_t twrite_access = GET_WRITE_ACCESS(table->header->access);
            if (twrite_access < uwrite_access) {
                TBM_free_table(table);
                return -3;
            }

            if (TBM_check_signature(table, data) != 1) return -2;
            return TBM_append_content(table, data, data_size);
        }

        int DB_insert_row(database_t* database, char* table_name, int row, uint8_t* data, size_t data_size, uint8_t access) {
            table_t* table = DB_get_table(database, table_name);
            if (table == NULL) return -1;

            uint8_t uwrite_access = GET_WRITE_ACCESS(access);
            uint8_t twrite_access = GET_WRITE_ACCESS(table->header->access);
            if (twrite_access < uwrite_access) {
                TBM_free_table(table);
                return -3;
            }
            // TODO:
            // 1) Offset calcauletion from row index
            return TBM_insert_content(table, 0, data, data_size);
        }

        int DB_delete_row(database_t* database, char* table_name, int row, uint8_t access) {
            table_t* table = DB_get_table(database, table_name);
            if (table == NULL) return -1;

            uint8_t udelete_access = GET_DELETE_ACCESS(access);
            uint8_t tdelete_access = GET_DELETE_ACCESS(table->header->access);
            if (tdelete_access < udelete_access) {
                TBM_free_table(table);
                return -3;
            }
            // TODO:
            // 1) Offset calculation from row index
            // 2) Size of row calculation
            return TBM_delete_content(table, 0, 0);
        }

        int DB_find_data_row(database_t* database, char* table_name, int offset, uint8_t* data, size_t data_size) {
            return 1; // TODO
        }

        int DB_find_value_row(database_t* database, char* table_name, int offset, uint8_t value) {
            return 1; // TODO
        }

    #pragma endregion

    #pragma region [Data]

        int DB_find_data(database_t* database, char* table_name, int offset, uint8_t* data, size_t data_size) {
            return 1; // TODO
        }

        int DB_find_value(database_t* database, char* table_name, int offset, uint8_t value) {
            return 1; // TODO
        }

        int DB_delete_data(database_t* database, char* table_name, int offset, size_t size) {
            return 1; // TODO
        }

    #pragma endregion

#pragma endregion

#pragma region [Database]

    table_t* DB_get_table(database_t* database, char* table_name) {
        // Try to get table from cache
        for (int i = 0; i < DATABASE_TABLE_CACHE_SIZE; i++) {
            if (database->tables_cache[i] == NULL) continue;
            if (strncmp(database->tables_cache[i]->header->name, table_name, TABLE_NAME_SIZE) == 0) return database->tables_cache[i];
        }

        // Try to load page from disk
        // Main difference with TBM_load in check, that table in database
        for (int i = 0; i < database->header->table_count; i++) {
            if (strncmp(database->table_names[i], table_name, TABLE_NAME_SIZE) == 0) {
                char save_path[50];
                sprintf(save_path, "%s%s.%s", TABLE_BASE_PATH, table_name, TABLE_EXTENSION);
                return TBM_load_table(save_path);
            }
        }

        return NULL;
    }

    int DB_link_table2database(database_t* database, table_t* table) {
        database->table_names = (uint8_t**)realloc(database->table_names, ++database->header->table_count * sizeof(uint8_t*));
        database->table_names[database->header->table_count] = (uint8_t*)malloc(TABLE_NAME_SIZE);
        memcpy(database->table_names[database->header->table_count], table->header->name, TABLE_NAME_SIZE);

        int page_cache_index = 0;
        for (int i = 0; i < DATABASE_TABLE_CACHE_SIZE; i++) {
            if (database->tables_cache[i] == NULL) page_cache_index = i;
        }

        TBM_free_table(database->tables_cache[page_cache_index]);
        database->tables_cache[page_cache_index] = table;

        return 1;
    }

    int DB_unlink_table_from_database(database_t* database, char* name) {
        for (int i = 0; i < database->header->table_count; ++i) {
            if (strcmp((char*)database->table_names[i], name) == 0) {
                free(database->table_names[i]);

                // Shift remaining names down
                for (size_t j = i; j < database->header->table_count - 1; ++j) {
                    database->table_names[j] = database->table_names[j + 1];
                }

                // Reallocate memory to shrink the array
                uint8_t** new_table_names = realloc(database->table_names, (database->header->table_count - 1) * sizeof(uint8_t*));
                if (new_table_names || (database->header->table_count - 1 == 0)) {
                    database->table_names = new_table_names;
                }

                // Decrement the table count
                database->header->table_count--;

                return 1;
            }
        }
        
        return -1;
    }

    database_t* DB_create_database(char* name) {
        database_header_t* header = (database_header_t*)malloc(sizeof(database_header_t));
        header->magic = DATABASE_MAGIC;
        strncpy(header->name, name, DATABASE_NAME_SIZE);
        header->table_count = 0;

        database_t* database = (database_t*)malloc(sizeof(database_t));
        database->header = header;

        return database;
    }

    int DB_free_database(database_t* database) {
        if (database == NULL) return -1;
        for (int i = 0; i < database->header->table_count; i++) 
            SOFT_FREE(database->table_names[i]);

        for (int i = 0; i < DATABASE_TABLE_CACHE_SIZE; i++) 
            TBM_free_table(database->tables_cache[i]);

        SOFT_FREE(database->table_names);
        SOFT_FREE(database->header);
        SOFT_FREE(database);

        return 1;
    }

    database_t* DB_load_database(char* name) {
        FILE* file = fopen(name, "rb");
        if (file == NULL) {
            return -1;
        }

        database_header_t* header = (database_header_t*)malloc(sizeof(database_header_t));
        fread(header, sizeof(database_header_t), SEEK_CUR, file);

        if (header->magic != DATABASE_MAGIC) {
            free(header);
            return -2;
        }

        uint8_t** names = (uint8_t**)malloc(header->table_count);
        for (int i = 0; i < header->table_count; i++) {
            names[i] = (uint8_t*)malloc(TABLE_NAME_SIZE);
            fread(names[i], TABLE_NAME_SIZE, SEEK_CUR, file);
        }

        database_t* database = (database_t*)malloc(sizeof(database_t));
        database->header = header;
        database->table_names = names;

        for (int i = 0; i < DATABASE_TABLE_CACHE_SIZE; i++) database->tables_cache[i] = NULL;
        for (int i = 0; i < MIN(DATABASE_TABLE_CACHE_SIZE, header->table_count); i++) {
            char save_path[50];
            sprintf(save_path, "%s%s.%s", TABLE_BASE_PATH, database->table_names[i], TABLE_EXTENSION);
            database->tables_cache[i] = TBM_load_table(save_path);
        }

        return database;
    }

    int DB_save_database(database_t* database, char* path) {
        FILE* file = fopen(path, "wb");
        if (file == NULL) {
            return -1;
        }

        fwrite(database->header, sizeof(database_header_t), SEEK_CUR, file);
        for (int i = 0; i < database->header->table_count; i++)
            fwrite(database->table_names[i], TABLE_NAME_SIZE, SEEK_CUR, file);

        #ifndef _WIN32
            fsync(fileno(file));
        #else
            fflush(file);
        #endif

        fclose(file);

        return 1;
    }

#pragma endregion