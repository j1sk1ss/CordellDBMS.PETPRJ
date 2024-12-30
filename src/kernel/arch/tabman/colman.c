#include "../../include/tabman.h"


table_column_t* TBM_create_column(unsigned char type, unsigned short size, char* name) {
#ifndef NO_CREATE_COMMAND
    if (size > COLUMN_MAX_SIZE) return NULL;
    table_column_t* column = (table_column_t*)malloc(sizeof(table_column_t));
    if (!column) return NULL;
    memset(column, 0, sizeof(table_column_t));

    column->magic = COLUMN_MAGIC;
    strncpy(column->name, name, COLUMN_NAME_SIZE);
    column->type = type;
    column->size = size;

    return column;
#endif
    return NULL;
}

int TBM_get_column_offset(table_t* table, char* column_name) {
    int offset = 0;
    for (int i = 0; i < table->header->column_count; i++) {
        if (strcmp(table->columns[i]->name, column_name) == 0) return offset;
        else offset += table->columns[i]->size;
    }

    return 0;
}

int TBM_check_signature(table_t* __restrict table, unsigned char* __restrict data) {
    unsigned char* data_pointer = data;
    for (int i = 0; i < table->header->column_count; i++) {
        char value[COLUMN_MAX_SIZE] = { 0 };
        strncpy(value, (char*)data_pointer, table->columns[i]->size);
        data_pointer += table->columns[i]->size;

        unsigned char data_type = GET_COLUMN_DATA_TYPE(table->columns[i]->type);
        switch (data_type) {
            case COLUMN_TYPE_INT:
                if (!is_integer(value)) return -2;
                break;

            case COLUMN_TYPE_STRING: 
            case COLUMN_TYPE_ANY: 
            case COLUMN_TYPE_MODULE: break;
            default: return -4;
        }
    }

    return 1;
}
