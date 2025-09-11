#include <common.h>

inline int get_load_path(char* name, int name_size, char* buffer, char* base_path, char* extension) {
    sprintf(buffer, "%s/%.*s.%s", base_path, name_size, name, extension);
    path_to_83(buffer);
    return 1;
}

char* generate_unique_filename(char* base_path, int name_size, char* extension) {
    char* name = (char*)malloc_s(name_size);
    if (!name) return NULL;
    str_memset(name, 0, name_size);

    int offset = 0;
    while (1) {
        strrand(name, name_size, offset++);
        char save_path[DEFAULT_PATH_SIZE] = { 0 };
        get_load_path(name, name_size, save_path, base_path, extension);

        if (file_exists(save_path, base_path, name)) {
            if (!name[0]) {
                free_s(name);
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

int file_exists(const char* path, char* base_path, const char* filename) {
    path_to_83(path);
    int status = NIFAT32_content_exists(path);
    if (!filename) return status;
    if (!CHC_find_entry(filename, base_path, ANY_CACHE)) return status;
    else return 1;
}

int delete_file(const char* filename, const char* basepath, const char* extension) {
    char delete_path[DEFAULT_PATH_SIZE] = { 0 };
    get_load_path((char*)filename, str_strlen(filename), delete_path, basepath, extension);
    char delete_path83[DEFAULT_PATH_SIZE] = { 0 };
    path_to_fatnames(delete_path, delete_path83);

    ci_t ci = NIFAT32_open_content(NO_RCI, delete_path83, DF_MODE);
    return ci >= 0 ? NIFAT32_delete_content(ci) : 0;
}