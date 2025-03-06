// #include <string.h>
// #include <stdlib.h>
// #include <stdio.h>


// #define GET_BIT(b, i) ((b >> i) & 1)
// #define SET_BIT(n, i, v) (v ? (n | (1 << i)) : (n & ~(1 << i)))
// #define TOGGLE_BIT(b, i) (b ^ (1 << i))


// unsigned short encode_hamming_15_11(unsigned short data) {
//     unsigned short encoded = 0;
//     encoded = SET_BIT(encoded, 2, GET_BIT(data, 0));
//     encoded = SET_BIT(encoded, 4, GET_BIT(data, 1));
//     encoded = SET_BIT(encoded, 5, GET_BIT(data, 2));
//     encoded = SET_BIT(encoded, 6, GET_BIT(data, 3));
//     encoded = SET_BIT(encoded, 8, GET_BIT(data, 4));
//     encoded = SET_BIT(encoded, 9, GET_BIT(data, 5));
//     encoded = SET_BIT(encoded, 10, GET_BIT(data, 6));
//     encoded = SET_BIT(encoded, 11, GET_BIT(data, 7));
//     encoded = SET_BIT(encoded, 12, GET_BIT(data, 8));
//     encoded = SET_BIT(encoded, 13, GET_BIT(data, 9));
//     encoded = SET_BIT(encoded, 14, GET_BIT(data, 10));

//     unsigned char p1 = GET_BIT(encoded, 2) ^ GET_BIT(encoded, 4) ^ GET_BIT(encoded, 6) ^ GET_BIT(encoded, 8) ^ GET_BIT(encoded, 10) ^ GET_BIT(encoded, 12) ^ GET_BIT(encoded, 14);
//     unsigned char p2 = GET_BIT(encoded, 2) ^ GET_BIT(encoded, 5) ^ GET_BIT(encoded, 6) ^ GET_BIT(encoded, 9) ^ GET_BIT(encoded, 10) ^ GET_BIT(encoded, 13) ^ GET_BIT(encoded, 14);
//     unsigned char p4 = GET_BIT(encoded, 4) ^ GET_BIT(encoded, 5) ^ GET_BIT(encoded, 6) ^ GET_BIT(encoded, 11) ^ GET_BIT(encoded, 12) ^ GET_BIT(encoded, 13) ^ GET_BIT(encoded, 14);
//     unsigned char p8 = GET_BIT(encoded, 8) ^ GET_BIT(encoded, 9) ^ GET_BIT(encoded, 10) ^ GET_BIT(encoded, 11) ^ GET_BIT(encoded, 12) ^ GET_BIT(encoded, 13) ^ GET_BIT(encoded, 14);

//     encoded = SET_BIT(encoded, 0, p1);
//     encoded = SET_BIT(encoded, 1, p2);
//     encoded = SET_BIT(encoded, 3, p4);
//     encoded = SET_BIT(encoded, 7, p8);
//     return encoded;
// }

// unsigned short decode_hamming_15_11(unsigned short encoded) {
//     unsigned char s1 = GET_BIT(encoded, 0) ^ GET_BIT(encoded, 2) ^ GET_BIT(encoded, 4) ^ GET_BIT(encoded, 6) ^ GET_BIT(encoded, 8) ^ GET_BIT(encoded, 10) ^ GET_BIT(encoded, 12) ^ GET_BIT(encoded, 14);
//     unsigned char s2 = GET_BIT(encoded, 1) ^ GET_BIT(encoded, 2) ^ GET_BIT(encoded, 5) ^ GET_BIT(encoded, 6) ^ GET_BIT(encoded, 9) ^ GET_BIT(encoded, 10) ^ GET_BIT(encoded, 13) ^ GET_BIT(encoded, 14);
//     unsigned char s4 = GET_BIT(encoded, 3) ^ GET_BIT(encoded, 4) ^ GET_BIT(encoded, 5) ^ GET_BIT(encoded, 6) ^ GET_BIT(encoded, 11) ^ GET_BIT(encoded, 12) ^ GET_BIT(encoded, 13) ^ GET_BIT(encoded, 14);
//     unsigned char s8 = GET_BIT(encoded, 7) ^ GET_BIT(encoded, 8) ^ GET_BIT(encoded, 9) ^ GET_BIT(encoded, 10) ^ GET_BIT(encoded, 11) ^ GET_BIT(encoded, 12) ^ GET_BIT(encoded, 13) ^ GET_BIT(encoded, 14);

