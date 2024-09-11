#include "../include/pageman.h"


page_t* create_page(char* name, uint8_t* buffer) {
    page_t* page = (page_t*)malloc(sizeof(page_t));
    page_header_t* header = (page_header_t*)malloc(sizeof(page_header_t));

    header->magic = PAGE_MAGIC;
    strncpy((char*)header->name, name, PAGE_NAME_SIZE);

    page->header = header;
    strncpy((char*)page->content, (char*)buffer, PAGE_CONTENT_SIZE);
    page->content[MIN(PAGE_CONTENT_SIZE, strlen((char*)buffer) + 1)] = PAGE_END;

    return page;
}

int save_page(page_t* page, char* path) {
    // Open or create file
    FILE* file = fopen(path, "wb");
    if (file == NULL) {
        return -1;
    }

    // Write header
    fwrite(page->header, sizeof(page_header_t), SEEK_CUR, file);

    // Write content
    fwrite(page->content, PAGE_CONTENT_SIZE, SEEK_CUR, file);

    // Close file
    fclose(file);

    return 1;
}

page_t* load_page(char* name) {
    // Open file page
    FILE* file = fopen(name, "rb");
    if (file == NULL) {
        return NULL;
    }

    // Read header from file
    uint8_t* header_data = (uint8_t*)malloc(sizeof(page_header_t));
    fread(header_data, sizeof(page_header_t), SEEK_CUR, file);
    page_header_t* header = (page_header_t*)header_data;

    // Check page magic
    if (header->magic != PAGE_MAGIC) {
        free(header);
        return NULL;
    }

    // Skip header
    fseek(file, sizeof(page_header_t), SEEK_SET);

    // Allocate memory for page structure
    page_t* page = (page_t*)malloc(sizeof(page_t));
    fread(page->content, PAGE_CONTENT_SIZE, SEEK_CUR, file);
    page->header = header;

    // Close file page
    fclose(file);

    return page;
}

int free_page(page_t* page) {
    free(page->header);
    free(page);

    return 1;
}