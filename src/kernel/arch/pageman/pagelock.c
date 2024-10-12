#include "../../include/pageman.h"


#pragma region [Lock]

    int PGM_lock_page(page_t* page, uint8_t owner) {
        #ifndef NO_PDT
            if (page == NULL) return -2;

            int delay = 99999;
            while (page->lock == LOCKED && page->lock_owner != owner) {
                if (page->lock_owner == NO_OWNER) break;
                if (--delay <= 0) return -1;
            }

            page->lock       = LOCKED;
            page->lock_owner = owner;
        #endif

        return 1;
    }

    int PGM_lock_test(page_t* page, uint8_t owner) {
        #ifndef NO_PDT
            if (page->lock_owner != owner) return LOCKED;
            return page->lock;
        #endif

        return UNLOCKED;
    }

    int PGM_release_page(page_t* page, uint8_t owner) {
        if (page == NULL) return -3;

        #ifndef NO_PDT
            if (page->lock == UNLOCKED) return -1;
            if (page->lock_owner != owner && page->lock_owner != NO_OWNER) return -2;

            page->lock       = UNLOCKED;
            page->lock_owner = NO_OWNER;
        #endif

        return 1;
    }

#pragma endregion