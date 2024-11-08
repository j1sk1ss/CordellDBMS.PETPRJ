/*
TODO: Implement FS from CordellOS
*/
#include "../include/fat.h"


void* fopen(char* __restrict path, char* __restrict mode) {
    return NULL;
}

size_t fread(void* __restrict buffer, size_t size, size_t count, void* __restrict file) {
    return 0;
}

size_t fwrite(const void* __restrict buffer, size_t size, size_t count, void* __restrict file) {
    return 0;
}

int fsync(int desc) {
    return 0;
}

void* popen(const char* path, const char* mode) {
    return 0;
}

int pclose(void* file) {
    return 0;
}

int fileno(void* file) {
    return 0;
}

int	fseek(void* file, long pos, int size) {
    return 0;
}

int fclose(void* file) {
    return 0;
}
