#include "../include/database.h"


#pragma region [Table]

    #pragma region [Row]

        int DB_append_row(char* table_name, uint8_t* data, size_t data_size, uint8_t access) {
            table_t* table = TBM_load_table(table_name);

            uint8_t uwrite_access = GET_WRITE_ACCESS(access);
            uint8_t twrite_access = GET_WRITE_ACCESS(table->header->access);
            if (twrite_access < uwrite_access) {
                TBM_free_table(table);
                return -3;
            }

            if (TBM_check_signature(table, data) != 1) return -2;
            return TBM_append_content(table, data, data_size);
        }

        int DB_insert_row(char* table_name, int row, uint8_t* data, size_t data_size, uint8_t access) {
            table_t* table = TBM_load_table(table_name);

            uint8_t uwrite_access = GET_WRITE_ACCESS(access);
            uint8_t twrite_access = GET_WRITE_ACCESS(table->header->access);
            if (twrite_access < uwrite_access) {
                TBM_free_table(table);
                return -3;
            }
            // TODO
            return 1;
        }

        int DB_delete_row(char* table_name, int row, uint8_t access) {
            table_t* table = TBM_load_table(table_name);

            uint8_t udelete_access = GET_DELETE_ACCESS(access);
            uint8_t tdelete_access = GET_DELETE_ACCESS(table->header->access);
            if (tdelete_access < udelete_access) {
                TBM_free_table(table);
                return -3;
            }
            // TODO
            return 1;
        }

    #pragma endregion

    #pragma region [Data]

        int DB_find_data(database_t* database, char* table_name, int offset, uint8_t* data, size_t data_size) {
            return 1;
        }

        int DB_find_value(database_t* database, char* table_name, int offset, uint8_t value) {
            return 1;
        }

        int DB_find_data_row(database_t* database, char* table_name, int offset, uint8_t* data, size_t data_size) {
            return 1;
        }

        int DB_find_value_row(database_t* database, char* table_name, int offset, uint8_t value) {
            return 1;
        }

        int DB_delete_data(database_t* database, char* table_name, int offset, size_t size) {
            return 1;
        }

    #pragma endregion

#pragma endregion

#pragma region [Database]

    int DB_link_table2database(database_t* database, table_t* table) {
        return 1;
    }

    int DB_unlink_table_from_database(database_t* database, char* name) {
        return 1;
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
        for (int i = 0; i < database->header->table_count; i++) {
            free(database->table_names[i]);
        }

        free(database->table_names);
        free(database->header);
        free(database);
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

        fsync(fileno(file));
        fclose(file);

        return 1;
    }

#pragma endregion