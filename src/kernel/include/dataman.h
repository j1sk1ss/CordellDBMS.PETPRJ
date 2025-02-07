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

#include <stdlib.h>

#ifndef _WIN32
    #include <unistd.h>
#endif

#include "logging.h"
#include "common.h"
#include "tabman.h"
#include "cache.h"


#define TABLES_PER_DATABASE     0xFF

#define DATABASE_EXTENSION      ENV_GET("DATABASE_EXTENSION", "db")
// Set here default path for save.
// Important Note ! : This path is main for ALL databases
#define DATABASE_BASE_PATH      ENV_GET("DATABASE_BASE_PATH", "")

// Database magic for file load_database function check
#define DATABASE_MAGIC      0xFC
// Database name (For higher abstractions)
#define DATABASE_NAME_SIZE  8

/*
Max size of table cache in database. For minimisation of IO file operations,
we use cache in pages (lowest level) and table cache at the highest level.
*/
#define DATABASE_TABLE_CACHE_SIZE   10


// We have *.db bin file, where at start placed header
//=======================================================
// HEADER (MAGIC | NAME) -> | TABLE_NAMES -> ... -> end |
//=======================================================

    typedef struct {
        // Database header magic
        unsigned char magic;

        // Database name
        char name[DATABASE_NAME_SIZE];

        // Table count in database
        unsigned char table_count;
    } database_header_t;

    typedef struct {
        // Database header
        database_header_t* header;

        // Database linked tables
        char table_names[TABLES_PER_DATABASE][TABLE_NAME_SIZE];
    } database_t;


