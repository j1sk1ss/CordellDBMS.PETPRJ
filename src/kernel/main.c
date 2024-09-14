/*
*   This is sandbox file
*/

#include <string.h>

#include "include/tabman.h"
#include "include/dirman.h"
#include "include/pageman.h"


#define TABLE_LOAD_TEST


int main() {
#ifdef DIR_PAGE_SAVE_TEST

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

#ifdef DIR_PAGE_LOAD_TEST

    directory_t* dr = DRM_load_directory("directory.dr");
    page_t* pg = PGM_load_page("page.pg");

    PGM_append_content(pg, "Hello there", 11);
    PGM_save_page(pg, "page.pg");

    printf("Page: name - %s\nContent - %s\n", pg->header->name, pg->content);
    PGM_free_page(pg);

#endif

#ifdef DIR_APPEND_TEST

    char content[1024];
    sprintf(content, "Column0Row0%cColumn1Row0%cColumn0Row1%cColumn1Row1", COLUMN_DELIMITER, ROW_DELIMITER, COLUMN_DELIMITER, ROW_DELIMITER);
    directory_t* dir = DRM_create_directory("dir1");

    DRM_append_content(dir, content, 50);
    DRM_save_directory(dir, "dir.dr");

    DRM_free_directory(dir);

#endif

#ifdef DIR_INSERT_TEST

    char content[1024];
    sprintf(content, "Column0Row0%cColumn1Row0%cColumn0Row1%cColumn1Row1", COLUMN_DELIMITER, ROW_DELIMITER, COLUMN_DELIMITER, ROW_DELIMITER);
    directory_t* dir = DRM_create_directory("dir1");

    DRM_insert_content(dir, 126, content, 50);
    DRM_save_directory(dir, "dir.dr");

    DRM_free_directory(dir);

#endif

#ifdef TABLE_SAVE_TEST

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

#ifdef TABLE_LOAD_TEST

    table_t* tab1 = TBM_load_table("table.tb");
    if (tab1 == NULL) {
        printf("Something wrong!\n");
    }

    printf("Name %s, CC %i, DC %i\nFCname: %s", tab1->header->name, tab1->header->column_count, tab1->header->dir_count, tab1->columns[0]->name);
    TBM_free_table(tab1);

#endif

#ifdef TABLE_APPEND_TEST



#endif
    return 1;
}