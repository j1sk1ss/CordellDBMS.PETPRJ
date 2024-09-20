/*
Table operator is a simple functions to work with OS filesystem.
Main idea that we have files with sizes near 1 KB for pages, dirs and tables.
Table contains list of dirs (names of file). In same time, every directory contain list pf pages (names of file).

Tabman abstraction level responsible for working with directories. It send requests and earns data from lower 
abstraction level. Also tabman don`t check data signature. This is work of database level.
Note: Tabman don`t work directly with pages. It can work only with directories.

CordellDBMS source code: https://github.com/j1sk1ss/CordellDBMS.EXMPL
Credits: j1sk1ss
*/

#ifndef FILE_MANAGER_
#define FILE_MANAGER_

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#include "dirman.h"


#pragma region [Access]

    #define CREATE_ACCESS_BYTE(read_access, write_access, delete_access) \
        (((read_access & 0b111) << 6) | ((write_access & 0b111) << 3) | (delete_access & 0b111))

    // Macros for getting access level
    #define GET_READ_ACCESS(access_byte)    ((access_byte >> 6) & 0b111)
    #define GET_WRITE_ACCESS(access_byte)   ((access_byte >> 3) & 0b111)
    #define GET_DELETE_ACCESS(access_byte)  (access_byte & 0b111)

    #define CHECK_READ_ACCESS(uaccess, taccess) GET_READ_ACCESS(taccess) < GET_READ_ACCESS(uaccess) ? -1 : 0
    #define CHECK_WRITE_ACCESS(uaccess, taccess) GET_WRITE_ACCESS(taccess) < GET_WRITE_ACCESS(uaccess) ? -1 : 0
    #define CHECK_DELETE_ACCESS(uaccess, taccess) GET_DELETE_ACCESS(taccess) < GET_DELETE_ACCESS(uaccess) ? -1 : 0

#pragma endregion

#define TABLE_MAGIC             0xAA
#define TABLE_NAME_SIZE         8

#define DIRECTORIES_PER_TABLE    0xFF

#define TABLE_EXTENSION         "tb"
// Set here default path for save. 
// Important Note ! : This path is main for ALL tables
#define TABLE_BASE_PATH         ""

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
    #define COLUMN_NAME_SIZE    16

    // Column auto increment bits TODO
    #define COLUMN_NO_AUTO_INC       0x00
    #define COLUMN_AUTO_INCREMENT    0x01

    // Column primary status bits
    #define COLUMN_NOT_PRIMARY       0x00
    #define COLUMN_PRIMARY           0x01
    // Any type say that user can insert any value that he want
    #define COLUMN_TYPE_ANY          0x00
    // Int type throw error, if user insert something, that not int
    #define COLUMN_TYPE_INT          0x01
    // Float type throw error, if user insert something, that not float
    #define COLUMN_TYPE_FLOAT        0x02
    // String type throw error, if user insert something, that not char*
    #define COLUMN_TYPE_STRING       0x03

    // Macros for getting column data
    #define GET_COLUMN_DATA_TYPE(type)      ((type >> 2) & 0b11)
    #define GET_COLUMN_PRIMARY(type)        ((type >> 4) & 0b11)
    #define GET_COLUMN_TYPE(type)           (type & 0b11)

    // Generate column type byte
    #define CREATE_COLUMN_TYPE_BYTE(is_primary, column_data_type, column_type) \
        (((is_primary & 0b11) << 4) | ((column_data_type & 0b11) << 2) | (column_type & 0b11))

#pragma endregion


// We have *.tb bin file, where at start placed header
//==========================================================================================================================
// HEADER (MAGIC | NAME | ACCESS | COLUMN_COUNT | DIR_COUNT) -> | COLUMNS (MAGIC | TYPE | NAME) | DIR_NAMES -> dyn. -> end |
//==========================================================================================================================

    struct table_column {
        // Column magic byte
        uint8_t magic;

        // Column type byte
        // Column type indicates what type should user insert to this column
        uint8_t type;

        // Column size
        uint8_t size;

        // Column name with fixed size
        // Column name
        uint8_t name[COLUMN_NAME_SIZE];
    } typedef table_column_t;

    struct table_header {
        // Table magic
        uint8_t magic;

        // Table name
        // Table name needs for working with modfication exist table
        uint8_t name[TABLE_NAME_SIZE];

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

        // TODO: Maybe add something like checksum?
        //       For fast comparing tables
    } typedef table_header_t;

    struct table {
        // Table header
        table_header_t* header;

        // Column names
        table_column_t** columns;

        // Table directories
        uint8_t dir_names[DIRECTORIES_PER_TABLE][DIRECTORY_NAME_SIZE];
    } typedef table_t;


