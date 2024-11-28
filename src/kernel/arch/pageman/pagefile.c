#include "../../include/pageman.h"


page_t* PGM_create_page(char* __restrict name, unsigned char* __restrict buffer, size_t data_size) {
page_t* PGM_create_page(char* __restrict name, unsigned char* __restrict buffer, size_t data_size) {
    page_t* page = (page_t*)malloc(sizeof(page_t));
    page_header_t* header = (page_header_t*)malloc(sizeof(page_header_t));

    memset_s(page, 0, sizeof(page_t));
    memset_s(header, 0, sizeof(page_header_t));

    header->magic = PAGE_MAGIC;
    strncpy_s(header->name, name, PAGE_NAME_SIZE);
    page->lock = THR_create_lock();
    page->is_cached = 0;

    page->header = header;
    if (buffer != NULL) memcpy_s(page->content, buffer, data_size);
    for (int i = data_size + 1; i < PAGE_CONTENT_SIZE; i++) page->content[i] = PAGE_EMPTY;
    return page;
}

page_t* PGM_create_empty_page() {
    char* unique_name = generate_unique_filename(PAGE_BASE_PATH, PAGE_NAME_SIZE, PAGE_EXTENSION);
    page_t* page = PGM_create_page(unique_name, NULL, 0);
    free(unique_name);
    return page;
}

int PGM_save_page(page_t* __restrict page, char* __restrict path) {
    int status = -1;
    // We generate default path
    char save_path[DEFAULT_PATH_SIZE] = { 0 };
    if (path == NULL) sprintf(save_path, "%s%.*s.%s", PAGE_BASE_PATH, PAGE_NAME_SIZE, page->header->name, PAGE_EXTENSION);
    else strcpy(save_path, path);

    lfs_file_t file;
    lfs_file_open(&lfs_body, &file, save_path, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);

    // Write data to disk
    status  = 1;
    int eof = PGM_set_pe_symbol(page, PAGE_START);
    int page_size = PGM_find_value(page, 0, PAGE_END);
    if (page_size <= 0) page_size = 0;

    lfs_file_write(&lfs_body, &file, page->header, sizeof(page_header_t));
    lfs_file_write(&lfs_body, &file, page->content, sizeof(unsigned char) * page_size);
    lfs_file_close(&lfs_body, &file);
    
    page->content[eof] = PAGE_EMPTY;

    return status;
}

page_t* PGM_load_page(char* __restrict path, char* __restrict name) {
    char load_path[DEFAULT_PATH_SIZE] = { 0 };
    char load_path[DEFAULT_PATH_SIZE] = { 0 };
    if (get_load_path(name, PAGE_NAME_SIZE, path, load_path, PAGE_BASE_PATH, PAGE_EXTENSION) == -1) {
        print_error("Path or name should be provided!");
        return NULL;
    }

    char file_name[PAGE_NAME_SIZE] = { 0 };
    if (get_filename(name, path, file_name, PAGE_NAME_SIZE) == -1) return NULL;
    page_t* loaded_page = (page_t*)CHC_find_entry(file_name, PAGE_CACHE);
    if (loaded_page != NULL) {
        print_debug("Loading page [%s] from GCT", load_path);
        return loaded_page;
    }

    // Open file page
    lfs_file_t file;
    lfs_file_open(&lfs_body, &file, load_path, LFS_O_RDONLY);
    print_debug("Loading page [%s]", load_path);
    
    // Read header from file
    page_header_t* header = (page_header_t*)malloc(sizeof(page_header_t));
    memset(header, 0, sizeof(page_header_t));
    lfs_file_read(&lfs_body, &file, header, sizeof(page_header_t));

    // Check page magic
    if (header->magic != PAGE_MAGIC) {
        print_error("Page file wrong magic for [%s]", load_path);
        free(header);
        lfs_file_close(&lfs_body, &file);
    } else {
        // Allocate memory for page structure
        page_t* page = (page_t*)malloc(sizeof(page_t));
        memset(page->content, PAGE_EMPTY, PAGE_CONTENT_SIZE);
        lfs_file_read(&lfs_body, &file, page->content, sizeof(unsigned char) * PAGE_CONTENT_SIZE);
        lfs_file_close(&lfs_body, &file);

        page->is_cached = 0;
        page->header    = header;
        loaded_page     = page;

        CHC_add_entry(loaded_page, loaded_page->header->name, PAGE_CACHE, PGM_free_page, PGM_save_page);
    }

    return loaded_page;
}

int PGM_flush_page(page_t* page) {
    if (page == NULL) return -2;
    if (page->is_cached == 1) return -1;

    PGM_save_page(page, NULL);
    return PGM_free_page(page);
}

int PGM_free_page(page_t* page) {
    if (page == NULL) return -1;

    SOFT_FREE(page->header);
    SOFT_FREE(page);

    return 1;
}
