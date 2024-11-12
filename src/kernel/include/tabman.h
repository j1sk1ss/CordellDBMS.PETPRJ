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
 *  Table operator is a simple functions to work with OS filesystem.
 *  Main idea that we have files with sizes near 4 KB for pages, 2 KB dirs and 13 KB tables.
 *  Table contains list of dirs (names of file). In same time, every directory contain list pf pages (names of file).
 *  We can:
 *      - Create new tables
 *      - Delete tables
 *      - Link dirs to tables
 *      - Unlink dirs from tables
 *      - Append / delete / insert and find data in tables
 *
 *  Tabman abstraction level responsible for working with directories. It send requests and earns data from lower
 *  abstraction level. Also tabman don't check data signature. This is work of database level.
 *  Note: Tabman don't work directly with pages. It can work only with directories.
 *
 *  CordellDBMS source code: https://github.com/j1sk1ss/CordellDBMS.EXMPL
 *  Credits: j1sk1ss
*/

#ifndef TABMAN_H_
#define TABMAN_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef _WIN32
    #include <zlib.h>
    #include <unistd.h>
#endif

#include "threading.h"
#include "logging.h"
#include "common.h"
#include "dirman.h"
#include "module.h"
#include "cache.h"


#define TABLE_MAGIC             0xAA
#define TABLE_NAME_SIZE         8
#define DIRECTORIES_PER_TABLE   0xFF

#define TABLE_EXTENSION         ENV_GET("TABLE_EXTENSION", "tb")
// Set here default path for save.
// Important Note ! : This path is main for ALL tables
#define TABLE_BASE_PATH         ENV_GET("TABLE_BASE_PATH", "")

#pragma region [Access]

    // Create access byte for new tables and for users. For input, this
    // function take values from 0 to 3.
    #define CREATE_ACCESS_BYTE(read_access, write_access, delete_access) \
        (((read_access & 0b11) << 4) | ((write_access & 0b11) << 2) | (delete_access & 0b11))

    // Macros for getting access level
    #define GET_READ_ACCESS(access_byte)    ((access_byte >> 4) & 0b11)
    #define GET_WRITE_ACCESS(access_byte)   ((access_byte >> 2) & 0b11)
    #define GET_DELETE_ACCESS(access_byte)  (access_byte & 0b11)

    /*
    Macros for checking read access level. Will return -1 if access denied.
    Note: This function usualy used in lower abstraction levels.
    uaccess - user access level.
    taccess - table access level.
    */
    #define CHECK_READ_ACCESS(uaccess, taccess) GET_READ_ACCESS(taccess) < GET_READ_ACCESS(uaccess) ? -1 : 0
    /*
    Macros for checking write access level. Will return -1 if access denied.
    Note: This function usualy used in lower abstraction levels.
    uaccess - user access level.
    taccess - table access level.
    */
    #define CHECK_WRITE_ACCESS(uaccess, taccess) GET_WRITE_ACCESS(taccess) < GET_WRITE_ACCESS(uaccess) ? -1 : 0
    /*
    Macros for checking delete access level. Will return -1 if access denied.
    Note: This function usualy used in lower abstraction levels.
    uaccess - user access level.
    taccess - table access level.
    */
    #define CHECK_DELETE_ACCESS(uaccess, taccess) GET_DELETE_ACCESS(taccess) < GET_DELETE_ACCESS(uaccess) ? -1 : 0

#pragma endregion

