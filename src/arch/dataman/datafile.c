#include <dataman.h>

database_t* DB_create_database(char* name) {
    database_t* database = (database_t*)malloc_s(sizeof(database_t));
    database_header_t* header = (database_header_t*)malloc_s(sizeof(database_header_t));
    if (!database || !header) {
        SOFT_FREE(database);
        SOFT_FREE(header);
        return NULL;
    }

    str_memset(database, 0, sizeof(database_t));
    str_memset(header, 0, sizeof(database_header_t));

    header->magic = DATABASE_MAGIC;
    if (name) str_strncpy(header->name, name, DATABASE_NAME_SIZE);
    database->header = header;
    return database;
}

int DB_delete_database(database_t* database, int full) {
#ifndef NO_DELETE_COMMAND
    if (!database) return -1;

    int result = 1;
    if (full) {
        #pragma omp parallel for schedule(dynamic, 1) shared(result)
        for (int i = 0; i < database->header->table_count; i++) {
            table_t* table = DB_get_table(database, database->table_names[i]);
            if (table == NULL) continue;
            result = MIN(TBM_delete_table(table, full), result);
        }
    }

    if (result != 1) return result;
    delete_file(database->header->name, DATABASE_BASE_PATH, DATABASE_EXTENSION);
    DB_free_database(database);
#endif
    return 1;
}

int DB_free_database(database_t* database) {
    if (!database) return -1;
    SOFT_FREE(database->header);
    SOFT_FREE(database);
    return 1;
}

int DB_save_database(database_t* database) {
    int status = -1;
    char save_path[DEFAULT_PATH_SIZE] = { 0 };
    get_load_path(database->header->name, DATABASE_NAME_SIZE, save_path, DATABASE_BASE_PATH, DATABASE_EXTENSION);

    ci_t ci = NIFAT32_open_content(NO_RCI, save_path, MODE(CR_MODE, FILE_TARGET));
    if (ci < 0) { print_error("Can`t create or open file: [%s]", save_path); }
    else {
        status = 1;
        decoded_t encoded_header[sizeof(database_header_t)] = { 0 };
        pack_memory((byte_t*)database->header, (decoded_t*)encoded_header, sizeof(database_header_t));
        if (status == 1 && NIFAT32_write_buffer2content(
            ci, 0, (const_buffer_t)encoded_header, sizeof(database_header_t) * sizeof(decoded_t)) != sizeof(database_header_t)
        ) status = -2;

        for (int i = 0; i < database->header->table_count; i++) {
            decoded_t encoded_table_name[TABLE_NAME_SIZE] = { 0 };
            pack_memory((byte_t*)database->table_names[i], (decoded_t*)encoded_table_name, TABLE_NAME_SIZE);
            if (status == 1 && NIFAT32_write_buffer2content(
                ci, (sizeof(database_header_t)  * sizeof(decoded_t)) + TABLE_NAME_SIZE * i, 
                (const_buffer_t)encoded_table_name, TABLE_NAME_SIZE * sizeof(decoded_t)
            ) != TABLE_NAME_SIZE) status = -3;
        }

        NIFAT32_close_content(ci);
    }

    return status;
}

database_t* DB_load_database(char* name) {
    char load_path[DEFAULT_PATH_SIZE] = { 0 };
    get_load_path(name, DATABASE_NAME_SIZE, load_path, DATABASE_BASE_PATH, DATABASE_EXTENSION);

    database_t* loaded_database = NULL;
    ci_t ci = NIFAT32_open_content(NO_RCI, load_path, DF_MODE);
    print_io("Loading database [%s]", load_path);
    if (ci < 0) { print_error("Database file not found! [%s]", load_path); }
    else {
        loaded_database = DB_create_database(NULL);
        if (loaded_database) {
            encoded_t encoded_header[sizeof(database_header_t)] = { 0 };
            NIFAT32_read_content2buffer(ci, 0, (buffer_t)encoded_header, sizeof(database_header_t) * sizeof(encoded_t));
            unpack_memory((encoded_t*)encoded_header, (byte_t*)loaded_database->header, sizeof(database_header_t));
            if (loaded_database->header->magic != DATABASE_MAGIC) {
                print_error("Database file wrong magic for [%s]", load_path);
                DB_free_database(loaded_database);
            } 
            else {
                for (int i = 0; i < loaded_database->header->table_count; i++) {
                    encoded_t encoded_table_name[TABLE_NAME_SIZE] = { 0 };
                    NIFAT32_read_content2buffer(
                        ci, (sizeof(database_header_t)  * sizeof(encoded_t)) + TABLE_NAME_SIZE * i, 
                        (buffer_t)encoded_table_name, TABLE_NAME_SIZE * sizeof(encoded_t)
                    );

                    unpack_memory((encoded_t*)encoded_table_name, (byte_t*)loaded_database->table_names[i], TABLE_NAME_SIZE);
                }
            }
        }

        NIFAT32_close_content(ci);
    }

    return loaded_database;
}
