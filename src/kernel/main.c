/*
*   This is sandbox file
*/

#include <string.h>

#include "include/tabman.h"
#include "include/dirman.h"
#include "include/pageman.h"


int main() {
    directory_t* dir = DRM_create_directory("dir1\0");

    char content[1024];
    sprintf(content, "Column0Row0%cColumn1Row0%cColumn0Row1%cColumn1Row1", COLUMN_DELIMITER, ROW_DELIMITER, COLUMN_DELIMITER, ROW_DELIMITER);
    page_t* page = PGM_create_page("page1\0", content);

    DRM_link_page2dir(dir, page);
    DRM_save_directory(dir, "directory.dr");
    PGM_save_page(page, "page.pg");

    PGM_free_page(page);
    DRM_free_directory(dir);

    directory_t* dr = DRM_load_directory("directory.dr");
    printf("Dir: name - %s | pc - %i\nPage - %s\n", dr->header->name, dir->header->page_count, dir->names[0]);

    page_t* pg = PGM_load_page("page.pg");
    printf("Page: name - %s\nContent - %s\n", pg->header->name, pg->content);

    return 1;
}