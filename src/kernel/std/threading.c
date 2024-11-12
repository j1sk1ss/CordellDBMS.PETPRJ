#include "../include/threading.h"


int THR_create_thread(void* entry, void* args) {
    #ifdef _WIN32
        HANDLE client_thread = CreateThread(NULL, 0, entry, args, 0, NULL);
        if (client_thread == NULL) {
            free(args);
            return -1;
        }

        CloseHandle(client_thread);
    #else
        pthread_t client_thread;
        if (pthread_create(&client_thread, NULL, entry, args) != 0) {
            free(args);
            return -1;
        }

        pthread_detach(client_thread);
    #endif

    return 1;
}

int THR_create_lock() {
    return PACK_LOCK(UNLOCKED, NO_OWNER);
}

int THR_require_lock(uint16_t* lock, uint8_t owner) {
    if (lock == NULL) return -2;
    int delay = DEFAULT_DELAY;
    while (THR_test_lock(lock, owner) == LOCKED)
        if (--delay <= 0) return -1;

    *lock = PACK_LOCK(LOCKED, owner);
    return 1;
}

int THR_test_lock(uint16_t* lock, uint8_t owner) {
    if (lock == NULL) return -1;
    uint16_t lock_owner = UNPACK_OWNER(*lock);
    if (lock_owner == NO_OWNER) return UNLOCKED;
    if (lock_owner != owner) return UNPACK_STATUS(*lock);
    return UNLOCKED;
}

int THR_release_lock(uint16_t* lock, uint8_t owner) {
    if (lock == NULL) return -3;
    uint16_t lock_status = UNPACK_STATUS(*lock);
    if (lock_status == UNLOCKED) return -1;
    if (THR_test_lock(lock, owner) == LOCKED) return -2;

    *lock = PACK_LOCK(UNLOCKED, NO_OWNER);
    return 1;
}