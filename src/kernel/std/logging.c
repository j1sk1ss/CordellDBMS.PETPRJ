#include "../include/logging.h"


static int _log_size = 0;
static FILE* _log_file = NULL;
static char* _log_file_name = NULL;


void _write_log(const char* level, const char* file, int line, const char* message, va_list args) {
    #pragma omp critical
    {
        FILE* log_output = stdout;
        #ifdef LOG_TO_FILE
            if (_log_file_name == NULL) _log_file_name = generate_unique_filename(LOG_FILE_PATH, LOG_FILE_NAME_SIZE, LOG_FILE_EXTENSION);
            if (_log_file_name != NULL && _log_file == NULL) {
                char log_path[DEFAULT_BUFFER_SIZE] = { 0 };
                get_load_path(_log_file_name, LOG_FILE_NAME_SIZE, log_path, LOG_FILE_PATH, LOG_FILE_EXTENSION);

                _log_file = fopen(log_path, "a");
            }

            if (_log_file) log_output = _log_file;
        #endif

        if (level == NULL) level = "(null)";
        if (file == NULL) file = "(null)";
        if (message == NULL) message = "(null)";

        fprintf(log_output, "[%s] [%s] (%s:%i) ", level, get_current_time(), file, line);
        vfprintf(log_output, message, args);
        fprintf(log_output, "\n");

        #ifdef LOG_TO_FILE
            if (log_output != stdout) {
                if (_log_size++ >= LOG_FILE_SIZE) {
                    fclose(_log_file);
                    SOFT_FREE(_log_file_name);
                    
                    _log_file = NULL;
                    _log_size = 0;
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