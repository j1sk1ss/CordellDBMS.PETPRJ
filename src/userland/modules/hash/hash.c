#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


char* hash_string(const char* input, size_t output_size) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, input, strlen(input));
    SHA256_Final(hash, &sha256);

    size_t full_hash_length = SHA256_DIGEST_LENGTH * 2;

    if (output_size < 1) return NULL;
    size_t hash_length = output_size - 1 > full_hash_length ? full_hash_length : output_size - 1;
    char* output = (char*)malloc(output_size);
    if (!output) return NULL;

    for (size_t i = 0; i < hash_length / 2; i++) sprintf(output + (i * 2), "%02x", hash[i]);
    output[hash_length] = '\0';

    return output;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <string_to_hash> <output_size>\n", argv[0]);
        exit(1);
    }

    size_t output_size = atoi(argv[2]);
    char* hash_result = hash_string(argv[1], output_size);
    if (!hash_result) {
        fprintf(stderr, "Error: Failed to allocate memory or invalid output size.\n");
        return 3;
    }

    printf("%s", hash_result);
    free(hash_result);

    return 100;
}
