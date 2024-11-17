#include "../include/common.h"


char* get_current_time() {
    static char time_str[72];
    time_t t = time(NULL);
    struct tm* tm_info = localtime(&t);
    snprintf(time_str, sizeof(time_str), "%04d-%02d-%02d %02d:%02d:%02d",
             tm_info->tm_year + 1900,
             tm_info->tm_mon + 1,
             tm_info->tm_mday,
             tm_info->tm_hour,
             tm_info->tm_min,
             tm_info->tm_sec);

    return time_str;
}
