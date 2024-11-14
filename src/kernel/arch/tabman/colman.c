#include "../../include/tabman.h"


int TBM_update_column_in_table(table_t* __restrict table, table_column_t* __restrict column, int by_index) {
    if (by_index == -1) {
        for (by_index = 0; by_index < table->header->column_count; by_index++) {
            if (strncmp(table->columns[by_index]->name, column->name, COLUMN_NAME_SIZE) == 0) {
                if (table->columns[by_index]->size != column->size && table->header->dir_count != 0) return -2;
                break;
            }
        }
    }

    SOFT_FREE(table->columns[by_index]);
    table->columns[by_index] = column;

    return 1;
}

table_column_t* TBM_create_column(uint8_t type, uint16_t size, char* name) {
    if (size > COLUMN_MAX_SIZE) return NULL;
    table_column_t* column = (table_column_t*)malloc(sizeof(table_column_t));
    memset(column, 0, sizeof(table_column_t));

    column->magic = COLUMN_MAGIC;
    strncpy(column->name, name, COLUMN_NAME_SIZE);
    column->type = type;
    column->size = size;

    return column;
}

int TBM_check_signature(table_t* __restrict table, uint8_t* __restrict data) {
    uint8_t* data_pointer = data;
    for (int i = 0; i < table->header->column_count; i++) {
        char value[table->columns[i]->size];
        strncpy(value, data_pointer, table->columns[i]->size);
        data_pointer += table->columns[i]->size;

        uint8_t data_type = GET_COLUMN_DATA_TYPE(table->columns[i]->type);
        switch (data_type) {
            case COLUMN_TYPE_STRING: 
            case COLUMN_TYPE_ANY: 
            case COLUMN_TYPE_MODULE: break;
            case COLUMN_TYPE_INT:
                if (!is_integer(value)) return -2;
                break;
            default: return -4;
        }
    }

    return 1;
}
