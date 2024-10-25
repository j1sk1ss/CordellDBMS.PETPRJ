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

char* get_current_time() {
    time_t rawtime;
    struct tm* timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    char* time_str = asctime(timeinfo);
    time_str[strlen(time_str) - 1] = '\0';

    return time_str;
}

int get_load_path(char* name, char* path, char* buffer, char* base_path, char* extension) {
    if (path == NULL && name != NULL) sprintf(buffer, "%s%.8s.%s", base_path, name, extension);
    else if (path != NULL) strcpy(buffer, path);
    else return -1;
    return 1;
}

int get_filename(char* name, char* path, char* buffer, size_t name_size) {
    if (path != NULL) {
        char temp_path[DEFAULT_PATH_SIZE];
        strcpy(temp_path, path);
        get_file_path_parts(temp_path, NULL, buffer, NULL);
    }
    else if (name != NULL) {
        strncpy(buffer, name, name_size);
    }

    return 1;
}

char* generate_unique_filename(char* base_path, int name_size, char* extension) {
    char* name = (char*)malloc(name_size * sizeof(char));
    char save_path[512];

    int delay = DEFAULT_DELAY;
    while (1) {
        rand_str(name, name_size);
        sprintf(save_path, "%s%.8s.%s", base_path, name, extension);

        if (file_exists(save_path)) {
            if (--delay <= 0) {
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

uint8_t* memrep(
    uint8_t* source, int source_size, uint8_t* sub, int sub_size, uint8_t* new, int new_size, size_t *result_len
) {
    uint8_t* result;   // the return array
    uint8_t* ins;      // the next insert point
    uint8_t* tmp;      // varies
    size_t len_front;  // distance between rep and end of last rep
    int count;         // number of replacements

    // sanity checks and initialization
    if (!source || !sub || sub_size == 0) return NULL;
    if (!new) {
        new = (uint8_t *)"";
        new_size = 0;
    }

    // count the number of replacements needed
    ins = source;
    for (count = 0; (tmp = (uint8_t *)memmem(ins, source_size - (ins - source), sub, sub_size)); ++count) {
        ins = tmp + sub_size;
    }

    // Calculate the length of the result array
    *result_len = source_size + (new_size - sub_size) * count;
    result = (uint8_t *)malloc(*result_len);
    if (!result) {
        return NULL;
    }

    tmp = result;
    ins = source;

    // Perform the replacements
    while (count--) {
        uint8_t *pos = (uint8_t *)memmem(ins, source_size - (ins - source), sub, sub_size);
        len_front = pos - ins;
        
        // Copy segment before the match
        memcpy(tmp, ins, len_front);
        tmp += len_front;

        // Copy the replacement segment
        memcpy(tmp, new, new_size);
        tmp += new_size;

        // Advance the original array pointer past the matched segment
        ins = pos + sub_size;
    }

    // Copy the remainder of the original array after the last match
    memcpy(tmp, ins, source_size - (ins - source));
    return result;
}