//     unsigned char error_pos = s1 + (s2 << 1) + (s4 << 2) + (s8 << 3);
//     if (error_pos) encoded = TOGGLE_BIT(encoded, (error_pos - 1));
    
//     unsigned short data = 0;
//     data = SET_BIT(data, 0, GET_BIT(encoded, 2));
//     data = SET_BIT(data, 1, GET_BIT(encoded, 4));
//     data = SET_BIT(data, 2, GET_BIT(encoded, 5));
//     data = SET_BIT(data, 3, GET_BIT(encoded, 6));
//     data = SET_BIT(data, 4, GET_BIT(encoded, 8));
//     data = SET_BIT(data, 5, GET_BIT(encoded, 9));
//     data = SET_BIT(data, 6, GET_BIT(encoded, 10));
//     data = SET_BIT(data, 7, GET_BIT(encoded, 11));
//     data = SET_BIT(data, 8, GET_BIT(encoded, 12));
//     data = SET_BIT(data, 9, GET_BIT(encoded, 13));
//     data = SET_BIT(data, 10, GET_BIT(encoded, 14));
//     return data;
// }

// unsigned char _get_byte(unsigned short* ptr, int offset) {
//     return (unsigned char)decode_hamming_15_11(ptr[offset]);
// }

// int _set_byte(unsigned short* ptr, int offset, unsigned char byte) {
//     ptr[offset] = encode_hamming_15_11((unsigned short)byte);
//     return 1;
// }

// void* unpack_memory(unsigned short* src, unsigned char* dst, size_t len) {
//     for (int i = 0; i < len; i++) dst[i] = _get_byte(src, i);
//     return (void*)dst;
// }

// void* pack_memory(unsigned char* src, unsigned short* dst, size_t len) {
//     for (int i = 0; i < len; i++) _set_byte(dst, i, src[i]);
//     return (void*)dst;
// }


// typedef struct {
//     // Magic namber for check
//     // If number nq magic, we know that this file broken
//     unsigned char magic;

//     // Page filename
//     // With this name we can save pages / compare pages
//     char name[8];

//     // Table checksum
//     unsigned int checksum;
// } page_header_t;


// int main() {
//     page_header_t header = {
//         .checksum = 0,
//         .magic = 0xDEADBEEF,
//         .name = "page"
//     };

//     printf("Name %s, Magic %llu, Checksum %i\n", header.name, header.magic, header.checksum);

//     printf("Saving...\n");
//     FILE* fd = fopen("page.pg", "wb");
//     if (fd) {
//         page_header_t encoded_header;
//         pack_memory((unsigned char*)&header, (unsigned short*)&encoded_header, sizeof(page_header_t));
//         fwrite(&encoded_header, sizeof(unsigned short), sizeof(page_header_t), fd);
//         fclose(fd);
//     }

//     printf("Reading...\n");
//     fd = fopen("page.pg", "rb");
//     if (fd) {
//         page_header_t encoded_header;
//         fread(&encoded_header, sizeof(unsigned short), sizeof(page_header_t), fd);
//         page_header_t decoded_header;
//         unpack_memory((unsigned short*)&encoded_header, (unsigned char*)&decoded_header, sizeof(page_header_t));
//         fclose(fd);

//         printf("Name %s, Magic %llu, Checksum %i\n", decoded_header.name, decoded_header.magic, decoded_header.checksum);
//     }

//     return 1;
// }