#include "../include/common.h"


void rand_str(char *dest, size_t length) {
    char charset[] = "0123456789"
                     "abcdefghijklmnopqrstuvwxyz"
                     "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    while (length-- > 0) {
        size_t index = (double) rand() / RAND_MAX * (sizeof charset - 1);
        *dest++ = charset[index];
    }
}

int is_integer(const char* str) {
    while (*str) {
        if (!isdigit(*str)) {
            return 0;
        }

        str++;
    }

    return 1;
}

int is_float(const char* str) {
    int has_dot = 0;
    while (*str) {
        if (*str == '.') {
            if (has_dot) {
                return 0;
            }
            has_dot = 1;
        } else if (!isdigit(*str)) {
            return 0;
        }

        str++;
    }

    return 1;
}

char* get_next_token(char** input, char delimiter) {
    char* token = *input;
    char* delimiter_position = strchr(*input, delimiter);
    
    if (delimiter_position != NULL) {
        *delimiter_position = '\0';
        *input = delimiter_position + 1;
    } else {
        *input = NULL;
    }
    
    return token;
}
