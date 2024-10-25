#include "../../include/pageman.h"


int PGM_lock_page(page_t* page, uint8_t owner) {
    if (page == NULL) return -2;
    print_debug("Try to lock page [%s]", page->header->name);

    int delay = DEFAULT_DELAY;
    while (PGM_lock_test(page, owner) == LOCKED)
        if (--delay <= 0) {
            print_error("Can't lock page [%s], owner: [%i], new owner [%i]", page->header->name, page->lock_owner, owner);
            return -1;
        }

    page->lock       = LOCKED;
    page->lock_owner = owner;
    return 1;
}

int PGM_lock_test(page_t* page, uint8_t owner) {
    if (page->lock_owner == NO_OWNER) return UNLOCKED;
    if (page->lock_owner != owner) return LOCKED;
    return UNLOCKED;
}

int PGM_release_page(page_t* page, uint8_t owner) {
    if (page == NULL) return -3;
    print_debug("Try to unlock page [%s]", page->header->name);

    if (page->lock == UNLOCKED) return -1;
    if (PGM_lock_test(page, owner) == LOCKED) return -2;

    page->lock       = UNLOCKED;
    page->lock_owner = NO_OWNER;
    return 1;
}
