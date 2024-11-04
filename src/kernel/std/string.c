#include "../include/common.h"


void strrand(char* dest, size_t length) {
    static int seeded = 0;
    if (!seeded) {
        srand(time(0));
        seeded = 1;
    }
    
    char charset[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    while (length-- > 1) {
        size_t index = (double)rand() / RAND_MAX * (sizeof charset - 1);
        *dest++ = charset[index];
    }

    *dest = '\0';
}

int is_integer(const char* str) {
    while (*str) {
        if (!isdigit(*str)) return 0;
        str++;
    }

    return 1;
}

int is_float(const char* str) {
    int has_dot = 0;
    while (*str) {
        if (*str == '.') {
            if (has_dot) return 0;
            has_dot = 1;
        }
        else if (!isdigit(*str)) return 0;

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
    }
    else *input = NULL;

    return token;
}

char* strrep(char* __restrict string, char* __restrict source, char* __restrict target) {
    char* result = NULL; // the return string
    char* ins = NULL;    // the next insert point
    char* tmp = NULL;    // varies
    int len_source = 0;  // length of source (the string to remove)
    int len_target = 0;  // length of target (the string to replace source target)
    int len_front = 0;   // distance between source and end of last source
    int count = 0;       // number of replacements

    // sanity checks and initialization
    if (!string || !source) return NULL;
    len_source = strlen(source);
    
    if (len_source == 0) return NULL; // empty source causes infinite loop during count
    if (!target) target = "";
    len_target = strlen(target);

    // count the number of replacements needed
    ins = string;
    for (count = 0; (tmp = strstr(ins, source)); ++count) {
        ins = tmp + len_source;
    }

    tmp = result = (char*)malloc(strlen(string) + (len_target - len_source) * count + 1);
    if (!result) return NULL;

    // first time through the loop, all the variable are set correctly
    // from here on,
    //    tmp points to the end of the result string
    //    ins points to the next occurrence of source in string
    //    string points to the remainder of string after "end of source"
    while (count--) {
        ins = strstr(string, source);
        len_front = ins - string;
        tmp = strncpy(tmp, string, len_front) + len_front;
        tmp = strcpy(tmp, target) + len_target;
        string += len_front + len_source; // move to next "end of source"
    }

    strcpy(tmp, string);
    return result;
}