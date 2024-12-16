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
 *  Directory is the next abstraction level, that work directly with pages.
 *  Dirman - list of functions for working with directories and pages:
 *  We can:
 *      - Create new directory
 *      - Link pages to directory
 *      - Unlink pages from directory
 *      - Free directory
 *      - Append / delete / find content in assosiated pages
 *
 *  Dirnam abstraction level responsible for working with pages. It send requests and earns data from lower
 *  abstraction level. Also dirman don`t check data signature. This is work of database level. Also dirman
 *  can`t create new directories during handling requests from higher abstraction levels.
 *
 *  CordellDBMS source code: https://github.com/j1sk1ss/CordellDBMS.EXMPL
 *  Credits: j1sk1ss
*/

#ifndef DIRMAN_H_
#define DIRMAN_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
    #include <unistd.h>
#endif

#include "cache.h"
#include "common.h"
#include "logging.h"
#include "pageman.h"
#include "threading.h"


#define DIRECTORY_EXTENSION ENV_GET("DIRECTORY_EXTENSION", "dr")
// Set here default path for save.
// Important Note ! : This path is main for ALL directories.
#define DIRECTORY_BASE_PATH ENV_GET("DIRECTORY_BASE_PATH", "")

// 62^5 * PAGES_PER_DIRECTORY = 233.613.872.160 maximum pages in database.
// 62^5 * 4096 = 233.6 * 10^9 KB = MIN(255TB, 211TB) - Maximum size of database.
#define DIRECTORY_NAME_SIZE 6
#define DIRECTORY_MAGIC     0xCC

#define PAGES_PER_DIRECTORY 0xFF
#define DIRECTORY_OFFSET    PAGES_PER_DIRECTORY * PAGE_CONTENT_SIZE


// We have *.dr bin file, where at start placed header
//====================================================================================================================
// HEADER (MAGIC | NAME | PAGE_COUNT | COLUMN_COUNT) -> | COLUMNS (TYPE | NAME, ... ) | PAGE_NAMES -> 8 * 256 -> end |
//====================================================================================================================

    typedef struct directory_header {
        // Magic namber for check
        unsigned char magic;

        // Directory filename
        // With this name we can save directories / compare directories
        char name[DIRECTORY_NAME_SIZE];

        // Page count in directory
        unsigned char page_count;

        // Directory checksum
        unsigned int checksum;
    } directory_header_t;

    typedef struct directory {
        // Lock directory flag
        unsigned short lock;
        unsigned char is_cached;

        // Directory header
        directory_header_t* header;
        unsigned char append_offset;

        // Page file names
        char page_names[PAGES_PER_DIRECTORY][PAGE_NAME_SIZE];
    } directory_t;


#pragma region [Pages]

    /*
    Get content return allocated copy of data by provided offset. If size larger than directory, will return trunc data.
    Note: This function don`t check signature, and can return any values, that's why be sure that you get right size of content.

    Params:
    - directory - pointer to directory
    - offset - global offset in bytes
    - size - size of content

    Return point to allocated copy of data from directory
    */
    unsigned char* DRM_get_content(directory_t* directory, int offset, size_t data_lenght);

    /*
    Rewrite all line by EMPTY symbols.

    Params:
    - directory - Pointer to directory.
    - offset - Offset in directory.
    - data_size - Length of data to mark as delete.

    Return 1 - if write was success.
    Return -1 - if index not found in page.
    Return size, that can't be deleted, if we reach directory end during work.
    */
    int DRM_delete_content(directory_t* directory, int offset, size_t data_size);

    /*
    Cleanup empty pages in directory.

    Params:
    - directory - pointer to directory.

    Return 1 if cleanup success.
    Return -1 if something goes wrong.
    */
    int DRM_cleanup_pages(directory_t* directory);

    /*
    Append content to directory. This function move page_end symbol to new location.
    Note: If it can't append to existed pages, it creates new one.
    Note 2: This function not guarantees that content will be append to last page.
            Content will be placed at first empty space with fit size.

    Params:
    - directory - Pointer to directory.
    - data - Data for append.
    - data_lenght - Lenght of data.

    Return 2 if all success and content was append to new page.
    Return 1 if all success and content was append to existed page.
    Return 0 if write succes, but content was trunc.
    Return -2 if we can't create uniqe name for page.
    Return -3 if data size too large for one page. Check [pageman.h] docs for explanation.
    Return size, that can`t fit to this directory, if we reach page limit in directory.
    */
    int DRM_append_content(directory_t* __restrict directory, unsigned char* __restrict data, size_t data_lenght);

    /*
    Insert content to directory. This function don't move page_end in first empty page symbol to new location.
    Note: This function don't give ability for creation new pages. If content too large - it will trunc.
          To avoid this, use DRM_append_content function.
    Note 2: If directory don't have any pages - this function will fall and return -1.

    Params:
    - directory - Pointer to directory.
    - offset - Offset in bytes.
    - data - Data for append.
    - data_lenght - Lenght of data.

    Return 1 if all success.
    Return 2 if write succes, but content was trunc.
    Return -1 if something goes wrong.
    Return data size, that can't be stored in existed pages if we reach directory end.
    */
    int DRM_insert_content(directory_t* __restrict directory, unsigned char offset, unsigned char* __restrict data, size_t data_lenght);

    /*
    Find data function return global index in directory of provided data. (Will return first entry of data).
    Note: Will return index, if we have perfect fit. That's means:
    Will return:
        Targer: hello (size 5)
        Source: helloworld
    Will skip:
        Target: hello (size 6)
        Source: helloworld

    Note 2: For avoiding situations, where function return part of word, add space to target data (Don't forget to encrease size).
    Note 3: Don't use CD and RD symbols in data. (Optionaly). If you want find row, use find row function. <DEPRECATED>
    Note 4: This function is sungle thread, because we should be sure, that data sequence is correct.

    Params:
    - directory - pointer to directory.
    - offset - global offset. For simple use, try:
                PAGE_CONTENT_SIZE for page offset.
    - data - data for seacrh.
    - data_size - data for search size .

    Return -2 if something goes wrong.
    Return -1 if data nfound.
    Return global index (first entry) of target data .
    */
    int DRM_find_content(directory_t* __restrict directory, int offset, unsigned char* __restrict data, size_t data_size);

