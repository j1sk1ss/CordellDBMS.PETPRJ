#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#define USERNAME_SIZE   8
#define USER_BASE_PATH  getenv("USER_BASE_PATH") == NULL ? "" : getenv("USER_BASE_PATH")
#define USER_EXTENSION  getenv("USER_EXTENSION") == NULL ? "usr" : getenv("USER_EXTENSION")
#define SALT    "CordellDBMS_SHA"
#define MAGIC   8
#define CREATE_ACCESS_BYTE(read_access, write_access, delete_access) \
    (((read_access & 0b11) << 4) | ((write_access & 0b11) << 2) | (delete_access & 0b11))


typedef struct user {
    unsigned char name[USERNAME_SIZE];
    unsigned char access;
    unsigned int pass_hash;
} user_t;


unsigned static int _str2hash(const char* str) {
    unsigned int hashedValue = 0;
    for (int i = 0; str[i] != '\0'; i++) hashedValue ^= (hashedValue << MAGIC) + (hashedValue >> 2) + str[i];
    for (int i = 0; i < MAGIC; i++) hashedValue ^= (hashedValue << MAGIC) + (hashedValue >> 2) + SALT[i];
    return hashedValue;
}


/*
./prog <name> <pass> <read> <write> <delete>
*/
int main(int argc, char* argv[]) {
    printf("Name: %s, pass: %s, rwd: %s-%s-%s\n", argv[1], argv[2], argv[3], argv[4], argv[5]);
    user_t* empty_user = (user_t*)malloc(sizeof(user_t));
    if (!empty_user) return -1;
    memset(empty_user, 0, sizeof(user_t));

    strncpy(empty_user->name, argv[1], USERNAME_SIZE);
    empty_user->access = CREATE_ACCESS_BYTE(atoi(argv[3]), atoi(argv[4]), atoi(argv[5]));
    empty_user->pass_hash = _str2hash((const unsigned char*)argv[2]);

    char save_path[50] = { 0 };
    sprintf(save_path, "%s.%s", argv[1], USER_EXTENSION);

    FILE* file = fopen(save_path, "wb");
    if (file) {
        fwrite(empty_user, sizeof(user_t), 1, file);
        fclose(file);
    }
    
    free(empty_user);
    return 1;
}