#include "../../include/dataman.h"


static int _unlink_table_from_database(database_t* __restrict database, char* __restrict name) {
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

static int _get_global_offset(int row_size, int row) {
    int rows_per_page = PAGE_CONTENT_SIZE / row_size;
    int pages_offset  = row / rows_per_page;
    int row_offset    = row % rows_per_page;
    int global_offset = pages_offset * PAGE_CONTENT_SIZE + row_offset * row_size;
    return global_offset;
}

static table_t* _get_table_access(
    database_t* __restrict database, char* __restrict table_name, int access, int (*check_access)(int, int)
) {
    table_t* table = DB_get_table(database, table_name);
    if (table == NULL) return NULL;

    if (check_access(access, table->header->access) == -1) {
        TBM_flush_table(table);
        return NULL;
    }

    return table;
}

#pragma region [CRUD]

int DB_append_row(
    database_t* __restrict database, char* __restrict table_name, 
    unsigned char* __restrict data, size_t data_size, unsigned char access
) {
    table_t* table = _get_table_access(database, table_name, access, check_write_access);
    if (table == NULL) return -4;
    if (table->row_size > data_size) {
        TBM_flush_table(table);
        return -5;
    }

    int result = TBM_check_signature(table, data); // O(n)
    if (result != 1) {
        TBM_flush_table(table);
        return result - 10;
    }

    // Get primary column and column offset
    int column_offset = 0;
    table_column_t* primary_column = NULL;
    for (int i = 0; i < table->header->column_count; i++) { // O(n)
        if (GET_COLUMN_PRIMARY(table->columns[i]->type) == COLUMN_PRIMARY) {
            primary_column = table->columns[i];
        }

        unsigned char* current_data = data + column_offset;
        if (
            GET_COLUMN_TYPE(table->columns[i]->type) == COLUMN_AUTO_INCREMENT && 
            GET_COLUMN_DATA_TYPE(table->columns[i]->type) == COLUMN_TYPE_INT
        ) {
            unsigned char* previous_data = (unsigned char*)malloc(table->row_size);
            if (previous_data != NULL) {
                if (DB_get_row(database, table_name, MAX(table->header->row_count - 1, 0), access, previous_data, table->row_size)) {
                    char number_buffer[128] = { 0 };
                    strncpy(number_buffer, (char*)(previous_data + column_offset), table->columns[i]->size);

                    char buffer[128] = { 0 };
                    sprintf(buffer, "%0*d", table->columns[i]->size, atoi(number_buffer) + 1);

                    memcpy(current_data, buffer, table->columns[i]->size);
                }

                free(previous_data);
            }
        }

        column_offset += table->columns[i]->size;
    }

    // If in provided table presented primary column
    if (primary_column != NULL) {
        table_columns_info_t primary_info;
        TBM_get_column_info(table, primary_column->name, &primary_info);
        int row = DB_find_data_row(
            database, table_name, primary_column->name, 0, data + primary_info.offset, primary_column->size, access
        );

        // If in table already presented this value.
        // That means, that this data not uniqe.
        if (row >= 0) {
            TBM_flush_table(table);
            return -20;
        }
    }

    TBM_invoke_modules(table, data, COLUMN_MODULE_PRELOAD); // O(n)
    result = TBM_append_content(table, data, data_size);

    table->header->row_count++;
    TBM_flush_table(table);
    return result;
}

int DB_get_row(
    database_t* __restrict database, char* __restrict table_name, int row, unsigned char access, 
    unsigned char* buffer, size_t buffer_size
) {
    table_t* table = _get_table_access(database, table_name, access, check_write_access);
    if (table == NULL) return 0;

    int get_result = TBM_get_content(table, _get_global_offset(table->row_size, row), buffer, buffer_size);
    if (get_result) {
        TBM_invoke_modules(table, buffer, COLUMN_MODULE_POSTLOAD);
    }

    TBM_flush_table(table);
    return get_result;
}

int DB_insert_row(
    database_t* __restrict database, char* __restrict table_name, 
    int row, unsigned char* __restrict data, size_t data_size, unsigned char access
) {
#ifndef NO_UPDATE_COMMAND
    table_t* table = _get_table_access(database, table_name, access, check_write_access);
    if (table == NULL) return -1;
    if (table->row_size > data_size) {
        TBM_flush_table(table);
        return -5;
    }

    int result = TBM_check_signature(table, data);
    if (result != 1) {
        TBM_flush_table(table);
        return result - 10;
    }

    TBM_invoke_modules(table, data, COLUMN_MODULE_PRELOAD);
    if (THR_require_lock(&table->lock, omp_get_thread_num()) == 1) {
        result = TBM_insert_content(table, _get_global_offset(table->row_size, row), data, data_size);
        THR_release_lock(&table->lock, omp_get_thread_num());
    }

    TBM_flush_table(table);
    return result;
#endif
    return 1;
}

int DB_delete_row(database_t* __restrict database, char* __restrict table_name, int row, unsigned char access) {
#ifndef NO_DELETE_COMMAND
    table_t* table = _get_table_access(database, table_name, access, check_delete_access);
    if (table == NULL) return -1;

    int result = -1;
    if (THR_require_lock(&table->lock, omp_get_thread_num()) == 1) {
        result = TBM_delete_content(table, _get_global_offset(table->row_size, row), table->row_size);
        THR_release_lock(&table->lock, omp_get_thread_num());
    }

    table->header->row_count = MAX(table->header->row_count - 1, 0);
    TBM_flush_table(table);
    return result;
#endif
    return 1;
}

#pragma endregion

int DB_cleanup_tables(database_t* database) {
#ifndef NO_DELETE_COMMAND
    #pragma omp parallel for schedule(dynamic, 1)
    for (int i = 0; i < database->header->table_count; i++) {
        table_t* table = DB_get_table(database, database->table_names[i]);
        if (!table) continue;
        TBM_cleanup_dirs(table);
        TBM_flush_table(table);
    }
#endif
    return 1;
}

int DB_find_data_row(
    database_t* __restrict database, char* __restrict table_name, 
    char* __restrict column, int offset, unsigned char* __restrict data, size_t data_size, unsigned char access
) {
    table_t* table = _get_table_access(database, table_name, access, check_read_access);
    if (table == NULL) return -1;

    table_columns_info_t col_info;
    TBM_get_column_info(table, column, &col_info);

    int answer = -1;
    if (THR_require_lock(&table->lock, omp_get_thread_num()) == 1) {
        while (1) {
            int global_offset = TBM_find_content(table, offset, data, data_size);
            TBM_flush_table(table);
            if (global_offset < 0) break;

            int row = global_offset / table->row_size;
            if (col_info.offset == -1 && col_info.size == -1) {
                answer = row;
                break;
            }

            int position_in_row = global_offset % table->row_size;
            if (position_in_row >= col_info.offset && position_in_row < col_info.offset + col_info.size) {
                answer = row;
                break;
            }
            
            offset = global_offset + data_size;
        }

        THR_release_lock(&table->lock, omp_get_thread_num());
    }

    return answer;
}

table_t* DB_get_table(database_t* __restrict database, char* __restrict table_name) {
    if (database == NULL) return NULL;
    if (table_name == NULL) return NULL;

    table_t* table = NULL;
    #pragma omp parallel for schedule(dynamic, 1)
    for (int i = 0; i < database->header->table_count; i++) {
        if (table) continue;
        if (strncmp(database->table_names[i], table_name, TABLE_NAME_SIZE) == 0) {
            table = TBM_load_table(table_name);
        }
    }

    // If table not in database, we return NULL
    if (table == NULL) print_warn("Table [%s] not in [%.*s] database!", table_name, DATABASE_NAME_SIZE, database->header->name);
    return table;
}

int DB_delete_table(database_t* __restrict database, char* __restrict table_name, int full) {
#ifndef NO_DELETE_COMMAND
    table_t* table = DB_get_table(database, table_name);
    if (table == NULL) return -1;

    _unlink_table_from_database(database, table_name);
    return TBM_delete_table(table, full);
#endif
    return 1;
}

int DB_link_table2database(database_t* __restrict database, table_t* __restrict table) {
    if (database->header->table_count + 1 >= TABLES_PER_DATABASE) return -1;

    #pragma omp critical (link_table2database)
    strncpy(database->table_names[database->header->table_count++], table->header->name, TABLE_NAME_SIZE);
    return 1;
}
