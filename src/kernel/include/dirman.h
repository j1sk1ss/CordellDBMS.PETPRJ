/*
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

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <zlib.h>

#ifndef _WIN32
    #include <unistd.h>
#endif

#include "common.h"
#include "pageman.h"


#define DIRECTORY_EXTENSION getenv("DIRECTORY_EXTENSION") == NULL ? "dr" : getenv("DIRECTORY_EXTENSION")
// Set here default path for save.
// Important Note ! : This path is main for ALL directories
#define DIRECTORY_BASE_PATH getenv("DIRECTORY_BASE_PATH") == NULL ? "" : getenv("DIRECTORY_BASE_PATH")

#define DDT_SIZE            10

#define DIRECTORY_NAME_SIZE 8
#define DIRECTORY_MAGIC     0xCC

#define PAGES_PER_DIRECTORY 0xFF
#define DIRECTORY_OFFSET    PAGES_PER_DIRECTORY * PAGE_CONTENT_SIZE


// We have *.dr bin file, where at start placed header
//====================================================================================================================
// HEADER (MAGIC | NAME | PAGE_COUNT | COLUMN_COUNT) -> | COLUMNS (TYPE | NAME, ... ) | PAGE_NAMES -> 8 * 256 -> end |
//====================================================================================================================

    typedef struct directory_header {
        // Magic namber for check
        uint8_t magic;

        // Directory filename
        // With this name we can save directories / compare directories
        uint8_t name[DIRECTORY_NAME_SIZE];

        // Page count in directory
        uint8_t page_count;

        // Directory checksum
        uint32_t checksum;
    } directory_header_t;

    typedef struct directory {
        // Directory header
        directory_header_t* header;

        // Lock directory flag
        uint8_t lock;
        uint8_t lock_owner;

        // Page file names
        uint8_t names[PAGES_PER_DIRECTORY][PAGE_NAME_SIZE];
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
    uint8_t* DRM_get_content(directory_t* directory, int offset, size_t size);

    /*
    Rewrite all line by EMPTY symbols.

    directory - pointer to directory.
    offset - offset in directory.
    length - length of data to mark as delete.

    Return 1 - if write was success.
    Return -1 - if index not found in page.
    Return size, that can't be deleted, if we reach directory end during work.
    */
    int DRM_delete_content(directory_t* directory, int offset, size_t length);

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

    directory - pointer to directory.
    data - data for append.
    data_lenght - lenght of data.

    Return 2 if all success and content was append to new page.
    Return 1 if all success and content was append to existed page.
    Return 0 if write succes, but content was trunc.
    Return -2 if we can't create uniqe name for page.
    Return -3 if data size too large for one page. Check [pageman.h] docs for explanation.
    Return size, that can`t fit to this directory, if we reach page limit in directory.
    */
    int DRM_append_content(directory_t* directory, uint8_t* data, size_t data_lenght);

    /*
    Insert content to directory. This function don't move page_end in first empty page symbol to new location.
    Note: This function don't give ability for creation new pages. If content too large - it will trunc.
          To avoid this, use DRM_append_content function.
    Note 2: If directory don't have any pages - this function will fall and return -1.

    directory - pointer to directory.
    offset - offset in bytes.
    data - data for append.
    data_lenght - lenght of data.

    Return 1 if all success.
    Return 2 if write succes, but content was trunc.
    Return -1 if something goes wrong.
    Return data size, that can't be stored in existed pages if we reach directory end.
    */
    int DRM_insert_content(directory_t* directory, uint8_t offset, uint8_t* data, size_t data_lenght);

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
    int DRM_find_content(directory_t* directory, int offset, uint8_t* data, size_t data_size);

    /*
    Find value in assosiatet pages.

    directory - pointer to directory.
    offset - offset in bytes.
    value - value that we want to find.

    Return -1 - if not found.
    Return index of value in page with offset.
    */
    int DRM_find_value(directory_t* directory, int offset, uint8_t value);

#pragma endregion

#pragma region [Directory]

    /*
    Save directory on the disk.
    Note: Be carefull with this function, it can rewrite existed content.
    Note 2: If you want update data on disk, just create same path with existed directory.

    directory - pointer to directory.
    path - path where save. If provided NULL, function try to save file by default path.

    Return -2 - if something goes wrong.
    Return -1 - if we can`t create file.
    Return 1 if file create success.
    */
    int DRM_save_directory(directory_t* directory, char* path);

    /*
    Link current page to this directory.
    This function just add page name to directory page names and increment page count.
    Note: You can avoid page loading from disk if you want just link.
          For avoiuding additional IO file operations, use create_page function with same name,
          then just link allocated struct to directory.

    directory - home directory.
    page - page for linking.

    Return -1 if we reach page limit per directory.
    If sighnature wrong, return 0.
    If all okey - return 1.
    */
    int DRM_link_page2dir(directory_t* directory, page_t* page);

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
    int DRM_unlink_page_from_directory(directory_t* directory, char* page_name);

    /*
    Allocate memory and create new directory.

    name - directory name.

    Return directory pointer.
    */
    directory_t* DRM_create_directory(char* name);

    /*
    Same function with create directory, but here you can avoid name input.
    Note: Directory will have no pages.
    Note 2: Function generates random name with delay. It need for avoid situations,
            where we can rewrite existed directory.
    Note 3: In future prefere avoid random generation by using something like hash generator
    Took from: https://stackoverflow.com/questions/230062/whats-the-best-way-to-check-if-a-file-exists-in-c

    Return pointer to allocated directory.
    Return NULL if we can`t create random name.
    */
    directory_t* DRM_create_empty_directory();

    /*
    Open file, load directory and page names, close file.
    Note: This function invoke create_directory function.

    Params:
    - path - path to directory.dr file. (Should be NULL, if provided name).
    - name - name of directory. This function will try to load dir by
             default path (Should be NULL, if provided path).

    Return -2 if Magic is wrong. Check file.
    Return -1 if file nfound. Check path.
    Return locked directory pointer.
    */
    directory_t* DRM_load_directory(char* path, char* name);

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
    Release directory.
    Imoortant Note!: that usualy directory, if we use load_directory function,
    saved in DDT, that's means, that you should avoid free_directory with dirs,
    that was created by load_directory.
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
    uint32_t DRM_get_checksum(directory_t* directory);

