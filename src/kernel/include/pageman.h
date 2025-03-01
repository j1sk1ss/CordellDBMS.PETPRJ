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

#include <fcntl.h>

#ifndef _WIN32
    #include <unistd.h>
    #include <libgen.h>
#else
    #include <io.h>
#endif

#include "threading.h"
#include "logging.h"
#include "common.h"
#include "cache.h"


#define PAGE_EXTENSION  ENV_GET("PAGE_EXTENSION", "pg")
// Set here default path for save.
// Important Note ! : This path is main for ALL pages
// #define PAGE_BASE_PATH  ENV_GET("PAGE_BASE_PATH", "")


#pragma region [Page memory]

    #define PAGE_EMPTY          0xFF
    // 4096 is default. Lower - less RAM. Higher - faster.
    #define PAGE_CONTENT_SIZE   4096
    #define PAGE_START          0x00

#pragma endregion

#define PAGE_MAGIC 0xCA
// 64^6 = 56.800.235.584 - unique page names.
// 64^6 * PAGE_CONTENT_SIZE = 211 TB
#define PAGE_NAME_SIZE 4

// We have *.pg bin file, where at start placed header
//================================================
// INDEX | CONTENT_SIZE | CONTENT -> size -> end |
//================================================

    typedef struct {
        // Magic namber for check
        // If number nq magic, we know that this file broken
        unsigned char magic;

        // Page filename
        // With this name we can save pages / compare pages
        char name[PAGE_NAME_SIZE];

        // Table checksum
        unsigned int checksum;
    } page_header_t;

    typedef struct {
        // Lock page flags
        unsigned short lock;
        unsigned char is_cached;

        // Page header with all special information
        page_header_t* header;

        // Page content
        unsigned short content[PAGE_CONTENT_SIZE];
        char* base_path;
    } page_t;


#pragma region [Content]

    /*
    Get content from page. This function just copy any content from page to buffer.

    Params:
    - page - Pointer to page.
    - offset - Offset in page content.
    - buffer - Location for data from page content.
    - data_lenght - Lenght of data.

    Return size of data, that can be stored in page.
    */
   int PGM_get_content(page_t* __restrict page, int offset, unsigned char* __restrict buffer, size_t data_length);

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
    int PGM_insert_content(page_t* __restrict page, int offset, unsigned char* __restrict data, size_t data_lenght);

    /*
    Rewrite all line by EMPTY symbols

    Params:
    - page - pointer to page
    - offset - offset in page
    - length - length of data to mark as delete

    Return 1 - if write was success
    Return -1 - if index not found in page
    */
    int PGM_delete_content(page_t* __restrict page, int offset, size_t length);

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
    int PGM_find_content(page_t* __restrict page, int offset, unsigned char* __restrict data, size_t data_size);

    /*
    Function that set PE symbol in content in page. Main idea:
    1) We find last not PAGE_EMPTY symbol.
    2) Go to the end of page, and if we don`t get any not PAGE_EMPTY symbol, save start index.

    Params:
    - page - page pointer.
    - offset - offset in page.

    Return index of PAGE_END symbol in page.
    */
    int PGM_get_page_occupie_size(page_t* page, int offset);

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
    page_t* PGM_create_page(char* __restrict name, unsigned char* __restrict buffer, size_t data_size);

    /*
    Same function with create page, but here you can avoid name and buffer input.
    Note: Page will have all EP content with EDP symblol at 0 index.
    Note 2: Function generates random name with delay. It need for avoid situations,
            where we can rewrite existed page
    Note 3: In future prefere avoid random generation by using something like hash generator
    Took from: https://stackoverflow.com/questions/230062/whats-the-best-way-to-check-if-a-file-exists-in-c

    Params:
    - base_path - Base path of pages. 

    Return pointer to allocated page.
    Return NULL if we can`t create random name.
    */
    page_t* PGM_create_empty_page(char* base_path);

    /*
    Save page on disk.

    Params:
    - page - pointer to page.

    Return -3 if content write corrupt.
    Return -2 if header write corrupt.
    Return -1 if file can`t be opened.
    Return 0 - if something goes wrong.
    Return 1 - if saving was success.
    */
    int PGM_save_page(page_t* page);

    /*
    Open file, load page, close file

    Params:
    - base_path - Base path of page.
    - name - name of page. This function will try to load page by
             default path (Should be NULL, if provided path).

    Return -2 if Magic is wrong. Check file.
    Return -1 if file nfound. Check path.
    Return NULL if magic wrong
    Return pointer to page struct
    */
    page_t* PGM_load_page(char* base_path, char* name);

    /*
    In difference with PGM_free_page, PGM_flush_page will free page in case, when
    page not cached in GCT.

    Params:
    - page - pointer to page.

    Return -1 - if page in GCT.
    Return 1 - if Release was success.
    */
    int PGM_flush_page(page_t* page);

    /*
    Release page
    Imoortant Note!: Usualy page, if we use load_page function,
    saved in PDT, that's means, that you should avoid free_page with pages,
    that was created by load_page.
    Note 1: Use this function with pages, that was created by create_page function.

    Params:
    - page - pointer to page.

    Return 0 - if something goes wrong.
    Return 1 - if Release was success.
    */
    int PGM_free_page(page_t* page);

    /*
    Generate page checksum.

    Params:
    - page - page pointer.

    Return page checksum.
    */
    unsigned int PGM_get_checksum(page_t* page);

#pragma endregion

#endif
