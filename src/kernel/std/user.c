#include "../include/user.h"


user_t* USR_auth(char* name, char* password) {
#ifndef NO_USER
    user_t* user = USR_load(name);
    if (user == NULL) return NULL;
    if (str2hash(password) != user->pass_hash) return NULL;
    return user;
#endif
    return NULL;
}

user_t* USR_load(char* name) {
#ifndef NO_USER
    char load_path[128] = { 0 };
<<<<<<< HEAD:src/userland/std/user.c
    if (path == NULL && name != NULL) sprintf(load_path, "%s%.*s.%s", USER_BASE_PATH, USERNAME_SIZE, name, USER_EXTENSION);
    else if (path != NULL) strcpy_s(load_path, path);
    else return NULL;

=======
    sprintf(load_path, "%s/%.*s.%s", USER_BASE_PATH, USERNAME_SIZE, name, USER_EXTENSION);
    
>>>>>>> main:src/kernel/std/user.c
    FILE* file = fopen(load_path, "rb");
    if (file == NULL) return NULL;
    else {
        user_t* user = (user_t*)malloc(sizeof(user_t));
        if (!user) return NULL;
        if (fread(user, sizeof(user_t), 1, file) != 1) printf("[%s %i] Something goes wrong?\n", __FILE__, __LINE__);
        fclose(file);

        return user;
    }
#endif
    return NULL;
}
