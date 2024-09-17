#include "../include/dirman.h"


/*
Directory destriptor table, is an a static array of directory indexes. Main idea in
saving dirs temporary in static table somewhere in memory. Max size of this 
table equals 1024 * 10 = 10Kb.

For working with table we have directory struct, that have index of table in DDT. If 
we access to dirs with full DRM_DDT, we also unload old dirs and load new dirs.
(Stack)

Main problem in parallel work. If we have threads, they can try to access this
table at one time. If you use OMP parallel libs, or something like this, please,
define NO_DDT flag (For avoiding deadlocks).

Why we need DDT? - https://stackoverflow.com/questions/26250744/efficiency-of-fopen-fclose
*/
directory_t* DRM_DDT[DDT_SIZE] = { NULL };


#pragma region [Page]

    int DRM_delete_content(directory_t* directory, int offset, size_t length) {
        if (directory->header->page_count < (offset / PAGE_CONTENT_SIZE) + 1) return -1;

        int page_offset = offset / PAGE_CONTENT_SIZE;
        int current_index = offset % PAGE_CONTENT_SIZE;

        char page_path[50];
        sprintf(page_path, "%s%s.%s", PAGE_BASE_PATH, directory->names[page_offset], PAGE_EXTENSION);

        page_t* page = PGM_load_page(page_path);
        PGM_delete_content(page, current_index, length);

        PGM_save_page(page, page_path);
        PGM_free_page(page);

        return 1;
    }

    int DRM_append_content(directory_t* directory, uint8_t* data, size_t data_lenght) {
        for (int i = 0; i < directory->header->page_count; i++) {
            char page_path[50];
            sprintf(page_path, "%s%s.%s", PAGE_BASE_PATH, directory->names[i], PAGE_EXTENSION);

            page_t* page = PGM_load_page(page_path);
            int index = PGM_get_fit_free_space(page, PAGE_START, data_lenght);
            if (index == -2 || index == -1) {
                PGM_free_page(page);
                continue;
            }

            char save_path[50];
            sprintf(save_path, "%s%s.%s", PAGE_BASE_PATH, page->header->name, PAGE_EXTENSION);

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
        char save_path[50];
        sprintf(save_path, "%s%s.%s", PAGE_BASE_PATH, new_page_name, PAGE_EXTENSION);

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
                sprintf(save_path, "%s%s.%s", PAGE_BASE_PATH, new_page_name, PAGE_EXTENSION);

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

        char page_path[50];
        sprintf(page_path, "%s%s.%s", PAGE_BASE_PATH, directory->names[page_offset], PAGE_EXTENSION);

        page_t* page = PGM_load_page(page_path);
        int result = PGM_insert_content(page, current_index, data, data_lenght);

        PGM_save_page(page, page_path);
        PGM_free_page(page);

        return result;
    }

    int DRM_find_content(directory_t* directory, int offset, uint8_t* data, size_t data_size) {
        if (directory->header->page_count < (offset / PAGE_CONTENT_SIZE) + 1) return -1;

        int page_offset = offset / PAGE_CONTENT_SIZE;
        int current_index = offset % PAGE_CONTENT_SIZE;

        char page_path[50];
        sprintf(page_path, "%s%s.%s", PAGE_BASE_PATH, directory->names[page_offset], PAGE_EXTENSION);

        page_t* page = PGM_load_page(page_path);
        int result = PGM_find_content(page, current_index, data, data_size);
        PGM_free_page(page);

        return page_offset * PAGE_CONTENT_SIZE + result;
    }

    int DRM_find_value(directory_t* directory, int offset, uint8_t value) {
        if (directory->header->page_count < (offset / PAGE_CONTENT_SIZE) + 1) return -1;

        int page_offset = offset / PAGE_CONTENT_SIZE;
        int current_index = offset % PAGE_CONTENT_SIZE;
        
        char page_path[50];
        sprintf(page_path, "%s%s.%s", PAGE_BASE_PATH, directory->names[page_offset], PAGE_EXTENSION);

        page_t* page = PGM_load_page(page_path);
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
        if (directory->header->page_count + 1 >= PAGES_PER_DIRECTORY) 
            return -1;

        strcpy((char*)directory->names[directory->header->page_count++], (char*)page->header->name);
        return 1;
    }

    int DRM_unlink_page_from_directory(directory_t* directory, char* page_name) {
        return 1; // TODO
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
        #ifndef _WIN32
            fsync(fileno(file));
        #else
            fflush(file);
        #endif

        fclose(file);

        return 1;
    }

    directory_t* DRM_load_directory(char* name) {
        char file_path[256];
        char file_name[25];
        char file_ext[8];
        get_file_path_parts(name, file_path, file_name, file_ext);

        directory_t* ddt_directory = DRM_DDT_find_directory(file_name);
        if (ddt_directory != NULL) return ddt_directory;

        // Open file page
        FILE* file = fopen(name, "rb");
        if (file == NULL) {
            return NULL;
        }

        // Read header from file
        directory_header_t* header = (directory_header_t*)malloc(sizeof(directory_header_t));
        fread(header, sizeof(directory_header_t), SEEK_CUR, file);

        // Check directory magic
        if (header->magic != DIRECTORY_MAGIC) {
            free(header);
            return NULL;
        }

        // First we allocate memory for directory struct
        // Then we read page names
        directory_t* directory = (directory_t*)malloc(sizeof(directory_t));
        for (int i = 0; i < MIN(header->page_count, PAGES_PER_DIRECTORY); i++) {
            fread(directory->names[i], PAGE_NAME_SIZE, SEEK_CUR, file);
        }

        directory->header = header;

        // Close file directory
        fclose(file);

        DRM_DDT_add_directory(directory);
        return directory;
    }

    int DRM_free_directory(directory_t* directory) {
        if (directory == NULL) return -1;
        SOFT_FREE(directory->header);
        SOFT_FREE(directory);

        return 1;
    }

    #pragma region [DDT]

        int DRM_DDT_add_directory(directory_t* directory) {
            #ifndef NO_DDT
                int current = 0;
                while (DRM_DDT[current]->lock == 1 && DRM_DDT[current] != NULL) 
                    if (current++ >= DDT_SIZE) current = 0;

                if (DRM_lock_directory(DRM_DDT[current], 0) != -1) {
                    DRM_DDT_flush(current);
                    DRM_DDT[current] = directory;
                }
            #endif

            return 1;
        }

        directory_t* DRM_DDT_find_directory(char* name) {
            #ifndef NO_DDT
                for (int i = 0; i < DDT_SIZE; i++) {
                    if (DRM_DDT[i] == NULL) continue;
                    if (strncmp((char*)DRM_DDT[i]->header->name, name, DIRECTORY_NAME_SIZE) == 0) {
                        return DRM_DDT[i];
                    }
                }
            #endif

            return NULL;
        }

        int DRM_DDT_sync() {
            #ifndef NO_DDT
                for (int i = 0; i < DDT_SIZE; i++) {
                    if (DRM_DDT[i] == NULL) continue;
                    char save_path[50];
                    sprintf(save_path, "%s%s.%s", DIRECTORY_BASE_PATH, DRM_DDT[i]->header->name, DIRECTORY_EXTENSION);

                    // TODO: Get thread ID
                    if (DRM_lock_directory(DRM_DDT[i], 0) == 1) {
                        DRM_DDT_flush(i);
                        DRM_DDT[i] = DRM_load_directory(save_path);
                    } else return -1;
                }
            #endif

            return 1;
        }

        int DRM_DDT_flush(int index) {
            #ifndef NO_DDT
                if (DRM_DDT[index] == NULL) return -1;

                char save_path[50];
                sprintf(save_path, "%s%s.%s", DIRECTORY_BASE_PATH, DRM_DDT[index]->header->name, DIRECTORY_EXTENSION);

                DRM_save_directory(DRM_DDT[index], save_path);
                DRM_free_directory(DRM_DDT[index]);

                DRM_DDT[index] = NULL;
            #endif

            return 1;
        }

    #pragma endregion

    #pragma region [Lock]

        int DRM_lock_directory(directory_t* directory, uint8_t owner) {
            if (directory == NULL) return -2;

            int delay = 99999;
            while (directory->lock == LOCKED && (directory->lock_owner != owner || directory->lock_owner != -1)) {
                if (--delay <= 0) return -1;
            }

            directory->lock = LOCKED;
            directory->lock_owner = owner;

            return 1;
        }

        int DRM_lock_test(directory_t* directory, uint8_t owner) {
            if (directory->lock_owner != owner) return 0;
            return directory->lock;
        }

        int DRM_release_directory(directory_t* directory, uint8_t owner) {
            if (directory->lock == UNLOCKED) return -1;
            if (directory->lock_owner != owner && directory->lock_owner != -1) return -2;

            directory->lock = UNLOCKED;
            directory->lock = -1;

            return 1;
        }

    #pragma endregion

#pragma endregion