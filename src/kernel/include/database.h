/*
 *  Database is the end abstraction level, that work with tables.
 *  Main idea is to have main list of functions for handle user commands.
 *
 *  Also thanks to https://habr.com/ru/articles/803347/ for additional info about fflash and fsync,
 *  info about file based DBMS optimisation and other usefull stuff. 
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
#include <unistd.h>

#include "tabman.h"


#define DATABASE_EXTENSION "db"

// Database magic for file load_database function check
#define DATABASE_MAGIC      0xFC
// Database name (For higher abstractions)
#define DATABASE_NAME_SIZE  8


// We have *.db bin file, where at start placed header
//=======================================================
// HEADER (MAGIC | NAME) -> | TABLE_NAMES -> ... -> end |
//=======================================================

    struct database_header {
        // Database header magic
        uint8_t magic;

        // Database name
        uint8_t name[DATABASE_NAME_SIZE];

        // Table count in database
        uint8_t table_count;

        // TODO: Maybe add something like checksum?
        //       For fast comparing databases
    } typedef database_header_t;

    struct database {
        // Database header
        database_header_t* header;

        // Database linked tables
        uint8_t** table_names;
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

        /*
        Find data row function return global index in databse of provided row.
        Note: Will return row, if we have perfect fit. That's means:
        Will return:
            Targer: hello (size 5)
            Source: helloworld
        Will skip:
            Target: hello (size 6)
            Source: helloworld

        Note 2: For avoiding situations, where function return row with part of word,
                add space to target data (Don't forget to encrease size).
        Note 3: In difference with find data, this function will return index of row,
                that't why result of this function can't be used as offset. For working with
                rows, use rows functions.

        Params:
        - table_name - table name
        - offset - global offset. For simple use, try:
                   DIRECTORY_OFFSET for directory offset,
                   PAGE_CONTENT_SIZE for page offset.
        - data - data for seacrh
        - data_size - data for search size 

        Return -2 if something goes wrong
        Return -1 if data nfound
        Return row index (first entry) of target data 
        */
        int DB_find_data_row(char* table_name, int offset, uint8_t* data, size_t data_size);
        
        /*
        Find value function return row index in databse of provided value.

        Params:
        - table_name - table name
        - offset - global offset. For simple use, try:
                   DIRECTORY_OFFSET for directory offset,
                   PAGE_CONTENT_SIZE for page offset.
        - value - value for seacrh

        Return -2 if something goes wrong
        Return -1 if data nfound
        Return row index (first entry) of target value
        */
        int DB_find_value_row(char* table_name, int offset, uint8_t value); 

    #pragma endregion

    #pragma region [Data]

        /*
        Find data function return global index in databse of provided data. (Will return first entry of data).
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
        - table_name - table name
        - offset - global offset. For simple use, try:
                   DIRECTORY_OFFSET for directory offset,
                   PAGE_CONTENT_SIZE for page offset.
        - data - data for seacrh
        - data_size - data for search size 

        Return -2 if something goes wrong
        Return -1 if data nfound
        Return global index (first entry) of target data 
        */
        int DB_find_data(char* table_name, int offset, uint8_t* data, size_t data_size);
        
        /*
        Find value function return global index in databse of provided value. (Will return index of byte).

        Params:
        - table_name - table name
        - offset - global offset. For simple use, try:
                   DIRECTORY_OFFSET for directory offset,
                   PAGE_CONTENT_SIZE for page offset.
        - value - value for seacrh

        Return -2 if something goes wrong
        Return -1 if data nfound
        Return global index (first entry) of target value
        */
        int DB_find_value(char* table_name, int offset, uint8_t value);

        /*
        Delete data function rewrite data in page with PAGE_EMPTY symbols.
        Note: If size larger then page, and next page not exist, we trunc delete area.
              If next page exists, we continue deleting (in difference with insert functions).

        Params:
        - table_name - name of table, where content should be deleted
        - offset - global offset. For simple use, try:
                   DIRECTORY_OFFSET for directory offset,
                   PAGE_CONTENT_SIZE for page offset.
        - size - data size for delete

        Return -1 if something goes wrong
        Return 1 if delete was success
        Return 2 if during deleting we reach nexisted page
        Return 3 if during deleting we reach nexisted directory
         */
        int DB_delete_data(char* table_name, int offset, size_t size);

    #pragma endregion

#pragma endregion

#pragma region [Database]

    /*
    Add table to database. You can add infinity count of tables.

    Params:
    - database - pointer to database
    - table - pointer to table

    Return -1 if something goes wrong
    Return 1 if link was success
    */
    int DB_link_table2database(database_t* database, table_t* table);

    /*
    Delete assosiation with provided table in provided database.
    Note: This function don't delete table. This function just unlink table.
          For deleting tables - manualy use C file delete (Or higher abstaction language file operation).

    Params:
    - database - pointer to database
    - name - table name for delete

    Return -1 if something goes wrong
    Return 1 if unlink was success
    */
    int DB_unlink_table_from_database(database_t* database, char* name);

    /*
    Create new empty data base with provided name.
    Created database has 0 tables.

    Params:
    - name - database name. Be sure that this name is uniqe. Function don't check this!

    Return pointer to new database
    */
    database_t* DB_create_database(char* name);

    /*
    Free allocated data base

    Params:
    - database - pointer to database

    Return -1 if something goes wrong
    Return 1 if realise was success
    */
    int DB_free_database(database_t* database);

    /*
    Load database from disk by provided path to *.db file.
    
    Params:
    - name - path to *.db file

    Return -2 if Magic is wrong. Check file.
    Return -1 if file nfound. Check path.
    Return pointer to database if all was success.
    */
    database_t* DB_load_database(char* name);

    /*
    Save database to disk

    Params:
    - database - pointer to database
    - path - save path

    Return -1 if something goes wrong
    Return 1 if save was success
    */
    int DB_save_database(database_t* database, char* path);

#pragma endregion

#endif