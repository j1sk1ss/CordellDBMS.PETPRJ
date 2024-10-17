#include "../include/user.h"


user_t* USR_create(char* name, char* password, uint8_t access) {
    user_t* user = (user_t*)malloc(sizeof(user_t));
    user->access = access;
    user->pass_hash = HASH_str2hash(password);
    memcpy(user->name, name, USERNAME_SIZE);
    return user;
}

user_t* USR_auth(char* name, char* password) {
    user_t* user = USR_load(NULL, name);
    if (user == NULL) return NULL;
    if (HASH_str2hash(password) != user->pass_hash) return NULL;
    return user;
}

user_t* USR_load(char* path, char* name) {
    char load_path[128];
    if (path == NULL && name != NULL) sprintf(load_path, "%s%.8s.%s", USER_BASE_PATH, name, USER_EXTENSION);
    else if (path != NULL) strcpy(load_path, path);
    else return NULL;

    user_t* user = NULL;
    #pragma omp critical (usr_load)
    {
        FILE* file = fopen(load_path, "rb");
        if (file == NULL) user = NULL;
        else {
            user = (user_t*)malloc(sizeof(user_t));
            fread(user, sizeof(user_t), 1, file);
            fclose(file);
        }
    }

    return user;
}

int USR_save(user_t* user, char* path) {
    char save_path[128];
    if (path == NULL) sprintf(save_path, "%s%.8s.%s", USER_BASE_PATH, user->name, USER_EXTENSION);
    else strcpy(save_path, path);

    int result = 0;
    #pragma omp critical (usr_save)
    {
        FILE* file = fopen(save_path, "wb");
        if (file == NULL) result = -1;
        else {
            if (fwrite(user, sizeof(user_t), 1, file) != 1) result = -2;
            fclose(file);
        }
    }

    return result;
}
