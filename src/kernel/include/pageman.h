/*
 *  CordellDBMS source code: https://github.com/j1sk1ss/CordellDBMS.EXMPL
 *  Credits: j1sk1ss
 */

#ifndef PAGEMAN_H_
#define PAGEMAN_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "common.h"


#pragma region [Page memory]

    #define PAGE_EMPTY          0xEE
    #define PAGE_END            0xED
    #define PAGE_CONTENT_SIZE   1024

#pragma endregion

#define PAGE_MAGIC          0xCA
#define PAGE_NAME_SIZE      8

// We have *.pg bin file, where at start placed header
//================================================
// INDEX | CONTENT_SIZE | CONTENT -> size -> end |
//================================================

    struct page_header {
        // Magic namber for check
        // If number nq magic, we know that this file broken
        uint8_t magic;

        // Page filename
        // With this name we can save pages / compare pages
        uint8_t name[PAGE_NAME_SIZE];

        // TODO: Maybe add something like checksum?
        //       For fast comparing pages
    } typedef page_header_t;

    struct page {
        // Page header with all special information
        page_header_t* header;

        // Page content
        uint8_t content[PAGE_CONTENT_SIZE];
    } typedef page_t;


#pragma region [Content]

    // Append content to page. This function move page_end symbol to new location
    //
    // page - pointer to page
    // data - data for append
    // data_lenght - lenght of data
    //
    // Return 1 if all success
    // Return 0 if write succes, but content was trunc
    // Return -1 if something goes wrong
    int PGM_append_content(page_t* page, uint8_t* data, size_t data_lenght);

    // Append content to page. This function move page_end symbol to new location
    //
    // page - pointer to page
    // data - data for append
    // data_lenght - lenght of data
    //
    // Return 1 if all success
    // Return 0 if write succes, but content was trunc
    // Return -1 if something goes wrong
    int PGM_insert_content(page_t* page, uint8_t offset, uint8_t* data, size_t data_lenght);

    // Rewrite all line by EMPTY symbols
    //
    // page - pointer to page
    // offset - offset in page
    // length - length of data to mark as delete
    //
    // Return 1 - if write was success
    // Return -1 - if index not found in page
    int PGM_delete_content(page_t* page, int offset, size_t length);

    // Find local index of line with input value
    //
    // page - pointer to page
    // offset - offset index
    // value - target value
    // data_size - size of value for compare
    //
    // Return -1 - if not found
    int PGM_find_content(page_t* page, int offset, uint8_t value, size_t data_size);

    // Return value in bytes of free page space
    // 
    // page - pointer to page
    //
    // Return -1 - if something goes wrong
    int PGM_get_free_space(page_t* page);

#pragma endregion

#pragma region [Page]

    // Create page
    //
    // name - page name
    // buffer - page content
    //
    // P.S. Function always pad content to fit default page size
    //      If buffer_size higher then default page-size, it will trunc
    page_t* PGM_create_page(char* name, uint8_t* buffer);

    // Save page on disk
    //
    // page - pointer to page
    // path - path where save
    //
    // Return 0 - if something goes wrong
    // Return 1 - if Release was success
    int PGM_save_page(page_t* page, char* path);

    // Open file, load page, close file
    //
    // name - page name (don`t forget path)
    // TODO: Maybe it will take too much time? Maybe save FILE descriptor?
    page_t* PGM_load_page(char* name);

    // Release page
    //
    // page - pointer to page
    //
    // Return 0 - if something goes wrong
    // Return 1 - if Release was success
    int PGM_free_page(page_t* page);

#pragma endregion

#endif