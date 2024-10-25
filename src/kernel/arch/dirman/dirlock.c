#include "../../include/dirman.h"


int DRM_lock_directory(directory_t* directory, uint8_t owner) {
    if (directory == NULL) return -2;
    print_debug("Try to lock directory [%s]", directory->header->name);

    int delay = DEFAULT_DELAY;
    while (DRM_lock_test(directory, owner) == LOCKED)
        if (--delay <= 0) {
            print_error("Can't lock directory [%s], owner: [%i], new owner [%i]", directory->header->name, directory->lock_owner, owner);
            return -1;
        }

    directory->lock = LOCKED;
    directory->lock_owner = owner;

    return 1;
}

int DRM_lock_test(directory_t* directory, uint8_t owner) {
    if (directory->lock_owner == NO_OWNER) return UNLOCKED;
    if (directory->lock_owner != owner) return LOCKED;
    return UNLOCKED;
}

int DRM_release_directory(directory_t* directory, uint8_t owner) {
    if (directory == NULL) return -3;
    print_debug("Try to unlock directory [%s]", directory->header->name);

    if (directory->lock == UNLOCKED) return -1;
    if (DRM_lock_test(directory, owner) == LOCKED) return -2;

    directory->lock = UNLOCKED;
    directory->lock_owner = NO_OWNER;
    return 1;
}
