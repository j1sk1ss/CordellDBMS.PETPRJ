#ifndef USER_H_
#define USER_H_

#include <stdint.h>
#include <stdlib.h>

#include "crypto.h"


#define USERNAME_SIZE   8
#define USER_BASE_PATH  getenv("USER_BASE_PATH") == NULL ? "" : getenv("USER_BASE_PATH")
#define USER_EXTENSION  getenv("USER_EXTENSION") == NULL ? "usr" : getenv("USER_EXTENSION")


typedef struct user {
    uint8_t name[USERNAME_SIZE];
    uint8_t access;
    uint8_t pass_hash[SHA256_BLOCK_SIZE];
} user_t;


user_t* USR_create(char* name, char* password, uint8_t access);
user_t* USR_auth(char* name, char* password);
user_t* USR_load(char* path, char* name);
int USR_save(user_t* user, char* path);

#endif
