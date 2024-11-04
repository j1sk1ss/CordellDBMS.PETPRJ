#include "../../include/dataman.h"


int DB_init_transaction(database_t* database) {
    if (database == NULL) return -4;
    if (CHC_sync() != 1) return -1;

    DB_save_database(database, NULL);
    return DB_cleanup_tables(database);
}

int DB_rollback(database_t** database) {
    if (*database == NULL) return -4;
    if (CHC_free() != 1) return -1;

    database_t* old_database = DB_load_database(NULL, (*database)->header->name);
    if (old_database == NULL) return -5;

    DB_free_database(*database);
    *database = old_database;

    return 1;
}
