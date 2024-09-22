/*
Table operator is a simple functions to work with OS filesystem.
Main idea that we have files with sizes near 1 KB for pages, dirs and tables.
Table contains list of dirs (names of file). In same time, every directory contain list pf pages (names of file).

Tabman abstraction level responsible for working with directories. It send requests and earns data from lower 
abstraction level. Also tabman don't check data signature. This is work of database level.
Note: Tabman don't work directly with pages. It can work only with directories.

CordellDBMS source code: https://github.com/j1sk1ss/CordellDBMS.EXMPL
Credits: j1sk1ss
*/

#ifndef FILE_MANAGER_
#define FILE_MANAGER_

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <omp.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#include "dirman.h"


#define TDT_SIZE 10

#pragma region [Access]

    // Create access byte for new tables and for users. For input, this
    // function take values from 0 to 7.
    #define CREATE_ACCESS_BYTE(read_access, write_access, delete_access) \
        (((read_access & 0b111) << 6) | ((write_access & 0b111) << 3) | (delete_access & 0b111))

    // Macros for getting access level
    #define GET_READ_ACCESS(access_byte)    ((access_byte >> 6) & 0b111)
    #define GET_WRITE_ACCESS(access_byte)   ((access_byte >> 3) & 0b111)
    #define GET_DELETE_ACCESS(access_byte)  (access_byte & 0b111)

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

    // Macros for getting column primary status. (Unique value at every row).
    #define GET_COLUMN_PRIMARY(type)        ((type >> 4) & 0b11)
    // Macros for getting column data type. What data type is set in this column.
    #define GET_COLUMN_DATA_TYPE(type)      ((type >> 2) & 0b11)
    // Macros for getting column type. Can it autoincrement or something like that.
    #define GET_COLUMN_TYPE(type)           (type & 0b11)

    // Generate column type byte
    #define CREATE_COLUMN_TYPE_BYTE(is_primary, column_data_type, column_type) \
        (((is_primary & 0b11) << 4) | ((column_data_type & 0b11) << 2) | (column_type & 0b11))

    #pragma region [Column link]

        // Link type, when we delete all rows in linked columns.
        #define LINK_CASCADE_DELETE  0x01
        // Link type, when we update all rows in linked columns.
        // This mean, that we update only linked column data (we take it from provided data).
        #define LINK_CASCADE_UPDATE  0x01
        // Link type, when we add data to linked columns with dummy data.
        // Note: Useless flag, but you can use it in tour special cases.
        #define LINK_CASCADE_APPEND  0x01
        // Link type, where we return from find row function index of all rows, that was found. <WIP>
        #define LINK_CASCADE_FIND    0x01
        // Do nothing flag.
        #define LINK_NOTHING         0x00

        // Macros for getting column link cf status.
        #define GET_CF_LINK_FLAG(type) ((type >> 6) & 0b11)
        // Macros for getting column link ca status.
        #define GET_CA_LINK_FLAG(type) ((type >> 4) & 0b11)
        // Macros for getting column link cu status.
        #define GET_CU_LINK_FLAG(type) ((type >> 2) & 0b11)
        // Macros for getting column link cd status.
        #define GET_CD_LINK_FLAG(type) (type & 0b11)

        /*
        Create link type for link. Input flags for link specification.
        Note: If you have less flags, then 4, use NOTHING flag.
        cf - Cascade find flag or NOTHING
        ca - Cascade append flag or NOTHING
        cu - Cascade uppend flag or NOTHING
        cd - Cascade delete flag or NOTHING
        */
        #define CREATE_LINK_TYPE_BYTE(cf, ca, cu, cd) \
             (((cf & 0b11) << 6) | ((ca & 0b11) << 4) | ((cu & 0b11) << 2) | (cd & 0b11))

    #pragma endregion

#pragma endregion


// We have *.tb bin file, where at start placed header
//========================================================================================================================================
// HEADER (MAGIC | NAME | ACCESS | COLUMN_COUNT | DIR_COUNT) -> | COLUMNS (MAGIC | TYPE | NAME) -> | LINKS -> | DIR_NAMES -> dyn. -> end |
//========================================================================================================================================

    struct table_column_link {
        // Source name of column in source table
        uint8_t master_column_name[COLUMN_NAME_SIZE];

        // Source table name
        uint8_t slave_table_name[TABLE_NAME_SIZE];

        // Target column name
        uint8_t slave_column_name[COLUMN_NAME_SIZE];

        // Link type
        uint8_t type;
    } typedef table_column_link_t;

    struct table_column {
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

        // Column link count
        // How much links in this table
        uint8_t column_link_count;

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

        // Column links
        table_column_link_t** column_links;

        // Lock table flag
        uint8_t lock;
        uint8_t lock_owner;

        // Table directories
        uint8_t dir_names[DIRECTORIES_PER_TABLE][DIRECTORY_NAME_SIZE];
    } typedef table_t;


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
    
    Return -3 if table don't have linked directories.
    Return -1 - if not found
    Return index of value in page with end offset
    */
    int TBM_find_value(table_t* table, int offset, uint8_t value);