#pragma region [DDT]

        /*
        Add directory to DDT table.
        Note: It will unload old directory if we earn end of DDT.
        Note 2: If we try to unload locked dir, we go to next, and try to unload next.
                We can get deadlock.

        Params:
        - directory - pointer to directory (Be sure that you don't realise this directory. We save link in DDT).

        Return 1 if add was success
        Return -1 if something goes wrong
        */
        int DRM_DDT_add_directory(directory_t* directory);

        /*
        Find directory in DDT by name

        Params:
        - name - name of directory for find.
                Note: not path to file. Name of directory.
                      Usuale names placed in directories.

        Return directory struct or NULL if directory not found.
        */
        directory_t* DRM_DDT_find_directory(char* name);

        /*
        Save and load directories from DDT.

        Return -1 if something goes wrong.
        Return 1 if sync success.
        */
        int DRM_DDT_sync();

        /*
        Free DDT table. In difference with clear function, this will avoid
        working with disk. That's why this function used in DB rollback.

        Return 1 if free was correct.
        Return -1 if function can't lock any directory from DTD.
        */
        int DRM_DDT_free();

        /*
        Hard cleanup of DDT. Really not recomment for use!
        Note: It will just unload data from DDT to disk by provided index.
        Note 2: Empty space will be marked by NULL.

        Params:
        - index - index of directory for flushing.

        Return -1 if something goes wrong.
        Return 1 if cleanup success.
        */
        int DRM_DDT_flush_index(int index);

        /*
        Hard cleanup of DDT. Really not recomment for use!
        Note: It will just unload data from DDT to disk by provided index.
        Note 2: Empty space will be marked by NULL.

        Params:
        - directory - pointer to directory for flushing.

        Return -1 if something goes wrong.
        Return 1 if cleanup success.
        */
        int DRM_DDT_flush_directory(directory_t* directory);

    #pragma endregion

    #pragma region [Lock]

        /*
        Lock directory for working.
        Note: Can cause deadlock, because we infinity wait for directory unlock.

        Params:
        - directory - pointer to directory.
        - owner - thread, that want lock this directory.

        Return -2 if we try to lock NULL
        Return -1 if we can`t lock directory (for some reason)
        Return 1 if directory now locked.
        */
        int DRM_lock_directory(directory_t* directory, uint8_t owner);

        /*
        Check lock status of directory.

        Params:
        - directory - pointer to directory.
        - owner - thread, that want test this directory.

        Return lock status (LOCKED and UNLOCKED).
        */
        int DRM_lock_test(directory_t* directory, uint8_t owner);

        /*
        Realise directory for working.

        Params:
        - directory - pointer to directory.
        - owner - thread, that want release this directory.

        Return -3 if directory is NULL.
        Return -2 if this directory has another owner.
        Return -1 if directory was unlocked. (Nothing changed)
        Return 1 if directory now unlocked.
        */
        int DRM_release_directory(directory_t* directory, uint8_t owner);

    #pragma endregion

#pragma endregion

#endif
