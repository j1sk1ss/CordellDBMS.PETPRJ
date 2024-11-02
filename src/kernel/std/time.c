#include "../include/common.h"


char* get_current_time() {
    time_t rawtime;
    struct tm* timeinfo;
    char* time_str = (char*)malloc(20);
    if (time_str == NULL) return NULL;

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(time_str, 20, "%Y-%m-%d %H:%M:%S", timeinfo);

    return time_str;
}
