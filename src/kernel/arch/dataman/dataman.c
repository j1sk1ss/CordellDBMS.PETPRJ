#include "../../include/dataman.h"


uint8_t* DB_get_row(database_t* __restrict database, char* __restrict table_name, int row, uint8_t access) {
    table_t* table = DB_get_table(database, table_name);
    if (table == NULL) return NULL;
    if (CHECK_WRITE_ACCESS(access, table->header->access) == -1) return NULL;

    int rows_per_page = PAGE_CONTENT_SIZE / table->row_size;
    int pages_offset  = row / rows_per_page;
    int page_offset   = row % rows_per_page;
    int global_offset = pages_offset * PAGE_CONTENT_SIZE + page_offset * table->row_size;

    uint8_t* data = TBM_get_content(table, global_offset, table->row_size);
    TBM_invoke_modules(table, data, COLUMN_MODULE_POSTLOAD);

    return data;
}

int DB_append_row(database_t* __restrict database, char* __restrict table_name, uint8_t* __restrict data, size_t data_size, uint8_t access) {
    table_t* table = DB_get_table(database, table_name);
    if (table == NULL) return -4;
    if (CHECK_WRITE_ACCESS(access, table->header->access) == -1) return -3;
    if (table->row_size != data_size) return -5;

    int result = TBM_check_signature(table, data);
    if (result != 1) return result - 10;

    TBM_invoke_modules(table, data, COLUMN_MODULE_PRELOAD);

    // Get primary column and column offset
    int column_offset = 0;
    table_column_t* primary_column_name = NULL;
    #pragma omp parallel for schedule(dynamic, 1)
    for (int i = 0; i < table->header->column_count; i++) {
        if (primary_column_name == NULL) {
            if (GET_COLUMN_PRIMARY(table->columns[i]->type) == COLUMN_PRIMARY) primary_column_name = table->columns[i];
            column_offset += table->columns[i]->size;
        }
    }

    // If in provided table presented primary column
    if (primary_column_name != NULL) {
        int row = DB_find_data_row(
            database, table_name, primary_column_name->name, 0, data + column_offset, primary_column_name->size, access
        );

        // If in table already presented this value.
        // That means, that this data not uniqe.
        if (row >= 0) return -20;
    }

    result = TBM_append_content(table, data, data_size);
    return result;
}

int DB_insert_row(database_t* __restrict database, char* __restrict table_name, int row, uint8_t* data, size_t data_size, uint8_t access) {
    table_t* table = DB_get_table(database, table_name);
    if (table == NULL) return -1;
    if (CHECK_WRITE_ACCESS(access, table->header->access) == -1) return -3;
    if (table->row_size != data_size) return -5;

    int result = TBM_check_signature(table, data);
    if (result != 1) return result - 10;

    TBM_invoke_modules(table, data, COLUMN_MODULE_PRELOAD);

    int rows_per_page = PAGE_CONTENT_SIZE / table->row_size;
    int pages_offset  = row / rows_per_page;
    int page_offset   = row % rows_per_page;
    int global_offset = pages_offset * PAGE_CONTENT_SIZE + page_offset * table->row_size;

    if (THR_require_lock(&table->lock, omp_get_thread_num()) == -1) return -4;
    result = TBM_insert_content(table, global_offset, data, data_size);
    THR_release_lock(&table->lock, omp_get_thread_num());

    return result;
}

int DB_delete_row(database_t* __restrict database, char* __restrict table_name, int row, uint8_t access) {
    table_t* table = DB_get_table(database, table_name);
    if (table == NULL) return -1;
    if (CHECK_DELETE_ACCESS(access, table->header->access) == -1) return -3;

    #ifndef NO_CASCADE_DELETE
        #pragma omp parallel for schedule(dynamic, 1)
        for (int i = 0; i < table->header->column_link_count; i++) {
            // Load slave table from disk
            table_t* slave_table = DB_get_table(database, table->column_links[i]->slave_table_name);
            if (slave_table == NULL) continue;
            if (CHECK_DELETE_ACCESS(access, slave_table->header->access) == -1) continue;

            // Get master column data and offset
            int master_column_offset = 0;
            table_column_t* master_column = NULL;
            for (int j = 0; j < table->header->column_count; j++) {
                if (strncmp(table->columns[j]->name, table->column_links[i]->master_column_name, COLUMN_NAME_SIZE) == 0) {
                    master_column = table->columns[j];
                    break;
                }

                master_column_offset += table->columns[j]->size;
            }

            // Get row by provided index
            if (master_column == NULL) continue;
            uint8_t* master_row = DB_get_row(database, table_name, row, access);
            uint8_t* master_row_pointer = master_row + master_column_offset;

            // Find row in slave table with same key on linked column
            int slave_row = DB_find_data_row(
                database, slave_table->header->name, table->column_links[i]->slave_column_name,
                0, master_row_pointer, master_column->size, access
            );

            free(master_row);

            if (THR_require_lock(&slave_table->lock, omp_get_thread_num()) == -1) continue;
            DB_delete_row(database, slave_table->header->name, slave_row, access);
            THR_release_lock(&slave_table->lock, omp_get_thread_num());
        }
    #endif

    int rows_per_page = PAGE_CONTENT_SIZE / table->row_size;
    int pages_offset  = row / rows_per_page;
    int page_offset   = row % rows_per_page;
    int global_offset = pages_offset * PAGE_CONTENT_SIZE + page_offset * table->row_size;

    if (THR_require_lock(&table->lock, omp_get_thread_num()) == -1) return -3;
    int result = TBM_delete_content(table, global_offset, table->row_size);
    THR_release_lock(&table->lock, omp_get_thread_num());
    return result;
}

