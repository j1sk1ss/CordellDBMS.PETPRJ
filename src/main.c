#include <string.h>
#include "include/dirman.h"
#include "include/pageman.h"


int main() {
    directory_t* dir = create_directory("dir1\0", NULL, 0);
    page_t* page = create_page("page1\0", "Hello there guys\nHello there table");

    link_page2dir(dir, page);
    save_directory(dir, "directory.dr");
    save_page(page, "page.pg");

    return 1;
}