#pragma endregion

#pragma region [Directory]

    /*
    Save directory on the disk.
    Note: Be carefull with this function, it can rewrite existed content.
    Note 2: If you want update data on disk, just create same path with existed directory.

    Params:
    - directory - Pointer to directory.
    - path - Path where save. If provided NULL, function try to save file by default path.

    Return -2 - if something goes wrong.
    Return -1 - if we can`t create file.
    Return 1 if file create success.
    */
    int DRM_save_directory(directory_t* __restrict directory, char* __restrict path);

    /*
    Link current page to this directory.
    This function just add page name to directory page names and increment page count.
    Note: You can avoid page loading from disk if you want just link.
          For avoiuding additional IO file operations, use create_page function with same name,
          then just link allocated struct to directory.

    Params:
    - directory - Home directory.
    - page - Page for linking.

    Return -1 if we reach page limit per directory.
    If sighnature wrong, return 0.
    If all okey - return 1.
    */
    int DRM_link_page2dir(directory_t* __restrict directory, page_t* __restrict page);

    /*
    Unlink page from directory. This function just remove page name from directory structure.
    Note: If you want to delete page permanently, be sure that you unlink it from directory.

    Params:
    - directory - directory pointer.
    - page_name - page name (Not path).

    Return -1 if something goes wrong.
    Return 0 if page not found.
    Return 1 if unlink was success.
    */
    int DRM_unlink_page_from_directory(directory_t* __restrict directory, char* __restrict page_name);

    /*
    Allocate memory and create new directory.

    Params:
    - Name - directory name.

    Return directory pointer.
    */
    directory_t* DRM_create_directory(char* name);

    /*
    Same function with create directory, but here you can avoid name input.
    Note: Directory will have no pages.
    Note 2: Function generates random name with delay. It need for avoid situations,
            where we can rewrite existed directory.
    Note 3: In future prefere avoid random generation by using something like hash generator

    Return pointer to allocated directory.
    Return NULL if we can`t create random name.
    */
    directory_t* DRM_create_empty_directory();

    /*
    Open file, load directory and page names, close file.
    Note: This function invoke create_directory function.
    Note 2: Don't forget about DRM_flush_directory after using this pointer.

    Params:
    - name - name of directory. This function will try to load dir by
             default path (Should be NULL, if provided path).

    Return -2 if Magic is wrong. Check file.
    Return -1 if file nfound. Check path.
    Return locked directory pointer.
    */
    directory_t* DRM_load_directory(char* name);

    /*
    Delete directory from disk.
    Note: Will delete all linked pages and linked pages if flag full is 1.

    Params:
    - directory - Directory pointer.
    - full - Special flag for full deleting.

    Return -1 if something goes wrong.
    Return 1 if all files was delete.
    */
    int DRM_delete_directory(directory_t* directory, int full);

    /*
    In difference with DRM_free_directory, DRM_flush_directory will free directory in case, when
    directory not cached in GCT.

    Params:
    - directory - pointer to directory.

    Return -1 - if directory in GCT.
    Return 1 - if Release was success.
    */
    int DRM_flush_directory(directory_t* directory);

    /*
    Release directory.
    Imoortant Note!: that usualy directory, if we use load_directory function,
    saved in DDT, that's means, that you should avoid free_directory with dirs,
    that was created by load_directory. Use directory flush.
    Note 1: Use this function with dirs, that was created by create_directory function.
    Note 2: If tou anyway want to free directory, prefere using flush_directory insted free_directory.
            Difference in part, where flush_directory first try to find provided directory in DDT, then
            NULL pointer, and free, instead simple free in free_directory case.

    directory - pointer to directory.

    Return 0 - if something goes wrong.
    Return 1 - if Release was success.
    */
    int DRM_free_directory(directory_t* directory);

    /*
    Generate directory checksum. Checksum is sum of all bytes of directory name,
    all bytes of page names.

    Params:
    - directory - directory pointer.

    Return directory checksum.
    */
    unsigned int DRM_get_checksum(directory_t* directory);

#pragma endregion

#endif
