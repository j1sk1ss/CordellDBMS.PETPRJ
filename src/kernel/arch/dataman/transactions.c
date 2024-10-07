#include "../../include/dataman.h"


#pragma region [Transaction]

    int DB_init_transaction(database_t* database) {
        int result = 1;
        result = TBM_TDT_sync();
        if (result != 1) return -1;

        result = DRM_DDT_sync();
        if (result != 1) return -2;

        result = PGM_PDT_sync();
        if (result != 1) return -3;

        return DB_cleanup_tables(database);
    }

    int DB_rollback() {
        int result = 1;
        result = TBM_TDT_free();
        if (result != 1) return -1;

        result = DRM_DDT_free();
        if (result != 1) return -2;

        result = PGM_PDT_free();
        if (result != 1) return -3;

        return 1;
    }

#pragma endregion