#pragma region [Column]

    /*
    <DEPRECATED>
    Main idea is create a simple presentation of info in binary data.
    With this delimiters we know, that every row has at start ROW_DELIMITER (it allows us use \n character).
    */
    #define COLUMN_DELIMITER    0xEE
    /*
    <DEPRECATED>
    For splitting data by columns, we reserve another byte value.
    In summary data has next structure:
    ... -> CD -> DATA_DATA_DATA -> CD -> DATA_DATA_DATA -> RD -> DATA_DATA_DATA -> CD -> ...
    Row delimiter equals column delimiter, but says, that this is a different column and different row
    */
    #define ROW_DELIMITER       0xEF
    #define COLUMN_MAX_SIZE     0xFF

    #define COLUMN_MAGIC        0xEA
    /*
    Column name size indicates how much bytes will reserve name field on disk.
    In optimisation purpuse, this value should be equals something like 16 or 8.
    Note: In PSQL, max size of column name is 64.
    */
    #define COLUMN_NAME_SIZE        8

    #define COLUMN_MODULE_SIZE      16
    #define COLUMN_MODULE_PRELOAD   0x00
    #define COLUMN_MODULE_POSTLOAD  0x01
    #define COLUMN_MODULE_BOTH      0x02

    // Column auto increment bits.
    // <Already not implemented yet>
    #define COLUMN_NO_AUTO_INC       0x00
    #define COLUMN_AUTO_INCREMENT    0x01

    // Column primary status bits. If column is primary
    // thats mean, that value in every row in this column is uniqe.
    #define COLUMN_NOT_PRIMARY       0x00
    #define COLUMN_PRIMARY           0x01

    // Any type say that user can insert any value that he want
    #define COLUMN_TYPE_ANY          0x00
    // Int type throw error, if user insert something, that not int
    #define COLUMN_TYPE_INT          0x01
    // Float type throw error, if user insert something, that not module
    #define COLUMN_TYPE_MODULE       0x02
    // String type throw error, if user insert something, that not char*
    #define COLUMN_TYPE_STRING       0x03

    // Macros for getting column primary status. (Unique value at every row).
    #define GET_COLUMN_PRIMARY(type)        ((type >> 4) & 0b11)
    // Macros for getting column data type. What data type is set in this column.
    #define GET_COLUMN_DATA_TYPE(type)      ((type >> 2) & 0b11)
    // Macros for getting column type. Can it autoincrement or something like that.
    #define GET_COLUMN_TYPE(type)           (type & 0b11)

    // Generate column type byte
    #define CREATE_COLUMN_TYPE_BYTE(is_primary, column_data_type, column_type) \
        (((is_primary & 0b11) << 4) | ((column_data_type & 0b11) << 2) | (column_type & 0b11))

#pragma endregion


// We have *.tb bin file, where at start placed header
//========================================================================================================================================
// HEADER (MAGIC | NAME | ACCESS | COLUMN_COUNT | DIR_COUNT) -> | COLUMNS (MAGIC | TYPE | NAME) -> | LINKS -> | DIR_NAMES -> dyn. -> end |
//========================================================================================================================================

    typedef struct table_column {
        // Column magic byte
        uint8_t magic;

        /*
        Column type indicates what type should user insert to this column.
        Main idea, that we save type, data type and primary status in one byte.
        In summary we have next byte:
        0x00|PP|DD|TT|

        Where:
        PP - Primary bits.
        DD - Data type bits.
        TT - Column type bits.
        */
        uint8_t type;

        // Column size
        uint8_t size;

        // Column name with fixed size
        // Column name
        char name[COLUMN_NAME_SIZE];

        // Column module data.
        // If in column used any module, we store command here.
        uint8_t module_params;
        char module_name[MODULE_NAME_SIZE];
        char module_querry[COLUMN_MODULE_SIZE];
    } table_column_t;

    typedef struct table_header {
        // Table magic
        uint8_t magic;

        // Table name
        // Table name needs for working with modfication exist table
        char name[TABLE_NAME_SIZE];

        /*
        Main idea:
        We have table with RWD access (Read, Write and Delete). If user have all levels, he can delete rows,
        append / insert data and read any values from table.
        If user level is higher then table level, that indicates that user can't do some stuff.
        In summary, 000 - highest level access (like root). 777 - lowest level access.

        How byte looks like:
        0bRRR|WWW|DDD

        Access byte used in higher abstraction levels like database, because here, in tabman, we don't know user.
        */
        uint8_t access;

        // Column count in this table
        // How much columns in this table
        uint8_t column_count;

        // Dir count in this table
        // How much directories in this table
        uint8_t dir_count;

        // Table checksum
        uint32_t checksum;
    } table_header_t;

    typedef struct table {
        // Lock table flag
        uint16_t lock;
        uint8_t is_cached;

        // Table header
        table_header_t* header;

        // Column names
        table_column_t** columns;
        uint16_t row_size;

        // Table directories
        char dir_names[DIRECTORIES_PER_TABLE][DIRECTORY_NAME_SIZE];
    } table_t;


