/*
 *  CordellDBMS source code: https://github.com/j1sk1ss/CordellDBMS.EXMPL
 *  Credits: j1sk1ss
 */

#ifndef USER_H_
#define USER_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "hash.h"


#define USERNAME_SIZE   8
#define USER_BASE_PATH  getenv("USER_BASE_PATH") == NULL ? "" : getenv("USER_BASE_PATH")
#define USER_EXTENSION  getenv("USER_EXTENSION") == NULL ? "usr" : getenv("USER_EXTENSION")


typedef struct user {
    uint8_t name[USERNAME_SIZE];
    uint8_t access;
    uint32_t pass_hash;
} user_t;


/*
Create new user with provided name and access byte.

Params:
- name - Name of new user.
- password - Password of new user.
- access - Access of new user.

Return new allocated user in RAM.
*/
user_t* USR_create(char* name, char* password, uint8_t access);

/*
Load user by name and compare passwords.

Params:
- name - User name.
- password - User password.

Return NULL if auth. not complete.
Return allocated user struct if auth. was success.
*/
user_t* USR_auth(char* name, char* password);

/*
Load user by name or path from disk to RAM.
Note: If path is NULL, it will use default path.

Params:
- path - Path to user file. (Can be NULL, if provided name).
- name - Name of user. (Can be NULL, if provided path).

Return NULL if user file not found.
Return allocated user struct if file was load success.
*/
user_t* USR_load(char* path, char* name);

/*
Save allocated user struct to file.
Note: If path is NULL, function will generate default path to save.

Params:
- user - Pointer to user struct.
- path - Path to saving. (Can be NULL).

Return 1 if save was success.
Return -1 if something was wrong.
*/
int USR_save(user_t* user, char* path);

#endif
