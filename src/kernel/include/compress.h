#ifndef COMPRESS_H_
#define COMPRESS_H_

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


// Insert bit to byte by provided index and value
#define INSERT_BIT(byte, index, value) \
    ((value) ? ((byte) |= (1U << (index))) : ((byte) &= ~(1U << (index))))
// Set bit to 1 by provided index
#define SET_BIT(byte, index)       ((byte) |= (1U << (index)))
// Set bit to 0 by provided index
#define CLEAR_BIT(byte, index)     ((byte) &= ~(1U << (index)))
// Inverse bit by provided index
#define TOGGLE_BIT(byte, index)    ((byte) ^= (1U << (index)))
// Check if bit is equals 1
#define CHECK_BIT(byte, index)     ((byte) & (1U << (index)))
// Get bit from byte by provided index
#define GET_BIT(byte, index)       (((byte) >> (index)) & 0x01)


int CMP_byte_by_bits(uint8_t* data, size_t data_size, uint8_t* output);
int UNZ_byte_by_bits(uint8_t* data, size_t data_size, uint8_t* output);

#endif
