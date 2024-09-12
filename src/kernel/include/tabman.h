/*
 *  Table operator is a simple functions to work with OS filesystem.
 *  Main idea that we have files with sizes near 1 KB for pages, dirs and tables.
 *  Table contains list of dirs (names of file). In same time, every directory contain list pf pages (names of file).
 * 
 *  CordellDBMS source code: https://github.com/j1sk1ss/CordellDBMS.EXMPL
 *  Credits: j1sk1ss
 */

#ifndef FILE_MANAGER_
#define FILE_MANAGER_

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "dirman.h"


#define TABLE_MAGIC     0xAA
#define TABLE_NAME_SIZE 8

#pragma region [Column]

    #define COLUMN_DELIMITER    0xEE
    #define ROW_DELIMITER       0xEF

    #define COLUMN_MAGIC        0xEA
    #define COLUMN_NAME_SIZE    16

    // Any type say that user can insert any value that he want
    #define COLUMN_TYPE_ANY          0xFF
    // Int type throw error, if user insert something, that not int
    #define COLUMN_TYPE_INT          0x00
    // Float type throw error, if user insert something, that not float
    #define COLUMN_TYPE_FLOAT        0x01
    // String type throw error, if user insert something, that not char*
    #define COLUMN_TYPE_STRING       0x02

#pragma endregion


// We have *.cdir bin file, where at start placed header
//=========================================================================================================
// HEADER (MAGIC | COLUMN_COUNT | PAGE_COUNT) -> | COLUMNS (TYPE | NAME, ... ) | DIR_NAMES -> dyn. -> end |
//=========================================================================================================

    struct table_column {
        // Column type byte
        // Column type indicates what type should user insert to this column
        uint8_t type;

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
        uint8_t** dir_names;
    } typedef table_t;


#pragma region [Directories]

    // TODO

#pragma endregion

#pragma region [Table]

    // TODO: Add some error codes
    // Link directory to table
    // Note: Be sure that directory has same signature with table
    //
    // table - pointer to table
    // directory - pointer to directory
    //
    // Return 0 - if something goes wrong
    // Return 1 - if link was success
    int TBM_link_dir2table(table_t* table, directory_t* directory);

    // Create new table
    //
    // name - name of table
    // columns - columns in table
    // col_count - columns count
    table_t* TBM_create_table(char* name, table_column_t* columns[], int col_count);

    // Save table to the disk
    //
    // table - pointer to table
    // path - place where table will be saved
    //
    // Return 0 - if something goes wrong
    // Return 1 - if save was success
    int TBM_save_table(table_t* table, char* path);

    // Load table from .tb bin file
    //
    // name - name of file
    // Note: Don't forget about full path. Be sure that all code coreectly use paths
    table_t* TBM_load_table(char* name);

    // Release table
    //
    // table - pointer to directory
    //
    // Return 0 - if something goes wrong
    // Return 1 - if Release was success
    int TBM_free_table(table_t* table);

#pragma endregion

#endif