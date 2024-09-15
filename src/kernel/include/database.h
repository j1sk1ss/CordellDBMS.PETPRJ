/*
 *  Database is the end abstraction level, that work with tables.
 *  Main idea is to have main list of functions for handle user commands.
 *
 *  CordellDBMS source code: https://github.com/j1sk1ss/CordellDBMS.EXMPL
 *  Credits: j1sk1ss
 */

#ifndef DATABASE_H_
#define DATABASE_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "tabman.h"


#define DATABASE_EXTENSION "db"

// Database magic for file load_database function check
#define DATABASE_MAGIC      0xFC
// Database name (For higher abstractions)
#define DATABASE_NAME_SIZE  8
// Maxim tables in one database
#define MAX_TABLE_COUNT     10


// We have *.db bin file, where at start placed header
//=======================================================
// HEADER (MAGIC | NAME) -> | TABLE_NAMES -> ... -> end |
//=======================================================

struct database_header {
    // Database header magic
    uint8_t magic;

    // Database name
    uint8_t name[DATABASE_NAME_SIZE];

    // TODO: Maybe add something like checksum?
    //       For fast comparing databases
} typedef database_header_t;

struct database {
    // Database header
    database_header_t header;

    // Database linked tables
    uint8_t table_names[MAX_TABLE_COUNT][TABLE_NAME_SIZE];
} typedef database_t;


#pragma region [Table]

    #pragma region [Row]

        /*
        Append row function append data to provided table. If table not provided, it will return fail status.
        Note: This function will create new directories and pages, if current pages and directories don't have enoght space.
        Note 2: This function will fail if signature of input data different with provided table.

        Data should have next format:
        DATA_DATA_DATA -> CD -> DATA_DATA_DATA -> ... -> DATA_DATA_DATA
        Don't use RD symbols in data, because it will cause failure. (Function append RD symbols).

        Params:
        - table_name - current table name
        - data - data for append (row for append)
        - data_size - size of row (No limits)
        - access - user access level

        Return -3 if access denied
        Return -2 if signature is wrong
        Return -1 if something goes wrong
        Return 0 if row append was success
        Return 1 if row append cause directory creation
        Return 2 if row append cause page creation
        */
        int DB_append_row(char* table_name, uint8_t* data, size_t data_size, uint8_t access);

        /*
        Insert row function works different with row_append function. Main difference in disabling auto-creation of pages and directories.
        Like in append_row, this function will check data signature, end return error code if it wrong.
        If data is larger then avaliable space in directory, like in example below:
        DATA_DATA...INSERT_DATA
        | DR-SIZE --------- |

        this function will trunc data and return specific error code. For avoiding this, prefere using delete_row, then append_row.
        This happens because dynamic creation of pages simple, but dynamic creation of directories are not.
        TODO: Think about dynamic creation of new pages and directories.

        Params:
        - table_name - current table name
        - row - row index in table (Use find_data_row for getting index)
        - data - data for insert (row for append)
        - data_size - size of row (No limits)
        - access - user access level

        Return -3 if access denied
        Return -2 if signature is wrong
        Return -1 if something goes wrong
        Return 0 if row insert was success
        Return 1 if row insert cause page creation
        */
        int DB_insert_row(char* table_name, int row, uint8_t* data, size_t data_size, uint8_t access);

        /*
        Delete row function iterate all database by RW symbol and delite entire row by rewriting him with PE symbol.
        For getting index of row, you can use find_data_row of find_value_row function.

        Params:
        - table_name - current table name
        - row - index of row for delete
        - access - user access level

        Return -2 if access denied
        Return -1 if something goes wrong
        Return 1 if row delete was success
        */
        int DB_delete_row(char* table_name, int row, uint8_t access);


    #pragma endregion

    #pragma region [Data]

        int DB_find_data(database_t* database, char* table_name, int offset, uint8_t* data, size_t data_size);
        int DB_delete_data(database_t* database, char* table_name, int offset, size_t size);
        int DB_find_value(database_t* database, char* table_name, int offset, uint8_t value);
        int DB_find_data_row(database_t* database, char* table_name, int offset, uint8_t* data, size_t data_size);
        int DB_find_value_row(database_t* database, char* table_name, int offset, uint8_t value);

    #pragma endregion

#pragma endregion

#pragma region [Database]

    int DB_link_table2database(database_t* database, table_t* table);
    int DB_unlink_table_from_database(database_t* database, char* name);
    database_t* DB_load_database(char* name);
    int DB_save_database(database_t* database, char* path);

#pragma endregion

#endif