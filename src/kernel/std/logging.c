#include "../include/logging.h"


void _write_log(const char* level, const char* file, int line, const char* message, va_list args) {
    #pragma omp critical
    {
        FILE* log_output = stdout;
        #ifdef LOG_TO_FILE
            static int log_size = 0;
            static FILE* log_file = NULL;
            static char* log_file_path = NULL;
            
            if (log_file_path == NULL) log_file_path = generate_unique_filename(LOG_FILE_PATH, LOG_FILE_NAME_SIZE, LOG_FILE_EXTENSION);
            if (log_file_path != NULL && log_file == NULL) {
                log_file = fopen(log_file_path, "a");
                log_output = log_file;
                if (log_output == NULL) log_output = stdout;
            }
        #endif

        if (level == NULL) level = "(null)";
        if (file == NULL) file = "(null)";
        if (message == NULL) message = "(null)";

        fprintf(log_output, "[%s] [%s] (%s:%i) ", level, get_current_time(), file, line);
        vfprintf(log_output, message, args);
        fprintf(log_output, "\n");

        #ifdef LOG_TO_FILE
            if (log_output != stdout) {
                if (log_size++ >= LOG_FILE_SIZE) {
                    fclose(log_file);
                    SOFT_FREE(log_file_path);
                    
                    log_file = NULL;
                    log_size = 0;
                }
            }
        #endif
    }
}

void log_message(const char* level, const char* file, int line, const char* message, ...) {
    va_list args;
    va_start(args, message);
    _write_log(level, file, line, message, args);
    va_end(args);
}