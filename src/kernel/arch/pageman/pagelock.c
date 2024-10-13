#include "../../include/pageman.h"


#pragma region [Lock]

    int PGM_lock_page(page_t* page, uint8_t owner) {
        #ifndef NO_PDT
            if (page == NULL) return -2;

            int delay = DEFAULT_DELAY;
            while (PGM_lock_test(page, owner) == LOCKED)
                if (--delay <= 0) return -1;

            page->lock       = LOCKED;
            page->lock_owner = owner;
        #endif

        return 1;
    }

    int PGM_lock_test(page_t* page, uint8_t owner) {
        #ifndef NO_PDT
            if (page->lock_owner == NO_OWNER) return UNLOCKED;
            if (page->lock_owner != owner) return LOCKED;
            else return UNLOCKED;
        #endif

        return UNLOCKED;
    }

    int PGM_release_page(page_t* page, uint8_t owner) {
        if (page == NULL) return -3;

        #ifndef NO_PDT
            if (page->lock == UNLOCKED) return -1;
            if (PGM_lock_test(page, owner) == LOCKED) return -2;

            page->lock       = UNLOCKED;
            page->lock_owner = NO_OWNER;
        #endif

        return 1;
    }

#pragma endregion