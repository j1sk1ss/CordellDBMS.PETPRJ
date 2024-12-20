#include "compress.h"


int CMP_byte_by_bits(unsigned char* data, size_t data_size, unsigned char* output) {
    int current_index = 0;

    for (size_t i = 0; i + 8 <= data_size; i += 8) {
        int compress_flag = 1;

        for (size_t j = i; j < i + 8; j++) {
            if (data[j] > 0b01111111) {
                compress_flag = 0;
                break;
            }
        }

        if (!compress_flag) continue;

        unsigned char target_byte = data[i + 8];
        for (size_t j = 0; j < 8; j++) {
            unsigned char source_byte = data[i + j];
            INSERT_BIT(source_byte, 7, GET_BIT(target_byte, j));
            output[current_index++] = source_byte;
        }
    }

    return current_index;
}

int UNZ_byte_by_bits(unsigned char* data, size_t data_size, unsigned char* output) {
    int current_index = 0;

    for (size_t i = 0; i + 8 <= data_size; i += 8) {
        unsigned char target_byte = 0;

        for (size_t j = 0; j < 8; j++) {
            unsigned char source_byte = data[i + j];
            INSERT_BIT(target_byte, j, GET_BIT(source_byte, 7));
            CLEAR_BIT(source_byte, 7);
            output[current_index++] = source_byte;
        }

        output[current_index++] = target_byte;
    }

    return current_index;
}
