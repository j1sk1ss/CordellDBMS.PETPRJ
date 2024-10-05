#include "../../include/dataman.h"


#pragma region [Table files]

    uint8_t* DB_get_row(
        database_t* database, char* table_name,
        int row, uint8_t access
    ) {
        table_t* table = DB_get_table(database, table_name);
        if (table == NULL) return NULL;
        if (CHECK_WRITE_ACCESS(access, table->header->access) == -1) return NULL;

        int row_size = 0;
        for (int i = 0; i < table->header->column_count; i++) row_size += table->columns[i]->size;

        int rows_per_page = PAGE_CONTENT_SIZE / row_size;
        int pages_offset  = row / rows_per_page;
        int page_offset   = row % rows_per_page;
        int global_offset = pages_offset * PAGE_CONTENT_SIZE + page_offset * row_size;
        return TBM_get_content(table, global_offset, row_size);
    }

    int DB_append_row(
        database_t* database, char* table_name,
        uint8_t* data, size_t data_size, uint8_t access
    ) {
        table_t* table = DB_get_table(database, table_name);
        if (table == NULL) return -4;
        if (CHECK_WRITE_ACCESS(access, table->header->access) == -1) return -3;

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

        return TBM_append_content(table, data, data_size);
    }

    int DB_insert_row(
        database_t* database, char* table_name,
        int row, uint8_t* data, size_t data_size,
        uint8_t access
    ) {
        table_t* table = DB_get_table(database, table_name);
        if (table == NULL) return -1;
        if (CHECK_WRITE_ACCESS(access, table->header->access) == -1) return -3;

        int result = TBM_check_signature(table, data);
        if (result != 1) return result - 10;

        int row_size = 0;
        for (int i = 0; i < table->header->column_count; i++) row_size += table->columns[i]->size;

        int rows_per_page = PAGE_CONTENT_SIZE / row_size;
        int pages_offset  = row / rows_per_page;
        int page_offset   = row % rows_per_page;
        int global_offset = pages_offset * PAGE_CONTENT_SIZE + page_offset * row_size;
        return TBM_insert_content(table, global_offset, data, data_size);
    }

    int DB_update_row(
        database_t* database, char* table_name,
        int row, char* column_name, uint8_t* data,
        size_t data_size, uint8_t access
    ) { // TODO: Cascade Update (If updated linked column)
        table_t* table = DB_get_table(database, table_name);
        if (table == NULL) return -1;
        if (CHECK_WRITE_ACCESS(access, table->header->access) == -1) return -3;

        int row_size      = 0;
        int column_offset = -1;
        int column_size   = -1;
        for (int i = 0; i < table->header->column_count; i++) {
            if (column_name != NULL) {
                if (strcmp((char*)table->columns[i]->name, column_name) == 0) {
                    column_offset = row_size;
                    column_size = table->columns[i]->size;
                }
            }

            row_size += table->columns[i]->size;
        }

        if ((int)data_size > column_size) return -1;
        if (column_size == -1 && column_offset == -1) return TBM_insert_content(table, row_size * row, data, row_size);
        return TBM_insert_content(table, row_size * row + column_offset, data, column_size);
    }

    int DB_delete_row(
        database_t* database, char* table_name,
        int row, uint8_t access
    ) {
        table_t* table = DB_get_table(database, table_name);
        if (table == NULL) return -1;
        if (CHECK_DELETE_ACCESS(access, table->header->access) == -1) return -3;

        int row_size = 0;
        for (int i = 0; i < table->header->column_count; i++) row_size += table->columns[i]->size;

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

        int rows_per_page = PAGE_CONTENT_SIZE / row_size;
        int pages_offset  = row / rows_per_page;
        int page_offset   = row % rows_per_page;
        int global_offset = pages_offset * PAGE_CONTENT_SIZE + page_offset * row_size;
        return TBM_delete_content(table, global_offset, row_size);
    }

    int DB_cleanup_tables(database_t* database) {
        for (int i = 0; i < database->header->table_count; i++) {
            table_t* table = DB_get_table(database, (char*)database->table_names[i]);
            TBM_cleanup_dirs(table);
        }

        int result = 1;
        result = TBM_TDT_sync();
        if (result != 1) return -1;

        result = DRM_DDT_sync();
        if (result != 1) return -2;

        result = PGM_PDT_sync();
        if (result != 1) return -3;

        return 1;
    }

    int DB_find_data_row(
        database_t* database, char* table_name,
        char* column, int offset, uint8_t* data,
        size_t data_size, uint8_t access
    ) {
        table_t* table = DB_get_table(database, table_name);
        if (table == NULL) return -1;
        if (CHECK_READ_ACCESS(access, table->header->access) == -1) return -3;

        int row_size      = 0;
        int column_offset = -1;
        int column_size   = -1;
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
            if (column_offset == -1 && column_size == -1) return row;

            int position_in_row = global_offset % row_size;
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
        if (CHECK_READ_ACCESS(access, table->header->access) == -1) return -3;

        int row_size      = 0;
        int column_offset = -1;
        int column_size   = -1;
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
            if (column_offset == -1 && column_size == -1) return row;

            int position_in_row = global_offset % row_size;
            if (position_in_row >= column_offset && position_in_row < column_offset + column_size) return row;
            else offset = global_offset + 1;
        }
    }

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
                database->cached_table = TBM_load_table(NULL, table_name);
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
        if (database->header->table_count + 1 >= TABLES_PER_DATABASE) return -1;

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

#pragma endregion

#pragma region [Transaction]

    int DB_init_transaction(database_t* database) {
        int result = 1;
        result = TBM_TDT_sync();
        if (result != 1) return -1;

        result = DRM_DDT_sync();
        if (result != 1) return -2;

        result = PGM_PDT_sync();
        if (result != 1) return -3;

        return DB_cleanup_tables(database);
    }

    int DB_rollback() {
        int result = 1;
        result = TBM_TDT_free();
        if (result != 1) return -1;

        result = DRM_DDT_free();
        if (result != 1) return -2;

        result = PGM_PDT_free();
        if (result != 1) return -3;

        return 1;
    }

#pragma endregion
