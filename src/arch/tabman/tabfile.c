#include <tabman.h>

table_t* TBM_create_table(char* __restrict name, table_column_t** __restrict columns, int col_count) {
#ifndef NO_CREATE_COMMAND
    int row_size = 0;
    for (int i = 0; i < col_count; i++) {
        row_size += columns[i]->size;
    }

    if (row_size >= PAGE_CONTENT_SIZE) return NULL;
    table_t* table = (table_t*)malloc_s(sizeof(table_t));
    table_header_t* header = (table_header_t*)malloc_s(sizeof(table_header_t));
    if (!table || !header) {
        SOFT_FREE(table);
        SOFT_FREE(header);
        return NULL;
    }

    str_memset(table, 0, sizeof(table_t));
    str_memset(header, 0, sizeof(table_header_t));

    header->magic  = TABLE_MAGIC;
    str_strncpy(header->name, name, TABLE_NAME_SIZE);
    header->column_count = col_count;

    table->columns  = columns;
    table->row_size = row_size;
    
    table->lock = NULL_LOCK;
    table->header = header;
    return table;
#endif
    return NULL;
}

int TBM_save_table(table_t* table) {
    int status = -1;
    unsigned int table_checksum = 0;
    #ifndef NO_TABLE_SAVE_OPTIMIZATION
    table_checksum = TBM_get_checksum(table);
    if (table_checksum != table->header->checksum)
    #endif
    {
        // We generate default path
        char save_path[DEFAULT_PATH_SIZE] = { 0 };
        get_load_path(table->header->name, TABLE_NAME_SIZE, save_path, TABLE_BASE_PATH, TABLE_EXTENSION);

        // Open or create file
        ci_t ci = NIFAT32_open_content(NO_RCI, save_path, MODE(CR_MODE, FILE_TARGET));
        if (ci < 0) { print_error("Can't save or create table [%s] file", save_path); }
        else {
            int offset = 0;
            status = 1;
            table->header->checksum = TBM_get_checksum(table);

            decoded_t encoded_header[sizeof(table_header_t)] = { 0 };
            pack_memory((byte_t*)table->header, (decoded_t*)encoded_header, sizeof(table_header_t));
            if (status == 1 && NIFAT32_write_buffer2content(
                ci, offset, (const_buffer_t)encoded_header, sizeof(table_header_t) * sizeof(decoded_t)
            ) != sizeof(table_header_t) * sizeof(decoded_t)) status = -2;
            offset += sizeof(table_header_t) * sizeof(decoded_t);
            
            for (int i = 0; i < table->header->column_count; i++) {
                decoded_t encoded_column[sizeof(table_column_t)] = { 0 };
                pack_memory((byte_t*)table->columns[i], (decoded_t*)encoded_column, sizeof(table_column_t));
                if (status == 1 && NIFAT32_write_buffer2content(
                    ci, offset, (const_buffer_t)encoded_column, sizeof(table_column_t) * sizeof(decoded_t)
                ) != sizeof(table_column_t) * sizeof(decoded_t)) {
                    status = -3;
                }

                offset += sizeof(table_column_t) * sizeof(decoded_t);
            }

            for (int i = 0; i < table->header->dir_count; i++) {
                decoded_t encoded_directory_name[sizeof(table_header_t)] = { 0 };
                pack_memory((byte_t*)table->dir_names[i], (decoded_t*)encoded_directory_name, DIRECTORY_NAME_SIZE);
                if (status == 1 && NIFAT32_write_buffer2content(
                    ci, offset, (const_buffer_t)encoded_directory_name, DIRECTORY_NAME_SIZE * sizeof(decoded_t)
                ) != DIRECTORY_NAME_SIZE * sizeof(decoded_t)) status = -5;
                offset += DIRECTORY_NAME_SIZE * sizeof(decoded_t);
            }

            NIFAT32_close_content(ci);
        }
    }

    return status;
}

