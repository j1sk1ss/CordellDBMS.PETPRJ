#include "../include/hash.h"


unsigned int HASH_str2hash(const char* str) {
    unsigned int hashedValue = 0;
    for (int i = 0; str[i] != '\0'; i++) hashedValue ^= (hashedValue << MAGIC) + (hashedValue >> 2) + str[i];
    for (int i = 0; i < MAGIC; i++) hashedValue ^= (hashedValue << MAGIC) + (hashedValue >> 2) + SALT[i];
    return hashedValue;
}
