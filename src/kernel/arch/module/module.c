#include "../../include/module.h"


int MDL_launch_module(char* module_name, char* args, unsigned char* buffer, size_t buffer_size) {
    memset(buffer, ' ', buffer_size);
    char module_path[DEFAULT_PATH_SIZE] = { 0 };
    char command[256] = { 0 };
    char result[128] = { 0 };

    sprintf(module_path, "%s%s.%s", MODULE_BASE_PATH, module_name, MODULE_EXTENSION);
    if (!file_exists(module_path, NULL)) {
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
            size_t result_len = strlen(result);
            size_t offset = buffer_size > result_len ? buffer_size - result_len : 0;
            memcpy(buffer + offset, result, MIN(buffer_size, result_len));
        }
    }

    return 1;
}
