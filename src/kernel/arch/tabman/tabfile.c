#include "../../include/tabman.h"


table_t* TBM_create_table(char* __restrict name, table_column_t** __restrict columns, int col_count, unsigned char access) {
#ifndef NO_CREATE_COMMAND
    int row_size = 0;
    for (int i = 0; i < col_count; i++)
        row_size += columns[i]->size;

    // If future row size is larger, then page content size
    // we return NULL. We don't want to make deal with row, larger
    // then page size, because that will brake all DB structure.
    if (row_size >= PAGE_CONTENT_SIZE) return NULL;

    table_t* table  = (table_t*)malloc(sizeof(table_t));
    table_header_t* header = (table_header_t*)malloc(sizeof(table_header_t));

    memset(table, 0, sizeof(table_t));
    memset(header, 0, sizeof(table_header_t));

    header->access = access;
    header->magic  = TABLE_MAGIC;
    strncpy(header->name, name, TABLE_NAME_SIZE);
    header->column_count = col_count;

    table->columns  = columns;
    table->row_size = row_size;
    
    table->lock   = THR_create_lock();
    table->header = header;
    return table;
#endif
    return NULL;
}

int TBM_save_table(table_t* __restrict table, char* __restrict path) {
    int status = -1;
    #pragma omp critical (table_save)
    {
        #ifndef NO_TABLE_SAVE_OPTIMIZATION
        if (TBM_get_checksum(table) != table->header->checksum)
        #endif
        {
            // We generate default path
            char save_path[DEFAULT_PATH_SIZE] = { 0 };
            if (path == NULL) sprintf(save_path, "%s%.*s.%s", TABLE_BASE_PATH, TABLE_NAME_SIZE, table->header->name, TABLE_EXTENSION);
            else strcpy(save_path, path);

            // Open or create file
            FILE* file = fopen(save_path, "wb");
            if (file == NULL) print_error("Can't save or create table [%s] file", save_path);
            else {
                // Write header
                status = 1;
                table->header->checksum = TBM_get_checksum(table);
                if (fwrite(table->header, sizeof(table_header_t), 1, file) != 1) status = -2;

                // Write table data to open file
                for (int i = 0; i < table->header->column_count; i++)
                    if (fwrite(table->columns[i], sizeof(table_column_t), 1, file) != 1) {
                        status = -3;
                    }

                for (int i = 0; i < table->header->dir_count; i++)
                    if (fwrite(table->dir_names[i], DIRECTORY_NAME_SIZE, 1, file) != 1) {
                        status = -5;
                    }

                // Close file and clear buffers
                #ifndef _WIN32
                fsync(fileno(file));
                #else
                fflush(file);
                #endif

                fclose(file);
            }
        }
    }

    return status;
}

table_t* TBM_load_table(char* name) {
    char load_path[DEFAULT_PATH_SIZE] = { 0 };
    if (get_load_path(name, TABLE_NAME_SIZE, load_path, TABLE_BASE_PATH, TABLE_EXTENSION) == -1) {
        print_error("Name should be provided!");
        return NULL;
    }

    // If path is not NULL, we use function for getting file name
    table_t* loaded_table = (table_t*)CHC_find_entry(name, TABLE_CACHE);
    if (loaded_table != NULL) {
        print_debug("Loading table [%s] from GCT", load_path);
        return loaded_table;
    }

    #pragma omp critical (table_load)
    {
        FILE* file = fopen(load_path, "rb");
        print_debug("Loading table [%s] from disk", load_path);
        if (file == NULL) print_error("Can't open table [%s]", load_path);
        else {
            // Read header of table from file.
            // Note: If magic is wrong, we can say, that this file isn`t table.
            //       We just return error code.
            table_header_t* header = (table_header_t*)malloc(sizeof(table_header_t));
            fread(header, sizeof(table_header_t), 1, file);
            if (header->magic != TABLE_MAGIC) {
                print_error("Table file wrong magic for [%s]", load_path);
                free(header);
                fclose(file);
            } else {
                // Read columns from file.
                table_t* table = (table_t*)malloc(sizeof(table_t));
                table_column_t** columns = (table_column_t**)malloc(header->column_count * sizeof(table_column_t*));

                memset(table, 0, sizeof(table_t));
                memset(columns, 0, header->column_count * sizeof(table_column_t*));

                for (int i = 0; i < header->column_count; i++) {
                    columns[i] = (table_column_t*)malloc(sizeof(table_column_t));
                    memset(columns[i], 0, sizeof(table_column_t));
                    fread(columns[i], sizeof(table_column_t), 1, file);
                }

                for (int i = 0; i < header->column_count; i++)
                    table->row_size += columns[i]->size;

                // Read directory names from file, that linked to this directory.
                for (int i = 0; i < header->dir_count; i++)
                    fread(table->dir_names[i], sizeof(unsigned char), DIRECTORY_NAME_SIZE, file);

                fclose(file);

                table->columns = columns;
                table->lock    = THR_create_lock();

                table->header = header;
                CHC_add_entry(table, table->header->name, TABLE_CACHE, TBM_free_table, TBM_save_table);
                loaded_table = table;
            }
        }
    }

    return loaded_table;
}

int TBM_delete_table(table_t* table, int full) {
#ifndef NO_DELETE_COMMAND
    if (table == NULL) return -1;
    if (THR_require_lock(&table->lock, omp_get_thread_num()) == 1) {
        if (full) {
            #pragma omp parallel for schedule(dynamic, 1)
            for (int i = 0; i < table->header->dir_count; i++) {
                directory_t* directory = DRM_load_directory(table->dir_names[i]);
                if (directory == NULL) continue;

                TBM_unlink_dir_from_table(table, table->dir_names[i]);
                DRM_delete_directory(directory, full);
            }
        }

        // Delete table from disk by provided, generated path
        char delete_path[DEFAULT_PATH_SIZE] = { 0 };
        get_load_path(table->header->name, TABLE_NAME_SIZE, delete_path, TABLE_BASE_PATH, TABLE_EXTENSION);
        remove(delete_path);

        CHC_flush_entry(table, TABLE_CACHE);
        return 1;
    }
    
#endif
    return -1;
}

int TBM_flush_table(table_t* table) {
    if (table == NULL) return -2;
    if (table->is_cached == 1) return -1;

    TBM_save_table(table, NULL);
    return TBM_free_table(table);
}

int TBM_free_table(table_t* table) {
    if (table == NULL) return -1;
    for (int i = 0; i < table->header->column_count; i++) SOFT_FREE(table->columns[i]);

    SOFT_FREE(table->columns);
    SOFT_FREE(table);

    return 1;
}

unsigned int TBM_get_checksum(table_t* table) {
    unsigned int prev_checksum = table->header->checksum;
    table->header->checksum = 0;

    unsigned int checksum = 0;
    if (table->header != NULL) checksum = crc32(checksum, (const unsigned char*)table->header, sizeof(table_header_t));
    if (table->columns != NULL) {
        for (unsigned short i = 0; i < table->header->column_count; i++) {
            if (table->columns[i] != NULL) checksum = crc32(checksum, (const unsigned char*)table->columns[i], sizeof(table_column_t));
        }
    }

    table->header->checksum = prev_checksum;
    checksum = crc32(checksum, (const unsigned char*)table->dir_names, sizeof(table->dir_names));
    return checksum;
}
