#include "../../include/dataman.h"


database_t* DB_create_database(char* name) {
    database_t* database = (database_t*)malloc(sizeof(database_t));
    database_header_t* header = (database_header_t*)malloc(sizeof(database_header_t));
    if (!database || !header) {
        SOFT_FREE(database);
        SOFT_FREE(header);
        return NULL;
    }

    memset(database, 0, sizeof(database_t));
    memset(header, 0, sizeof(database_header_t));

    header->magic = DATABASE_MAGIC;
    if (name != NULL) strncpy(header->name, name, DATABASE_NAME_SIZE);

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
        if (fd < 0) print_error("Can`t create or open file: [%s]", save_path);
        else {
            status = 1;
            if (pwrite(fd, database->header, sizeof(database_header_t), 0) != sizeof(database_header_t)) status = -2;
            for (int i = 0; i < database->header->table_count; i++)
                if (pwrite(fd, database->table_names[i], TABLE_NAME_SIZE, sizeof(database_header_t) + TABLE_NAME_SIZE * i) != 1) {
                    status = -3;
                }

            fsync(fd);
            close(fd);
        }
    }

    return status;
}

database_t* DB_load_database(char* name) {
    char load_path[DEFAULT_PATH_SIZE] = { 0 };
    if (get_load_path(name, DATABASE_NAME_SIZE, load_path, DATABASE_BASE_PATH, DATABASE_EXTENSION) == -1) {
        print_error("Name should be provided!");
        return NULL;
    }

    database_t* loaded_database = NULL;
    #pragma omp critical (load_database)
    {
        int fd = open(load_path, O_RDONLY);
        print_debug("Loading database [%s]", load_path);
        if (fd < 0) print_error("Database file not found! [%s]", load_path);
        else {
            loaded_database = DB_create_database(NULL);
            pread(fd, loaded_database->header, sizeof(database_header_t), 0);
            if (loaded_database->header->magic != DATABASE_MAGIC) {
                print_error("Database file wrong magic for [%s]", load_path);
                DB_free_database(loaded_database);
                close(fd);
            } else {
                for (int i = 0; i < loaded_database->header->table_count; i++)
                    pread(fd, loaded_database->table_names[i], TABLE_NAME_SIZE, sizeof(database_header_t) + TABLE_NAME_SIZE * i);

                close(fd);
            }
        }
    }

    return loaded_database;
}
