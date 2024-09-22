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
        int global_offset = row * row_size;

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
        if (result != 1) return result + 10;

        int append_result = TBM_append_content(table, data, data_size);

        char save_path[DEFAULT_PATH_SIZE];
        sprintf(save_path, "%s%.8s.%s", TABLE_BASE_PATH, table_name, TABLE_EXTENSION);
        TBM_save_table(table, save_path);

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
        int global_offset = row * row_size;

        if (TBM_check_signature(table, data) != 1) return -2;
        return TBM_insert_content(table, global_offset, data, data_size);
    }

    int DB_update_row(
        database_t* database, char* table_name, 
        int row, char* column_name, uint8_t* data, 
        size_t data_size, uint8_t access
    ) {
        table_t* table = DB_get_table(database, table_name);
        if (table == NULL) return -1;
        if (CHECK_WRITE_ACCESS(access, table->header->access) == -1) {
            return -3;
        }

        int row_size = 0;
        int column_offset = -1;
        int column_size = -1;
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
        int global_offset = row * row_size;

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

        int row_size = 0;
        int column_offset = -1;
        int column_size = -1;
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

        int row_size = 0;
        int column_offset = -1;
        int column_size = -1;
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
        // Main difference with TBM_load in check, that table in database
        for (int i = 0; i < database->header->table_count; i++) {
            if (strncmp((char*)database->table_names[i], table_name, TABLE_NAME_SIZE) == 0) {
                char save_path[DEFAULT_PATH_SIZE];
                sprintf(save_path, "%s%.8s.%s", TABLE_BASE_PATH, table_name, TABLE_EXTENSION);
                return TBM_load_table(save_path);
            }
        }

        // If table not in database, we return NULL
        return NULL;
    }

    int DB_delete_table(database_t* database, char* table_name, int full) {
        table_t* table = DB_get_table(database, table_name);
        if (table == NULL) return -1;

        DB_unlink_table_from_database(database, table_name);

        char save_path[DEFAULT_PATH_SIZE];
        sprintf(save_path, "%s%.8s.%s", DATABASE_BASE_PATH, database->header->name, DATABASE_EXTENSION);
        DB_save_database(database, save_path);

        return TBM_delete_table(table, full);
    }

    int DB_link_table2database(database_t* database, table_t* table) {
        memcpy(database->table_names[database->header->table_count++], table->header->name, TABLE_NAME_SIZE);
        return 1;
    }

    int DB_unlink_table_from_database(database_t* database, char* name) {
        return 1; // TODO
    }

    database_t* DB_create_database(char* name) {
        database_header_t* header = (database_header_t*)malloc(sizeof(database_header_t));
        header->magic = DATABASE_MAGIC;
        strncpy((char*)header->name, name, DATABASE_NAME_SIZE);
        header->table_count = 0;

        database_t* database = (database_t*)malloc(sizeof(database_t));
        database->header = header;

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
        #pragma omp critical (database_load)
        {
            FILE* file = fopen(path, "rb");
            if (file == NULL) {
                printf("Database file not found! [%s]\n", path);
                loaded_database = NULL;
            } else {
                database_header_t* header = (database_header_t*)malloc(sizeof(database_header_t));
                fread(header, sizeof(database_header_t), SEEK_CUR, file);

                if (header->magic != DATABASE_MAGIC) {
                    free(header);
                    loaded_database = NULL;
                } else {
                    database_t* database = (database_t*)malloc(sizeof(database_t));
                    for (int i = 0; i < header->table_count; i++) {
                        fread(database->table_names[i], TABLE_NAME_SIZE, SEEK_CUR, file);
                    }

                    database->header = header;
                    loaded_database = database;
                }
            }
        }

        return loaded_database;
    }

    int DB_save_database(database_t* database, char* path) {
        int status = 1;
        #pragma omp critical (database_save)
        {
            FILE* file = fopen(path, "wb");
            if (file == NULL) {
                status = -1;
            } else {
                if (fwrite(database->header, sizeof(database_header_t), SEEK_CUR, file) != SEEK_CUR) status = -2;
                for (int i = 0; i < database->header->table_count; i++)
                    if (fwrite(database->table_names[i], TABLE_NAME_SIZE, SEEK_CUR, file) != SEEK_CUR) {
                        status = -3;
                        break;
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