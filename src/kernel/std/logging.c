#include "../include/logging.h"


void write_log(const char* level, const char* file, int line, const char* message, va_list args) {
    FILE* log_output = stdout;

    #ifdef LOG_TO_FILE
        if ((LOG_FILE_PATH)[0] != '\0') {
            log_output = fopen((LOG_FILE_PATH), "a");
            if (log_output == NULL) log_output = stdout;
        }
    #endif

    char* time = get_current_time();
    if (message == NULL) message = "(null)";
    if (time != NULL) fprintf(log_output, "[%s] [%s] (%s:%i) ", level, time, file, line);
    SOFT_FREE(time);

    vfprintf(log_output, message, args);
    fprintf(log_output, "\n");

    #ifdef LOG_TO_FILE
        if (log_output != stdout)
            fclose(log_output);
    #endif
}

void log_message(const char* level, const char* file, int line, const char* message, ...) {
    va_list args;
    va_start(args, message);
    write_log(level, file, line, message, args);
    va_end(args);
}