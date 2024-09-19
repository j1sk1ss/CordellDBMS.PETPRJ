#include "../include/traceback.h"


void handler(int sig) {
    #ifndef _WIN32
    void* array[10];
    size_t size;

    size = backtrace(array, 10);

    fprintf(stderr, "Error: signal %d:\n", sig);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    exit(1);
    #endif
}

int TB_enable(void) {
    signal(SIGSEGV, handler);
    return 1;
}
