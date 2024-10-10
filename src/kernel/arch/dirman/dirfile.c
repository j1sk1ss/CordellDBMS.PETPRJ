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
        char directory_name[DIRECTORY_NAME_SIZE];
        char save_path[DEFAULT_PATH_SIZE];

        int delay = 1000;
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
        int status = 1;
        #pragma omp critical (directory_save)
        {
            #ifndef NO_DIRECTORY_SAVE_OPTIMIZATION
            if (DRM_get_checksum(directory) != directory->header->checksum)
            #endif
            {
                char save_path[DEFAULT_PATH_SIZE];
                if (path == NULL) {
                    sprintf(save_path, "%s%.8s.%s", DIRECTORY_BASE_PATH, directory->header->name, DIRECTORY_EXTENSION);
                }
                else strcpy(save_path, path);

                FILE* file = fopen(save_path, "wb");
                if (file == NULL) {
                    status = -1;
                    print_error("Can`t create file: [%s]", save_path);
                } else {
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
        char buffer[512];
        char file_name[DIRECTORY_NAME_SIZE];
        char load_path[DEFAULT_PATH_SIZE];

        if (path == NULL && name != NULL) sprintf(load_path, "%s%.8s.%s", DIRECTORY_BASE_PATH, name, DIRECTORY_EXTENSION);
        else strcpy(load_path, path);

        if (path != NULL) {
            char temp_path[DEFAULT_PATH_SIZE];
            strcpy(temp_path, path);
            get_file_path_parts(temp_path, buffer, file_name, buffer);
        }
        else if (name != NULL) {
            strncpy(file_name, name, DIRECTORY_NAME_SIZE);
        }
        else {
            print_error("No path or name provided!");
            return NULL;
        }

        directory_t* loaded_directory = DRM_DDT_find_directory(file_name);
        if (loaded_directory != NULL) return loaded_directory;

        #pragma omp critical (directory_load)
        {
            // Open file directory
            FILE* file = fopen(load_path, "rb");
            if (file == NULL) {
                loaded_directory = NULL;
                print_error("Directory not found! Path: [%s]\n", load_path);
            } else {
                // Read header from file
                directory_header_t* header = (directory_header_t*)malloc(sizeof(directory_header_t));
                fread(header, sizeof(directory_header_t), 1, file);

                // Check directory magic
                if (header->magic != DIRECTORY_MAGIC) {
                    loaded_directory = NULL;

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
                page_t* page = PGM_load_page(page_path, NULL);
                if (PGM_lock_page(page, omp_get_thread_num()) == 1) {
                    PGM_PDT_flush_page(page);
                    print_log("Page [%s] was deleted with result [%i]", page_path, remove(page_path));
                }
                else {
                    print_error("Can't lock page [%s]", page_path);
                }
            }

            char delete_path[DEFAULT_PATH_SIZE];
            sprintf(delete_path, "%s%.8s.%s", DIRECTORY_BASE_PATH, directory->header->name, DIRECTORY_EXTENSION);
            DRM_DDT_flush_directory(directory);
            print_log("Directory [%s] was deleted with result [%i]", delete_path, remove(delete_path));
        }
        else {
            print_error("Can't lock directory [%s]", directory->header->name);
        }

        return 1;
    }

    int DRM_free_directory(directory_t* directory) {
        if (directory == NULL) return -1;
        SOFT_FREE(directory->header);
        SOFT_FREE(directory);

        return 1;
    }

    uint32_t DRM_get_checksum(directory_t* directory) {
        uint32_t checksum = 0;
        for (int i = 0; i < DIRECTORY_NAME_SIZE; i++) checksum += directory->header->name[i];
        checksum += strlen((char*)directory->header->name);

        for (int i = 0; i < directory->header->page_count; i++) {
            for (int j = 0; j < PAGE_NAME_SIZE; j++) {
                checksum += directory->names[i][j];
            }

            checksum += strlen((char*)directory->names[i]);
        }

        return checksum;
    }

#pragma endregion