#pragma region [Directories]

    /*
    Get content return allocated copy of data by provided offset. If size larger than table, will return trunc data.
    Note: This function don`t check signature, and can return any values, that's why be sure that you get right size of content.

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
    Note 2: If during append process, we reach page limit in directory, we return error code (-2)

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
    int TBM_insert_content(table_t* table, int offset, uint8_t* data, size_t data_size);

    /*
    Append data to content pages in directories
    Note: If table don't have any directories, it will create one, then create one additional page
    Note 2: If during append process, we reach page limit in directory, we create a new one
    
    Params:
    - table - pointer to table
    - data - append data
    - data_size - size of data
    
    Return -1 if something goes wrong
    Return 0 if append was success
    Return 1 if append was success and we create new pages
    Return 2 if append was success and we create new directories
    */
    int TBM_append_content(table_t* table, uint8_t* data, size_t data_size);

    /*
    Delete content in table. All steps below:
    TABLE -> DIRECTORY -> PAGE
    This is a highest abstraction level delete function, that can delete content in many directories at one function call
    Note: If you will try to delete content from not existed pages or directories, this function will return -1
    Note 2: For offset in pages or directories use defined vars like:
    - DIRECTORY_OFFSET for directory offset
    - PAGE_CONTENT_SIZE for page offset
    
    Params:
    - table - pointer to table
    - offset - offset in bytes
    - size - size of deleted content
    
    Return -2 if something goes wrong
    Return -1 if you try to delete more, then already have
    Return 0 if delete was success
    */
    int TBM_delete_content(table_t* table, int offset, size_t size);

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
    int TBM_find_content(table_t* table, int offset, uint8_t* data, size_t data_size);

    /*
    Find value in assosiatet directories
    In summary it just invoke similar functions in directories for finding value in pages
    Note: For offset in pages or directories use defined vars like:
    - DIRECTORY_OFFSET for directory offset
    - PAGE_CONTENT_SIZE for page offset
    
    Params:
    - directory - pointer to directory
    - offset - offset in bytes
    - value - value that we want to find
    
    Return -1 - if not found
    Return index of value in page with end offset
    */
    int TBM_find_value(table_t* table, int offset, uint8_t value);

#pragma endregion

#pragma region [Column]

    /*
    Create column and allocate memory for.
    Note: Use only defined types of column.
    Note 2: If you want use any value (disable in-build check), use ANY type.
    
    Params:
    - type - column type
    - name - column name (Should equals or smaller then column max size).
           If it large then max size, name will trunc for fit.
    
    Return pointer to column
    */
    table_column_t* TBM_create_column(uint8_t type, uint8_t size, char* name);

    /*
    Realise column allocated memory
    
    Params:
    - column - pointer to column
    
    Return -1 if something goes wrong
    Return 1 if realise was success
    */
    int TBM_free_column(table_column_t* column);

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
    Return -2 if signature is wrong. You provide value for INT column, but value is not integer.
    Return -1 if provided data too small. Maybe you forgot additional CD?
              This error indicates, that data to small for this column count.
    Return 1 if signature is correct.
    */
    int TBM_check_signature(table_t* table, uint8_t* data);

    /*
    TODO: Add some error codes
    Link directory to table
    Note: Be sure that directory has same signature with table
    
    Params:
    - table - pointer to table (Can be freed after function)
    - directory - pointer to directory (Can be freed after function)
    
    Return 0 - if something goes wrong
    Return 1 - if link was success
    */
    int TBM_link_dir2table(table_t* table, directory_t* directory);

    /*
    Create new table
    
    Params:
    - name - name of table (Can be freed after function)
    - columns - columns in table (Please avoid free operations)
    - col_count - columns count
    - access - access of table
    
    Return pointer to new table
    */
    table_t* TBM_create_table(char* name, table_column_t** columns, int col_count, uint8_t access);

    /*
    Save table to the disk
    
    Params:
    - table - pointer to table (Can be freed after function)
    - path - place where table will be saved (Can be freed after function)
    
    Return 0 - if something goes wrong
    Return 1 - if save was success
    */
    int TBM_save_table(table_t* table, char* path);

    /*
    Load table from .tb bin file
    
    Params:
    - name - name of file (Can be freed after function)
    Note: Don't forget about full path. Be sure that all code coreectly use paths
    
    Return allocated table from disk 
    */
    table_t* TBM_load_table(char* name);

    /*
    Release table
    
    Params:
    - table - pointer to table
    
    Return 0 - if something goes wrong
    Return 1 - if Release was success
    */
    int TBM_free_table(table_t* table);

#pragma endregion

#endif