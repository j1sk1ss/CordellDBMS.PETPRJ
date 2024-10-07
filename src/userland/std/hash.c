#include "../include/hash.h"


uint32_t HASH_str2hash(const char* str) {
    uint32_t hashedValue = 0;
    for (int i = 0; str[i] != '\0'; i++) hashedValue ^= (hashedValue << MAGIC) + (hashedValue >> 2) + str[i];
    for (int i = 0; i < MAGIC; i++) hashedValue ^= (hashedValue << MAGIC) + (hashedValue >> 2) + SALT[i];
    return hashedValue;
}
