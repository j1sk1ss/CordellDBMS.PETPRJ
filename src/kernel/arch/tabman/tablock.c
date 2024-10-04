#include "../../include/tabman.h"


#pragma region [Lock]

    int TBM_lock_table(table_t* table, uint8_t owner) {
        #ifndef NO_TDT
            if (table == NULL) return -2;

            int delay = 99999;
            while (table->lock == LOCKED && table->lock_owner != owner) {
                if (table->lock_owner == NO_OWNER) break;
                if (--delay <= 0) return -1;
            }

            table->lock = LOCKED;
            table->lock_owner = owner;
        #endif

        return 1;
    }

    int TBM_lock_test(table_t* table, uint8_t owner) {
        #ifndef NO_TDT
            if (table->lock_owner != owner) return 0;
            return table->lock;
        #endif

        return UNLOCKED;
    }

    int TBM_release_table(table_t* table, uint8_t owner) {
        #ifndef NO_TDT
            if (table->lock == UNLOCKED) return -1;
            if (table->lock_owner != owner && table->lock_owner != NO_OWNER) return -2;

            table->lock = UNLOCKED;
            table->lock_owner = NO_OWNER;
        #endif

        return 1;
    }

#pragma endregion