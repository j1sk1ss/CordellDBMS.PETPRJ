#include "../../include/tabman.h"


int TBM_link_column2column(table_t* __restrict master, char* master_column_name, table_t* __restrict slave, char* slave_column_name, uint8_t type) {
    master->column_links = (table_column_link_t**)realloc(master->column_links, (master->header->column_link_count + 1) * sizeof(table_column_link_t*));
    master->column_links[master->header->column_link_count] = (table_column_link_t*)malloc(sizeof(table_column_link_t));
    strncpy(
        master->column_links[master->header->column_link_count]->master_column_name,
        master_column_name, COLUMN_NAME_SIZE
    );

    strncpy(
        master->column_links[master->header->column_link_count]->slave_table_name,
        slave->header->name, TABLE_NAME_SIZE
    );

    strncpy(
        master->column_links[master->header->column_link_count]->slave_column_name,
        slave_column_name, COLUMN_NAME_SIZE
    );

    master->column_links[master->header->column_link_count]->type = type;
    master->header->column_link_count++;
    return 1;
}

int TBM_unlink_column_from_column(table_t* __restrict master, char* master_column_name, table_t* __restrict slave, char* slave_column_name) {
    for (int i = 0; i < master->header->column_link_count; i++) {
        if (
            strncmp(master->column_links[i]->master_column_name, master_column_name, COLUMN_NAME_SIZE) == 0 &&
            strncmp(master->column_links[i]->slave_column_name, slave_column_name, COLUMN_NAME_SIZE) == 0 &&
            strncmp(master->column_links[i]->slave_table_name, slave->header->name, TABLE_NAME_SIZE) == 0
        ) {
            free(master->column_links[i]);
            for (int j = i; j < master->header->column_link_count - 1; j++)
                master->column_links[j] = master->column_links[j + 1];

            table_column_link_t** new_links = (table_column_link_t**)realloc(
                master->column_links, (master->header->column_link_count - 1) * sizeof(table_column_link_t*)
            );

            if (new_links || master->header->column_link_count - 1 == 0) master->column_links = new_links;
            else return -1;

            master->header->column_link_count--;
            return 1;
        }
    }

    return 0;
}

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

table_column_t* TBM_create_column(uint8_t type, uint8_t size, char* name) {
    table_column_t* column = (table_column_t*)malloc(sizeof(table_column_t));
    memset(column, 0, sizeof(table_column_t));

    column->magic = COLUMN_MAGIC;
    memcpy(column->name, name, COLUMN_NAME_SIZE);
    column->type = type;
    column->size = size;

    return column;
}

int TBM_check_signature(table_t* __restrict table, uint8_t* __restrict data) {
    uint8_t* data_pointer = data;
    for (int i = 0; i < table->header->column_count; i++) {
        char value[COLUMN_MAX_SIZE] = { '\0' };
        memcpy(value, data_pointer, table->columns[i]->size);
        data_pointer += table->columns[i]->size;

        uint8_t data_type = GET_COLUMN_DATA_TYPE(table->columns[i]->type);
        switch (data_type) {
            case COLUMN_TYPE_INT:
                if (!is_integer(value)) return -2;
                break;
            case COLUMN_TYPE_STRING: break;
            case COLUMN_TYPE_ANY: break;
            case COLUMN_TYPE_MODULE: break;
            default: return -4;
        }
    }

    return 1;
}
