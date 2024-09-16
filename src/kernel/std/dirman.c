#include "../include/dirman.h"


#pragma region [Page]

    int DRM_delete_content(directory_t* directory, int offset, size_t length) {
        if (directory->header->page_count < (offset / PAGE_CONTENT_SIZE) + 1) return -1;

        int page_offset = offset / PAGE_CONTENT_SIZE;
        int current_index = offset % PAGE_CONTENT_SIZE;

        char page_name[25];
        sprintf(page_name, "%s.%s", directory->names[page_offset], PAGE_EXTENSION);

        page_t* page = PGM_load_page(page_name);
        PGM_delete_content(page, current_index, length);

        // TODO: Think about optimization
        //       Maybe save pages in pages table?
        //       I mean we will have something like static array of pages in pagman (Like file descriptor in Linux and etc)
        PGM_save_page(page, page_name);
        PGM_free_page(page);

        return 1;
    }

    int DRM_append_content(directory_t* directory, uint8_t* data, size_t data_lenght) {
        for (int i = 0; i < directory->header->page_count; i++) {
            char page_name[25];
            sprintf(page_name, "%s.%s", directory->names[i], PAGE_EXTENSION);

            page_t* page = PGM_load_page(page_name);
            int index = PGM_get_fit_free_space(page, PAGE_START, data_lenght);
            if (index == -2 || index == -1) {
                PGM_free_page(page);
                continue;
            }

            char save_path[25];
            sprintf(save_path, "%s.%s", page->header->name, PAGE_EXTENSION);

            PGM_append_content(page, data, data_lenght);
            PGM_save_page(page, save_path);
            PGM_free_page(page);

            return 1;
        }

        // We create new page if don't find enoght empty space in existed pages
        // First we generate random 8-bytes name
        char new_page_name[8];
        rand_str(new_page_name, 8);

        // Then we create save path with extention
        char save_path[25];
        sprintf(save_path, "%s.%s", new_page_name, PAGE_EXTENSION);

        // Here we check generated name
        // It need for avoid situations, where we can rewrite existed page
        // Note: in future prefere avoid random generation by using something like hash generator
        // Took from: https://stackoverflow.com/questions/230062/whats-the-best-way-to-check-if-a-file-exists-in-c
        int delay = 1000;
        while (1) {
            FILE* file;
            if ((file = fopen(save_path,"r")) != NULL) {
                // file exists and we regenerate name
                fclose(file);

                rand_str(new_page_name, 8);
                sprintf(save_path, "%s.%s", new_page_name, PAGE_EXTENSION);

                delay--;
                if (delay <= 0) return -2;
            }
            else {
                // File not found, no memory leak since 'file' == NULL
                // fclose(file) would cause an error
                break;
            }
        }

        // If we reach pages limit we return error code
        // We return error instead creationg a new directory, because it not our abstraction level
        if (directory->header->page_count + 1 >= PAGES_PER_DIRECTORY) {
            return -1;
        }

        // We allocate memory for page structure with all needed data
        page_t* new_page = PGM_create_page(new_page_name, data, data_lenght);
        PGM_save_page(new_page, save_path);

        // We link page to directory
        DRM_link_page2dir(directory, new_page);
        PGM_free_page(new_page);

        return 2;
    }

    int DRM_insert_content(directory_t* directory, uint8_t offset, uint8_t* data, size_t data_lenght) {
        if (directory->header->page_count < (offset / PAGE_CONTENT_SIZE) + 1) return -1;

        int page_offset = offset / PAGE_CONTENT_SIZE;
        int current_index = offset % PAGE_CONTENT_SIZE;

        char page_name[25];
        sprintf(page_name, "%s.%s", directory->names[page_offset], PAGE_EXTENSION);

        page_t* page = PGM_load_page(page_name);
        int result = PGM_insert_content(page, current_index, data, data_lenght);

        // TODO: Think about optimization
        //       Maybe save pages in pages table?
        //       I mean we will have something like static array of pages in pagman (Like file descriptor in Linux and etc)
        PGM_save_page(page, page_name);
        PGM_free_page(page);

        return result;
    }

    int DRM_find_value(directory_t* directory, int offset, uint8_t value) {
        if (directory->header->page_count < (offset / PAGE_CONTENT_SIZE) + 1) return -1;

        int page_offset = offset / PAGE_CONTENT_SIZE;
        int current_index = offset % PAGE_CONTENT_SIZE;
        
        char page_name[25];
        sprintf(page_name, "%s.%s", directory->names[page_offset], PAGE_EXTENSION);

        page_t* page = PGM_load_page(page_name);
        int result = PGM_find_value(page, current_index, value);
        
        PGM_free_page(page);

        return page_offset * PAGE_CONTENT_SIZE + result;
    }

#pragma endregion

#pragma region [Directory]

    directory_t* DRM_create_directory(char* name) {
        directory_t* dir = (directory_t*)malloc(sizeof(directory_t));
        dir->header = (directory_header_t*)malloc(sizeof(directory_header_t));

        strncpy((char*)dir->header->name, name, DIRECTORY_NAME_SIZE);
        dir->header->magic = DIRECTORY_MAGIC;
        dir->header->page_count = 0;

        return dir;
    }

    int DRM_link_page2dir(directory_t* directory, page_t* page) {
        strcpy((char*)directory->names[directory->header->page_count++], (char*)page->header->name);
        return 1;
    }

    int DRM_save_directory(directory_t* directory, char* path) {
        // Open or create file
        FILE* file = fopen(path, "wb");
        if (file == NULL) {
            return -1;
        }

        // Write header
        fwrite(directory->header, sizeof(directory_header_t), SEEK_CUR, file);

        // Write names
        for (int i = 0; i < directory->header->page_count; i++) 
            fwrite(directory->names[i], PAGE_NAME_SIZE, SEEK_CUR, file);

        // Close file
        fclose(file);

        return 1;
    }

    directory_t* DRM_load_directory(char* name) {
        // Open file page
        FILE* file = fopen(name, "rb");
        if (file == NULL) {
            return -1;
        }

        // Read header from file
        directory_header_t* header = (directory_header_t*)malloc(sizeof(directory_header_t));
        fread(header, sizeof(directory_header_t), SEEK_CUR, file);

        // Check directory magic
        if (header->magic != DIRECTORY_MAGIC) {
            free(header);
            return -2;
        }

        // First we allocate memory for directory struct
        // Then we read page names
        directory_t* directory = (directory_t*)malloc(sizeof(directory_t));
        for (int i = 0; i < MIN(header->page_count, PAGES_PER_DIRECTORY); i++) {
            fread(directory->names[i], PAGE_NAME_SIZE, SEEK_CUR, file);
        }

        directory->header = header;

        // Close file page
        fclose(file);

        return directory;
    }

    int DRM_free_directory(directory_t* directory) {
        free(directory->header);
        free(directory);

        return 1;
    }

#pragma endregion