#pragma region [Directories]

    /*
    Get content return allocated copy of data by provided offset. If size larger than table, will return trunc data.
    Note: This function don't check signature, and can return any values, that's why be sure that you get right size of content.

    Params:
    - table - pointer to table
    - offset - global offset in bytes
    - size - size of content

    Return point to allocated copy of data from table
    */
    uint8_t* TBM_get_content(table_t* table, int offset, size_t size);

    /*
    Insert data row to content pages in directories. Main difference with append_content is hard part.
    This maeans, that we don't care about signature and other stuff. One thing that can cause fail, directory end.
    Note: If table don't have any directories, it will return error code (-3)
    Note 2: If during insert process, we reach page limit in directory, we return error code (-2)

    ! In summary, this function shouldn't be used in ususal tasks. It may broke whole table at one time. !

    Params:
    - table - pointer to table
    - data - append data
    - data_size - size of data

    Return -3 if table is empty
    Return -2 if we reach page limit in directory (Prefere using append_content)
    Return -1 if something goes wrong
    Return 0 if row append was success
    Return 1 if row append was success and we create new pages
    */
    int TBM_insert_content(table_t* __restrict table, int offset, uint8_t* __restrict data, size_t data_size);

    /*
    Append data to content pages in directories
    Note: If table don't have any directories, it will create one, then create one additional page
    Note 2: If during append process, we reach page limit in directory, we create a new one

    Params:
    - table - pointer to table
    - data - append data
    - data_size - size of data

    Return {
    Return -12 if we can't create uniqe name for page.
    Return -13 if data size too large for one page. Check [pageman.h] docs for explanation.
    }
    Return -1 if something goes wrong
    Return 0 if append was success
    Return 1 if append was success and we create new pages
    Return 2 if append was success and we create new directories
    */
    int TBM_append_content(table_t* __restrict table, uint8_t* __restrict data, size_t data_size);

    /*
    Delete content in table. All steps below:
    TABLE -> DIRECTORY -> PAGE
    This is a highest abstraction level delete function, that can delete content in many directories at one function call.
    Note: If you will try to delete content from not existed pages or directories, this function will return -1.
    Note 2: For offset in pages or directories use defined vars like:
    - DIRECTORY_OFFSET for directory offset.
    - PAGE_CONTENT_SIZE for page offset.

    Params:
    - table - pointer to table.
    - offset - offset in bytes.
    - size - size of deleted content.

    Return -3 if table don't have directories.
    Return -2 if something goes wrong.
    Return -1 if you try to delete more, then already have.
    Return 0 if delete was success.
    */
    int TBM_delete_content(table_t* table, int offset, size_t size);

    /*
    Cleanup empty directories in table.

    Params:
    - table - pointer to table.

    Return 1 if cleanup success.
    Return -1 if something goes wrong.
    */
    int TBM_cleanup_dirs(table_t* table);

    /*
    Find data function return global index in table of provided data. (Will return first entry of data).
    Note: Will return index, if we have perfect fit. That's means:
    Will return:
        Targer: hello (size 5)
        Source: helloworld
    Will skip:
        Target: hello (size 6)
        Source: helloworld

    Note 2: For avoiding situations, where function return part of word, add space to target data (Don't forget to encrease size).
    Note 3: Don't use CD and RD symbols in data. (Optionaly). If you want find row, use find row function.
    Note 4: This function is sungle thread, because we should be sure, that data sequence is correct.

    Params:
    - table - pointer to table.
    - offset - global offset. For simple use, try:
                DIRECTORY_OFFSET for directory offset,
                PAGE_CONTENT_SIZE for page offset.
    - data - data for seacrh
    - data_size - data for search size

    Return -2 if something goes wrong
    Return -1 if data nfound
    Return global index (first entry) of target data
    */
    int TBM_find_content(table_t* __restrict table, int offset, uint8_t* __restrict data, size_t data_size);

#pragma endregion

#pragma region [Column]

    /*
    Update column in provided table.
    Note: Provided column should have same size and same name with column, that we want to replace.
          If you want to change name of column, provide index of column to by_index variable.
    Note 1: If you don't want to change table by index, pass -1 to by_index variable.

    Params:
    - table - Pointer to table, where we want update column.
    - column - Column for update.

    Return -2 if provided column has different size.
    Return -1 if we don't find column with same name.
    Return 1 if update was success.
    */
    int TBM_update_column_in_table(table_t* __restrict table, table_column_t* __restrict column, int by_index);

    /*
    Create column and allocate memory for.
    Note: Use only defined types of column.
    Note 2: If you want use any value (disable in-build check), use TYPE_ANY type.

    Params:
    - type - column type
    - name - column name (Should equals or smaller then column max size).
           If it large then max size, name will trunc for fit.

    Return pointer to column
    */
    table_column_t* TBM_create_column(uint8_t type, uint8_t size, char* name);

#pragma endregion