#pragma region [Table]

    /*
    Get row function return pointer to allocated data. This data don't contain RD symbols.
    Note: This is allocated data, that's why you should free row after use.
    Note 2: Pointers shouldn't overlap each other!

    Params:
    - database - Pointer to database. (If NULL, we don`t use database table cache).
    - table_name - Current table name.
    - row - Index of row. You can get index by:
            1) find value row function,
            2) find data row function.
            For additional info check docs.
    - access - User access level.

    Return -3 if access denied.
    Return -2 if table nfound.
    Return -1 if something goes wrong.
    Return pointer to data.
    */
    unsigned char* DB_get_row(database_t* __restrict database, char* __restrict table_name, int row, unsigned char access);

    /*
    Append row function append data to provided table. If table not provided, it will return fail status.
    Note: This function will create new directories and pages, if current pages and directories don't have enoght space.
    Note 2: This function will fail if signature of input data different with provided table.
    Note 3: Pointers shouldn't overlap each other!

    Data should have next format:
    DATA_DATA_DATA -> CD -> DATA_DATA_DATA -> ... -> DATA_DATA_DATA.
    Don't use RD symbols in data, because it will cause failure. (Function append RD symbols).

    Params:
    - database - Pointer to database. (If NULL, we don`t use database table cache).
    - table_name - Current table name.
    - data - Data for append (row for append).
    - data_size - Size of row (No limits).
    - access - User access level.

    Return -20 if primary row check failed.
    Return -5 if data size != row size.
    Return -4 if table not found in database.
    Return -3 if access denied.
    Return -2 if signature is wrong: {
        Return -14 if column type unknown. Check table, that you provide into function.
        Return -13 if signature is wrong. You provide value for FLOAT column, but value is not float.
        Return -12 if signature is wrong. You provide value for TYPE_INT column, but value is not integer.
        Return -11 if provided data too small. Maybe you forgot additional CD? <DEPRECATED>
    }
    Return -1 if something goes wrong.
    Return 0 if row append was success.
    Return 1 if row append cause directory creation.
    Return 2 if row append cause page creation.
    */
    int DB_append_row(database_t* __restrict database, char* __restrict table_name, unsigned char* __restrict data, size_t data_size, unsigned char access);

    /*
    Insert row function works different with row_append function. Main difference in disabling auto-creation of pages and directories.
    Like in append_row, this function will check data signature, end return error code if it wrong.
    If data is larger then avaliable space in directory, like in example below:
    DATA_DATA...INSERT_DATA
    | DR-SIZE --------- |

    this function will trunc data and return specific error code. For avoiding this, prefere using delete_row, then append_row.
    This happens because dynamic creation of pages simple, but dynamic creation of directories are not.
    Note: Pointers shouldn't overlap each other!

    Params:
    - database - Pointer to database. (If NULL, we don`t use database table cache).
    - table_name - Current table name.
    - row - Row index in table (Use find_data_row for getting index).
    - data - Data for insert (row for append).
    - data_size - Size of row (No limits).
    - access - User access level.

    Return -3 if access denied
    Return -2 if signature is wrong: {
        Return -14 if column type unknown. Check table, that you provide into function.
        Return -13 if signature is wrong. You provide value for FLOAT column, but value is not float.
        Return -12 if signature is wrong. You provide value for TYPE_INT column, but value is not integer.
        Return -11 if provided data too small. Maybe you forgot additional CD? <DEPRECATED>
    }
    Return -1 if something goes wrong
    Return 1 if row insert success
    Return 2 if row insert was success, but was trunc
    */
    int DB_insert_row(
        database_t* __restrict database, char* __restrict table_name, int row, unsigned char* __restrict data, size_t data_size, unsigned char access
    );

    /*
    Delete row function iterate all database by RW symbol and delite entire row by rewriting him with PE symbol.
    For getting index of row, you can use find_data_row of find_value_row function.
    Note: Pointers shouldn't overlap each other!

    Params:
    - database - Pointer to database. (If NULL, we don`t use database table cache).
    - table_name - Current table name.
    - row - Index of row for delete.
    - access - User access level.

    Return -2 if access denied.
    Return -1 if something goes wrong.
    Return 1 if row delete was success.
    */
    int DB_delete_row(database_t* __restrict database, char* __restrict table_name, int row, unsigned char access);

    /*
    Init cascade cleanup of empty directories and empty pages in all table in database.
    Note: This function, also, call sync fuinction like init_transaction method.

    Params:
    - database - pointer to database.

    Return 1 if cleanup was success.
    */
    int DB_cleanup_tables(database_t* database);

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
    Note 3: <DEPRECATED> In difference with find data, this function will return index of row,
            that't why result of this function can't be used as offset. For working with
            rows, use rows functions.
    Note 4: Pointers shouldn't overlap each other!

    Params:
    - database - Pointer to database. (If NULL, we don`t use database table cache).
    - table_name - Table name.
    - column - Column name. Provide NULL, if you don't need specified column.
    - offset - Global offset. For simple use, try:
                DIRECTORY_OFFSET for directory offset,
                PAGE_CONTENT_SIZE for page offset.
    - data - Data for search.
    - data_size - Data for search size.
    - access - User access level.

    Return -3 if access denied.
    Return -2 if something goes wrong.
    Return -1 if data nfound.
    Return row index (first entry) of target data.
    */
    int DB_find_data_row(
        database_t* __restrict database, char* __restrict table_name, char* __restrict column, 
        int offset, unsigned char* __restrict data, size_t data_size, unsigned char access
    );

#pragma endregion

#pragma region [Database]

    /*
    Get table from database by table name
    Note: Returned pointer shouldn't be flushed by user with TLB_free_table, because
          this is a pointer to cached table in datadabe. Please use DB_unlink_table_from_database.
          Anyway, if you want to flash table, be sure that you replace it by NULL in database cache.
    Note 2: Pointers shouldn't overlap each other!

    Params:
    - table_name - Name of table.

    Return NULL if table nfound.
    Return pointer to table.
    */
    table_t* DB_get_table(database_t* __restrict database, char* __restrict table_name);

    /*
    Delete table from database.
    Note: Pointers shouldn't overlap each other!

    Params:
    - database - Database pointer.
    - table_name - Table name for delete.
    - full - Special flag for full deleting.

    Return -1 if something goes wrong.
    Return 1 if all files was delete.
    */
    int DB_delete_table(database_t* __restrict database, char* __restrict table_name, int full);

    /*
    Add table to database. You can add infinity count of tables.
    Note: Pointers shouldn't overlap each other!

    Params:
    - database - Pointer to database.
    - table - Pointer to table (Be sure that you don`t flust this table after link).
              This function also save link in database cache to this table.

    Return -1 if something goes wrong.
    Return 1 if link was success.
    */
    int DB_link_table2database(database_t* __restrict database, table_t* __restrict table);

    /*
    Delete assosiation with provided table in provided database.
    Note: This function don't delete table. This function just unlink table.
          For deleting tables - manualy use C file delete (Or higher abstaction language file operation).
    Note 2: Pointers shouldn't overlap each other!

    Params:
    - database - Pointer to database.
    - name - Table name for delete.

    Return -1 if something goes wrong.
    Return 1 if unlink was success.
    */
    int DB_unlink_table_from_database(database_t* __restrict database, char* __restrict name);

    /*
    Create new empty data base with provided name.
    Created database has 0 tables.

    Params:
    - name - Database name. Be sure that this name is uniqe. Function don't check this!

    Return pointer to new database.
    */
    database_t* DB_create_database(char* name);

    /*
    Delete database from disk. <NOT TRANSACTION SAFE!>

    Params:
    - database - Pointer to database.
    - full - Recurse deleting.

    Return 1 if database deleted.
    Return -1 if something goes wrong.
    */
    int DB_delete_database(database_t* database, int full);

    /*
    Free allocated data base.

    Params:
    - database - pointer to database.

    Return -1 if something goes wrong.
    Return 1 if realise was success.
    */
    int DB_free_database(database_t* database);

    /*
    Load database from disk by provided path to *.db file.
    Note: Pointers shouldn't overlap each other!

    Params:
    - path - path to database.db file. (Should be NULL, if provided name).
    - name - name of database. This function will try to load database by
             default path (Should be NULL, if provided path).

    Return NULL if file can't be opened, or magic is wrong.
    Return pointer to database if all was success.
    */
    database_t* DB_load_database(char* name);

    /*
    Save database to disk.
    Note: Pointers shouldn't overlap each other!

    Params:
    - database - pointer to database

    Return -3 if table names write corrupt.
    Return -2 if header write corrupt.
    Return -1 if can`t create or open file.
    Return 1 if save was success.
    */
    int DB_save_database(database_t* database);

#pragma endregion

#pragma region [Transaction]

    /*
    Init transaction method prepare DBMS for transaction by flush all buffers.
    Note: Be sure, that transaction will work with:
    - MAX_TABLES or less tables.
    - MAX_DIRECTORIES or less directories.
    - MAX_PAGES or less pages.
    In few words, that means, that you can input data with 40960KB (40MB) size to 10 directories at one time.
    Note 2: MAX_TABLES, MAX_DIRECTORIES and MAX_PAGES can be found in "cache.h".

    Return 1 if transaction init success.
    Return -1 if we can't free GCT.
    Return -4 if database is NULL.
    */
    int DB_init_transaction(database_t* database);

    /*
    When we init transaction with flushing buffers, we prepare space for all pages, dirs and tabs that will be used
    in future transactions. If something goes wrong during transaction, we can just vipe all buffers before it will be written to disk.

    Return 1 if rollback success.
    Return -1 if we can't free GCT.
    Return -4 if database is NULL.
    */
    int DB_rollback(database_t** database);

#pragma endregion

    /*
    Get global offset.

    Params:
    - row_size - Row size in table.
    - row - Index of row.

    Return global offset.
    */
    int _get_global_offset(int row_size, int row);

#endif
