#include "../include/database.h"


#pragma region [Table]

    uint8_t* DB_get_row(database_t* database, char* table_name, int row, uint8_t access) {
        table_t* table = DB_get_table(database, table_name);
        if (table == NULL) return NULL;
        if (CHECK_WRITE_ACCESS(access, table->header->access) == -1) {
            TBM_free_table(table);
            return NULL;
        }

        int row_size = 0;
        for (int i = 0; i < table->header->column_count; i++) row_size += table->columns[i]->size;
        int global_offset = row * row_size;

        uint8_t* row_data = TBM_get_content(table, global_offset, row_size);
        return row_data;
    }

    int DB_append_row(database_t* database, char* table_name, uint8_t* data, size_t data_size, uint8_t access) {
        table_t* table = DB_get_table(database, table_name);
        if (table == NULL) return -1;
        if (CHECK_WRITE_ACCESS(access, table->header->access) == -1) {
            TBM_free_table(table);
            return -3;
        }

        int result = TBM_check_signature(table, data);
        if (result != 1) return result;

        int append_result = TBM_append_content(table, data, data_size);

        char save_path[DEFAULT_PATH_SIZE];
        sprintf(save_path, "%s%.8s.%s", TABLE_BASE_PATH, table_name, TABLE_EXTENSION);
        TBM_save_table(table, save_path);
        TBM_free_table(table);

        return append_result;
    }

    int DB_insert_row(database_t* database, char* table_name, int row, uint8_t* data, size_t data_size, uint8_t access) {
        table_t* table = DB_get_table(database, table_name);
        if (table == NULL) return -1;
        if (CHECK_WRITE_ACCESS(access, table->header->access) == -1) {
            TBM_free_table(table);
            return -3;
        }

        int row_size = 0;
        for (int i = 0; i < table->header->column_count; i++) row_size += table->columns[i]->size;
        int global_offset = row * row_size;

        if (TBM_check_signature(table, data) != 1) return -2;
        return TBM_insert_content(table, global_offset, data, data_size);
    }

    int DB_delete_row(database_t* database, char* table_name, int row, uint8_t access) {
        table_t* table = DB_get_table(database, table_name);
        if (table == NULL) return -1;
        if (CHECK_DELETE_ACCESS(access, table->header->access) == -1) {
            TBM_free_table(table);
            return -3;
        }

        int row_size = 0;
        for (int i = 0; i < table->header->column_count; i++) row_size += table->columns[i]->size;
        int global_offset = row * row_size;

        return TBM_delete_content(table, global_offset, row_size);
    }

    int DB_find_data_row(database_t* database, char* table_name, int offset, uint8_t* data, size_t data_size, uint8_t access) {
        table_t* table = DB_get_table(database, table_name);
        if (table == NULL) return -1;
        if (CHECK_READ_ACCESS(access, table->header->access) == -1) {
            TBM_free_table(table);
            return -3;
        }

        int row_size = 0;
        for (int i = 0; i < table->header->column_count; i++) row_size += table->columns[i]->size;

        int global_offset = TBM_find_content(table, offset, data, data_size);
        int row = global_offset / row_size;

        return row;
    }

    int DB_find_value_row(database_t* database, char* table_name, int offset, uint8_t value, uint8_t access) {
        table_t* table = DB_get_table(database, table_name);
        if (table == NULL) return -1;
        if (CHECK_READ_ACCESS(access, table->header->access) == -1) {
            TBM_free_table(table);
            return -3;
        }

        int row_size = 0;
        for (int i = 0; i < table->header->column_count; i++) row_size += table->columns[i]->size;

        int global_offset = TBM_find_value(table, offset, value);
        int row = global_offset / row_size;

        return row;
    }

#pragma endregion

#pragma region [Database]

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

    database_t* DB_load_database(char* name) {
        FILE* file = fopen(name, "rb");
        if (file == NULL) {
            return NULL;
        }

        database_header_t* header = (database_header_t*)malloc(sizeof(database_header_t));
        fread(header, sizeof(database_header_t), SEEK_CUR, file);

        if (header->magic != DATABASE_MAGIC) {
            free(header);
            return NULL;
        }

        database_t* database = (database_t*)malloc(sizeof(database_t));
        for (int i = 0; i < header->table_count; i++) {
            fread(database->table_names[i], TABLE_NAME_SIZE, SEEK_CUR, file);
        }

        database->header = header;

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