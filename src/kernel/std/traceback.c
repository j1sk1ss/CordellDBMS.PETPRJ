#include "../include/sighandler.h"


void traceback_handler(int sig) {
    #ifndef _WIN32
        void* array[10] = { NULL };
        size_t size = 0;

        size = backtrace(array, 10);

        fprintf(stderr, "Error: signal %d:\n", sig);
        backtrace_symbols_fd(array, size, STDERR_FILENO);
        exit(1);
    #endif
}

int TB_enable(void) {
    signal(SIGSEGV, traceback_handler);
    signal(SIGFPE, traceback_handler);
    return 1;
}
