/*
 * This is sandbox file for testing functions in DBMS
 *
 * ! Building of this code will produce test program !
 * building without OMP: gcc -Wall .\test.c .\kernel\std\* .\server\* -Wunknown-pragmas -fpermissive -o cdbms.exe
 * buildinc with OMP: gcc -Wall .\test.c .\kernel\std\* .\server\* -fopenmp -fpermissive -o cdbms.exe
*/

#include <string.h>

#include "kernel/include/tabman.h"
#include "kernel/include/dirman.h"
#include "kernel/include/pageman.h"
#include "kernel/include/database.h"
#include "kernel/include/traceback.h"


#define DATABASE_APPEND_AND_LINK_TEST

int main() {
    TB_enable();

#ifdef DIR_PAGE_SAVE_TEST  // Success

    directory_t* dir = DRM_create_directory("dir1");

    char content[1024];
    sprintf(content, "Column0Row0%cColumn1Row0%cColumn0Row1%cColumn1Row1", COLUMN_DELIMITER, ROW_DELIMITER, COLUMN_DELIMITER, ROW_DELIMITER);
    page_t* page = PGM_create_page("page1", content, 51);

    DRM_link_page2dir(dir, page);
    DRM_save_directory(dir, "directory.dr");
    PGM_save_page(page, "page.pg");

    PGM_free_page(page);
    DRM_free_directory(dir);

#endif

#ifdef DIR_PAGE_LOAD_TEST  // Success

    directory_t* dr = DRM_load_directory("directory.dr");
    page_t* pg = PGM_load_page("page.pg");

    PGM_append_content(pg, "Hello there", 11);
    PGM_save_page(pg, "page.pg");

    printf("Page: name - %s\nContent - %s\n", pg->header->name, pg->content);
    PGM_free_page(pg);

#endif

#ifdef TABLE_SAVE_TEST  // Success

    table_column_t** columns = (table_column_t**)malloc(3);

    table_column_t* column1 = TBM_create_column(COLUMN_TYPE_ANY, "col1");
    table_column_t* column2 = TBM_create_column(COLUMN_TYPE_STRING, "col2");
    table_column_t* column3 = TBM_create_column(COLUMN_TYPE_INT, "col3");

    columns[0] = column1;
    columns[1] = column2;
    columns[2] = column3;

    table_t* table = TBM_create_table("table1", columns, 3);
    TBM_save_table(table, "table.tb");
    TBM_free_table(table);

#endif

#ifdef TABLE_LOAD_TEST  // Success

    table_t* tab1 = TBM_load_table("table.tb");
    if (tab1 == NULL) {
        printf("Something wrong!\n");
    }

    printf("Name %s, CC %i, DC %i\nFCname: %s", tab1->header->name, tab1->header->column_count, tab1->header->dir_count, tab1->columns[0]->name);
    TBM_free_table(tab1);

#endif

#ifdef SIGNATURE_TEST  // Success

    table_column_t** columns = (table_column_t**)calloc(4, sizeof(table_column_t*));

    table_column_t* column1 = TBM_create_column(COLUMN_TYPE_ANY, "col1");
    table_column_t* column2 = TBM_create_column(COLUMN_TYPE_STRING, "col2");
    table_column_t* column3 = TBM_create_column(COLUMN_TYPE_INT, "col3");

    columns[0] = column1;
    columns[1] = column2;
    columns[2] = column3; 

    table_t* table = TBM_create_table("table1", columns, 3);
    
    char wrong_data[60];
    sprintf(wrong_data, "column%cstring%c1.00", COLUMN_DELIMITER, COLUMN_DELIMITER);

    char wrong_data_1[60];
    sprintf(wrong_data_1, "column%c123%c1.00", COLUMN_DELIMITER, COLUMN_DELIMITER);

    char wrong_data_2[60];
    sprintf(wrong_data_2, "column%c1.00", COLUMN_DELIMITER);

    char correct_data[60];
    sprintf(correct_data, "column%cstring%c100", COLUMN_DELIMITER, COLUMN_DELIMITER);
    
    printf("Wrong data: %i\n", TBM_check_signature(table, wrong_data));
    printf("Wrong data 1: %i\n", TBM_check_signature(table, wrong_data_1));
    printf("Wrong data 2: %i\n", TBM_check_signature(table, wrong_data_2));
    printf("Correct data: %i\n", TBM_check_signature(table, correct_data));

#endif

#ifdef DATABASE_APPEND_AND_LINK_TEST  // Success

    database_t* database = DB_create_database("db1");
    table_column_t** columns = (table_column_t**)calloc(3, sizeof(table_column_t*));
    table_column_t** columns1 = (table_column_t**)calloc(3, sizeof(table_column_t*));

    table_column_t* column1 = TBM_create_column(CREATE_COLUMN_TYPE_BYTE(
        COLUMN_PRIMARY,
        COLUMN_TYPE_ANY,
        COLUMN_NO_AUTO_INC
    ), 10, "col1");

    table_column_t* column2 = TBM_create_column(CREATE_COLUMN_TYPE_BYTE(
        COLUMN_NOT_PRIMARY,
        COLUMN_TYPE_STRING,
        COLUMN_AUTO_INCREMENT
    ), 10, "col2");

    table_column_t* column3 = TBM_create_column(CREATE_COLUMN_TYPE_BYTE(
        COLUMN_NOT_PRIMARY,
        COLUMN_TYPE_ANY,
        COLUMN_NO_AUTO_INC
    ), 10, "col3");

    table_column_t* column4 = TBM_create_column(CREATE_COLUMN_TYPE_BYTE(
        COLUMN_PRIMARY,
        COLUMN_TYPE_INT,
        COLUMN_NO_AUTO_INC
    ), 10, "col4");

    columns[0] = column1;
    columns[1] = column2;
    columns[2] = column3;
    columns1[0] = column4; 

    table_t* table = TBM_create_table("table1", columns, 3, CREATE_ACCESS_BYTE(7, 3, 7));
    table_t* table1 = TBM_create_table("table11", columns1, 1, CREATE_ACCESS_BYTE(7, 3, 7));
    DB_link_table2database(database, table);
    DB_link_table2database(database, table1);
    DB_save_database(database, "db.db");

    TBM_link_column2column(table, "col2", table1, "col4", CREATE_LINK_TYPE_BYTE(LINK_NOTHING, LINK_NOTHING, LINK_NOTHING, LINK_NOTHING));

    TBM_save_table(table1, "table11.tb");
    TBM_free_table(table1);

    TBM_save_table(table, "table1.tb");
    TBM_free_table(table);

    int result = DB_append_row(database, "table1", "hello guysstring  101000000000", 30,  CREATE_ACCESS_BYTE(0, 0, 0));
    printf("First table append result %i\n", result);

    result = DB_append_row(database, "table1", "hello LOLLOLLOLU  101000000000", 30,  CREATE_ACCESS_BYTE(0, 0, 0));
    printf("First table append result %i\n", result);

    result = DB_append_row(database, "table11", "0000000001", 10,  CREATE_ACCESS_BYTE(0, 0, 0));
    printf("Second table append result %i\n", result);

    uint8_t* data = DB_get_row(database, "table1", 0, CREATE_ACCESS_BYTE(0, 0, 0));
    printf("[%s]\n", data);
    free(data);

    data = DB_get_row(database, "table1", 1, CREATE_ACCESS_BYTE(0, 0, 0));
    printf("[%s]\n", data);
    free(data);

#endif

#ifdef DATABASE_FIND_ROW_TEST // Success

    database_t* database = DB_create_database("db1");
    table_column_t** columns = (table_column_t**)calloc(3, sizeof(table_column_t*));
    table_column_t** columns1 = (table_column_t**)calloc(3, sizeof(table_column_t*));

    table_column_t* column1 = TBM_create_column(CREATE_COLUMN_TYPE_BYTE(
        COLUMN_PRIMARY,
        COLUMN_TYPE_ANY,
        COLUMN_NO_AUTO_INC
    ), 10, "col1");

    table_column_t* column2 = TBM_create_column(CREATE_COLUMN_TYPE_BYTE(
        COLUMN_NOT_PRIMARY,
        COLUMN_TYPE_STRING,
        COLUMN_AUTO_INCREMENT
    ), 10, "col2");

    table_column_t* column3 = TBM_create_column(CREATE_COLUMN_TYPE_BYTE(
        COLUMN_NOT_PRIMARY,
        COLUMN_TYPE_ANY,
        COLUMN_NO_AUTO_INC
    ), 10, "col3");

    columns[0] = column1;
    columns[1] = column2;
    columns[2] = column3;

    table_t* table = TBM_create_table("table1", columns, 3, CREATE_ACCESS_BYTE(7, 3, 7));
    DB_link_table2database(database, table);
    DB_save_database(database, "db.db");

    TBM_save_table(table, "table1.tb");
    TBM_free_table(table);

    int result = DB_append_row(database, "table1", "he11o 11111LOLLU  102000000000", 30,  CREATE_ACCESS_BYTE(0, 0, 0));
    printf("Append result %i\n", result);

    result = DB_append_row(database, "table1", "LOLlo \n\t\r1OLLOLU  101000000000", 30,  CREATE_ACCESS_BYTE(0, 0, 0));
    printf("Append result %i\n", result);

    int result1 = DB_find_data_row(database, "table1", "col1", 0, "LOL", 3, CREATE_ACCESS_BYTE(0, 0, 0));
    int result2 = DB_find_value_row(database, "table1", "col3", 0, '1', CREATE_ACCESS_BYTE(0, 0, 0));

    printf("Found rows of data: %i == 1 | %i == 1\n", result1, result2);

    uint8_t* lol_data = DB_get_row(database, "table1", result1, CREATE_ACCESS_BYTE(0, 0, 0));
    printf("Data from %i row: [%s]", result1, lol_data);
    free(lol_data);

#endif

#ifdef DATABASE_DELETE_ROW_TEST // Success

    database_t* database = DB_create_database("db1");
    table_column_t** columns = (table_column_t**)calloc(3, sizeof(table_column_t*));
    table_column_t** columns1 = (table_column_t**)calloc(3, sizeof(table_column_t*));

    table_column_t* column1 = TBM_create_column(CREATE_COLUMN_TYPE_BYTE(
        COLUMN_PRIMARY,
        COLUMN_TYPE_ANY,
        COLUMN_NO_AUTO_INC
    ), 10, "col1");

    table_column_t* column2 = TBM_create_column(CREATE_COLUMN_TYPE_BYTE(
        COLUMN_NOT_PRIMARY,
        COLUMN_TYPE_STRING,
        COLUMN_AUTO_INCREMENT
    ), 10, "col2");

    table_column_t* column3 = TBM_create_column(CREATE_COLUMN_TYPE_BYTE(
        COLUMN_NOT_PRIMARY,
        COLUMN_TYPE_ANY,
        COLUMN_NO_AUTO_INC
    ), 10, "col3");

    columns[0] = column1;
    columns[1] = column2;
    columns[2] = column3;

    table_t* table = TBM_create_table("table1", columns, 3, CREATE_ACCESS_BYTE(7, 3, 7));
    DB_link_table2database(database, table);
    DB_save_database(database, "db.db");

    TBM_save_table(table, "table1.tb");
    TBM_free_table(table);

    int result = DB_append_row(database, "table1", "he11o 11111LOLLU  102000000000", 30,  CREATE_ACCESS_BYTE(0, 0, 0));
    printf("Append result %i\n", result);

    result = DB_append_row(database, "table1", "LOLlo \n\t\r1OLLOLU  101000000000", 30,  CREATE_ACCESS_BYTE(0, 0, 0));
    printf("Append result %i\n", result);

    int result1 = DB_find_data_row(database, "table1", "col1", 0, "LOL", 3, CREATE_ACCESS_BYTE(0, 0, 0));
    int result2 = DB_find_value_row(database, "table1", "col3", 0, '1', CREATE_ACCESS_BYTE(0, 0, 0));

    printf("Found rows of data: %i == 1 | %i == 1\n", result1, result2);

    uint8_t* lol_data = DB_get_row(database, "table1", result1, CREATE_ACCESS_BYTE(0, 0, 0));
    printf("Data from %i row: [%s]", result1, lol_data);
    free(lol_data);

    DB_delete_row(database, "table1", 0, CREATE_ACCESS_BYTE(0, 0, 0));
    lol_data = DB_get_row(database, "table1", 0, CREATE_ACCESS_BYTE(0, 0, 0));
    printf("Data from deleted row: [%s]", lol_data);
    free(lol_data);

    result1 = DB_find_data_row(database, "table1", "col1", 0, "LOL", 3, CREATE_ACCESS_BYTE(0, 0, 0));
    printf("Try to find deleted row: %i == -1\n", result1);

    result = DB_append_row(database, "table1", "222222222221OLLOLU  1010000000", 30,  CREATE_ACCESS_BYTE(0, 0, 0));
    printf("Append result %i\n", result);

#endif

    printf("End of code\n");
    return 1;
}