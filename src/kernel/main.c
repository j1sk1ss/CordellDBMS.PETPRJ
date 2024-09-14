/*
*   This is sandbox file
*/

#include <string.h>

#include "include/tabman.h"
#include "include/dirman.h"
#include "include/pageman.h"


#define DIR_APPEND_TEST


int main() {
#ifdef BASIC_TEST
    directory_t* dir = DRM_create_directory("dir1");

    char content[1024];
    sprintf(content, "Column0Row0%cColumn1Row0%cColumn0Row1%cColumn1Row1", COLUMN_DELIMITER, ROW_DELIMITER, COLUMN_DELIMITER, ROW_DELIMITER);
    page_t* page = PGM_create_page("page1", content, 51);

    DRM_link_page2dir(dir, page);
    DRM_save_directory(dir, "directory.dr");
    PGM_save_page(page, "page.pg");

    PGM_free_page(page);
    DRM_free_directory(dir);

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
    return 1;
}