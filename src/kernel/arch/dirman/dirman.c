#include "../../include/dirman.h"


unsigned char* DRM_get_content(directory_t* directory, int offset, size_t data_lenght) {
    unsigned char* content = (unsigned char*)malloc(data_lenght);
    if (!content) return NULL;
    unsigned char* content_pointer = content;
    memset(content_pointer, 0, data_lenght);

    int page_offset   = offset / PAGE_CONTENT_SIZE;
    int current_index = offset % PAGE_CONTENT_SIZE;

    for (int i = page_offset; i < directory->header->page_count && data_lenght > 0; i++) {
        // We load current page
        page_t* page = PGM_load_page(directory->page_names[i]);
        if (page == NULL) continue;
        if (THR_require_lock(&page->lock, omp_get_thread_num()) == 1) {
            // We work with page
            int current_size = MIN(PAGE_CONTENT_SIZE - current_index, (int)data_lenght);
            memcpy(content_pointer, (unsigned char*)page->content + current_index, current_size);

            // We reload local index and update size2get
            // Also we move content pointer to next location
            current_index = 0;
            data_lenght -= current_size;
            content_pointer += current_size;

            THR_release_lock(&page->lock, omp_get_thread_num());
        }

        PGM_flush_page(page);
    }

    return content;
}

int DRM_append_content(directory_t* __restrict directory, unsigned char* __restrict data, size_t data_lenght) {
    // First we try to find fit empty place somewhere in linked pages
    // We skip this part if data_lenght larger then PAGE_CONTENT_SIZE
    if (data_lenght < PAGE_CONTENT_SIZE) {
        for (int i = directory->append_offset; i < directory->header->page_count; i++) {
            page_t* page = PGM_load_page(directory->page_names[i]);
            if (page != NULL) {
                int index = PGM_get_fit_free_space(page, PAGE_START, data_lenght);
                if (index >= 0) {
                    if (THR_require_lock(&page->lock, omp_get_thread_num()) == 1) {
                        PGM_insert_content(page, index, data, data_lenght);
                        THR_release_lock(&page->lock, omp_get_thread_num());

                        PGM_flush_page(page);
                        return 1;
                    }
                }
            }

            PGM_flush_page(page);
        }

        if (directory->header->page_count + 1 >= PAGES_PER_DIRECTORY) return (int)data_lenght;

        // We allocate memory for page structure with all needed data
        page_t* new_page = PGM_create_empty_page();
        if (new_page == NULL) return -2;

        // Insert new content to page and mark end
        directory->append_offset = directory->header->page_count;
        PGM_insert_content(new_page, 0, data, data_lenght);

        // We link page to directory
        DRM_link_page2dir(directory, new_page);
        CHC_add_entry(new_page, new_page->header->name, PAGE_CACHE, (void*)PGM_free_page, (void*)PGM_save_page);
        PGM_flush_page(new_page);

        return 2;
    }
    
    return -3;
}

int DRM_insert_content(
    directory_t* __restrict directory, int offset, unsigned char* __restrict data, size_t data_lenght
) {
#ifndef NO_UPDATE_COMMAND
    int page_offset  = offset / PAGE_CONTENT_SIZE;
    int index_offset = offset % PAGE_CONTENT_SIZE;

    unsigned char* data_pointer = data;
    for (int i = page_offset; i < directory->header->page_count && data_lenght > 0; i++) {
        // We load current page to memory
        page_t* page = PGM_load_page(directory->page_names[i]);
        if (page == NULL) return -1;

        // We insert current part of content with local offset
        if (THR_require_lock(&page->lock, omp_get_thread_num()) == 1) { 
            int result = PGM_insert_content(page, index_offset, data_pointer, (int)data_lenght);
            THR_release_lock(&page->lock, omp_get_thread_num());

            // We reload local index and update size2delete
            index_offset = 0;
            data_lenght -= result;
            data_pointer += result;
        }

        PGM_flush_page(page);
    }

    if (data_lenght > 0) return 2;
    else return 1;
#endif
    return 1;
}

