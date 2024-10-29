#include "../include/sighandler.h"


void cleanup_handler() {
    cleanup_kernel();
}

int CL_enable(void) {
    atexit(cleanup_handler);
    return 1;
}