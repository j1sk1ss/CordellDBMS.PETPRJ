#include "../include/common.h"


int isdigit_s(int c) {
    return (c >= '0' && c <= '9');
}

int isspace_s(int c) {
    return (c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '\v' ||
            c == '\b');
}

int islower_s(int c) {
    return c >= 'a' && c <= 'z';
}

int tolower_s(int c) {
	if (!islower_s(c)) return c | 32;
	return c;
}

int toupper_s(int c) {
    if (islower_s(c)) return c - 'a' + 'A';
    else return c;
}