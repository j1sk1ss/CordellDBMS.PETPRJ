#include "../../include/dirman.h"


#pragma region [Lock]

    int DRM_lock_directory(directory_t* directory, uint8_t owner) {
        #ifndef NO_DDT
            if (directory == NULL) return -2;

            int delay = 99999;
            while (directory->lock == LOCKED && directory->lock_owner != owner) {
                if (directory->lock_owner == NO_OWNER) break;
                if (--delay <= 0) return -1;
            }

            directory->lock = LOCKED;
            directory->lock_owner = owner;
        #endif

        return 1;
    }

    int DRM_lock_test(directory_t* directory, uint8_t owner) {
        #ifndef NO_DDT
            if (directory->lock_owner != owner) return 0;
            return directory->lock;
        #endif

        return UNLOCKED;
    }

    int DRM_release_directory(directory_t* directory, uint8_t owner) {
        #ifndef NO_DDT
            if (directory->lock == UNLOCKED) return -1;
            if (directory->lock_owner != owner && directory->lock_owner != NO_OWNER) return -2;

            directory->lock = UNLOCKED;
            directory->lock_owner = NO_OWNER;
        #endif

        return 1;
    }

#pragma endregion