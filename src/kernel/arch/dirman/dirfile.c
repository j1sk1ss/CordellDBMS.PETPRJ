#include "../../include/dirman.h"


directory_t* DRM_create_directory(char* name) {
    directory_t* directory = (directory_t*)malloc(sizeof(directory_t));
    directory_header_t* header = (directory_header_t*)malloc(sizeof(directory_header_t));

    memset(directory, 0, sizeof(directory_t));
    memset(header, 0, sizeof(directory_header_t));

    strncpy(header->name, name, DIRECTORY_NAME_SIZE);
    header->magic = DIRECTORY_MAGIC;

    directory->lock   = THR_create_lock();
    directory->header = header;
    return directory;
}

directory_t* DRM_create_empty_directory() {
    char directory_name[DIRECTORY_NAME_SIZE] = { 0 };
    char* unique_name = generate_unique_filename(DIRECTORY_BASE_PATH, DIRECTORY_NAME_SIZE, DIRECTORY_EXTENSION);
    strncpy(directory_name, unique_name, DIRECTORY_NAME_SIZE);
    free(unique_name);

    return DRM_create_directory(directory_name);
}

int DRM_save_directory(directory_t* __restrict directory, char* __restrict path) {
    int status = -1;
    #pragma omp critical (directory_save)
    {
        #ifndef NO_DIRECTORY_SAVE_OPTIMIZATION
        if (DRM_get_checksum(directory) != directory->header->checksum)
        #endif
        {
            char save_path[DEFAULT_PATH_SIZE];
            if (path == NULL) sprintf(save_path, "%s%.*s.%s", DIRECTORY_BASE_PATH, DIRECTORY_NAME_SIZE, directory->header->name, DIRECTORY_EXTENSION);
            else strcpy(save_path, path);

            FILE* file = fopen(save_path, "wb");
            if (file == NULL) print_error("Can`t create file: [%s]", save_path);
            else {
                status = 1;
                directory->header->checksum = DRM_get_checksum(directory);
                if (fwrite(directory->header, sizeof(directory_header_t), 1, file) != 1)
                    status = -1;

                for (int i = 0; i < directory->header->page_count; i++) {
                    if (fwrite(directory->page_names[i], sizeof(unsigned char), PAGE_NAME_SIZE, file) != 1) {
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

directory_t* DRM_load_directory(char* name) {
    char load_path[DEFAULT_PATH_SIZE] = { 0 };
    if (get_load_path(name, DIRECTORY_NAME_SIZE, load_path, DIRECTORY_BASE_PATH, DIRECTORY_EXTENSION) == -1) {
        print_error("Name should be provided!");
        return NULL;
    }

    directory_t* loaded_directory = (directory_t*)CHC_find_entry(name, DIRECTORY_CACHE);
    if (loaded_directory != NULL) {
        print_debug("Loading directory [%s] from GCT", load_path);
        return loaded_directory;
    }

    #pragma omp critical (directory_load)
    {
        // Open file directory
        FILE* file = fopen(load_path, "rb");
        print_debug("Loading directory [%s]", load_path);
        if (file == NULL) print_error("Directory not found! Path: [%s]", load_path);
        else {
            // Read header from file
            directory_header_t* header = (directory_header_t*)malloc(sizeof(directory_header_t));
            memset(header, 0, sizeof(directory_header_t));
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
                memset(directory, 0, sizeof(directory_t));
                for (int i = 0; i < MIN(header->page_count, PAGES_PER_DIRECTORY); i++)
                    fread(directory->page_names[i], sizeof(unsigned char), PAGE_NAME_SIZE, file);

                // Close file directory
                fclose(file);

                directory->lock   = THR_create_lock();
                directory->header = header;
                loaded_directory  = directory;

                CHC_add_entry(
                    loaded_directory, loaded_directory->header->name, 
                    DIRECTORY_CACHE, (void*)DRM_free_directory, (void*)DRM_save_directory
                );
            }
        }
    }

    return loaded_directory;
}

int DRM_delete_directory(directory_t* directory, int full) {
#ifndef NO_DELETE_COMMAND
    if (directory == NULL) return -1;
    if (THR_require_lock(&directory->lock, omp_get_thread_num()) == 1) {
        if (full) {
            #pragma omp parallel for schedule(dynamic, 1)
            for (int i = 0; i < directory->header->page_count; i++) {
                print_debug(
                    "Page [%s] was deleted and flushed with results [%i | %i]",
                    directory->page_names[i], CHC_flush_entry(PGM_load_page(directory->page_names[i]), PAGE_CACHE), delete_file(directory->page_names[i], PAGE_BASE_PATH, PAGE_EXTENSION)
                );
            }
        }

        print_debug("Directory [%s] was deleted with result [%i]", directory->header->name, delete_file(directory->header->name, DIRECTORY_BASE_PATH, DIRECTORY_EXTENSION));
        return 1;
    }
    else {
        print_error("Can't lock directory [%.*s]", DIRECTORY_NAME_SIZE, directory->header->name);
        return -1;
    }
#endif
    return 1;
}

int DRM_flush_directory(directory_t* directory) {
    if (directory == NULL) return -2;
    if (directory->is_cached == 1) return -1;

    DRM_save_directory(directory, NULL);
    return DRM_free_directory(directory);
}

int DRM_free_directory(directory_t* directory) {
    if (directory == NULL) return -1;
    SOFT_FREE(directory->header);
    SOFT_FREE(directory);

    return 1;
}

unsigned int DRM_get_checksum(directory_t* directory) {
    unsigned int prev_checksum = directory->header->checksum;
    directory->header->checksum = 0;

    unsigned int checksum = 0;
    if (directory->header != NULL)
        checksum = crc32(checksum, (const unsigned char*)directory->header, sizeof(directory_header_t));

    directory->header->checksum = prev_checksum;
    checksum = crc32(checksum, (const unsigned char*)directory->page_names, sizeof(directory->page_names));
    return checksum;
}
