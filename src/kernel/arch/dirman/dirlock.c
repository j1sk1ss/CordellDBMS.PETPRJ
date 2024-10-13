#include "../../include/dirman.h"


#pragma region [Lock]

    int DRM_lock_directory(directory_t* directory, uint8_t owner) {
        #ifndef NO_DDT
            if (directory == NULL) return -2;

            int delay = DEFAULT_DELAY;
            while (DRM_lock_test(directory, owner) == LOCKED)
                if (--delay <= 0) return -1;

            directory->lock = LOCKED;
            directory->lock_owner = owner;
        #endif

        return 1;
    }

    int DRM_lock_test(directory_t* directory, uint8_t owner) {
        #ifndef NO_DDT
            if (directory->lock_owner == NO_OWNER) return UNLOCKED;
            if (directory->lock_owner != owner) return LOCKED;
            else return UNLOCKED;
        #endif

        return UNLOCKED;
    }

    int DRM_release_directory(directory_t* directory, uint8_t owner) {
        if (directory == NULL) return -3;

        #ifndef NO_DDT
            if (directory->lock == UNLOCKED) return -1;
            if (DRM_lock_test(directory, owner) == LOCKED) return -2;

            directory->lock = UNLOCKED;
            directory->lock_owner = NO_OWNER;
        #endif

        return 1;
    }

#pragma endregion