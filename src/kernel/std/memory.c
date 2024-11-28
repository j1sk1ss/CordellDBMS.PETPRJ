#include "../include/common.h"


void* memcpy_s(void* destination, void* source, size_t num) {
    unsigned char* u8Dst = (unsigned char*)destination;
    const unsigned char* u8Src = (const unsigned char*)source;
    for (unsigned short i = 0; i < num; i++)
        u8Dst[i] = u8Src[i];

    return destination;
}

void* memset_s(void* pointer, unsigned char value, size_t num) {
    unsigned char* u8Ptr = (unsigned char*)pointer;
    for (unsigned short i = 0; i < num; i++)
        u8Ptr[i] = value;

    return pointer;
}

int memcmp_s(void* firstPointer, void* secondPointer, size_t num) {
    const unsigned char* u8Ptr1 = (const unsigned char *)firstPointer;
    const unsigned char* u8Ptr2 = (const unsigned char *)secondPointer;
    for (unsigned short i = 0; i < num; i++)
        if (u8Ptr1[i] != u8Ptr2[i])
            return 1;

    return 0;
}