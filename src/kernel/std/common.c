#include "../include/common.h"


void rand_str(char *dest, size_t length) {
    srand(time(0));
    char charset[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    while (length-- > 0) {
        size_t index = (double) rand() / RAND_MAX * (sizeof charset - 1);
        *dest++ = charset[index];
    }

    *dest = '\0';
}

int is_integer(const char* str) {
    while (*str) {
        if (!isdigit(*str)) return 0;
        str++;
    }

    return 1;
}

int is_float(const char* str) {
    int has_dot = 0;
    while (*str) {
        if (*str == '.') {
            if (has_dot) return 0;
            has_dot = 1;
        }
        else if (!isdigit(*str)) return 0;

        str++;
    }

    return 1;
}

char* get_next_token(char** input, char delimiter) {
    char* token = *input;
    char* delimiter_position = strchr(*input, delimiter);
    
    if (delimiter_position != NULL) {
        *delimiter_position = '\0';
        *input = delimiter_position + 1;
    } 
    else *input = NULL;
    
    return token;
}

void get_file_path_parts(char* path, char* path_, char* base_, char* ext_) {
    char* base;
    char* ext;

    char nameKeep[256];
    char pathKeep[256];
    char pathKeep2[256];
    char File_Ext[40];
    char baseK[40];

    int lenFullPath, lenExt_, lenBase_;
    char* sDelim = { 0 };
    int iDelim = 0;
    int rel = 0, i;

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
                    if (ext_ != NULL) ext_[0]  = 0;
                }
            }
            else {
                nameKeep[0]   = 0; // works with C:\\dir1\file.txt
                pathKeep[0]   = 0;
                pathKeep2[0]  = 0; // preserves *path
                File_Ext[0]   = 0;
                baseK[0]      = 0;

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
                baseK[lenBase_-1]=0;
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
            } // replace first "." for relative path

            free(sDelim);
        }
    }
}
