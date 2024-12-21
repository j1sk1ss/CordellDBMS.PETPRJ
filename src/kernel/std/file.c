#include "../include/common.h"


int get_load_path(char* name, int name_size, char* buffer, char* base_path, char* extension) {
    sprintf(buffer, "%s%.*s.%s", base_path, name_size, name, extension);
    return 1;
}

char* generate_unique_filename(char* base_path, int name_size, char* extension) {
    char* name = (char*)malloc(name_size);
    memset(name, 0, name_size);

    int offset = 0;
    while (1) {
        strrand(name, name_size, offset++);
        char save_path[DEFAULT_PATH_SIZE] = { 0 };
        sprintf(save_path, "%s%.*s.%s", base_path, name_size, name, extension);

        if (file_exists(save_path, name)) {
            if (name[0] == 0) {
                free(name);
                return NULL;
            }
        }
        else {
            // File not found, no memory leak since 'file' == NULL
            // fclose(file) would cause an error
            break;
        }
    }

    return name;
}

int file_exists(const char* path, const char* filename) {
    int status = 0;
    #ifdef _WIN32
        DWORD fileAttr = GetFileAttributes(path);
        status = (fileAttr != INVALID_FILE_ATTRIBUTES && !(fileAttr & FILE_ATTRIBUTE_DIRECTORY));
    #else
        struct stat buffer;
        status = (stat(path, &buffer) == 0);
    #endif

    if (filename == NULL) return status;
    if (CHC_find_entry((char*)filename, ANY_CACHE) == NULL) return status;
    else return 1;
}

int delete_file(const char* filename, const char* basepath, const char* extension) {
    char delete_path[DEFAULT_PATH_SIZE] = { 0 };
    get_load_path((char*)filename, strlen(filename), (char*)delete_path, (char*)basepath, (char*)extension);
    return remove(delete_path);
}