int DRM_delete_content(directory_t* directory, int offset, size_t data_size) {
#ifndef NO_DELETE_COMMAND
    int page_offset   = offset / PAGE_CONTENT_SIZE;
    int current_index = offset % PAGE_CONTENT_SIZE;

    int deleted_data = 0;
    for (int i = page_offset; i < directory->header->page_count && data_size > 0; i++) {
        // We load current page
        page_t* page = PGM_load_page(directory->page_names[i]);
        if (page == NULL) return -1;
        if (THR_require_lock(&page->lock, omp_get_thread_num()) == 1) {
            int result = PGM_delete_content(page, current_index, data_size);
            directory->append_offset = i;

            // We reload local index and update size2delete
            current_index = 0;
            data_size -= result;
            deleted_data += result;

            THR_release_lock(&page->lock, omp_get_thread_num());
        }

        PGM_flush_page(page);
    }

    return deleted_data;
#endif
    return 1;
}

int DRM_cleanup_pages(directory_t* directory) {
#ifndef NO_DELETE_COMMAND
    #pragma omp parallel for schedule(dynamic, 1)
    for (int i = 0; i < directory->header->page_count; i++) {
        char page_path[DEFAULT_PATH_SIZE] = { 0 };
        sprintf(page_path, "%s%.*s.%s", PAGE_BASE_PATH, PAGE_NAME_SIZE, directory->page_names[i], PAGE_EXTENSION);
        page_t* page = PGM_load_page(directory->page_names[i]);
        if (page != NULL) {
            if (THR_require_lock(&page->lock, omp_get_thread_num()) == 1) {
                // If page, after delete operation, full empty, we delete page.
                // Also we realise page pointer in RAM.
                if (PGM_get_free_space(page, PAGE_START) == PAGE_CONTENT_SIZE) {
                    DRM_unlink_page_from_directory(directory, page->header->name);
                    CHC_flush_entry(page, PAGE_CACHE);
                    print_debug("Page [%s] was deleted with result [%i]", page_path, remove(page_path));
                }
                else {
                    THR_release_lock(&page->lock, omp_get_thread_num());
                }
            }

            PGM_flush_page(page);
        }
    }

#endif
    return 1;
}

int DRM_find_content(
    directory_t* __restrict directory, int offset, unsigned char* __restrict data, size_t data_size
) {
    int target_global_index = -1;
    int page_offset   = offset / PAGE_CONTENT_SIZE;
    int current_index = offset % PAGE_CONTENT_SIZE;
    int pages4search  = directory->header->page_count - page_offset;
    size_t temp_data_size = data_size;

    unsigned char* data_pointer = data;
    for (; pages4search > 0 && temp_data_size > 0 && page_offset < directory->header->page_count; pages4search--) {
        // We load current page to memory.
        page_t* page = PGM_load_page(directory->page_names[page_offset]);
        if (page == NULL) return -2;

        // We search part of data in this page, save index and unload page.
        int current_size = MIN(PAGE_CONTENT_SIZE - current_index, (int)temp_data_size);
        if (THR_require_lock(&page->lock, omp_get_thread_num()) == 1) {
            int result = PGM_find_content(page, current_index, data_pointer, current_size);
            THR_release_lock(&page->lock, omp_get_thread_num());

            // If TGI is -1, we know that we start seacrhing from start.
            // Save current TGI of find part of data.
            if (target_global_index == -1) {
                target_global_index = result + page_offset * PAGE_CONTENT_SIZE;
            }

            if (result == -1) {
                // We don`t find any entry of data part.
                // This indicates, that we don`t find any data.
                // Restore size4search and datapointer, we go to start
                temp_data_size = data_size;
                data_pointer = data;
                target_global_index = -1;
            } 
            else {
                // Move pointer to next position
                temp_data_size -= current_size;
                data_pointer += current_size;
            }

            // Set local index to 0. We don`t need offset now.
            current_index = 0;
            page_offset++;
        }

        PGM_flush_page(page);
    }

    return target_global_index;
}

int DRM_link_page2dir(directory_t* __restrict directory, page_t* __restrict page) {
    #pragma omp critical (link_page2dir)
    strncpy_s(directory->page_names[directory->header->page_count++], page->header->name, PAGE_NAME_SIZE);
    return 1;
}

int DRM_unlink_page_from_directory(directory_t* __restrict directory, char* __restrict page_name) {
    int status = 0;
    #pragma omp critical (unlink_page_from_directory)
    {
        for (int i = 0; i < directory->header->page_count; i++) {
            if (strncmp_s(directory->page_names[i], page_name, PAGE_NAME_SIZE) == 0) {
                for (int j = i; j < directory->header->page_count - 1; j++)
                    memcpy_s(directory->page_names[j], directory->page_names[j + 1], PAGE_NAME_SIZE);

                directory->header->page_count--;
                status = 1;
                break;
            }
        }
    }

    return status;
}
