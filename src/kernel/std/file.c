#include "../include/common.h"


void get_file_path_parts(char* path, char* path_, char* base_, char* ext_) {
    char* base = NULL;
    char* ext  = NULL;

    char nameKeep[DEFAULT_BUFFER_SIZE]  = { 0 };
    char pathKeep[DEFAULT_BUFFER_SIZE]  = { 0 };
    char pathKeep2[DEFAULT_BUFFER_SIZE] = { 0 };
    char File_Ext[40] = { 0 };
    char baseK[40]    = { 0 };

    int lenFullPath = 0, lenExt_ = 0, lenBase_ = 0;
    char* sDelim = NULL;
    int iDelim = 0;
    int rel = 0, i = 0;

    if (path) {
        if ((strlen_s(path) > 1) &&
            (
                ((path[1] == ':' ) &&
                (path[2] == '\\')) ||

                ((path[0] == '.' ) &&
                (path[1] == '/' )) ||

                ((path[0] == '.' ) &&
                (path[1] == '\\')) ||
                
                (path[0] == '\\') ||
                (path[0] == '/' )
            )
        ) {
            sDelim = (char*)calloc(5, sizeof(char));
            /*  //   */ if (path[0] == '\\') iDelim = '\\', strcpy_s(sDelim, "\\");
            /*  c:\\ */ if (path[1] == ':' ) iDelim = '\\', strcpy_s(sDelim, "\\"); // also satisfies path[2] == '\\'
            /*  /    */ if (path[0] == '/' ) iDelim = '/' , strcpy_s(sDelim, "/" );
            /* ./    */ if ((path[0] == '.') && (path[1] == '/')) iDelim = '/' , strcpy_s(sDelim, "/" );
            /* .\\   */ if ((path[0] == '.') && (path[1] == '\\')) iDelim = '\\' , strcpy_s(sDelim, "\\" );
            /*  \\\\ */ if ((path[0] == '\\') && (path[1] == '\\')) iDelim = '\\', strcpy_s(sDelim, "\\");
            if (path[0] == '.') {
                rel = 1;
                path[0] = '*';
            }

            if (!strstr_s(path, ".")) {
                lenFullPath = strlen_s(path);
                if (path[lenFullPath - 1] != iDelim) {
                    strcat_s(path, sDelim);
                    if (path_ != NULL) path_[0] = 0;
                    if (base_ != NULL) base_[0] = 0;
                    if (ext_ != NULL) ext_[0] = 0;
                }
            }
            else {
                nameKeep[0]  = 0; // works target C:\\dir1\file.txt
                pathKeep[0]  = 0;
                pathKeep2[0] = 0; // preserves *path
                File_Ext[0]  = 0;
                baseK[0]     = 0;

                // Get lenth of full path
                lenFullPath = strlen_s(path);

                strcpy_s(nameKeep, path);
                strcpy_s(pathKeep, path);
                strcpy_s(pathKeep2, path);
                if (path_ != NULL) strcpy_s(path_, path); // capture path

                // Get length of extension:
                for (i = lenFullPath - 1; i >= 0; i--) {
                    if (pathKeep[i] == '.') break;
                }

                lenExt_ = (lenFullPath - i) - 1;
                base = strtok_s(path, sDelim);
                while (base) {
                    strcpy_s(File_Ext, base);
                    base = strtok_s(NULL, sDelim);
                }


                strcpy_s(baseK, File_Ext);
                lenBase_ = strlen_s(baseK) - lenExt_;
                baseK[lenBase_ - 1] = 0;

                if (base_ != NULL) strcpy_s(base_, baseK);
                if (path_ != NULL) path_[lenFullPath -lenExt_ -lenBase_ -1] = 0;

                ext = strtok_s(File_Ext, ".");
                ext = strtok_s(NULL, ".");
                if (ext_ != NULL) {
                    if (ext) strcpy_s(ext_, ext);
                    else strcpy_s(ext_, "");
                }
            }

            memset_s(path, 0, lenFullPath);
            strcpy_s(path, pathKeep2);

            if (path_ != NULL) {
                if (rel) path_[0] = '.';
            }

            free(sDelim);
        }
    }
}

int get_load_path(char* name, int name_size, char* path, char* buffer, char* base_path, char* extension) {
    if (path == NULL && name != NULL) sprintf(buffer, "%s%.*s.%s", base_path, name_size, name, extension);
    else if (path != NULL) strcpy_s(buffer, path);
    else return -1;
    return 1;
}

int get_filename(char* name, char* path, char* buffer, int name_size) {
    if (name != NULL) strncpy_s(buffer, name, name_size);
    else if (path != NULL && name == NULL) {
        char temp_path[DEFAULT_PATH_SIZE] = { 0 };
        strcpy_s(temp_path, path);
        get_file_path_parts(temp_path, NULL, buffer, NULL);
    }

    return 1;
}

char* generate_unique_filename(char* base_path, int name_size, char* extension) {
    char* name = (char*)malloc(name_size * sizeof(char));
    memset_s(name, 0, name_size * sizeof(char));

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
