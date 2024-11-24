/*
 *  CordellDBMS source code: https://github.com/j1sk1ss/CordellDBMS.EXMPL
 *  Credits: j1sk1ss
 */

#ifndef USER_H_
#define USER_H_

#include <stdlib.h>
#include <stdio.h>

#include "hash.h"


#define USERNAME_SIZE   8
#define USER_BASE_PATH  getenv("USER_BASE_PATH") == NULL ? "" : getenv("USER_BASE_PATH")
#define USER_EXTENSION  getenv("USER_EXTENSION") == NULL ? "usr" : getenv("USER_EXTENSION")


typedef struct user {
    unsigned char name[USERNAME_SIZE];
    unsigned char access;
    unsigned int pass_hash;
} user_t;


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

#endif