#pragma endregion

#pragma region [Column]

    /*
    Link column to foreing key. In summary we link provided column to column from master table.
    Note: This function don't check column. It means, that you can easily link this column to master table.
    Note 1: This function also don't check signature. If you try link string column to existed int column,
            you don't see any warns.
    Note 2: You can link slave column to not existed column in master. It can cause, because function don't
            check columns in master table. That's why be sure in master column name.

    Params:
    - master - Master table pointer.
    - master_column_name - Foreing key in master table.
    - slave - Slave table, where will be saves link data.
    - slave_column_name - Slave column name in slave table.
    - type - Link type. Check link docs.

    Return -1 if something goes wrong.
    Return 1 if link was success.
    */
    int TBM_link_column2column(table_t* master, char* master_column_name, table_t* slave, char* slave_column_name, uint8_t type);

    /*
    Delete link from slave column.

    Params:
    - master - Master table pointer.
    - master_column_name - Foreing key in master table.
    - slave - Slave table, where will be deleted link data.
    - slave_column_name - Slave column name in slave table.

    Return -1 if something goes wrong.
    Return 0 if column name not found.
    Return 1 if unlink was success.
    */
    int TBM_unlink_column_from_column(table_t* master, char* master_column_name, table_t* slave, char* slave_column_name);

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
    int TBM_update_column_in_table(table_t* table, table_column_t* column, int by_index);

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
    table_t* TBM_create_table(char* name, table_column_t** columns, int col_count, uint8_t access);

    /*
    Save table to the disk
    
    Params:
    - table - pointer to table (Can be freed after function)
    - path - place where table will be saved (Can be freed after function)
    
    Return -5 if dir names write corrupt.
    Return -4 if column links write corrupt.
    Return -3 if column names write corrupt.
    Return -2 if header write corrupt.
    Return -1 if file can't be open.
    Return 0 - if something goes wrong
    Return 1 - if save was success
    */
    int TBM_save_table(table_t* table, char* path);

    /*
    Load table from .tb bin file
    
    Params:
    - path - path of file (Can be freed after function)
    Note: Don't forget about full path. Be sure that all code coreectly use paths
    
    Return allocated table from disk 
    */
    table_t* TBM_load_table(char* path);

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

    #pragma region [TDT]

        /*
        Add table to TDT table.
        Note: It will unload old table if we earn end of TDT.
        Note 1: If we try to unload locked table, we go to next, and try to unload next.
                Also this means, that we can get deadlock.

        Params:
        - table - pointer to table (Be sure that you don't realise this table. We save link in TDT).

        Return struct table pointer
        */
        int TBM_TDT_add_table(table_t* table);

        /*
        Find table in TDT by name

        Params:
        - name - name of table for find.
                Note: not path to file. Name of table.
                      Usuale names placed in databases.

        Return directory struct or NULL if table not found.
        */
        table_t* TBM_TDT_find_table(char* name);

        /*
        Save and load tables from TDT.

        Return -1 if something goes wrong.
        Return 1 if sync success.
        */
        int TBM_TDT_sync();

        /*
        Hard cleanup of TDT. Really not recomment for use!
        Note: It will just unload data from TDT to disk by provided index.
        Note 2: Empty space will be marked by NULL.

        Params:
        - index - index of table for flushing.

        Return -1 if something goes wrong.
        Return 1 if cleanup success.
        */
        int TBM_TDT_flush_index(int index);

        /*
        Hard cleanup of TDT. Really not recomment for use!
        Note: It will just unload data from TDT to disk by provided index.
        Note 2: Empty space will be marked by NULL.

        Params:
        - table - pointer to table for flush.

        Return -1 if something goes wrong.
        Return 1 if cleanup success.
        */
        int TBM_TDT_flush_table(table_t* table);

    #pragma endregion

    #pragma region [Lock]

        /*
        Lock table for working.
        Note: Can cause deadlock, because we infinity wait for table unlock. <deprecated>
        Note 1: If we earn delay of lock try, we return -1;

        Params:
        - table - pointer to table.
        - owner - thread, that want lock this table.

        Return -2 if we try to lock NULL
        Return -1 if we can`t lock table (for some reason)
        Return 1 if table now locked.
        */
        int TBM_lock_table(table_t* table, uint8_t owner);

        /*
        Check lock status of table.

        Params:
        - table - pointer to table.
        - owner - thread, that want test this table.

        Return lock status (LOCKED and UNLOCKED).
        */
        int TBM_lock_test(table_t* table, uint8_t owner);

        /*
        Realise table for working.

        Params:
        - table - pointer to table.
        - owner - thread, that want release this table.

        Return -3 if table is NULL.
        Return -2 if this table has another owner.
        Return -1 if table was unlocked. (Nothing changed)
        Return 1 if table now unlocked.
        */
        int TBM_release_table(table_t* table, uint8_t owner);

    #pragma endregion

#pragma endregion

#endif