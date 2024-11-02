#include "../include/common.h"


uint8_t* memrep(
    uint8_t* source, int source_size, uint8_t* sub, int sub_size, uint8_t* new, int new_size, size_t* result_len
) {
    uint8_t* result;   // the return array
    uint8_t* ins;      // the next insert point
    uint8_t* tmp;      // varies
    size_t len_front;  // distance between source and end of last source
    int count;         // number of replacements

    if (!source || !sub || sub_size == 0) return NULL;
    if (!new) {
        new = (uint8_t*)"";
        new_size = 0;
    }

    ins = source;
    while ((tmp = (uint8_t*)memmem(ins, source_size - (ins - source), sub, sub_size)) != NULL) {
        ins = tmp + sub_size;
        count++;
    }

    *result_len = source_size + (new_size - sub_size) * count;
    result = (uint8_t*)malloc(*result_len + 1);
    if (!result) return NULL;
    memset(result, 0, *result_len + 1);

    tmp = result;
    ins = source;

    while (count--) {
        uint8_t* pos = (uint8_t*)memmem(ins, source_size - (ins - source), sub, sub_size);
        if (!pos) break;
        
        len_front = pos - ins;
        memcpy(tmp, ins, len_front);
        tmp += len_front;

        if (new_size > 0) {
            memcpy(tmp, new, new_size);
            tmp += new_size;
        }

        ins = pos + sub_size;
    }

    memcpy(tmp, ins, source_size - (ins - source));
    return result;
}