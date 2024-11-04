/*
 *  License:
 *  Copyright (C) 2024 Nikolaj Fot
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of 
 *  the GNU General Public License as published by the Free Software Foundation, version 3.
 *  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  See the GNU General Public License for more details.
 *  You should have received a copy of the GNU General Public License along with this program. 
 *  If not, see https://www.gnu.org/licenses/.
 * 
 *  Description:
 *  Pages - first abstraction level, that responsible for working with files. Main idea of page,
 *  that this file contains rows without trunc. That's mean, that we can't store in one page a part
 *  of row. Only whole row. This is important point, that guarantee data strength.
 *  Pageman - list of functions for working with pages and low-level content presentation in files:
 *  We can:
 *      - Create new pages
 *      - Free pages
 *      - Append / delete / find content in files
 *
 *  Pageman abstraction level responsible for working with data. Read, save and insert data directly
 *  to content in pages. Also pageman don`t check data signature. This is work of database level. Also pageman
 *  can`t create new pages during handling requests from higher abstraction levels.
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
#include <zlib.h>

#ifndef _WIN32
    #include <unistd.h>
    #include <libgen.h>
#endif

#include "logging.h"
#include "common.h"
#include "threading.h"
#include "cache.h"


#define PAGE_EXTENSION  ENV_GET("PAGE_EXTENSION", "pg")
// Set here default path for save.
// Important Note ! : This path is main for ALL pages
#define PAGE_BASE_PATH  ENV_GET("PAGE_BASE_PATH", "")


#pragma region [Page memory]

    #define PAGE_EMPTY          0xEE
    #define PAGE_END            0xED
    #define PAGE_CONTENT_SIZE   4096
    #define PAGE_START          0x00

#pragma endregion

#define PAGE_MAGIC          0xCA
#define PAGE_NAME_SIZE      8

// We have *.pg bin file, where at start placed header
//================================================
// INDEX | CONTENT_SIZE | CONTENT -> size -> end |
//================================================

    typedef struct page_header {
        // Magic namber for check
        // If number nq magic, we know that this file broken
        uint8_t magic;

        // Page filename
        // With this name we can save pages / compare pages
        char name[PAGE_NAME_SIZE];

        // Table checksum
        uint32_t checksum;
    } page_header_t;

    typedef struct page {
        // Lock page flags
        uint16_t lock;

        // Page header with all special information
        page_header_t* header;

        // Page content
        uint8_t content[PAGE_CONTENT_SIZE];
    } page_t;


#pragma region [Content]

    /*
    Insert value to page.

    Params:
    - page - pointer to page.
    - offset - offset in page.
    - value - value for input.

    Return 1 if insert success.
    Return -1 if offset too large.
    */
    int PGM_insert_value(page_t* page, int offset, uint8_t value);

    /*
    Insert content to page. This function don't move page_end symbol to new location.
    In summary this function just rewrite part of page with provided data.
    Note: You should use this function carefull, because it don't warn if it rewrite special bytes.

    Params:
    - page - pointer to page.
    - data - data for append.
    - data_lenght - lenght of data.

    Return size of data, that can be stored in page.
    */
    int PGM_insert_content(page_t* page, int offset, uint8_t* data, size_t data_lenght);

    /*
    Rewrite all line by EMPTY symbols

    Params:
    - page - pointer to page
    - offset - offset in page
    - length - length of data to mark as delete

    Return 1 - if write was success
    Return -1 - if index not found in page
    */
    int PGM_delete_content(page_t* page, int offset, size_t length);

    /*
    Find data function return index in page of provided data. (Will return first entry of data).
    Note: Will return index, if we have perfect fit. That's means:
    Will return:
        Targer: hello (size 5)
        Source: helloworld
    Will skip:
        Target: hello (size 6)
        Source: helloworld

    Note 2: For avoiding situations, where function return part of word, add space to target data (Don't forget to encrease size).
    Note 3: Don't use CD and RD symbols in data. (Optionaly). If you want find row, use find row function.

    Params:
    - page - pointer to page.
    - offset - offset in page
    - data - data for search
    - data_size - data for search size

    Return -2 if something goes wrong
    Return -1 if data nfound
    Return index (first entry) of target data
    */
    int PGM_find_content(page_t* page, int offset, uint8_t* data, size_t data_size);

    /*
    Find local index of line with input value

    Params:
    - page - pointer to page
    - offset - offset index
    - value - target value

    Return -2 - if something goes wrong
    Return -1 - if not found
    Return index of value in content
    */
    int PGM_find_value(page_t* page, int offset, uint8_t value);

    /*
    Function that set PE symbol in content in page. Main idea:
    1) We find last not PAGE_EMPTY symbol.
    2) Go to the end of page, and if we don`t get any not PAGE_EMPTY symbol, save start index.

    Params:
    - page - page pointer.
    - offset - offset in page.

    Return index of PAGE_END symbol in page.
    */
    int PGM_set_pe_symbol(page_t* page, int offset);

    /*
    Return value in bytes of free page space
    Note: Will return only block of free space. For examle if in page we have situation like below:
    NFREE -> SMALL FREE -> NFREE -> LARGE FREE -> ...
    function will return SMALL FREE size. That's why try to use offset (or provide -1 for getting all free space)

    Params:
    - page - pointer to page
    - offset - offset in page. It needs to skip small part, if they too small

    Return -1 - if something goes wrong
    */
    int PGM_get_free_space(page_t* page, int offset);

    /*
    Return index in page of empty space, that more or equals provided size

    Params:
    - page - pointer to page
    - offset - offset in page. It needs to skip small part, if they too small
    - size - needed size of block.
           If provided -1, return first empty space

    Return -1 - if something goes wrong
    Return start index of empty space in page
    Return -2 if empty space with provided size of more not exists in page
    */
    int PGM_get_fit_free_space(page_t* page, int offset, int size);

