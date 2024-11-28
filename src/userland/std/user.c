#include "../include/user.h"


user_t* USR_auth(char* name, char* password) {
    user_t* user = USR_load(NULL, name);
    if (user == NULL) return NULL;
    if (HASH_str2hash(password) != user->pass_hash) return NULL;
    return user;
}

user_t* USR_load(char* path, char* name) {
    char load_path[128] = { 0 };
    if (path == NULL && name != NULL) sprintf(load_path, "%s%.*s.%s", USER_BASE_PATH, USERNAME_SIZE, name, USER_EXTENSION);
    else if (path != NULL) strcpy(load_path, path);
    else return NULL;

    FILE* file = fopen(load_path, "rb");
    if (file == NULL) return NULL;
    else {
        user_t* user = (user_t*)malloc(sizeof(user_t));
        if (fread(user, sizeof(user_t), 1, file) != 1) printf("[%s %i] Something goes wrong?\n", __FILE__, __LINE__);
        fclose(file);

        return user;
    }

    return NULL;
}
