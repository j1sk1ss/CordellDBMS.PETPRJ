#ifndef FILE_MANAGER_
#define FILE_MANAGER_

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>


#define CONTENT_SIZE 256

// We have bin file, where at start placed info-structure
// Info structure that have 281 bytes size (content include)
//=======================================================================================================
// NAME -> 8 -> | ACCESS | CONTENT_SIZE | PREV_NAME -> 8 -> | NEXT_NAME -> 8 -> | CONTENT -> 256 -> end |
//=======================================================================================================

struct part_data {
    uint8_t name[8];
    uint8_t access;

    // Linked list implementation
    uint8_t prev_name[8];
    uint8_t next_name[8];

    // Content
    uint8_t content[CONTENT_SIZE];
} typedef part_data_t;


// Open file, load part, close file
// TODO: Maybe it will take too much time? Maybe save FILE descriptor?
part_data_t* load_part(char* name);
void free_part(part_data_t* part);

#endif