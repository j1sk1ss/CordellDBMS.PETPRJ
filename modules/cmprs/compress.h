/*
 *  License:
 *  Copyright (C) 2024 Nikolaj Fot
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of 
 *  the GNU General Public License as published by the Free Software Foundation, version 3.
 *  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  See the GNU General Public License for more details.
 *  You should have received a copy of the GNU General Public License along with this program. 
 *  If not, see https://www.gnu.org/licenses/.
 */

#ifndef COMPRESS_H_
#define COMPRESS_H_

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


int CMP_byte_by_bits(unsigned char* data, size_t data_size, unsigned char* output);
int UNZ_byte_by_bits(unsigned char* data, size_t data_size, unsigned char* output);

#endif
