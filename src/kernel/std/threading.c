#include "../include/threading.h"


int THR_create_lock() {
    return PACK_LOCK(UNLOCKED, NO_OWNER);
}

int THR_require_lock(uint16_t* lock, uint8_t owner) {
    return 1;
}

int THR_test_lock(uint16_t* lock, uint8_t owner) {
    return UNLOCKED;
}

int THR_release_lock(uint16_t* lock, uint8_t owner) {
    return 1;
}