table_t* TBM_load_table(char* name) {
    char load_path[DEFAULT_PATH_SIZE] = { 0 };
    get_load_path(name, TABLE_NAME_SIZE, load_path, TABLE_BASE_PATH, TABLE_EXTENSION);

    // If path is not NULL, we use function for getting file name
    table_t* loaded_table = (table_t*)CHC_find_entry(name, TABLE_BASE_PATH, TABLE_CACHE);
    if (loaded_table != NULL) {
        print_io("Loading table [%s] from GCT", load_path);
        return loaded_table;
    }

    int table_load_break = 0;
    ci_t ci = NIFAT32_open_content(NO_RCI, load_path, DF_MODE);
    print_io("Loading table [%s] from disk", load_path);
    if (ci < 0) { print_error("Can't open table [%s]", load_path); }
    else {
        // Read header of table from file.
        // Note: If magic is wrong, we can say, that this file isn`t table.
        //       We just return error code.
        table_header_t* header = (table_header_t*)malloc_s(sizeof(table_header_t));
        if (header) {
            int offset = 0;
            decoded_t encoded_header[sizeof(table_header_t)] = { 0 };
            NIFAT32_read_content2buffer(ci, offset, (buffer_t)encoded_header, sizeof(table_header_t) * sizeof(decoded_t));
            unpack_memory((decoded_t*)encoded_header, (unsigned char*)header, sizeof(table_header_t));
            offset += sizeof(table_header_t) * sizeof(decoded_t);
            if (header->magic != TABLE_MAGIC) {
                print_error("Table file wrong magic for [%s]", load_path);
                SOFT_FREE(header);
                NIFAT32_close_content(ci);
            } 
            else {
                // Read columns from file.
                table_t* table = (table_t*)malloc_s(sizeof(table_t));
                table_column_t** columns = (table_column_t**)malloc_s(header->column_count * sizeof(table_column_t*));
                if (!table || !columns) {
                    SOFT_FREE(header);
                    SOFT_FREE(table);
                    ARRAY_SOFT_FREE(columns, header->column_count);
                } 
                else {
                    str_memset(table, 0, sizeof(table_t));
                    str_memset(columns, 0, header->column_count * sizeof(table_column_t*));
                    for (int i = 0; i < header->column_count; i++) {
                        columns[i] = (table_column_t*)malloc_s(sizeof(table_column_t));
                        if (!columns[i]) { 
                            table_load_break = 1;                               
                            continue;
                        }

                        str_memset(columns[i], 0, sizeof(table_column_t));
                        encoded_t encoded_column[sizeof(table_column_t)] = { 0 };
                        NIFAT32_read_content2buffer(ci, offset, (buffer_t)encoded_column, sizeof(table_column_t) * sizeof(encoded_t));
                        unpack_memory((encoded_t*)encoded_header, (byte_t*)columns[i], sizeof(table_column_t));
                        offset += sizeof(table_column_t) * sizeof(encoded_t);
                    }

                    for (int i = 0; i < header->column_count; i++) {
                        table->row_size += columns[i]->size;
                    }

                    // Read directory names from file, that linked to this directory.
                    for (int i = 0; i < header->dir_count; i++) {
                        encoded_t encoded_directory_name[DIRECTORY_NAME_SIZE] = { 0 };
                        NIFAT32_read_content2buffer(ci, offset, (buffer_t)encoded_directory_name, DIRECTORY_NAME_SIZE * sizeof(encoded_t));
                        unpack_memory((encoded_t*)encoded_directory_name, (byte_t*)table->dir_names[i], DIRECTORY_NAME_SIZE);
                        offset += DIRECTORY_NAME_SIZE * sizeof(encoded_t);
                    }

                    NIFAT32_close_content(ci);

                    table->columns = columns;
                    table->lock = NULL_LOCK;
                    table->header = header;
                    CHC_add_entry(
                        table, table->header->name, TABLE_BASE_PATH, TABLE_CACHE, 
                        (void*)TBM_free_table, (void*)TBM_save_table
                    );

                    loaded_table = table;
                }
            }
        }
    }

    if (table_load_break) {
        SOFT_FREE(loaded_table->header);
        ARRAY_SOFT_FREE(loaded_table->columns, loaded_table->header->column_count);
        SOFT_FREE(loaded_table);
        return NULL;
    }

    return loaded_table;
}

int TBM_delete_table(table_t* table, int full) {
#ifndef NO_DELETE_COMMAND
    if (!table) return -1;
    if (THR_require_write(&table->lock, get_thread_num())) {
        if (full) {
            for (int i = 0; i < table->header->dir_count; i++) {
                directory_t* directory = DRM_load_directory(table->dir_names[i]);
                if (!directory) continue;
                DRM_delete_directory(directory, full);
            }
        }

        // Delete table from disk by provided, generated path
        delete_file(table->header->name, TABLE_BASE_PATH, TABLE_EXTENSION);
        if (CHC_flush_entry(table, TABLE_CACHE) == -2) TBM_flush_table(table);
        return 1;
    }
    
#endif
    return -1;
}

int TBM_flush_table(table_t* table) {
    if (!table) return -2;
    if (table->is_cached == 1) return -1;
    TBM_save_table(table);
    return TBM_free_table(table);
}

int TBM_free_table(table_t* table) {
    if (!table) return -1;
    ARRAY_SOFT_FREE(table->columns, table->header->column_count);
    SOFT_FREE(table->header);
    SOFT_FREE(table);
    return 1;
}

unsigned int TBM_get_checksum(table_t* table) {
    if (!table) return 0;
    unsigned int prev_checksum = table->header->checksum;
    table->header->checksum = 0;

    unsigned int _checksum = 0;
    if (table->header) _checksum = murmur3_x86_32((const unsigned char*)table->header, sizeof(table_header_t), 0);
    if (table->columns) {
        for (unsigned short i = 0; i < table->header->column_count; i++) {
            if (table->columns[i]) _checksum = murmur3_x86_32((const unsigned char*)table->columns[i], sizeof(table_column_t), 0);
        }
    }

    table->header->checksum = prev_checksum;
    _checksum = murmur3_x86_32((const unsigned char*)table->dir_names, sizeof(table->dir_names), 0);
    return _checksum;
}
