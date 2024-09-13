// TODO: Think about page table for minimizatio IO file operations
//       and move main operations to RAM

/*
 *  Directory is the next abstraction level, that work directly with pages
 *
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

    // Rewrite all line by EMPTY symbols
    //
    // directory - pointer to directory
    // offset - offset in directory
    // length - length of data to mark as delete
    //
    // Return 1 - if write was success
    // Return -1 - if index not found in page
    int DRM_delete_content(directory_t* directory, int offset, size_t length);

    // Append content to directory. This function move page_end symbol to new location
    // Note: If it can't append to existed pages, it creates new one
    // Note 2: This function not guarantees that content will be append to last page. 
    //         Content will be placed at first empty space with fit size
    //
    // directory - pointer to directory
    // data - data for append
    // data_lenght - lenght of data
    //
    // Return 2 if all success and content was append to new page
    // Return 1 if all success and content was append to existed page
    // Return 0 if write succes, but content was trunc
    // Return -1 if something goes wrong
    int DRM_append_content(directory_t* directory, uint8_t* data, size_t data_lenght);

    // Insert content to directory. This function don't move page_end in first empty page symbol to new location
    // Note: This function don't give ability for creation new pages. If content too large - it will trunc.
    //       To avoid this, use append_content function
    // Note 2: If directory don't have any pages - this function don't work
    //
    // directory - pointer to directory
    // offset - offset in bytes
    // data - data for append
    // data_lenght - lenght of data
    //
    // Return 1 if all success
    // Return 2 if write succes, but content was trunc
    // Return -1 if something goes wrong
    int DRM_insert_content(directory_t* directory, uint8_t offset, uint8_t* data, size_t data_lenght);

    // Find value in assosiatet pages
    //
    // directory - pointer to directory
    // offset - offset in bytes
    // value - value that we want to find
    //
    // Return -1 - if not found
    // Return index of value in page
    int DRM_find_value(directory_t* directory, int offset, uint8_t value);

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