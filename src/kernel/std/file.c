#include "../include/common.h"


int get_load_path(char* name, int name_size, char* buffer, char* base_path, char* extension) {
    sprintf(buffer, "%s/%.*s.%s", base_path, name_size, name, extension);
    return 1;
}

char* generate_unique_filename(char* base_path, int name_size, char* extension) {
    char* name = (char*)malloc(name_size);
    if (!name) return NULL;
    memset(name, 0, name_size);

    int offset = 0;
    while (1) {
        strrand(name, name_size, offset++);
        char save_path[DEFAULT_PATH_SIZE] = { 0 };
        get_load_path(name, name_size, save_path, base_path, extension);

        if (file_exists(save_path, base_path, name)) {
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

int file_exists(const char* path, char* base_path, const char* filename) {
    int status = 0;
    #ifdef _WIN32
        DWORD fileAttr = GetFileAttributes(path);
        status = (fileAttr != INVALID_FILE_ATTRIBUTES && !(fileAttr & FILE_ATTRIBUTE_DIRECTORY));
    #else
        struct stat buffer;
        status = (stat(path, &buffer) == 0);
    #endif

    if (filename == NULL) return status;
    if (CHC_find_entry((char*)filename, base_path, ANY_CACHE) == NULL) return status;
    else return 1;
}

int delete_file(const char* filename, const char* basepath, const char* extension) {
    char delete_path[DEFAULT_PATH_SIZE] = { 0 };
    get_load_path((char*)filename, strlen(filename), (char*)delete_path, (char*)basepath, (char*)extension);
    return remove(delete_path);
}

#ifdef _WIN32

    intptr_t pwrite(int fd, const void *buf, size_t count, long long int offset) {
        HANDLE hFile = (HANDLE)_get_osfhandle(fd);
        if (hFile == INVALID_HANDLE_VALUE) return -1;

        OVERLAPPED ol = {0};
        ol.Offset = offset & 0xFFFFFFFF;
        ol.OffsetHigh = (offset >> 32) & 0xFFFFFFFF;

        DWORD written;
        BOOL success = WriteFile(hFile, buf, (DWORD)count, &written, &ol);

        return success ? written : -1;
    }

    intptr_t pread(int fd, void *buf, size_t count, long long int offset) {
        HANDLE hFile = (HANDLE)_get_osfhandle(fd);
        if (hFile == INVALID_HANDLE_VALUE) return -1;

        OVERLAPPED ol = {0};
        ol.Offset = offset & 0xFFFFFFFF;
        ol.OffsetHigh = (offset >> 32) & 0xFFFFFFFF;

        DWORD bytesRead;
        BOOL success = ReadFile(hFile, buf, (DWORD)count, &bytesRead, &ol);

        return success ? bytesRead : -1;
    }

#endif