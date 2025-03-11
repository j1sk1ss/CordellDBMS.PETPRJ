#include "../../include/pageman.h"


page_t* PGM_create_page(unsigned char* __restrict buffer, size_t data_size) {
    page_t* page = (page_t*)malloc(sizeof(page_t));
    page_header_t* header = (page_header_t*)malloc(sizeof(page_header_t));
    if (!page || !header) {
        SOFT_FREE(page);
        SOFT_FREE(header);
        return NULL;
    }

    memset(page, 0, sizeof(page_t));
    memset(header, 0, sizeof(page_header_t));

    header->magic = PAGE_MAGIC;
    page->lock = THR_create_lock();

    page->header = header;
    memset(page->content, PAGE_EMPTY, PAGE_CONTENT_SIZE);
    if (buffer != NULL) memcpy(page->content, buffer, data_size);
    return page;
}

page_t* PGM_create_empty_page(char* directory_name) {
    page_t* page = PGM_create_page(NULL, 0);
    page->directory_name = (char*)malloc(strlen(directory_name) + 1);
    if (!page->directory_name) {
        return NULL;
    }

    strcpy(page->directory_name, directory_name);
    return page;
}

int PGM_save_page(page_t* page) {
    int status = -1;
    #pragma omp critical (page_save)
    {
        unsigned int page_cheksum = 0;
        #ifndef NO_PAGE_SAVE_OPTIMIZATION
        page_cheksum = PGM_get_checksum(page);
        if (page_cheksum != page->header->checksum)
        #endif
        {
            // We generate default path
            char save_path[DEFAULT_PATH_SIZE] = { 0 };
            get_load_path(page->directory_name, strlen(page->directory_name), save_path, PAGE_BASE_PATH, PAGE_EXTENSION);

            // Open or create file
            int fd = open(save_path, O_WRONLY | O_CREAT, 0644);
            if (fd < 0) { print_error("Can't save or create [%s] file", save_path); }
            else {
                // Write data to disk
                status = 1;
                page->header->checksum = page_cheksum;
                off_t offset = page->header->offset * (sizeof(page_header_t) + PAGE_CONTENT_SIZE);
                if (pwrite(fd, page->header, sizeof(page_header_t), offset) != sizeof(page_header_t)) status = -2;
                if (pwrite(fd, page->content, PAGE_CONTENT_SIZE, offset + sizeof(page_header_t)) != PAGE_CONTENT_SIZE) status = -3;

                fsync(fd);
                close(fd);
            }
        }
    }

    return status;
}

page_t* PGM_load_page(char* directory_name, int offset) {
    char load_path[DEFAULT_PATH_SIZE] = { 0 };
    if (get_load_path(directory_name, strlen(directory_name), load_path, PAGE_BASE_PATH, PAGE_EXTENSION) == -1) {
        print_error("Name should be provided!");
        return NULL;
    }

    char page_entry_name[PAGE_NAME_SIZE];
    sprintf(page_entry_name, "%d", offset);
    page_t* loaded_page = (page_t*)CHC_find_entry(page_entry_name, directory_name, PAGE_CACHE);
    if (loaded_page != NULL) {
        print_io("Loading page [%s] from GCT (%s)", page_entry_name, load_path);
        return loaded_page;
    }

    #pragma omp critical (page_load)
    {
        // Open file page
        int fd = open(load_path, O_RDONLY);
        print_io("Loading page [%s] (%s)", page_entry_name, load_path);
        if (fd < 0) { print_error("Page not found! Path: [%s]", load_path); }
        else {
            // Read header from file
            page_header_t* header = (page_header_t*)malloc(sizeof(page_header_t));
            if (header) {
                off_t file_offset = offset * (sizeof(page_header_t) + PAGE_CONTENT_SIZE);
                memset(header, 0, sizeof(page_header_t));
                pread(fd, header, sizeof(page_header_t), file_offset);
                file_offset += sizeof(page_header_t);

                // Check page magic
                if (header->magic != PAGE_MAGIC) {
                    print_error("Page file wrong magic for [%s]", load_path);
                    free(header);
                    close(fd);
                } else {
                    // Allocate memory for page structure
                    page_t* page = (page_t*)malloc(sizeof(page_t));
                    if (!page) free(header);
                    else {
                        memset(page->content, PAGE_EMPTY, PAGE_CONTENT_SIZE);
                        pread(fd, page->content, PAGE_CONTENT_SIZE, file_offset);
                        close(fd);

                        page->lock   = THR_create_lock();
                        page->header = header;
                        loaded_page  = page;

                        char entry_page_name[PAGE_NAME_SIZE];
                        sprintf(entry_page_name, "%d", offset);
                        CHC_add_entry(
                            loaded_page, entry_page_name, directory_name, PAGE_CACHE, (void*)PGM_free_page, (void*)PGM_save_page
                        );
                    }
                }
            }
        }
    }

    loaded_page->directory_name = (char*)malloc(strlen(directory_name) + 1);
    if (!loaded_page->directory_name) {
        PGM_free_page(loaded_page);
        return NULL;
    }

    strcpy(loaded_page->directory_name, directory_name);
    return loaded_page;
}

int PGM_delete_page(page_t* page) {
    if (!page) return -1;
    
    int status = 1;
    char load_path[DEFAULT_PATH_SIZE] = { 0 };
    if (get_load_path(page->directory_name, strlen(page->directory_name), load_path, PAGE_BASE_PATH, PAGE_EXTENSION) == -1) {
        print_error("Name should be provided!");
        return -1;
    }

    #pragma omp critical (page_delete)
    {
        int fd = open(load_path, O_WRONLY, 0644);
        if (fd < 0) { 
            print_error("Can't open page file [%s]", load_path); 
            status = -1;
        } else {
            char zero_buffer[sizeof(page_header_t) + PAGE_CONTENT_SIZE] = { 0 };
            off_t offset = page->header->offset * (sizeof(page_header_t) + PAGE_CONTENT_SIZE);
            if (pwrite(fd, zero_buffer, sizeof(zero_buffer), offset) != sizeof(zero_buffer)) {
                print_error("Failed to zero page at offset %ld", offset);
                status = -2;
            }
            
            fsync(fd);
            close(fd);
        }
    }

    return status;
}

int PGM_flush_page(page_t* page) {
    if (!page) return -2;
    if (page->is_cached == 1) return -1;
    PGM_save_page(page);
    return PGM_free_page(page);
}

int PGM_free_page(page_t* page) {
    if (!page) return -1;
    SOFT_FREE(page->header);
    SOFT_FREE(page->directory_name);
    SOFT_FREE(page);
    return 1;
}

unsigned int PGM_get_checksum(page_t* page) {
    if (!page) return 0;
    unsigned int prev_checksum = page->header->checksum;
    page->header->checksum = 0;

    unsigned int checksum = 0;
    if (page->header != NULL)
        checksum = crc32(checksum, (const unsigned char*)page->header, sizeof(page_header_t));

    page->header->checksum = prev_checksum;
    checksum = crc32(checksum, (const unsigned char*)page->content, sizeof(page->content));
    return checksum;
}
