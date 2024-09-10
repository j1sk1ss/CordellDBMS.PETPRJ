#include "../include/table_operator.h"


#pragma region [Directories]

    int save_directory(directory_t* directory, char* path) {
        return 1;
    }

    directory_t* load_directory(char* name) {
        // Open file page
        FILE* file = fopen(name, "r");

        // Read header from file
        uint8_t* header_data = (uint8_t*)malloc(sizeof(directory_header_t));
        fgets(header_data, sizeof(directory_header_t), file);
        directory_header_t* header = (directory_header_t*)header_data;

        // Check directory magic
        if (header->magic != DIRECTORY_MAGIC) {
            free(header);
            return NULL;
        }

        // Skeep header data in file
        fseek(file, sizeof(directory_header_t), SEEK_SET);

        // Read page names from file
        directory_t* directory = (directory_t*)malloc(sizeof(directory_t));
        directory->names = (uint8_t**)malloc(header->page_count * sizeof(uint8_t*));
        for (int i = 0; i < header->page_count; i++) {
            directory->names[i] = (uint8_t*)malloc(8 * sizeof(uint8_t));
            fgets(directory->names[i], 8 * sizeof(uint8_t), file);
            fseek(file, 8 * sizeof(uint8_t), SEEK_CUR);
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

        free(directory->names);
        free(directory->header);
        free(directory);

        return 1;
    }

#pragma endregion

#pragma region [Pages]

    int save_page(page_t* page, char* path) {

    }

    page_t* load_page(char* name) {
        // Open file page
        FILE* file = fopen(name, "r");

        // Read header from file
        uint8_t* header_data = (uint8_t*)malloc(sizeof(page_header_t));
        fgets(header_data, sizeof(page_header_t), file);
        page_header_t* header = (page_header_t*)header_data;

        // Check page magic
        if (header->magic != PAGE_MAGIC) {
            free(header);
            return NULL;
        }

        // Read content from file
        uint8_t* file_data = (uint8_t*)malloc(header->content_size);
        fseek(file, sizeof(page_header_t), SEEK_SET);
        fgets(file_data, sizeof(header->content_size), file);

        // Allocate memory for page structure
        page_t* page = (page_t*)malloc(sizeof(page_t));
        page->content = file_data;
        page->header = header;

        // Close file page
        fclose(file);

        return page;
    }

    int free_page(page_t* page) {
        free(page->content);
        free(page->header);
        free(page);

        return 1;
    }

#pragma endregion
