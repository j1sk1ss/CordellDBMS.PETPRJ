#include "../../include/tabman.h"


table_column_t* TBM_create_column(unsigned char type, unsigned short size, char* name) {
#ifndef NO_CREATE_COMMAND
    if (size > COLUMN_MAX_SIZE) return NULL;
    table_column_t* column = (table_column_t*)malloc(sizeof(table_column_t));
    if (!column) return NULL;
    memset_s(column, 0, sizeof(table_column_t));

    column->magic = COLUMN_MAGIC;
    strncpy_s(column->name, name, COLUMN_NAME_SIZE);
    column->type = type;
    column->size = size;

    return column;
#endif
    return NULL;
}

int TBM_get_column_info(table_t* table, char* column_name, table_columns_info_t* info) {
    info->offset = -1;
    info->size = -1;

    int offset = 0;
    for (int i = 0; i < table->header->column_count; i++) {
        if (strcmp_s(table->columns[i]->name, column_name)) offset += table->columns[i]->size;
        else {
            info->offset = offset;
            info->size = table->columns[i]->size;
            return 1;
        }
    }

    return -1;
}

int TBM_check_signature(table_t* __restrict table, unsigned char* __restrict data) {
#ifndef DISABLE_CHECK_SIGNATURE
    unsigned char* data_pointer = data;
    for (int i = 0; i < table->header->column_count; i++) {
        unsigned char data_type = GET_COLUMN_DATA_TYPE(table->columns[i]->type);
        if (data_type == COLUMN_TYPE_ANY || data_type == COLUMN_TYPE_MODULE) continue;

        char value[COLUMN_MAX_SIZE] = { 0 };
        strncpy_s(value, (char*)data_pointer, table->columns[i]->size);
        data_pointer += table->columns[i]->size;

        switch (data_type) {
            case COLUMN_TYPE_INT:
                if (!is_integer(value)) return -2;
                break;

            case COLUMN_TYPE_STRING: break;
            default: return -4;
        }
    }
#endif
    return 1;
}
