#include "../../include/pageman.h"


page_t* PGM_create_page(char* __restrict name, unsigned char* __restrict buffer, size_t data_size) {
    page_t* page = (page_t*)malloc(sizeof(page_t));
    page_header_t* header = (page_header_t*)malloc(sizeof(page_header_t));

    memset(page, 0, sizeof(page_t));
    memset(header, 0, sizeof(page_header_t));

    header->magic = PAGE_MAGIC;
    strncpy(header->name, name, PAGE_NAME_SIZE);
    page->lock = THR_create_lock();

    page->header = header;
    if (buffer != NULL) memcpy(page->content, buffer, data_size);
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
    #pragma omp critical (page_save)
    {
        unsigned int page_cheksum = PGM_get_checksum(page);
        #ifndef NO_PAGE_SAVE_OPTIMIZATION
        if (page_cheksum != page->header->checksum)
        #endif
        {
            // We generate default path
            char save_path[DEFAULT_PATH_SIZE] = { 0 };
            if (path == NULL) sprintf(save_path, "%s%.*s.%s", PAGE_BASE_PATH, PAGE_NAME_SIZE, page->header->name, PAGE_EXTENSION);
            else strcpy(save_path, path);

            // Open or create file
            FILE* file = fopen(save_path, "wb");
            if (file == NULL) print_error("Can't save or create [%s] file", save_path);
            else {
                // Write data to disk
                status = 1;
                int eof = PGM_get_page_occupie_size(page, PAGE_START);

                page->header->checksum = page_cheksum;
                if (fwrite(page->header, sizeof(page_header_t), 1, file) != 1) status = -2;
                if (fwrite(page->content, sizeof(unsigned char), eof, file) != (size_t)eof) status = -3;

                // Close file
                #ifndef _WIN32
                fsync(fileno(file));
                #else
                fflush(file);
                #endif

                fclose(file);
                page->content[eof] = PAGE_EMPTY;
            }
        }
    }

    return status;
}

page_t* PGM_load_page(char* name) {
    char load_path[DEFAULT_PATH_SIZE] = { 0 };
    if (get_load_path(name, PAGE_NAME_SIZE, load_path, PAGE_BASE_PATH, PAGE_EXTENSION) == -1) {
        print_error("Name should be provided!");
        return NULL;
    }

    page_t* loaded_page = (page_t*)CHC_find_entry(name, PAGE_CACHE);
    if (loaded_page != NULL) {
        print_debug("Loading page [%s] from GCT", load_path);
        return loaded_page;
    }

    #pragma omp critical (page_load)
    {
        // Open file page
        FILE* file = fopen(load_path, "rb");
        print_debug("Loading page [%s]", load_path);
        if (file == NULL) print_error("Page not found! Path: [%s]", load_path);
        else {
            // Read header from file
            page_header_t* header = (page_header_t*)malloc(sizeof(page_header_t));
            memset(header, 0, sizeof(page_header_t));
            fread(header, sizeof(page_header_t), 1, file);

            // Check page magic
            if (header->magic != PAGE_MAGIC) {
                print_error("Page file wrong magic for [%s]", load_path);
                free(header);
                fclose(file);
            } else {
                // Allocate memory for page structure
                page_t* page = (page_t*)malloc(sizeof(page_t));
                memset(page->content, PAGE_EMPTY, PAGE_CONTENT_SIZE);
                fread(page->content, sizeof(unsigned char), PAGE_CONTENT_SIZE, file);

                fclose(file);

                page->lock      = THR_create_lock();
                page->header    = header;
                loaded_page     = page;

                CHC_add_entry(
                    loaded_page, loaded_page->header->name, 
                    PAGE_CACHE, (void*)PGM_free_page, (void*)PGM_save_page
                );
            }
        }
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

unsigned int PGM_get_checksum(page_t* page) {
    unsigned int prev_checksum = page->header->checksum;
    page->header->checksum = 0;

    unsigned int checksum = 0;
    if (page->header != NULL)
        checksum = crc32(checksum, (const unsigned char*)page->header, sizeof(page_header_t));

    page->header->checksum = prev_checksum;
    checksum = crc32(checksum, (const unsigned char*)page->content, sizeof(page->content));
    return checksum;
}