#pragma endregion

#pragma region [Page]

    /*
    Create page

    Params:
    - name - page name
    - buffer - page content
    - data_size - data size

    P.S. Function always pad content to fit default page size
         If buffer_size higher then default page-size, it will trunc
    */
    page_t* PGM_create_page(char* name, uint8_t* buffer, size_t data_size);

    /*
    Same function with create page, but here you can avoid name and buffer input.
    Note: Page will have all EP content with EDP symblol at 0 index.
    Note 2: Function generates random name with delay. It need for avoid situations,
            where we can rewrite existed page
    Note 3: In future prefere avoid random generation by using something like hash generator
    Took from: https://stackoverflow.com/questions/230062/whats-the-best-way-to-check-if-a-file-exists-in-c

    Return pointer to allocated page.
    Return NULL if we can`t create random name.
    */
    page_t* PGM_create_empty_page();

    /*
    Save page on disk.

    Params:
    - page - pointer to page.
    - path - path where save. If provided NULL, function try to save file by default path.

    Return -3 if content write corrupt.
    Return -2 if header write corrupt.
    Return -1 if file can`t be opened.
    Return 0 - if something goes wrong.
    Return 1 - if saving was success.
    */
    int PGM_save_page(page_t* page, char* path);

    /*
    Open file, load page, close file

    Params:
    - path - path to page.pg file. (Should be NULL, if provided name).
    - name - name of page. This function will try to load page by
             default path (Should be NULL, if provided path).

    Return -2 if Magic is wrong. Check file.
    Return -1 if file nfound. Check path.
    Return NULL if magic wrong
    Return pointer to page struct
    */
    page_t* PGM_load_page(char* path, char* name);

    /*
    Release page
    Imoortant Note!: Usualy page, if we use load_page function,
    saved in PDT, that's means, that you should avoid free_page with pages,
    that was created by load_page.
    Note 1: Use this function with pages, that was created by create_page function.

    Params:
    - page - pointer to page

    Return 0 - if something goes wrong
    Return 1 - if Release was success
    */
    int PGM_free_page(page_t* page);

    /*
    Generate page checksum.

    Params:
    - page - page pointer.

    Return page checksum.
    */
    uint32_t PGM_get_checksum(page_t* page);

#pragma endregion

#endif
