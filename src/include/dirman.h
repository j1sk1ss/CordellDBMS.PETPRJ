#ifndef DIRMAN_H_
#define DIRMAN_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pageman.h"


#define DIRECTORY_MAGIC     0xCC
#define PAGES_PER_DIRECTORY 32
#define DIRECTORY_NAME_SIZE 8

#pragma region [Column]

    #define COLUMN_MAGIC        0xEA
    #define COLUMN_NAME_SIZE    8
    #define COLUMN_INT          0x00
    #define COLUMN_FLOAT        0x01
    #define COLUMN_STRING       0x02

#pragma endregion


// We have *.cdir bin file, where at start placed header
//====================================================================================================================
// HEADER (MAGIC | NAME | PAGE_COUNT | COLUMN_COUNT) -> | COLUMNS (TYPE | NAME, ... ) | PAGE_NAMES -> 8 * 256 -> end |
//====================================================================================================================

    struct directory_column {
        uint8_t type;
        uint8_t name[COLUMN_NAME_SIZE];
    } typedef directory_column_t;

    struct directory_header {
        // Magic namber for check
        uint8_t magic;

        // Directory filename
        // With this name we can save directories / compare directories
        uint8_t name[DIRECTORY_NAME_SIZE];

        // Page count in directory
        uint8_t page_count;

        // Count of columns
        uint8_t column_count;

        // TODO: Maybe add something like checksum?
        //       For fast comparing directories
    } typedef directory_header_t;

    struct directory {
        // Directory header
        directory_header_t* header;
        
        // Columns
        directory_column_t** columns;

        // Page file names
        uint8_t names[PAGES_PER_DIRECTORY][PAGE_NAME_SIZE];
    } typedef directory_t;


// Save directory on the disk
// directory - pointer to directory
// path - path where save
int save_directory(directory_t* directory, char* path);

// Link current page to this directory
// If sighnature wrong, return 0
// If all okey - return 1
// directory - home directory
// page - page for linking
int link_page2dir(directory_t* directory, page_t* page);

// Allocate memory and create new directory
// name - directory name
// col - column names (First symbol reserver for type)
// col_count - column count
directory_t* create_directory(char* name, directory_column_t* col[], int col_count);

// Open file, load directory and page names, close file
// name - file name (don`t forget path)
directory_t* load_directory(char* name);

// Release directory
// directory - pointer to directory
int free_directory(directory_t* directory);

#endif