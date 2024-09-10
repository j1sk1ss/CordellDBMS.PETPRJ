/*
 *  Table operator is a simple functions to work with OS filesystem.
 *  Main idea that we have files with sizes near 1 KB for pages, dirs and tables.
 *  Table contains list of dirs (names of file). In same time, every directory contain list pf pages (names of file).
 * 
 *  CordellDBMS source code: https://github.com/j1sk1ss/CordellDBMS.EXMPL
 *  Credits: j1sk1ss
 */

#ifndef FILE_MANAGER_
#define FILE_MANAGER_

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>


#define PAGE_MAGIC  0xC07DE
#define DIRECTORY_MAGIC 0xC08De


#pragma region [Pages]

// We have *.cpg bin file, where at start placed header
//================================================
// INDEX | CONTENT_SIZE | CONTENT -> size -> end |
//================================================

    struct page_header {
        // Magic namber for check
        uint8_t magic;

        // Page filename
        // With this name we can save pages / compare pages
        uint8_t name[8];

        // Page content size
        uint8_t content_size;

        // TODO: Maybe add something like checksum?
        //       For fast comparing pages
    } typedef page_header_t;

    struct page {
        page_header_t* header;
        uint8_t* content;
    } typedef page_t;

#pragma endregion

#pragma region [Directories]

// We have *.cdir bin file, where at start placed header
//==============================================
// PAGE_COUNT | PAGE_NAMES -> 8 * count -> end |
//==============================================

    struct directory_header {
        // Magic namber for check
        uint8_t magic;

        // Directory filename
        // With this name we can save directories / compare directories
        uint8_t name[8];

        // Page count in directory
        uint8_t page_count;

        // TODO: Maybe add something like checksum?
        //       For fast comparing directories
    } typedef directory_header_t;

    struct directory {
        // Directory header
        directory_header_t* header;
        
        // Page file names
        uint8_t** names;
    } typedef directory_t;

#pragma endregion


// Save directory on the disk
// directory - pointer to directory
// path - path where save
int save_directory(directory_t* directory, char* path);

// Open file, load directory and page names, close file
// name - file name (don`t forget path)
directory_t* load_directory(char* name);

// Release directory
// directory - pointer to directory
int free_directory(directory_t* directory);

// Save page on disk
// page - pointer to page
// path - path where save
int save_page(page_t* page, char* path);

// Open file, load page, close file
// name - page name (don`t forget path)
// TODO: Maybe it will take too much time? Maybe save FILE descriptor?
page_t* load_page(char* name);

// Release page
// page - pointer to page
int free_page(page_t* page);

#endif