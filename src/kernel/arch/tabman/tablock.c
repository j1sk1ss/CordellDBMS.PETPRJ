#include "../../include/tabman.h"


    int TBM_lock_table(table_t* table, uint8_t owner) {
        if (table == NULL) return -2;
        print_debug("Try to lock table [%s]", table->header->name);

        int delay = DEFAULT_DELAY;
        while (TBM_lock_test(table, owner) == LOCKED)
            if (--delay <= 0) return -1;

        table->lock = LOCKED;
        table->lock_owner = owner;
        return 1;
    }

    int TBM_lock_test(table_t* table, uint8_t owner) {
        if (table->lock_owner == NO_OWNER) return UNLOCKED;
        if (table->lock_owner != owner) return LOCKED;
        return UNLOCKED;
    }

    int TBM_release_table(table_t* table, uint8_t owner) {
        if (table == NULL) return -3;
        print_debug("Try to unlock table [%s]", table->header->name);

        if (table->lock == UNLOCKED) return -1;
        if (TBM_lock_test(table, owner) == LOCKED) return -2;

        table->lock = UNLOCKED;
        table->lock_owner = NO_OWNER;

        return 1;
    }