#pragma region [Table]

    /*
    Check signature function check input data with column signature.
    Be sure that you include CD symbols into your data.

    Params:
    - table - table signature
    - data - data for check

    Return -4 if column type unknown. Check table, that you provide into function.
    Return -3 if signature is wrong. You provide value for FLOAT column, but value is not float.
    Return -2 if signature is wrong. You provide value for TYPE_INT column, but value is not integer.
    Return -1 if provided data too small. Maybe you forgot additional CD? <DEPRECATED>
              This error indicates, that data to small for this column count.
    Return 1 if signature is correct.
    */
    int TBM_check_signature(table_t* __restrict table, uint8_t* __restrict data);

    /*
    Link directory to table
    Note: Be sure that directory has same signature with table

    Params:
    - table - pointer to table (Can be freed after function)
    - directory - pointer to directory (Can be freed after function)

    Return 0 - if something goes wrong
    Return 1 - if link was success
    */
    int TBM_link_dir2table(table_t* __restrict table, directory_t* __restrict directory);

    /*
    Unlink directory from table. This function just remove directory name from table structure.
    Note: If you want to delete directory permanently, be sure that you unlink it from table.

    Params:
    - table - table pointer.
    - dir_name - directory name (Not path).

    Return -1 if something goes wrong.
    Return 0 if directory not found.
    Return 1 if unlink was success.
    */
    int TBM_unlink_dir_from_table(table_t* table, const char* dir_name);

    /*
    Create new table

    Params:
    - name - name of table (Can be freed after function)
    - columns - columns in table (Please avoid free operations)
    - col_count - columns count
    - access - access of table

    Return pointer to new table
    */
    table_t* TBM_create_table(char* __restrict name, table_column_t** __restrict columns, int col_count, uint8_t access);

    /*
    Save table to the disk

    Params:
    - table - pointer to table (Can be freed after function)
    - path - place where table will be saved (Can be freed after function). If provided NULL, function try to save file by default path.

    Return -5 if dir names write corrupt.
    Return -4 if column links write corrupt.
    Return -3 if column names write corrupt.
    Return -2 if header write corrupt.
    Return -1 if file can't be open.
    Return 0 - if something goes wrong
    Return 1 - if save was success
    */
    int TBM_save_table(table_t* __restrict table, char* __restrict path);

    /*
    Load table from .tb bin file

    Params:
    - path - path to table.tb file. (Should be NULL, if provided name).
    - name - name of table. This function will try to load table by
             default path (Should be NULL, if provided path).
    Note: Don't forget about full path. Be sure that all code coreectly use paths

    Return allocated table from disk
    */
    table_t* TBM_load_table(char* __restrict path, char* __restrict name);

    /*
    Delete table from disk.
    Note: Will delete all linked directories and linked pages if flag full is 1.

    Params:
    - table - Table pointer.
    - full - Special flag for full deleting.

    Return -1 if something goes wrong.
    Return 1 if all files was delete.
    */
    int TBM_delete_table(table_t* table, int full);

    /*
    In difference with TBM_free_table, TBM_flush_table will free table in case, when
    table not cached in GCT.

    Params:
    - table - pointer to table.

    Return -1 - if table in GCT.
    Return 1 - if Release was success.
    */
    int TBM_flush_table(table_t* table);

    /*
    Release table.
    Imoortant Note!: Usualy table, if we use load_table function,
    saved in TDT, that means, that you should avoid free_table with tables,
    that was created by load_table.
    Note 1: Use this function with tables, that was created by create_table function.
    Note 2: If you anyway want to free table, prefere using flush_table insted free_table.
            Difference in part, where flush_table first try to find provided table in TDT, then
            set it to NULL pointer, and free, instead simple free in free_table case.

    Params:
    - table - pointer to table

    Return 0 - if something goes wrong
    Return 1 - if Release was success
    */
    int TBM_free_table(table_t* table);

    /*
    Generate table checksum. Checksum is sum of all bytes of table name,
    all bytes of dir names, all bytes of columns and links.

    Params:
    - table - table pointer.

    Return table checksum.
    */
    uint32_t TBM_get_checksum(table_t* table);

    /*
    Invoke modules in table and change input data.

    Params:
    - table - Pointer to table.
    - data - Pointer to data for module invoke.
    - type - Addition params.

    Return 1 if success.
    */
    int TBM_invoke_modules(table_t* __restrict table, uint8_t* __restrict data, uint8_t type);

#pragma endregion

#endif
