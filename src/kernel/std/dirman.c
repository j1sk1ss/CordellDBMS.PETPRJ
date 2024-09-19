#include "../include/dirman.h"

// TODO: Make get_content function

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

    uint8_t* DRM_get_content(directory_t* directory, int offset, size_t size) {
        uint8_t* content = (uint8_t*)malloc(size);
        uint8_t* content_pointer = content;

        int page_offset     = offset / PAGE_CONTENT_SIZE;
        int current_index   = offset % PAGE_CONTENT_SIZE;
        int pages4work      = (int)size / PAGE_CONTENT_SIZE;
        int current_page    = page_offset;
        int size2get        = (int)size;

        for (int i = 0; i < pages4work + 1 && size2get > 0; i++) {
            if (current_page > directory->header->page_count) {
                // To  many pages. We reach directory end.
                return content;
            } 

            // We load current page
            char page_path[DEFAULT_PATH_SIZE];
            sprintf(page_path, "%s%s.%s", PAGE_BASE_PATH, directory->names[current_page++], PAGE_EXTENSION);
            page_t* page = PGM_load_page(page_path);

            // We work with page
            int current_size = MIN(PAGE_CONTENT_SIZE - current_index, size2get);
            memcpy(content_pointer, page->content, current_size);

            PGM_free_page(page);

            // We reload local index and update size2get
            // Also we move content pointer to next location
            current_index = 0;
            size2get        -= current_size;
            content_pointer += current_size;
        }

        return content;
    }

    int DRM_delete_content(directory_t* directory, int offset, size_t length) {
        int page_offset     = offset / PAGE_CONTENT_SIZE;
        int current_index   = offset % PAGE_CONTENT_SIZE;
        int pages4work      = (int)length / PAGE_CONTENT_SIZE;
        int current_page    = page_offset;
        int size2delete     = (int)length;

        for (int i = 0; i < pages4work + 1 && size2delete > 0; i++) {
            if (current_page > directory->header->page_count) {
                // To  many pages. We reach directory end.
                return -2;
            } 

            // We load current page
            char page_path[DEFAULT_PATH_SIZE];
            sprintf(page_path, "%s%s.%s", PAGE_BASE_PATH, directory->names[current_page++], PAGE_EXTENSION);
            page_t* page = PGM_load_page(page_path);

            // We work with page
            int current_size = MIN(PAGE_CONTENT_SIZE - current_index, size2delete);
            PGM_delete_content(page, current_index, current_size);
            PGM_save_page(page, page_path);
            PGM_free_page(page);

            // We reload local index and update size2delete
            current_index = 0;
            size2delete -= current_size;
        }

        return 1;
    }

    int DRM_append_content(directory_t* directory, uint8_t* data, size_t data_lenght) {
        // First we try to find fit empty place somewhere in linked pages
        // We skip this part if data_lenght larger then PAGE_CONTENT_SIZE
        // TODO: We can have situation, when we have many full empty pages, but content larger then one page.
        if (data_lenght < PAGE_CONTENT_SIZE) {
            for (int i = 0; i < directory->header->page_count; i++) {
                char page_path[DEFAULT_PATH_SIZE];
                sprintf(page_path, "%s%s.%s", PAGE_BASE_PATH, directory->names[i], PAGE_EXTENSION);

                page_t* page = PGM_load_page(page_path);
                int index = PGM_get_fit_free_space(page, PAGE_START, data_lenght);
                if (index == -2 || index == -1) {
                    PGM_free_page(page);
                    continue;
                }

                PGM_insert_content(page, index, data, data_lenght);
                PGM_save_page(page, page_path);
                PGM_free_page(page);

                return 1;
            }
        }

        // If we reach pages limit we return error code
        // We return error instead creationg a new directory, because this is not our abstraction level
        if (directory->header->page_count + 1 >= PAGES_PER_DIRECTORY) {
            return -1;
        }

        int pages4work  = data_lenght / PAGE_CONTENT_SIZE;
        int size2append = data_lenght;
        uint8_t* data_pointer = data;

        // Allocate pages for input data
        for (int i = 0; i < pages4work + 1 && size2append > 0; i++) {
            int current_size = MIN(PAGE_CONTENT_SIZE, size2append); 

            // We allocate memory for page structure with all needed data
            page_t* new_page = PGM_create_empty_page();
            if (new_page == NULL) return -2;

            memcpy(new_page->content, data_pointer, current_size);

            char save_path[DEFAULT_PATH_SIZE];
            sprintf(save_path, "%s%s.%s", PAGE_BASE_PATH, new_page->header->name, PAGE_EXTENSION);
            PGM_save_page(new_page, save_path);

            // We link page to directory
            DRM_link_page2dir(directory, new_page);
            PGM_free_page(new_page);

            size2append -= current_size;
            data_pointer += current_size;
        }

        return 2;
    }

    int DRM_insert_content(directory_t* directory, uint8_t offset, uint8_t* data, size_t data_lenght) {
        int pages4work      = data_lenght / PAGE_CONTENT_SIZE;
        int page_offset     = offset / PAGE_CONTENT_SIZE;
        int current_index   = offset % PAGE_CONTENT_SIZE;
        int current_page    = page_offset;
        int size2insert     = data_lenght;

        uint8_t* data_pointer = data;
        for (int i = 0; i < pages4work + 1 && size2insert > 0; i++) {
            // If we reach pages count in current directory, we return error code
            // We return error instead creationg a new directory, because this is not our abstraction level
            if (current_page > directory->header->page_count) {
                // To  many pages. We reach directory end.
                return -2;
            } 

            // We load current page to memory
            char page_path[DEFAULT_PATH_SIZE];
            sprintf(page_path, "%s%s.%s", PAGE_BASE_PATH, directory->names[current_page++], PAGE_EXTENSION);
            page_t* page = PGM_load_page(page_path);

            // We insert current part of content with local offset 
            int current_size = MIN(PAGE_CONTENT_SIZE - current_index, size2insert);
            PGM_insert_content(page, current_index, data_pointer, current_size);
            PGM_save_page(page, page_path);
            PGM_free_page(page);

            // We reload local index and update size2delete
            current_index = 0;
            size2insert -= current_size;
            data_pointer += current_size;
        }

        return 1;
    }

    int DRM_find_content(directory_t* directory, int offset, uint8_t* data, size_t data_size) {
        if (directory->header->page_count < (offset / PAGE_CONTENT_SIZE) + 1) return -1;

        int page_offset         = offset / PAGE_CONTENT_SIZE;
        int current_index       = offset % PAGE_CONTENT_SIZE;
        int size4seach          = (int)data_size;
        int pages4search        = directory->header->page_count - page_offset;
        int target_global_index = -1;
        int current_page        = page_offset;

        uint8_t* data_pointer = data;
        for (int i = 0; i < pages4search + 1 && size4seach > 0; i++) {
            // If we reach pages count in current directory, we return error code.
            // We return error instead creationg a new directory, because this is not our abstraction level.
            if (current_page > directory->header->page_count) {
                // To  many pages. We reach directory end.
                return -2;
            } 
            
            // We load current page to memory
            char page_path[DEFAULT_PATH_SIZE];
            sprintf(page_path, "%s%s.%s", PAGE_BASE_PATH, directory->names[current_page], PAGE_EXTENSION);
            page_t* page = PGM_load_page(page_path);

            // We search part of data in this page, save index and unload page.
            int current_size = MIN(PAGE_CONTENT_SIZE - current_index, size4seach);
            int result = PGM_find_content(page, current_index, data_pointer, current_size);
            PGM_free_page(page);

            // If TGI is -1, we know that we start seacrhing from start.
            // Save current TGI of find part of data.
            if (target_global_index == -1) {
                target_global_index = result + current_page * PAGE_CONTENT_SIZE;
            }

            if (result == -1) {
                // We don`t find any entry of data part.
                // This indicates, that we don`t find any data.
                // Restore size4search and datapointer, we go to start 
                size4seach          = (int)data_size;
                data_pointer        = data;
                target_global_index = -1;
            } else {
                // Move pointer to next position
                size4seach      -= current_size;
                data_pointer    += current_size;
            }

            // Set local index to 0. We don`t need offset now.
            current_index = 0;
            current_page++;
        }

        return target_global_index;
    }

    int DRM_find_value(directory_t* directory, int offset, uint8_t value) {
        int page_offset     = offset / PAGE_CONTENT_SIZE;
        int current_index   = offset % PAGE_CONTENT_SIZE;
        int pages4search    = directory->header->page_count - page_offset;
        int current_page    = page_offset;

        for (int i = 0; i < pages4search + 1; i++) {
            // Load current page
            char page_path[DEFAULT_PATH_SIZE];
            sprintf(page_path, "%s%s.%s", PAGE_BASE_PATH, directory->names[current_page], PAGE_EXTENSION);
            page_t* page = PGM_load_page(page_path);

            // Try to find value in content of current page
            int result = PGM_find_value(page, current_index, value);
            PGM_free_page(page);
            
            // If we find content, return TGI
            if (result != -1) return result + current_page * PAGE_CONTENT_SIZE; 
            current_page++;
        }

        return -1;
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