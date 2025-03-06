#include "../../include/pageman.h"


page_t* PGM_create_page(char* __restrict name, unsigned char* __restrict buffer, size_t data_size) {
    page_t* page = (page_t*)malloc_s(sizeof(page_t));
    page_header_t* header = (page_header_t*)malloc_s(sizeof(page_header_t));
    if (!page || !header) {
        SOFT_FREE(page);
        SOFT_FREE(header);
        return NULL;
    }

    memset_s(page, 0, sizeof(page_t));
    memset_s(header, 0, sizeof(page_header_t));

    header->magic = PAGE_MAGIC;
    strncpy_s(header->name, name, PAGE_NAME_SIZE);
    page->lock = THR_create_lock();

    page->header = header;
    if (buffer != NULL) memcpy_s(page->content, buffer, data_size);
    for (int i = data_size + 1; i < PAGE_CONTENT_SIZE; i++) page->content[i] = encode_hamming_15_11(PAGE_EMPTY);
    return page;
}

page_t* PGM_create_empty_page(char* base_path) {
    char* unique_name = generate_unique_filename(base_path, PAGE_NAME_SIZE, PAGE_EXTENSION);
    if (!unique_name) return NULL;

    page_t* page = PGM_create_page(unique_name, NULL, 0);
    page->base_path = (char*)malloc_s(strlen_s(base_path) + 1);
    if (!page->base_path) {
        SOFT_FREE(unique_name);
        return NULL;
    }

    strcpy_s(page->base_path, base_path);
    
    SOFT_FREE(unique_name);
    return page;
}

int PGM_save_page(page_t* page) {
    int status = -1;
    #pragma omp critical (page_save)
    {
        unsigned int page_cheksum = PGM_get_checksum(page);
        #ifndef NO_PAGE_SAVE_OPTIMIZATION
        if (page_cheksum != page->header->checksum)
        #endif
        {
            // We generate default path
            char save_path[DEFAULT_PATH_SIZE] = { 0 };
            get_load_path(page->header->name, PAGE_NAME_SIZE, save_path, page->base_path, PAGE_EXTENSION);
            mkdir(page->base_path, 0777);

            // Open or create file
            int fd = open(save_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) { print_error("Can't save or create [%s] file", save_path); }
            else {
                // Write data to disk
                int eof = PGM_get_page_occupie_size(page, PAGE_START);

                status = 1;
                page->header->checksum = page_cheksum;

                unsigned short encoded_header[sizeof(page_header_t)] = { 0 };
                pack_memory((unsigned char*)page->header, (unsigned short*)encoded_header, sizeof(page_header_t));
                if (pwrite(fd, &encoded_header, sizeof(page_header_t) * sizeof(unsigned short), 0) != sizeof(page_header_t) * sizeof(unsigned short)) status = -2;
                if (pwrite(fd, page->content, eof * sizeof(unsigned short), sizeof(page_header_t) * sizeof(unsigned short)) != (ssize_t)(eof * sizeof(unsigned short))) status = -3;

                fsync(fd);
                close(fd);
            }
        }
    }

    return status;
}

page_t* PGM_load_page(char* base_path, char* name) {
    char load_path[DEFAULT_PATH_SIZE] = { 0 };
    get_load_path(name, PAGE_NAME_SIZE, load_path, base_path, PAGE_EXTENSION);

    page_t* loaded_page = (page_t*)CHC_find_entry(name, base_path, PAGE_CACHE);
    if (loaded_page != NULL) {
        print_io("Loading page [%s] from GCT", load_path);
        return loaded_page;
    }

    #pragma omp critical (page_load)
    {
        // Open file page
        int fd = open(load_path, O_RDONLY);
        print_io("Loading page [%s]", load_path);
        if (fd < 0) { print_error("Page not found! Path: [%s]", load_path); }
        else {
            // Read header from file
            page_header_t* header = (page_header_t*)malloc_s(sizeof(page_header_t));
            if (header) {
                memset_s(header, 0, sizeof(page_header_t));

                unsigned short encoded_header[sizeof(page_header_t)] = { 0 };
                pread(fd, encoded_header, sizeof(page_header_t) * sizeof(unsigned short), 0);
                unpack_memory((unsigned short*)encoded_header, (unsigned char*)header, sizeof(page_header_t));

                // Check page magic
                if (header->magic != PAGE_MAGIC) {
                    print_error("Page file wrong magic for [%s]", load_path);
                    free_s(header);
                    close(fd);
                } else {
                    // Allocate memory for page structure
                    page_t* page = (page_t*)malloc_s(sizeof(page_t));
                    if (!page) free_s(header);
                    else {
                        unsigned short encoded_pm = encode_hamming_15_11((unsigned short)PAGE_EMPTY);
                        for (int i = 0; i < PAGE_CONTENT_SIZE; i++) page->content[i] = encoded_pm;
                        pread(fd, page->content, PAGE_CONTENT_SIZE * sizeof(unsigned short), sizeof(page_header_t) * sizeof(unsigned short));
                        close(fd);

                        page->lock   = THR_create_lock();
                        page->header = header;
                        loaded_page  = page;

                        CHC_add_entry(
                            loaded_page, loaded_page->header->name, base_path, PAGE_CACHE, (void*)PGM_free_page, (void*)PGM_save_page
                        );
                    }
                }
            }
        }
    }

    loaded_page->base_path = (char*)malloc_s(strlen_s(base_path) + 1);
    if (!loaded_page->base_path) {
        PGM_free_page(loaded_page);
        return NULL;
    }

    strcpy_s(loaded_page->base_path, base_path);
    return loaded_page;
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
    SOFT_FREE(page->base_path);
    SOFT_FREE(page);
    return 1;
}

unsigned int PGM_get_checksum(page_t* page) {
    if (!page) return 0;
    unsigned int prev_checksum = page->header->checksum;
    page->header->checksum = 0;

    unsigned int _checksum = 0;
    if (page->header != NULL)
        _checksum = checksum(_checksum, (const unsigned char*)page->header, sizeof(page_header_t));

    page->header->checksum = prev_checksum;
    _checksum = checksum(_checksum, (const unsigned char*)page->content, sizeof(page->content));
    return _checksum;
}
