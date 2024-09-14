#include "../include/tabman.h"


#pragma region [Directory]

int TBM_append_content(table_t* table, uint8_t* data, size_t data_size) {
    return 1;
}

int TBM_delete_content(table_t* table, int offset, size_t size) {
    return 1;
}

int TBM_find_content(table_t* table, int offset, uint8_t value) {
    return 1;
}

#pragma endregion

#pragma region [Table]

int TBM_link_dir2table(table_t* table, directory_t* directory) {
    return 1;
}

table_t* TBM_create_table(char* name, table_column_t* columns[], int col_count) {
    return NULL;
}

int TBM_save_table(table_t* table, char* path) {
    return 1;
}

table_t* TBM_load_table(char* name) {
    return NULL;
}

int TBM_free_table(table_t* table) {
    return 1;
}

#pragma endregion
