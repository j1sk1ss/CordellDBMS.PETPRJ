#include "../../include/dataman.h"


database_t* DB_create_database(char* name) {
    database_t* database = (database_t*)malloc_s(sizeof(database_t));
    database_header_t* header = (database_header_t*)malloc_s(sizeof(database_header_t));
    if (!database || !header) {
        SOFT_FREE(database);
        SOFT_FREE(header);
        return NULL;
    }

    memset_s(database, 0, sizeof(database_t));
    memset_s(header, 0, sizeof(database_header_t));

    header->magic = DATABASE_MAGIC;
    if (name != NULL) strncpy_s(header->name, name, DATABASE_NAME_SIZE);

    database->header = header;
    return database;
}

int DB_delete_database(database_t* database, int full) {
#ifndef NO_DELETE_COMMAND
    if (database == NULL) return -1;

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
    if (database == NULL) return -1;
    SOFT_FREE(database->header);
    SOFT_FREE(database);
    return 1;
}

int DB_save_database(database_t* database) {
    int status = -1;
    #pragma omp critical (save_database)
    {
        // We generate default path
        char save_path[DEFAULT_PATH_SIZE] = { 0 };
        get_load_path(database->header->name, DATABASE_NAME_SIZE, save_path, DATABASE_BASE_PATH, DATABASE_EXTENSION);

        int fd = open(save_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) { print_error("Can`t create or open file: [%s]", save_path); }
        else {
            status = 1;
            unsigned short encoded_header[sizeof(database_header_t)] = { 0 };
            pack_memory((unsigned char*)database->header, (unsigned short*)encoded_header, sizeof(database_header_t));
            if (pwrite(fd, encoded_header, sizeof(database_header_t) * sizeof(unsigned short), 0) != sizeof(database_header_t)) status = -2;
            for (int i = 0; i < database->header->table_count; i++) {
                unsigned short encoded_table_name[TABLE_NAME_SIZE] = { 0 };
                pack_memory((unsigned char*)database->table_names[i], (unsigned short*)encoded_table_name, TABLE_NAME_SIZE);
                if (pwrite(fd, encoded_table_name, TABLE_NAME_SIZE * sizeof(unsigned short), (sizeof(database_header_t)  * sizeof(unsigned short)) + TABLE_NAME_SIZE * i) != TABLE_NAME_SIZE) {
                    status = -3;
                }
            }

            fsync(fd);
            close(fd);
        }
    }

    return status;
}

database_t* DB_load_database(char* name) {
    char load_path[DEFAULT_PATH_SIZE] = { 0 };
    get_load_path(name, DATABASE_NAME_SIZE, load_path, DATABASE_BASE_PATH, DATABASE_EXTENSION);

    database_t* loaded_database = NULL;
    #pragma omp critical (load_database)
    {
        int fd = open(load_path, O_RDONLY);
        print_io("Loading database [%s]", load_path);
        if (fd < 0) { print_error("Database file not found! [%s]", load_path); }
        else {
            loaded_database = DB_create_database(NULL);
            if (loaded_database) {
                unsigned short encoded_header[sizeof(database_header_t)] = { 0 };
                pread(fd, encoded_header, sizeof(database_header_t) * sizeof(unsigned short), 0);
                unpack_memory((unsigned short*)encoded_header, (unsigned char*)loaded_database->header, sizeof(database_header_t));
                if (loaded_database->header->magic != DATABASE_MAGIC) {
                    print_error("Database file wrong magic for [%s]", load_path);
                    DB_free_database(loaded_database);
                } else {
                    for (int i = 0; i < loaded_database->header->table_count; i++) {
                        unsigned short encoded_table_name[TABLE_NAME_SIZE] = { 0 };
                        pread(fd, encoded_table_name, TABLE_NAME_SIZE * sizeof(unsigned short), (sizeof(database_header_t)  * sizeof(unsigned short)) + TABLE_NAME_SIZE * i);
                        unpack_memory((unsigned short*)encoded_table_name, (unsigned char*)loaded_database->table_names[i], TABLE_NAME_SIZE);
                    }
                }
            }

            close(fd);
        }
    }

    return loaded_database;
}
