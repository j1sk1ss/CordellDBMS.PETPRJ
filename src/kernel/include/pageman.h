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

#ifndef _WIN32
#include <unistd.h>
#include <libgen.h>
#endif

#include "common.h"


#define PAGE_EXTENSION "pg"
// Set here default path for save. 
// Important Note ! : This path is main for ALL pages
#define PAGE_BASE_PATH ""

#define PDT_SIZE        10

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

    /*
    Append content to page. This function move page_end symbol to new location
    
    Params:
    - page - pointer to page
    - data - data for append
    - data_lenght - lenght of data
    
    Return 1 if all success
    Return 0 if write succes, but content was trunc
    Return -1 if something goes wrong
    */
    int PGM_append_content(page_t* page, uint8_t* data, size_t data_lenght);

    /*
    Insert content to page. This function don't move page_end symbol to new location.
    In summary this function just rewrite part of page with provided data.
    Note: You should use this function carefull, because it don't warn if it rewrite special bytes.
    
    Params:
    - page - pointer to page
    - data - data for append
    - data_lenght - lenght of data
    
    Return 2 if data was trunc because it earn page end
    Return 1 if all success
    Return 0 if write succes, but content was trunc
    Return -1 if something goes wrong
    */
    int PGM_insert_content(page_t* page, uint8_t offset, uint8_t* data, size_t data_lenght);

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
    Return value in bytes of free page space
    Note: Will return only block of free space. For examle if in page we have situation like below:
    NFREE -> SMALL FREE -> NFREE -> LARGE FREE -> ...
    function will return SMALL FREE size. That's why try to use offset (or provide -1 for getting all free space)
    TODO: Too slow. Maybe use old with PAGE_CONTENT_SIZE - index?
    
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
    Save page on disk
    
    Params:
    - page - pointer to page
    - path - path where save
    
    Return 0 - if something goes wrong
    Return 1 - if saving was success
    */
    int PGM_save_page(page_t* page, char* path);

    /*
    Open file, load page, close file
    
    Params:
    - name - page name (don`t forget path)
    TODO: Maybe it will take too much time? Maybe save FILE descriptor?
    
    Return -2 if Magic is wrong. Check file.
    Return -1 if file nfound. Check path.
    Return NULL if magic wrong
    Return pointer to page struct
    */
    page_t* PGM_load_page(char* name);

    /*
    Release page
    
    Params:
    - page - pointer to page
    
    Return 0 - if something goes wrong
    Return 1 - if Release was success
    */
    int PGM_free_page(page_t* page);

    #pragma region [PDT]

        /*
        Add page to PDT table.
        Note: It will unload old page if we earn end of PDT. 

        Params:
        - page - pointer to page (Be sure that you don't realise this page. We save link in PDT)

        Return struct page_t pointer
        */
        int PGM_PDT_add_page(page_t* page);

        /*
        Find page in PDT by name

        Params:
        - name - name of page for find.
                Note: not path to file. Name of page.
                    Usuale names placed in directories.

        Return page_t struct or NULL if page not found.
        */
        page_t* PGM_PDT_find_page(char* name);

        /*
        Save and load pages from PDT.

        Return -1 if something goes wrong.
        Return 1 if sync success.
        */
        int PGM_PDT_sync();

        /*
        Hard cleanup of PDT. Really not recomment for use!
        Note: It will just unload data from PDT to disk by provided index.
        Note 2: Empty space will be marked by NULL.

        Params:
        - index - index of page for flushing

        Return -1 if something goes wrong.
        Return 1 if cleanup success.
        */
        int PGM_PDT_flush(int index);

    #pragma endregion

#pragma endregion

#endif