#include "../include/dirman.h"


directory_t* create_directory(char* name) {
    directory_t* dir = (directory_t*)malloc(sizeof(directory_t));
    dir->header = (directory_header_t*)malloc(sizeof(directory_header_t));

    strncpy((char*)dir->header->name, name, DIRECTORY_NAME_SIZE);
    dir->header->magic = DIRECTORY_MAGIC;
    dir->header->page_count = 0;

    return dir;
}

int link_page2dir(directory_t* directory, page_t* page) {
    strcpy((char*)directory->names[directory->header->page_count++], (char*)page->header->name);
    return 1;
}

int save_directory(directory_t* directory, char* path) {
    // Open or create file
    FILE* file = fopen(path, "wb");
    if (file == NULL) {
        return -1;
    }

    // Write header
    fwrite(directory->header, sizeof(directory_header_t), SEEK_CUR, file);

    // Write names
    for (int i = 0; i < directory->header->page_count; i++) {
        fwrite(directory->names[i], PAGE_NAME_SIZE, SEEK_CUR, file);
        fseek(file, PAGE_NAME_SIZE, SEEK_CUR);
    }

    // Close file
    fclose(file);

    return 1;
}

directory_t* load_directory(char* name) {
    // Open file page
    FILE* file = fopen(name, "rb");
    if (file == NULL) {
        return NULL;
    }

    // Read header from file
    uint8_t* header_data = (uint8_t*)malloc(sizeof(directory_header_t));
    fread(header_data, sizeof(directory_header_t), SEEK_CUR, file);
    directory_header_t* header = (directory_header_t*)header_data;

    // Check directory magic
    if (header->magic != DIRECTORY_MAGIC) {
        free(header);
        return NULL;
    }

    // First we allocate memory for directory struct
    // Then we read page names
    directory_t* directory = (directory_t*)malloc(sizeof(directory_t));
    for (int i = 0; i < header->page_count; i++) {
        fread(directory->names[i], PAGE_NAME_SIZE, SEEK_CUR, file);
    }

    directory->header = header;

    // Close file page
    fclose(file);

    return directory;
}

int free_directory(directory_t* directory) {
    for (int i = 0; i < directory->header->page_count; i++) {
        free(directory->names[i]);
    }

    free(directory->header);
    free(directory);

    return 1;
}