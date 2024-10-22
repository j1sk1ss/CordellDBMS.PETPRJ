#include "../../include/dirman.h"


#pragma region [Directory file]

    directory_t* DRM_create_directory(char* name) {
        directory_t* dir = (directory_t*)malloc(sizeof(directory_t));

        dir->header = (directory_header_t*)malloc(sizeof(directory_header_t));
        strncpy((char*)dir->header->name, name, DIRECTORY_NAME_SIZE);
        dir->header->magic = DIRECTORY_MAGIC;
        dir->header->page_count = 0;

        return dir;
    }

    directory_t* DRM_create_empty_directory() {
        char directory_name[DIRECTORY_NAME_SIZE] = { '\0' };
        char save_path[DEFAULT_PATH_SIZE]        = { '\0' };

        int delay = DEFAULT_DELAY;
        while (1) {
            rand_str(directory_name, DIRECTORY_NAME_SIZE);
            sprintf(save_path, "%s%.8s.%s", DIRECTORY_BASE_PATH, directory_name, DIRECTORY_EXTENSION);

            FILE* file;
            if ((file = fopen(save_path,"r")) != NULL) {
                fclose(file);
                if (--delay <= 0) return NULL;
            }
            else {
                // File not found, no memory leak since 'file' == NULL
                // fclose(file) would cause an error
                break;
            }
        }

        return DRM_create_directory(directory_name);
    }

    int DRM_save_directory(directory_t* directory, char* path) {
        int status = -1;
        #pragma omp critical (directory_save)
        {
            #ifndef NO_DIRECTORY_SAVE_OPTIMIZATION
            if (DRM_get_checksum(directory) != directory->header->checksum)
            #endif
            {
                char save_path[DEFAULT_PATH_SIZE];
                if (path == NULL) sprintf(save_path, "%s%.8s.%s", DIRECTORY_BASE_PATH, directory->header->name, DIRECTORY_EXTENSION);
                else strcpy(save_path, path);

                FILE* file = fopen(save_path, "wb");
                if (file == NULL) print_error("Can`t create file: [%s]", save_path);
                else {
                    status = 1;
                    directory->header->checksum = DRM_get_checksum(directory);
                    if (fwrite(directory->header, sizeof(directory_header_t), 1, file) != 1)
                        status = -1;

                    for (int i = 0; i < directory->header->page_count; i++) {
                        if (fwrite(directory->names[i], sizeof(uint8_t), PAGE_NAME_SIZE, file) != 1) {
                            status = -1;
                        }
                    }

                    #ifndef _WIN32
                    fsync(fileno(file));
                    #else
                    fflush(file);
                    #endif

                    fclose(file);
                }
            }
        }

        return status;
    }

    directory_t* DRM_load_directory(char* path, char* name) {
        char load_path[DEFAULT_PATH_SIZE];
        if (path == NULL && name != NULL) sprintf(load_path, "%s%.8s.%s", DIRECTORY_BASE_PATH, name, DIRECTORY_EXTENSION);
        else if (path != NULL) strcpy(load_path, path);
        else {
            print_error("Path or name should be provided!");
            return NULL;
        }

        char file_name[DIRECTORY_NAME_SIZE];
        if (path != NULL) {
            char temp_path[DEFAULT_PATH_SIZE];
            strcpy(temp_path, path);
            get_file_path_parts(temp_path, NULL, file_name, NULL);
        }
        else if (name != NULL) {
            strncpy(file_name, name, DIRECTORY_NAME_SIZE);
        }

        directory_t* loaded_directory = DRM_DDT_find_directory(file_name);
        if (loaded_directory != NULL) return loaded_directory;

        #pragma omp critical (directory_load)
        {
            // Open file directory
            FILE* file = fopen(load_path, "rb");
            print_debug("Loading directory [%s]", load_path);
            if (file == NULL) print_error("Directory not found! Path: [%s]\n", load_path);
            else {
                // Read header from file
                directory_header_t* header = (directory_header_t*)malloc(sizeof(directory_header_t));
                fread(header, sizeof(directory_header_t), 1, file);

                // Check directory magic
                if (header->magic != DIRECTORY_MAGIC) {
                    print_error("Directory file wrong magic for [%s]", load_path);
                    free(header);
                    fclose(file);
                } else {
                    // First we allocate memory for directory struct
                    // Then we read page names
                    directory_t* directory = (directory_t*)malloc(sizeof(directory_t));
                    for (int i = 0; i < MIN(header->page_count, PAGES_PER_DIRECTORY); i++)
                        fread(directory->names[i], sizeof(uint8_t), PAGE_NAME_SIZE, file);

                    // Close file directory
                    fclose(file);

                    directory->lock_owner = NO_OWNER;
                    directory->lock = UNLOCKED;

                    directory->header = header;
                    DRM_DDT_add_directory(directory);
                    loaded_directory = directory;
                }
            }
        }

        return loaded_directory;
    }

    int DRM_delete_directory(directory_t* directory, int full) {
        if (DRM_lock_directory(directory, omp_get_thread_num()) == 1) {
            #pragma omp parallel
            for (int i = 0; i < directory->header->page_count && full == 1; i++) {
                char page_path[DEFAULT_PATH_SIZE];
                sprintf(page_path, "%s%.8s.%s", PAGE_BASE_PATH, directory->names[i], PAGE_EXTENSION);
                print_debug(
                    "Page [%s] was deleted and flushed with results [%i | %i]", 
                    page_path, PGM_PDT_flush_page(PGM_load_page(page_path, NULL)), remove(page_path)
                );
            }

            char delete_path[DEFAULT_PATH_SIZE];
            sprintf(delete_path, "%s%.8s.%s", DIRECTORY_BASE_PATH, directory->header->name, DIRECTORY_EXTENSION);
            DRM_DDT_flush_directory(directory);
            print_debug("Directory [%s] was deleted with result [%i]", delete_path, remove(delete_path));

            return 1;
        }
        else {
            print_error("Can't lock directory [%s]", directory->header->name);
            return -1;
        }
    }

    int DRM_free_directory(directory_t* directory) {
        if (directory == NULL) return -1;
        SOFT_FREE(directory->header);
        SOFT_FREE(directory);

        return 1;
    }

    uint32_t DRM_get_checksum(directory_t* directory) {
        DRM_lock_directory(directory, omp_get_thread_num());
        uint32_t prev_checksum = directory->header->checksum;
        directory->header->checksum = 0;
        
        uint32_t checksum = 0;
        if (directory->header != NULL)
            checksum = crc32(checksum, (const uint8_t*)directory->header, sizeof(directory_header_t));

        directory->header->checksum = prev_checksum;
        checksum = crc32(checksum, (const uint8_t*)directory->names, sizeof(directory->names));
        DRM_release_directory(directory, omp_get_thread_num());

        return checksum;
    }

#pragma endregion
