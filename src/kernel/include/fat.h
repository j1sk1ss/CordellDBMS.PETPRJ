#ifndef IO_H_
#define IO_H_

#include <stdlib.h>
#include <stdio.h>


void* fopen(char* __restrict path, char* __restrict mode);

size_t fread(void* __restrict buffer, size_t size, size_t count, void* __restrict file);

size_t fwrite(const void* __restrict buffer, size_t size, size_t count, void* __restrict file);

int fsync(int desc);

void* popen(const char* path, const char* mode);

int pclose(void* file);

int fileno(void* file);

int	fseek(void* file, long pos, int size);

int fclose(void* file);

#endif