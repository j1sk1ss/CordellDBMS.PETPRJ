/*
 *  Pages - first abstraction level, that responsible for working with files
 *  Pageman - list of functions for working with pages and low-level content presentation in files:
 *  We can:
 *      - Create new pages
 *      - Free pages
 *      - Append / delete / find content in files
 * 
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
    #define PAGE_START          0

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

    // Insert content to page. This function don't move page_end symbol to new location.
    // In summary this function just rewrite part of page with provided data.
    // Note: You should use this function carefull, because it don't warn if it rewrite special bytes.
    //
    // page - pointer to page
    // data - data for append
    // data_lenght - lenght of data
    //
    // Return 2 if data was trunc because it earn page end
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
    //
    // Return -1 - if not found
    int PGM_find_value(page_t* page, int offset, uint8_t value);

    // Return value in bytes of free page space
    // Note: Will return only block of free space. For examle if in page we have situation like below:
    // NFREE -> SMALL FREE -> NFREE -> LARGE FREE -> ...
    // function will return SMALL FREE size. That's why try to use offset (or provide -1 for getting all free space)
    // TODO: Too slow. Maybe use old with PAGE_CONTENT_SIZE - index?
    // 
    // page - pointer to page
    // offset - offset in page. It needs to skip small part, if they too small
    //
    // Return -1 - if something goes wrong
    int PGM_get_free_space(page_t* page, int offset);

    // Return index in page of empty space, that more or equals provided size
    // 
    // page - pointer to page
    // offset - offset in page. It needs to skip small part, if they too small
    // size - needed size of block.
    //        If provided -1, return first empty space
    //
    // Return -1 - if something goes wrong
    // Return start index of empty space in page
    // Return -2 if empty space with provided size of more not exists in page
    int PGM_get_fit_free_space(page_t* page, int offset, int size);

#pragma endregion

#pragma region [Page]

    // Create page
    //
    // name - page name
    // buffer - page content
    // data_size - data size
    //
    // P.S. Function always pad content to fit default page size
    //      If buffer_size higher then default page-size, it will trunc
    page_t* PGM_create_page(char* name, uint8_t* buffer, size_t data_size);

    // Save page on disk
    //
    // page - pointer to page
    // path - path where save
    //
    // Return 0 - if something goes wrong
    // Return 1 - if saving was success
    int PGM_save_page(page_t* page, char* path);

    // Open file, load page, close file
    //
    // name - page name (don`t forget path)
    // TODO: Maybe it will take too much time? Maybe save FILE descriptor?
    // Return NULL if magic wrong
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