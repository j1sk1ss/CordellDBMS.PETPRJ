unsigned short encode_hamming_15_11(unsigned short data) {
    unsigned char d1 = (data >> 10) & 1;
    unsigned char d2 = (data >> 9) & 1;
    unsigned char d3 = (data >> 8) & 1;
    unsigned char d4 = (data >> 7) & 1;
    unsigned char d5 = (data >> 6) & 1;
    unsigned char d6 = (data >> 5) & 1;
    unsigned char d7 = (data >> 4) & 1;
    unsigned char d8 = (data >> 3) & 1;
    unsigned char d9 = (data >> 2) & 1;
    unsigned char d10 = (data >> 1) & 1;
    unsigned char d11 = (data >> 0) & 1;

    unsigned char p1 = d1 ^ d2 ^ d4 ^ d5 ^ d7 ^ d8 ^ d10;
    unsigned char p2 = d1 ^ d3 ^ d4 ^ d6 ^ d7 ^ d9 ^ d10;
    unsigned char p3 = d2 ^ d3 ^ d4 ^ d6 ^ d8 ^ d9 ^ d11;
    unsigned char p4 = d5 ^ d6 ^ d7 ^ d8 ^ d10 ^ d11;

    unsigned short result = 0;
    result |= (p1 << 14);
    result |= (p2 << 13);
    result |= (d1 << 12);
    result |= (p3 << 10);
    result |= (d2 << 11);
    result |= (d3 << 9);
    result |= (p4 << 6);
    result |= (d4 << 8);
    result |= (d5 << 7);
    result |= (d6 << 6);
    result |= (d7 << 5);
    result |= (d8 << 4);
    result |= (d9 << 3);
    result |= (d10 << 2);
    result |= (d11 << 1);

    return result;
}


unsigned short decode_hamming_15_11(unsigned short encoded) {
    unsigned char p1 = (encoded >> 14) & 1;
    unsigned char p2 = (encoded >> 13) & 1;
    unsigned char p3 = (encoded >> 10) & 1;
    unsigned char p4 = (encoded >> 6) & 1;

    unsigned char d1  = (encoded >> 12) & 1;
    unsigned char d2  = (encoded >> 11) & 1;
    unsigned char d3  = (encoded >> 9) & 1;
    unsigned char d4  = (encoded >> 8) & 1;
    unsigned char d5  = (encoded >> 7) & 1;
    unsigned char d6  = (encoded >> 5) & 1;
    unsigned char d7  = (encoded >> 4) & 1;
    unsigned char d8  = (encoded >> 3) & 1;
    unsigned char d9  = (encoded >> 2) & 1;
    unsigned char d10 = (encoded >> 1) & 1;
    unsigned char d11 = encoded & 1;

    unsigned char s1 = p1 ^ d1 ^ d2 ^ d4 ^ d5 ^ d7 ^ d8 ^ d10;
    unsigned char s2 = p2 ^ d1 ^ d3 ^ d4 ^ d6 ^ d7 ^ d9 ^ d10;
    unsigned char s3 = p3 ^ d2 ^ d3 ^ d4 ^ d6 ^ d8 ^ d9 ^ d11;
    unsigned char s4 = p4 ^ d5 ^ d6 ^ d7 ^ d8 ^ d10 ^ d11;

    unsigned char error_position = (s1 << 3) | (s2 << 2) | (s3 << 1) | s4;

    if (error_position != 0) {
        encoded ^= (1 << (error_position - 1));
    }

    unsigned short decoded_data = 0;
    decoded_data |= (d1 << 10);
    decoded_data |= (d2 << 9);
    decoded_data |= (d3 << 8);
    decoded_data |= (d4 << 7);
    decoded_data |= (d5 << 6);
    decoded_data |= (d6 << 5);
    decoded_data |= (d7 << 4);
    decoded_data |= (d8 << 3);
    decoded_data |= (d9 << 2);
    decoded_data |= (d10 << 1);
    decoded_data |= (d11);
    return decoded_data;
}
