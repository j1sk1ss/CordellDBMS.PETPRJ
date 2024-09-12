/*
 *  CordellDBMS source code: https://github.com/j1sk1ss/CordellDBMS.EXMPL
 *  Credits: j1sk1ss
 */

#ifndef DIRMAN_H_
#define DIRMAN_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pageman.h"


#define DIRECTORY_MAGIC     0xCC
#define PAGES_PER_DIRECTORY 255
#define DIRECTORY_NAME_SIZE 8


// We have *.dr bin file, where at start placed header
//====================================================================================================================
// HEADER (MAGIC | NAME | PAGE_COUNT | COLUMN_COUNT) -> | COLUMNS (TYPE | NAME, ... ) | PAGE_NAMES -> 8 * 256 -> end |
//====================================================================================================================

    struct directory_header {
        // Magic namber for check
        uint8_t magic;

        // Directory filename
        // With this name we can save directories / compare directories
        uint8_t name[DIRECTORY_NAME_SIZE];

        // Page count in directory
        uint8_t page_count;

        // TODO: Maybe add something like checksum?
        //       For fast comparing directories
    } typedef directory_header_t;

    struct directory {
        // Directory header
        directory_header_t* header;

        // Page file names
        uint8_t names[PAGES_PER_DIRECTORY][PAGE_NAME_SIZE];
    } typedef directory_t;


#pragma region [Pages]

    // TODO

#pragma endregion

#pragma region [Directory]

    // Save directory on the disk
    //
    // directory - pointer to directory
    // path - path where save
    //
    // Return 0 - if something goes wrong
    // Return 1 - if Release was success
    int DRM_save_directory(directory_t* directory, char* path);

    // Link current page to this directory
    //
    // directory - home directory
    // page - page for linking
    //
    // If sighnature wrong, return 0
    // If all okey - return 1
    int DRM_link_page2dir(directory_t* directory, page_t* page);

    // Allocate memory and create new directory
    //
    // name - directory name
    // col - column names (First symbol reserver for type)
    // col_count - column count
    directory_t* DRM_create_directory(char* name);

    // Open file, load directory and page names, close file
    //
    // name - file name (don`t forget path)
    directory_t* DRM_load_directory(char* name);

    // Release directory
    //
    // directory - pointer to directory
    //
    // Return 0 - if something goes wrong
    // Return 1 - if Release was success
    int DRM_free_directory(directory_t* directory);

#pragma endregion

#endif