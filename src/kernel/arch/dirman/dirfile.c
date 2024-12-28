#include "../../include/dirman.h"


directory_t* DRM_create_directory(char* name) {
    directory_t* directory = (directory_t*)malloc(sizeof(directory_t));
    directory_header_t* header = (directory_header_t*)malloc(sizeof(directory_header_t));

    memset_s(directory, 0, sizeof(directory_t));
    memset_s(header, 0, sizeof(directory_header_t));

    strncpy_s(header->name, name, DIRECTORY_NAME_SIZE);
    header->magic      = DIRECTORY_MAGIC;
    header->page_count = 0;

    directory->header    = header;
    directory->is_cached = 0;
    directory->append_offset = 0;
    return directory;
}

directory_t* DRM_create_empty_directory() {
    char directory_name[DIRECTORY_NAME_SIZE] = { 0 };
    char* unique_name = generate_unique_filename(DIRECTORY_BASE_PATH, DIRECTORY_NAME_SIZE, DIRECTORY_EXTENSION);
    strncpy_s(directory_name, unique_name, DIRECTORY_NAME_SIZE);
    free(unique_name);

    return DRM_create_directory(directory_name);
}

int DRM_save_directory(directory_t* directory) {
    int status = -1;
    #pragma omp critical (directory_save)
    {
        #ifndef NO_DIRECTORY_SAVE_OPTIMIZATION
        // if (DRM_get_checksum(directory) != directory->header->checksum)
        #endif
        {
            char save_path[DEFAULT_PATH_SIZE] = { 0 };
            if (path == NULL) sprintf(save_path, "%s%.*s.%s", DIRECTORY_BASE_PATH, DIRECTORY_NAME_SIZE, directory->header->name, DIRECTORY_EXTENSION);
            else strcpy_s(save_path, path);

            FILE* file = fopen(save_path, "wb");
            if (file == NULL) print_error("Can`t create file: [%s]", save_path);
            else {
                status = 1;
                if (fwrite(directory->header, sizeof(directory_header_t), 1, file) != 1)
                    status = -1;

                for (int i = 0; i < directory->header->page_count; i++) {
                    if (fwrite(directory->page_names[i], sizeof(unsigned char), PAGE_NAME_SIZE, file) != 1) {
                        status = -1;
                    }
                }

                lfs_file_close(&lfs_body, &file);
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
            memset_s(header, 0, sizeof(directory_header_t));
            fread(header, sizeof(directory_header_t), 1, file);

            // Check directory magic
            if (header->magic != DIRECTORY_MAGIC) {
                print_error("Directory file wrong magic for [%s]", load_path);
                free(header);
                lfs_file_close(&lfs_body, &file);
            } else {
                // First we allocate memory for directory struct
                // Then we read page names
                directory_t* directory = (directory_t*)malloc(sizeof(directory_t));
                memset_s(directory, 0, sizeof(directory_t));
                for (int i = 0; i < MIN(header->page_count, PAGES_PER_DIRECTORY); i++)
                    fread(directory->page_names[i], sizeof(unsigned char), PAGE_NAME_SIZE, file);

                // Close file directory
                lfs_file_close(&lfs_body, &file);

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
    if (full) {
        for (int i = 0; i < directory->header->page_count; i++) {
            char page_path[DEFAULT_PATH_SIZE] = { 0 };
            sprintf(page_path, "%s%.*s.%s", PAGE_BASE_PATH, PAGE_NAME_SIZE, directory->page_names[i], PAGE_EXTENSION);
            print_debug(
                "Page [%s] was deleted and flushed with results [%i | %i]",
                page_path, CHC_flush_entry(PGM_load_page(page_path, NULL), PAGE_CACHE), remove(page_path)
            );
        }
    }

    char delete_path[DEFAULT_PATH_SIZE] = { 0 };
    sprintf(delete_path, "%s%.*s.%s", DIRECTORY_BASE_PATH, DIRECTORY_NAME_SIZE, directory->header->name, DIRECTORY_EXTENSION);
    CHC_flush_entry(directory, DIRECTORY_CACHE);
    print_debug("Directory [%s] was deleted with result [%i]", delete_path, remove(delete_path));

    return 1;
}

int DRM_flush_directory(directory_t* directory) {
    if (directory == NULL) return -2;
    if (directory->is_cached == 1) return -1;

    DRM_save_directory(directory);
    return DRM_free_directory(directory);
}

int DRM_free_directory(directory_t* directory) {
    if (directory == NULL) return -1;
    SOFT_FREE(directory->header);
    SOFT_FREE(directory);

    return 1;
}