int DB_cleanup_tables(database_t* database) {
    #pragma omp parallel for schedule(dynamic, 1)
    for (int i = 0; i < database->header->table_count; i++) {
        table_t* table = DB_get_table(database, database->table_names[i]);
        TBM_cleanup_dirs(table);
    }

    if (CHC_sync() != 1) return -1;
    return 1;
}

int DB_find_data_row(
    database_t* __restrict database, char* __restrict table_name, char* __restrict column, int offset, 
    uint8_t* __restrict data, size_t data_size, uint8_t access
) {
    table_t* table = DB_get_table(database, table_name);
    if (table == NULL) return -1;
    if (CHECK_READ_ACCESS(access, table->header->access) == -1) return -3;

    int row_size      = 0;
    int column_offset = -1;
    int column_size   = -1;
    for (int i = 0; i < table->header->column_count; i++) {
        if (column != NULL) {
            if (strncmp(table->columns[i]->name, column, COLUMN_NAME_SIZE) == 0) {
                column_offset = row_size;
                column_size = table->columns[i]->size;
            }

            row_size += table->columns[i]->size;
        }
        else {
            row_size = table->row_size;
            break;
        }
    }

    while (1) {
        int global_offset = TBM_find_content(table, offset, data, data_size);
        if (global_offset < 0) return -1;

        int row = global_offset / row_size;
        if (column_offset == -1 && column_size == -1) return row;

        int position_in_row = global_offset % row_size;
        if (position_in_row >= column_offset && position_in_row < column_offset + column_size) return row;
        else offset = global_offset + data_size;
    }
}

table_t* DB_get_table(database_t* __restrict database, char* __restrict table_name) {
    if (database == NULL) return NULL;
    if (table_name == NULL) return NULL;

    table_t* table = NULL;
    #pragma omp parallel for schedule(dynamic, 1)
    for (int i = 0; i < database->header->table_count; i++) {
        if (strncmp(database->table_names[i], table_name, TABLE_NAME_SIZE) == 0 && table == NULL) {
            table = TBM_load_table(NULL, table_name);
        }
    }

    // If table not in database, we return NULL
    if (table == NULL) print_warn("Table [%s] not in [%.*s] database!", table_name, DATABASE_NAME_SIZE, database->header->name);
    return table;
}

int DB_delete_table(database_t* __restrict database, char* __restrict table_name, int full) {
    table_t* table = DB_get_table(database, table_name);
    if (table == NULL) return -1;

    DB_unlink_table_from_database(database, table_name);
    return TBM_delete_table(table, full);
}

int DB_link_table2database(database_t* __restrict database, table_t* __restrict table) {
    if (database->header->table_count + 1 >= TABLES_PER_DATABASE) return -1;

    #pragma omp critical (link_table2database)
    strncpy(database->table_names[database->header->table_count++], table->header->name, TABLE_NAME_SIZE);
    return 1;
}

int DB_unlink_table_from_database(database_t* __restrict database, char* __restrict name) {
    int status = 0;
    #pragma omp critical (unlink_table_from_database)
    {
        for (int i = 0; i < database->header->table_count; i++) {
            if (strncmp(database->table_names[i], name, TABLE_NAME_SIZE) == 0) {
                for (int j = i; j < database->header->table_count - 1; j++) {
                    strncpy(database->table_names[j], database->table_names[j + 1], TABLE_NAME_SIZE);
                }

                database->header->table_count--;
                status = 1;
                break;
            }
        }
    }

    return status;
}
