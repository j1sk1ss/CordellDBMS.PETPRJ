#include "../../include/module.h"


int MDL_launch_module(char* module_name, char* args, uint8_t* buffer, size_t buffer_size) {
    memset(buffer, 0, buffer_size);
    print_debug("Module [%s] invoked with [%s] args", module_name, args);

    char module_path[DEFAULT_PATH_SIZE];
    char command[256];
    char result[128];

    sprintf(module_path, "%s%.*s.%s", MODULE_BASE_PATH, MODULE_NAME_SIZE, module_name, MODULE_EXTENSION);
    if (!file_exists(module_path)) {
        print_warn("Module not found");
        return -1;
    }

    snprintf(command, sizeof(command), "./%s %s", module_path, args);
    FILE* fp = popen(command, "r");
    if (fp == NULL) {
        print_error("Error executing command");
        return -2;
    }

    fgets(result, sizeof(result), fp);

    int status = pclose(fp);
    if (status == -1) print_error("Error closing pipe");
    else {
        int exit_code = WEXITSTATUS(status);
        print_log("Module [%s] exit code: [%d]", module_name, exit_code);
        if (exit_code == 100) {
            print_debug("Result: [%s]", result);
            size_t result_len = strlen(result);
            size_t offset = buffer_size > result_len ? buffer_size - result_len : 0;
            memcpy(buffer + offset, result, MIN(buffer_size, result_len));
        }
    }

    return 1;
}
