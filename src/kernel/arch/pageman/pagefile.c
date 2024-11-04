#include "../../include/pageman.h"


page_t* PGM_create_page(char* name, uint8_t* buffer, size_t data_size) {
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
    char page_name[PAGE_NAME_SIZE] = { '\0' };
    char* unique_name = generate_unique_filename(PAGE_BASE_PATH, PAGE_NAME_SIZE, PAGE_EXTENSION);
    strncpy(page_name, unique_name, PAGE_NAME_SIZE);
    free(unique_name);

    return PGM_create_page(page_name, NULL, 0);
}

int PGM_save_page(page_t* page, char* path) {
    int status = -1;
    #pragma omp critical (page_save)
    {
        #ifndef NO_PAGE_SAVE_OPTIMIZATION
        if (PGM_get_checksum(page) != page->header->checksum)
        #endif
        {
            // We generate default path
            char save_path[DEFAULT_PATH_SIZE];
            if (path == NULL) sprintf(save_path, "%s%.*s.%s", PAGE_BASE_PATH, PAGE_NAME_SIZE, page->header->name, PAGE_EXTENSION);
            else strcpy(save_path, path);

            // Open or create file
            FILE* file = fopen(save_path, "wb");
            if (file == NULL) print_error("Can't save or create [%s] file", save_path);
            else {
                // Write data to disk
                status  = 1;
                int eof = PGM_set_pe_symbol(page, PAGE_START);
                int page_size = PGM_find_value(page, 0, PAGE_END);
                if (page_size <= 0) page_size = 0;

                page->header->checksum = PGM_get_checksum(page);
                if (fwrite(page->header, sizeof(page_header_t), 1, file) != 1) status = -2;
                if (fwrite(page->content, sizeof(uint8_t), page_size, file) != (size_t)page_size) status = -3;

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

page_t* PGM_load_page(char* path, char* name) {
    char load_path[DEFAULT_PATH_SIZE];
    if (get_load_path(name, PAGE_NAME_SIZE, path, load_path, PAGE_BASE_PATH, PAGE_EXTENSION) == -1) {
        print_error("Path or name should be provided!");
        return NULL;
    }

    char file_name[PAGE_NAME_SIZE];
    if (get_filename(name, path, file_name, PAGE_NAME_SIZE) == -1) return NULL;
    page_t* loaded_page = PGM_PDT_find_page(file_name);
    if (loaded_page != NULL) return loaded_page;

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
                fread(page->content, sizeof(uint8_t), PAGE_CONTENT_SIZE, file);

                fclose(file);

                page->lock = THR_create_lock();
                page->header = header;
                PGM_PDT_add_page(page);
                loaded_page = page;
            }
        }
    }

    return loaded_page;
}

int PGM_free_page(page_t* page) {
    if (page == NULL) return -1;
    SOFT_FREE(page->header);
    SOFT_FREE(page);

    return 1;
}

uint32_t PGM_get_checksum(page_t* page) {
    uint32_t prev_checksum = page->header->checksum;
    page->header->checksum = 0;

    uint32_t checksum = 0;
    if (page->header != NULL)
        checksum = crc32(checksum, (const uint8_t*)page->header, sizeof(page_header_t));

    page->header->checksum = prev_checksum;
    checksum = crc32(checksum, (const uint8_t*)page->content, sizeof(page->content));
    return checksum;
}
