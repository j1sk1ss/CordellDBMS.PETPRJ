#include "../include/common.h"


void strrand(char* dest, size_t length, int offset) {
    char charset[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    size_t charset_size = sizeof(charset) - 1;
    
    for (size_t i = 0; i < length - 1; i++) {
        int index = offset % charset_size;
        if (index > (int)strlen(charset)) {
            dest[0] = 0;
            return;
        }

        dest[i] = charset[index];
        offset /= charset_size;
    }

    dest[length - 1] = '\0';
    for (size_t i = 0, j = length - 2; i < j; i++, j--) {
        char temp = dest[i];
        dest[i] = dest[j];
        dest[j] = temp;
    }
}

int is_integer(const char* str) {
    while (*str) {
        if (!isdigit(*str)) return 0;
        str++;
    }

    return 1;
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