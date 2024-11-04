#include "../../include/dirman.h"


directory_t* DRM_create_directory(char* name) {
    directory_t* directory = (directory_t*)malloc(sizeof(directory_t));
    directory_header_t* header = (directory_header_t*)malloc(sizeof(directory_header_t));

    memset(directory, 0, sizeof(directory_t));
    memset(header, 0, sizeof(directory_header_t));

    strncpy(header->name, name, DIRECTORY_NAME_SIZE);
    header->magic      = DIRECTORY_MAGIC;
    header->page_count = 0;
    directory->lock    = THR_create_lock();
    directory->header  = header;
    return directory;
}

directory_t* DRM_create_empty_directory() {
    char directory_name[DIRECTORY_NAME_SIZE] = { '\0' };
    char* unique_name = generate_unique_filename(DIRECTORY_BASE_PATH, DIRECTORY_NAME_SIZE, DIRECTORY_EXTENSION);
    strncpy(directory_name, unique_name, DIRECTORY_NAME_SIZE);
    free(unique_name);

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
                    if (fwrite(directory->page_names[i], sizeof(uint8_t), PAGE_NAME_SIZE, file) != 1) {
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
    if (get_load_path(name, DIRECTORY_NAME_SIZE, path, load_path, DIRECTORY_BASE_PATH, DIRECTORY_EXTENSION) == -1) {
        print_error("Path or name should be provided!");
        return NULL;
    }

    char file_name[DIRECTORY_NAME_SIZE];
    if (get_filename(name, path, file_name, DIRECTORY_NAME_SIZE) == -1) return NULL;
    directory_t* loaded_directory = (directory_t*)CHC_find_entry(file_name, DIRECTORY_CACHE);
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
                    fread(directory->page_names[i], sizeof(uint8_t), PAGE_NAME_SIZE, file);

                // Close file directory
                fclose(file);

                directory->lock = THR_create_lock();
                directory->header = header;
                CHC_add_entry(directory, directory->header->name, DIRECTORY_CACHE, DRM_free_directory, DRM_save_directory);
                loaded_directory = directory;
            }
        }
    }

    return loaded_directory;
}

int DRM_delete_directory(directory_t* directory, int full) {
    if (directory == NULL) return -1;
    if (THR_require_lock(&directory->lock, omp_get_thread_num()) == 1) {
        if (full) {
            #pragma omp parallel for schedule(dynamic, 1)
            for (int i = 0; i < directory->header->page_count; i++) {
                char page_path[DEFAULT_PATH_SIZE];
                sprintf(page_path, "%s%.*s.%s", PAGE_BASE_PATH, PAGE_NAME_SIZE, directory->page_names[i], PAGE_EXTENSION);
                print_debug(
                    "Page [%s] was deleted and flushed with results [%i | %i]",
                    page_path, CHC_flush_entry(PGM_load_page(page_path, NULL), PAGE_CACHE), remove(page_path)
                );
            }
        }

        char delete_path[DEFAULT_PATH_SIZE];
        sprintf(delete_path, "%s%.*s.%s", DIRECTORY_BASE_PATH, DIRECTORY_NAME_SIZE, directory->header->name, DIRECTORY_EXTENSION);
        CHC_flush_entry(directory, DIRECTORY_CACHE);
        print_debug("Directory [%s] was deleted with result [%i]", delete_path, remove(delete_path));

        return 1;
    }
    else {
        print_error("Can't lock directory [%.*s]", DIRECTORY_NAME_SIZE, directory->header->name);
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
    uint32_t prev_checksum = directory->header->checksum;
    directory->header->checksum = 0;

    uint32_t checksum = 0;
    if (directory->header != NULL)
        checksum = crc32(checksum, (const uint8_t*)directory->header, sizeof(directory_header_t));

    directory->header->checksum = prev_checksum;
    checksum = crc32(checksum, (const uint8_t*)directory->page_names, sizeof(directory->page_names));
    return checksum;
}
