#include "../include/tabman.h"


#pragma region [Directory]

    uint8_t* TBM_get_content(table_t* table, int offset, size_t size) {
        return NULL; // TODO
    }

    int TBM_append_content(table_t* table, uint8_t* data, size_t data_size) {
        return 1; // TODO
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

    table_column_t* TBM_create_column(uint8_t type, uint8_t size, char* name) {
        table_column_t* column = (table_column_t*)malloc(sizeof(table_column_t));

        column->magic = COLUMN_MAGIC;
        strncpy((char*)column->name, name, COLUMN_NAME_SIZE);
        column->type = type;
        column->size = size;

        return column;
    }

    int TBM_free_column(table_column_t* column) {
        SOFT_FREE(column);
        return 1;
    }

#pragma endregion

#pragma region [Table]

    int TBM_check_signature(table_t* table, uint8_t* data) {
        for (int i = 0; i < table->header->column_count; i++) {
            char value[COLUMN_MAX_SIZE];
            memcpy(value, data, table->columns[i]->size);
            data += table->columns[i]->size;

            switch (table->columns[i]->type) {
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
        table->dir_names = (uint8_t**)realloc(table->dir_names, ++table->header->dir_count);
        table->dir_names[table->header->dir_count] = (uint8_t*)malloc(DIRECTORY_NAME_SIZE);
        memcpy(table->dir_names[table->header->dir_count], directory->header->name, DIRECTORY_NAME_SIZE);
        return 1;
    }

    table_t* TBM_create_table(char* name, table_column_t** columns, int col_count) {
        table_header_t* header = (table_header_t*)malloc(sizeof(table_header_t));

        header->magic = TABLE_MAGIC;
        header->dir_count = 0;
        header->column_count = col_count;
        strncpy((char*)header->name, name, TABLE_NAME_SIZE);

        table_t* table = (table_t*)malloc(sizeof(table_t));

        table->header = header;
        table->columns = columns;

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

        // Write columns
        for (int i = 0; i < table->header->column_count; i++) 
            fwrite(table->columns[i], sizeof(table_column_t), SEEK_CUR, file);

        for (int i = 0; i < table->header->dir_count; i++) 
            fwrite(table->dir_names[i], DIRECTORY_NAME_SIZE, SEEK_CUR, file);

        // Close file
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

        table_header_t* header = (table_header_t*)malloc(sizeof(table_header_t));
        fread(header, sizeof(table_header_t), SEEK_CUR, file);
        if (header->magic != TABLE_MAGIC) {
            free(header);
            return NULL;
        }
        
        table_t* table = (table_t*)malloc(sizeof(table_t));
        table->columns = (table_column_t**)malloc(header->column_count);
        for (int i = 0; i < header->column_count; i++) {
            table_column_t* column = (table_column_t*)malloc(sizeof(table_column_t));
            fread(column, sizeof(table_column_t), SEEK_CUR, file);
            table->columns[i] = column;
        }

        table->dir_names = (uint8_t**)malloc(header->dir_count);
        for (int i = 0; i < header->dir_count; i++) {
            uint8_t* name = (uint8_t*)malloc(DIRECTORY_NAME_SIZE);
            fread(name, DIRECTORY_NAME_SIZE, SEEK_CUR, file);
            table->dir_names[i] = name;
        }

        fclose(file);
        table->header = header;
        return table;
    }

    int TBM_free_table(table_t* table) {
        if (table == NULL) return -1;
        for (int i = 0; i < table->header->dir_count; i++) 
            SOFT_FREE(table->dir_names[i]);

        for (int i = 0; i < table->header->column_count; i++) 
            TBM_free_column(table->columns[i]);

        SOFT_FREE(table->dir_names);
        SOFT_FREE(table->columns);
        SOFT_FREE(table);

        return 1;
    }

#pragma endregion
