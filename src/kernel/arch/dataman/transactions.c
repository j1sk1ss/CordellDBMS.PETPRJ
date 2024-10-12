#include "../../include/dataman.h"


#pragma region [Transaction]

    int DB_init_transaction(database_t* database) {
        if (TBM_TDT_sync() != 1) return -1;
        if (DRM_DDT_sync() != 1) return -2;
        if (PGM_PDT_sync() != 1) return -3;
        DB_save_database(database, NULL);
        return DB_cleanup_tables(database);
    }

    int DB_rollback(database_t* database) {
        if (TBM_TDT_free() != 1) return -1;
        if (DRM_DDT_free() != 1) return -2;
        if (PGM_PDT_free() != 1) return -3;

        database_t* old_database = DB_load_database(NULL, (char*)database->header->name);
        DB_free_database(database);
        database = old_database;

        return 1;
    }

#pragma endregion
