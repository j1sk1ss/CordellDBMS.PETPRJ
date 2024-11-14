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
        if ((strlen(path) > 1) &&
            (
                ((path[1] == ':' ) &&
                (path[2] == '\\')) ||

                (path[0] == '\\') ||

                (path[0] == '/' ) ||

                ((path[0] == '.' ) &&
                (path[1] == '/' )) ||

                ((path[0] == '.' ) &&
                (path[1] == '\\'))
            )
        ) {
            sDelim = (char*)calloc(5, sizeof(char));
            /*  //   */ if (path[0] == '\\') iDelim = '\\', strcpy(sDelim, "\\");
            /*  c:\\ */ if (path[1] == ':' ) iDelim = '\\', strcpy(sDelim, "\\"); // also satisfies path[2] == '\\'
            /*  /    */ if (path[0] == '/' ) iDelim = '/' , strcpy(sDelim, "/" );
            /* ./    */ if ((path[0] == '.') && (path[1] == '/')) iDelim = '/' , strcpy(sDelim, "/" );
            /* .\\   */ if ((path[0] == '.') && (path[1] == '\\')) iDelim = '\\' , strcpy(sDelim, "\\" );
            /*  \\\\ */ if ((path[0] == '\\') && (path[1] == '\\')) iDelim = '\\', strcpy(sDelim, "\\");
            if (path[0] == '.') {
                rel = 1;
                path[0] = '*';
            }

            if (!strstr(path, ".")) {
                lenFullPath = strlen(path);
                if (path[lenFullPath - 1] != iDelim) {
                    strcat(path, sDelim);
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
                lenFullPath = strlen(path);

                strcpy(nameKeep, path);
                strcpy(pathKeep, path);
                strcpy(pathKeep2, path);
                if (path_ != NULL) strcpy(path_, path); // capture path

                // Get length of extension:
                for (i = lenFullPath - 1; i >= 0; i--) {
                    if (pathKeep[i] == '.') break;
                }

                lenExt_ = (lenFullPath - i) - 1;
                base = strtok(path, sDelim);
                while (base) {
                    strcpy(File_Ext, base);
                    base = strtok(NULL, sDelim);
                }


                strcpy(baseK, File_Ext);
                lenBase_ = strlen(baseK) - lenExt_;
                baseK[lenBase_ - 1] = 0;

                if (base_ != NULL) strcpy(base_, baseK);
                if (path_ != NULL) path_[lenFullPath -lenExt_ -lenBase_ -1] = 0;

                ext = strtok(File_Ext, ".");
                ext = strtok(NULL, ".");
                if (ext_ != NULL) {
                    if (ext) strcpy(ext_, ext);
                    else strcpy(ext_, "");
                }
            }

            memset(path, 0, lenFullPath);
            strcpy(path, pathKeep2);

            if (path_ != NULL) {
                if (rel) path_[0] = '.';
            }

            free(sDelim);
        }
    }
}

int get_load_path(char* name, int name_size, char* path, char* buffer, char* base_path, char* extension) {
    if (path == NULL && name != NULL) sprintf(buffer, "%s%.*s.%s", base_path, name_size, name, extension);
    else if (path != NULL) strcpy(buffer, path);
    else return -1;
    return 1;
}

int get_filename(char* name, char* path, char* buffer, int name_size) {
    if (name != NULL) strncpy(buffer, name, name_size);
    else if (path != NULL && name == NULL) {
        char temp_path[DEFAULT_PATH_SIZE] = { 0 };
        strcpy(temp_path, path);
        get_file_path_parts(temp_path, NULL, buffer, NULL);
    }

    return 1;
}

char* generate_unique_filename(char* base_path, int name_size, char* extension) {
    char* name = (char*)malloc(name_size * sizeof(char));
    char save_path[DEFAULT_PATH_SIZE] = { 0 };

    int offset = 0;
    while (1) {
        strrand(name, name_size, offset++);
        sprintf(save_path, "%s%.*s.%s", base_path, name_size, name, extension);

        if (file_exists(save_path)) {
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

int file_exists(const char* filename) {
    #ifdef _WIN32
        DWORD fileAttr = GetFileAttributes(filename);
        return (fileAttr != INVALID_FILE_ATTRIBUTES && !(fileAttr & FILE_ATTRIBUTE_DIRECTORY));
    #else
        struct stat buffer;
        return (stat(filename, &buffer) == 0);
    #endif
}
