#include <dirman.h>

directory_t* DRM_create_directory(char* name) {
    directory_t* directory = (directory_t*)malloc_s(sizeof(directory_t));
    directory_header_t* header = (directory_header_t*)malloc_s(sizeof(directory_header_t));
    if (!directory || !header) {
        SOFT_FREE(directory);
        SOFT_FREE(header);
        return NULL;
    }

    str_memset(directory, 0, sizeof(directory_t));
    str_memset(header, 0, sizeof(directory_header_t));

    str_strncpy(header->name, name, DIRECTORY_NAME_SIZE);
    header->magic = DIRECTORY_MAGIC;

    directory->lock = NULL_LOCK;
    directory->header = header;
    return directory;
}

directory_t* DRM_create_empty_directory() {
    char directory_name[DIRECTORY_NAME_SIZE] = { 0 };
    char* unique_name = generate_unique_filename(DIRECTORY_BASE_PATH, DIRECTORY_NAME_SIZE, DIRECTORY_EXTENSION);
    if (!unique_name) return NULL;
    str_strncpy(directory_name, unique_name, DIRECTORY_NAME_SIZE);
    SOFT_FREE(unique_name);
    return DRM_create_directory(directory_name);
}

int DRM_save_directory(directory_t* directory) {
    int status = -1;
    unsigned int directory_checksum = 0;
    #ifndef NO_DIRECTORY_SAVE_OPTIMIZATION
    directory_checksum = DRM_get_checksum(directory);
    if (directory_checksum != directory->header->checksum)
    #endif
    {
        char save_path[DEFAULT_PATH_SIZE] = { 0 };
        get_load_path(directory->header->name, DIRECTORY_NAME_SIZE, save_path, DIRECTORY_BASE_PATH, DIRECTORY_EXTENSION);
        ci_t ci = NIFAT32_open_content(NO_RCI, save_path, MODE(CR_MODE, FILE_TARGET));
        if (ci < 0) { print_error("Can`t create dirfile: [%s]", save_path); }
        else {
            int offset = 0;
            status = 1;
            directory->header->checksum = DRM_get_checksum(directory);

            unsigned short encoded_header[sizeof(directory_header_t)] = { 0 };
            pack_memory((byte_t*)directory->header, (decoded_t*)encoded_header, sizeof(directory_header_t));
            if (status == 1 && NIFAT32_write_buffer2content(
                ci, offset, (const_buffer_t)encoded_header, sizeof(directory_header_t) * sizeof(decoded_t)
            ) != sizeof(directory_header_t) * sizeof(decoded_t)) status = -1;
            offset += sizeof(directory_header_t) * sizeof(decoded_t);

            for (int i = 0; i < directory->header->page_count; i++) {
                unsigned short encoded_page_name[PAGE_NAME_SIZE] = { 0 };
                pack_memory((byte_t*)directory->page_names[i], (decoded_t*)encoded_page_name, PAGE_NAME_SIZE);
                if (status == 1 && NIFAT32_write_buffer2content(
                    ci, offset, (const_buffer_t)encoded_page_name, PAGE_NAME_SIZE * sizeof(decoded_t)
                ) != PAGE_NAME_SIZE * sizeof(decoded_t)) status = -2;
                offset += PAGE_NAME_SIZE * sizeof(unsigned short);
            }

            NIFAT32_close_content(ci);
        }
    }

    return status;
}

