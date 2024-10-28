#include "../include/sighandler.h"


void cleanup_handler() {
    TBM_TDT_free();
    DRM_DDT_free();
    PGM_PDT_free();
}

int CL_enable(void) {
    atexit(cleanup_handler);
    return 1;
}