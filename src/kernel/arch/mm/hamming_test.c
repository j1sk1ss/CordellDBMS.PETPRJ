#include <stdio.h>


#define GET_BIT(b, i) ((b >> i) & 1)
#define SET_BIT(n, i, v) (v ? (n | (1 << i)) : (n & ~(1 << i)))
#define TOGGLE_BIT(b, i) (b ^ (1 << i))


unsigned short encode_hamming_15_11(unsigned short data) {
    unsigned short encoded = 0;

    encoded = SET_BIT(encoded, 2, GET_BIT(data, 0));
    encoded = SET_BIT(encoded, 4, GET_BIT(data, 1));
    encoded = SET_BIT(encoded, 5, GET_BIT(data, 2));
    encoded = SET_BIT(encoded, 6, GET_BIT(data, 3));
    encoded = SET_BIT(encoded, 8, GET_BIT(data, 4));
    encoded = SET_BIT(encoded, 9, GET_BIT(data, 5));
    encoded = SET_BIT(encoded, 10, GET_BIT(data, 6));
    encoded = SET_BIT(encoded, 11, GET_BIT(data, 7));
    encoded = SET_BIT(encoded, 12, GET_BIT(data, 8));
    encoded = SET_BIT(encoded, 13, GET_BIT(data, 9));
    encoded = SET_BIT(encoded, 14, GET_BIT(data, 10));

    unsigned char p1 = GET_BIT(encoded, 2) ^ GET_BIT(encoded, 4) ^ GET_BIT(encoded, 6) ^ GET_BIT(encoded, 8) ^ GET_BIT(encoded, 10) ^ GET_BIT(encoded, 12) ^ GET_BIT(encoded, 14);
    unsigned char p2 = GET_BIT(encoded, 2) ^ GET_BIT(encoded, 5) ^ GET_BIT(encoded, 6) ^ GET_BIT(encoded, 9) ^ GET_BIT(encoded, 10) ^ GET_BIT(encoded, 13) ^ GET_BIT(encoded, 14);
    unsigned char p4 = GET_BIT(encoded, 4) ^ GET_BIT(encoded, 5) ^ GET_BIT(encoded, 6) ^ GET_BIT(encoded, 11) ^ GET_BIT(encoded, 12) ^ GET_BIT(encoded, 13) ^ GET_BIT(encoded, 14);
    unsigned char p8 = GET_BIT(encoded, 8) ^ GET_BIT(encoded, 9) ^ GET_BIT(encoded, 10) ^ GET_BIT(encoded, 11) ^ GET_BIT(encoded, 12) ^ GET_BIT(encoded, 13) ^ GET_BIT(encoded, 14);

    encoded = SET_BIT(encoded, 0, p1);
    encoded = SET_BIT(encoded, 1, p2);
    encoded = SET_BIT(encoded, 3, p4);
    encoded = SET_BIT(encoded, 7, p8);

    return encoded;
}

unsigned short decode_hamming_15_11(unsigned short encoded) {
    unsigned char s1 = GET_BIT(encoded, 0) ^ GET_BIT(encoded, 2) ^ GET_BIT(encoded, 4) ^ GET_BIT(encoded, 6) ^ GET_BIT(encoded, 8) ^ GET_BIT(encoded, 10) ^ GET_BIT(encoded, 12) ^ GET_BIT(encoded, 14);
    unsigned char s2 = GET_BIT(encoded, 1) ^ GET_BIT(encoded, 2) ^ GET_BIT(encoded, 5) ^ GET_BIT(encoded, 6) ^ GET_BIT(encoded, 9) ^ GET_BIT(encoded, 10) ^ GET_BIT(encoded, 13) ^ GET_BIT(encoded, 14);
    unsigned char s4 = GET_BIT(encoded, 3) ^ GET_BIT(encoded, 4) ^ GET_BIT(encoded, 5) ^ GET_BIT(encoded, 6) ^ GET_BIT(encoded, 11) ^ GET_BIT(encoded, 12) ^ GET_BIT(encoded, 13) ^ GET_BIT(encoded, 14);
    unsigned char s8 = GET_BIT(encoded, 7) ^ GET_BIT(encoded, 8) ^ GET_BIT(encoded, 9) ^ GET_BIT(encoded, 10) ^ GET_BIT(encoded, 11) ^ GET_BIT(encoded, 12) ^ GET_BIT(encoded, 13) ^ GET_BIT(encoded, 14);

    unsigned char error_pos = s1 + (s2 << 1) + (s4 << 2) + (s8 << 3);
    if (error_pos != 0) {
        encoded = TOGGLE_BIT(encoded, (error_pos - 1));
    }

    unsigned short data = 0;
    data = SET_BIT(data, 0, GET_BIT(encoded, 2));
    data = SET_BIT(data, 1, GET_BIT(encoded, 4));
    data = SET_BIT(data, 2, GET_BIT(encoded, 5));
    data = SET_BIT(data, 3, GET_BIT(encoded, 6));
    data = SET_BIT(data, 4, GET_BIT(encoded, 8));
    data = SET_BIT(data, 5, GET_BIT(encoded, 9));
    data = SET_BIT(data, 6, GET_BIT(encoded, 10));
    data = SET_BIT(data, 7, GET_BIT(encoded, 11));
    data = SET_BIT(data, 8, GET_BIT(encoded, 12));
    data = SET_BIT(data, 9, GET_BIT(encoded, 13));
    data = SET_BIT(data, 10, GET_BIT(encoded, 14));

    return data;
}

int main() {
    unsigned short data = 0x7FF & 0xABCD;
    printf("Original data: 0x%X\n", data);

    unsigned short encoded = encode_hamming_15_11(data);
    printf("Encoded data: 0x%X\n", encoded);

    encoded = TOGGLE_BIT(encoded, 5);
    printf("Encoded data with error: 0x%X\n", encoded);

    unsigned short decoded = decode_hamming_15_11(encoded);
    printf("Decoded data: 0x%X\n", decoded);

    if (data == decoded) {
        printf("Test passed\n");
    } else {
        printf("Test failed\n");
    }

    return 0;
}