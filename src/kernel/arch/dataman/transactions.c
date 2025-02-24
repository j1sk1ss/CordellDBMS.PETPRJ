#include "../../include/dataman.h"


int DB_init_transaction(database_t* database) {
    DB_save_database(database);
    DB_cleanup_tables(database);
    return CHC_sync();
}

int DB_rollback(database_t** database) {
    if (CHC_free() != 1) return -1;

    database_t* old_database = DB_load_database((*database)->header->name);
    if (old_database == NULL) return -5;

    DB_free_database(*database);
    *database = old_database;

    return 1;
}
