#include "../../include/dataman.h"


#pragma region [Database file]

    database_t* DB_create_database(char* name) {
        database_header_t* header = (database_header_t*)malloc(sizeof(database_header_t));
        header->magic = DATABASE_MAGIC;
        strncpy((char*)header->name, name, DATABASE_NAME_SIZE);
        header->table_count = 0;

        database_t* database = (database_t*)malloc(sizeof(database_t));
        database->header        = header;

        return database;
    }

    int DB_free_database(database_t* database) {
        if (database == NULL) return -1;

        SOFT_FREE(database->header);
        SOFT_FREE(database);

        return 1;
    }

    database_t* DB_load_database(char* path, char* name) {
        char load_path[DEFAULT_PATH_SIZE];
        if (path == NULL && name != NULL) sprintf(load_path, "%s%.8s.%s", DATABASE_BASE_PATH, name, DATABASE_EXTENSION);
        else strcpy(load_path, path);

        database_t* loaded_database = NULL;
        #pragma omp critical (load_database)
        {
            FILE* file = fopen(load_path, "rb");
            if (file == NULL) {
                print_error("Database file not found! [%s]", load_path);
                loaded_database = NULL;
            } else {
                database_header_t* header = (database_header_t*)malloc(sizeof(database_header_t));
                fread(header, sizeof(database_header_t), 1, file);

                if (header->magic != DATABASE_MAGIC) {
                    loaded_database = NULL;

                    free(header);
                    fclose(file);
                } else {
                    database_t* database = (database_t*)malloc(sizeof(database_t));
                    for (int i = 0; i < header->table_count; i++)
                        fread(database->table_names[i], sizeof(uint8_t), TABLE_NAME_SIZE, file);

                    fclose(file);

                    database->header        = header;
                    loaded_database = database;
                }
            }
        }

        return loaded_database;
    }

    int DB_save_database(database_t* database, char* path) {
        int status = 1;
        #pragma omp critical (save_database)
        {
            // We generate default path
            char save_path[DEFAULT_PATH_SIZE];
            if (path == NULL) {
                sprintf(save_path, "%s%.8s.%s", DATABASE_BASE_PATH, database->header->name, DATABASE_EXTENSION);
            }
            else strcpy(save_path, path);

            FILE* file = fopen(save_path, "wb");
            if (file == NULL) {
                print_error("Can`t create file: [%s]", save_path);
                status = -1;
            } else {
                if (fwrite(database->header, sizeof(database_header_t), 1, file) != 1) status = -2;
                for (int i = 0; i < database->header->table_count; i++)
                    if (fwrite(database->table_names[i], sizeof(uint8_t), TABLE_NAME_SIZE, file) != TABLE_NAME_SIZE) {
                        status = -3;
                    }

                #ifndef _WIN32
                fsync(fileno(file));
                #else
                fflush(file);
                #endif

                fclose(file);
            }
        }

        return status;
    }

#pragma endregion
