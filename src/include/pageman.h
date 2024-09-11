#ifndef PAGEMAN_H_
#define PAGEMAN_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "common.h"


#define COLUMN_DELIMITER    0xEE
#define PAGE_END            0xED
#define PAGE_CONTENT_SIZE   256
#define PAGE_MAGIC          0xCA
#define PAGE_NAME_SIZE      8

// We have *.cpg bin file, where at start placed header
//================================================
// INDEX | CONTENT_SIZE | CONTENT -> size -> end |
//================================================

    struct page_header {
        // Magic namber for check
        uint8_t magic;

        // Page filename
        // With this name we can save pages / compare pages
        uint8_t name[PAGE_NAME_SIZE];

        // TODO: Maybe add something like checksum?
        //       For fast comparing pages
    } typedef page_header_t;

    struct page {
        page_header_t* header;
        uint8_t content[PAGE_CONTENT_SIZE];
    } typedef page_t;


// Create page
// name - page name
// buffer - page content
//
// P.S. Function always pad content to fit default page size
//      If buffer_size higher then default page-size, it will trunc
page_t* create_page(char* name, uint8_t* buffer);

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