directory_t* DRM_load_directory(char* name) {
    char load_path[DEFAULT_PATH_SIZE] = { 0 };
    get_load_path(name, DIRECTORY_NAME_SIZE, load_path, DIRECTORY_BASE_PATH, DIRECTORY_EXTENSION);

    directory_t* loaded_directory = (directory_t*)CHC_find_entry(name, DIRECTORY_BASE_PATH, DIRECTORY_CACHE);
    if (loaded_directory != NULL) {
        print_io("Loading directory [%s] from GCT", load_path);
        return loaded_directory;
    }

    #pragma omp critical (directory_load)
    {
        ci_t ci = NIFAT32_open_content(NO_RCI, load_path, DF_MODE);
        print_io("Loading directory [%s]", load_path);
        if (ci < 0) { print_error("Directory not found! Path: [%s]", load_path); }
        else {
            directory_header_t* header = (directory_header_t*)malloc_s(sizeof(directory_header_t));
            if (header) {
                int offset = 0;
                str_memset(header, 0, sizeof(directory_header_t));

                unsigned short encoded_header[sizeof(directory_header_t)] = { 0 };
                NIFAT32_read_content2buffer(ci, offset, (const_buffer_t)encoded_header, sizeof(directory_header_t) * sizeof(unsigned short));
                unpack_memory((unsigned short*)encoded_header, (unsigned char*)header, sizeof(directory_header_t));
                offset += sizeof(directory_header_t) * sizeof(unsigned short);

                if (header->magic != DIRECTORY_MAGIC) {
                    print_error("Directory file wrong magic for [%s]", load_path);
                    free_s(header);
                    NIFAT32_close_content(ci);
                } 
                else {
                    directory_t* directory = (directory_t*)malloc_s(sizeof(directory_t));
                    if (!directory) free_s(header);
                    else {
                        str_memset(directory, 0, sizeof(directory_t));
                        for (int i = 0; i < MIN(header->page_count, PAGES_PER_DIRECTORY); i++) {
                            unsigned short encoded_page_name[PAGE_NAME_SIZE] = { 0 };
                            NIFAT32_read_content2buffer(ci, offset, (const_buffer_t)encoded_page_name, PAGE_NAME_SIZE * sizeof(unsigned short));
                            unpack_memory((unsigned short*)encoded_page_name, (unsigned char*)directory->page_names[i], PAGE_NAME_SIZE);
                            offset += PAGE_NAME_SIZE * sizeof(unsigned short);
                        }

                        NIFAT32_close_content(ci);

                        directory->lock   = NULL_LOCK;
                        directory->header = header;
                        loaded_directory  = directory;

                        CHC_add_entry(
                            loaded_directory, loaded_directory->header->name, DIRECTORY_BASE_PATH, DIRECTORY_CACHE, 
                            (void*)DRM_free_directory, (void*)DRM_save_directory
                        );
                    }
                }
            }
        }
    }

    return loaded_directory;
}

int DRM_delete_directory(directory_t* directory, int full) {
#ifndef NO_DELETE_COMMAND
    if (!directory) return -1;
    if (THR_require_write(&directory->lock, get_thread_num())) {
        if (full) {
            #pragma omp parallel for schedule(dynamic, 1)
            for (int i = 0; i < directory->header->page_count; i++) {
                print_io(
                    "Page [%s] was deleted and flushed with results [%i | %i]",
                    directory->page_names[i], CHC_flush_entry(
                        PGM_load_page(directory->header->name, directory->page_names[i]), PAGE_CACHE
                    ), delete_file(directory->page_names[i], directory->header->name, PAGE_EXTENSION)
                );
            }
        }

        delete_file(directory->header->name, DIRECTORY_BASE_PATH, DIRECTORY_EXTENSION);
        CHC_flush_entry(directory, DIRECTORY_CACHE);
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
    if (!directory) return -2;
    if (directory->is_cached) return -1;
    DRM_save_directory(directory);
    return DRM_free_directory(directory);
}

int DRM_free_directory(directory_t* directory) {
    if (!directory) return -1;
    SOFT_FREE(directory->header);
    SOFT_FREE(directory);
    return 1;
}

unsigned int DRM_get_checksum(directory_t* directory) {
    if (!directory) return 0;
    unsigned int prev_checksum = directory->header->checksum;
    directory->header->checksum = 0;

    unsigned int _checksum = 0;
    if (directory->header) {
        _checksum = murmur3_x86_32((const unsigned char*)directory->header, sizeof(directory_header_t), 0);
    }

    directory->header->checksum = prev_checksum;
    _checksum = murmur3_x86_32((const unsigned char*)directory->page_names, sizeof(directory->page_names), 0);
    return _checksum;
}
