#include "../include/common.h"


void strrand(char* dest, size_t length) {
    static int seeded = 0;
    if (!seeded) {
        srand(time(0));
        seeded = 1;
    }
    
    char charset[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    while (length-- > 0) {
        size_t index = (double) rand() / RAND_MAX * (sizeof charset - 1);
        *dest++ = charset[index];
    }
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
                nameKeep[0]   = 0; // works target C:\\dir1\file.txt
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
    char* time_str = (char*)malloc(20);
    if (time_str == NULL) return NULL;

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(time_str, 20, "%Y-%m-%d %H:%M:%S", timeinfo);

    return time_str;
}


int get_load_path(char* name, int name_size, char* path, char* buffer, char* base_path, char* extension) {
    if (path == NULL && name != NULL) sprintf(buffer, "%s%.*s.%s", base_path, name_size, name, extension);
    else if (path != NULL) strcpy(buffer, path);
    else return -1;
    return 1;
}

int get_filename(char* name, char* path, char* buffer, int name_size) {
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
        strrand(name, name_size);
        sprintf(save_path, "%s%.*s.%s", base_path, name_size, name, extension);

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
    uint8_t* source, int source_size, uint8_t* sub, int sub_size, uint8_t* new, int new_size, size_t* result_len
) {
    uint8_t* result;   // the return array
    uint8_t* ins;      // the next insert point
    uint8_t* tmp;      // varies
    size_t len_front;  // distance between source and end of last source
    int count;         // number of replacements

    if (!source || !sub || sub_size == 0) return NULL;
    if (!new) {
        new = (uint8_t*)"";
        new_size = 0;
    }

    ins = source;
    while ((tmp = (uint8_t*)memmem(ins, source_size - (ins - source), sub, sub_size)) != NULL) {
        ins = tmp + sub_size;
        count++;
    }

    *result_len = source_size + (new_size - sub_size) * count;
    result = (uint8_t*)malloc(*result_len + 1);
    if (!result) return NULL;
    memset(result, 0, *result_len + 1);

    tmp = result;
    ins = source;

    while (count--) {
        uint8_t* pos = (uint8_t*)memmem(ins, source_size - (ins - source), sub, sub_size);
        if (!pos) break;
        
        len_front = pos - ins;
        memcpy(tmp, ins, len_front);
        tmp += len_front;

        if (new_size > 0) {
            memcpy(tmp, new, new_size);
            tmp += new_size;
        }

        ins = pos + sub_size;
    }

    memcpy(tmp, ins, source_size - (ins - source));
    return result;
}

char* strrep(char* string, char* source, char* target) {
    char* result; // the return string
    char* ins;    // the next insert point
    char* tmp;    // varies
    int len_source;  // length of source (the string to remove)
    int len_target;  // length of target (the string to replace source target)
    int len_front;   // distance between source and end of last source
    int count;       // number of replacements

    // sanity checks and initialization
    if (!string || !source) return NULL;
    len_source = strlen(source);
    if (len_source == 0) return NULL; // empty source causes infinite loop during count
    if (!target) target = "";
    len_target = strlen(target);

    // count the number of replacements needed
    ins = string;
    for (count = 0; (tmp = strstr(ins, source)); ++count) {
        ins = tmp + len_source;
    }

    tmp = result = (char*)malloc(strlen(string) + (len_target - len_source) * count + 1);
    if (!result) return NULL;

    // first time through the loop, all the variable are set correctly
    // from here on,
    //    tmp points to the end of the result string
    //    ins points to the next occurrence of source in string
    //    string points to the remainder of string after "end of source"
    while (count--) {
        ins = strstr(string, source);
        len_front = ins - string;
        tmp = strncpy(tmp, string, len_front) + len_front;
        tmp = strcpy(tmp, target) + len_target;
        string += len_front + len_source; // move to next "end of source"
    }

    strcpy(tmp, string);
    return result;
}