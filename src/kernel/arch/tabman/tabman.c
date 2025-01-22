#include "../../include/tabman.h"


unsigned char* TBM_get_content(table_t* table, int offset, size_t size) {
    int global_page_offset = offset / PAGE_CONTENT_SIZE;
    int directory_index    = -1;
    int current_page       = 0;

    // Find directory that has first page of data (first page).
    directory_t* directory = NULL;
    for (int i = 0; i < table->header->dir_count; i++) {
        // Load directory from disk to memory
        directory = DRM_load_directory(table->dir_names[i]);
        if (directory == NULL) continue;

        // Check:
        // If current directory include page, that we want, we save this directory.
        // Else we unload directory and go to the next dir. name.
        int page_count = directory->header->page_count;
        DRM_flush_directory(directory);
        if (current_page + page_count < global_page_offset) {
            current_page += page_count;
            continue;
        }
        else {
            // Get offset in pages for local directory and save directory index
            directory_index = i;
            break;
        }
    }

    // Data for DRM
    if (directory_index == -1) return NULL;
    int content2get_size = (int)size;

    // Allocate data for output
    unsigned char* output_content = (unsigned char*)malloc(size);
    if (!output_content) return NULL;
    unsigned char* output_content_pointer = output_content;
    memset(output_content_pointer, 0, size);

    // Iterate from all directories in table
    for (int i = directory_index; i < table->header->dir_count && content2get_size > 0; i++) {
        // Load directory to memory
        directory = DRM_load_directory(table->dir_names[i]);
        if (directory == NULL) continue;
        if (THR_require_lock(&directory->lock, omp_get_thread_num()) == 1) {
            // Get data from directory
            // After getting data, copy it to allocated output
            int current_size = MIN(directory->header->page_count * PAGE_CONTENT_SIZE, content2get_size);
            unsigned char* directory_content = DRM_get_content(directory, offset, current_size);
            THR_release_lock(&directory->lock, omp_get_thread_num());
            if (directory_content != NULL) {
                memcpy(output_content_pointer, directory_content, current_size);

                // Realise data
                free(directory_content);

                // Set offset to 0, because we go to next directory
                // Update size of getcontent
                offset = 0;
                content2get_size -= current_size;
                output_content_pointer += current_size;
            }
        }

        DRM_flush_directory(directory);
    }

    return output_content;
}

int TBM_append_content(table_t* __restrict table, unsigned char* __restrict data, size_t data_size) {
    unsigned char* data_pointer = data;
    int size4append = (int)data_size;

    // Iterate existed directories. Maybe we can store data here?
    for (int i = table->append_offset; i < table->header->dir_count; i++) {
        // Load directory to memory
        directory_t* directory = DRM_load_directory(table->dir_names[i]);
        if (directory != NULL) {
            if (THR_require_lock(&directory->lock, omp_get_thread_num()) == 1) {
                if (directory->header->page_count + size4append <= PAGES_PER_DIRECTORY) {
                    int result = DRM_append_content(directory, data_pointer, size4append);
                    THR_release_lock(&directory->lock, omp_get_thread_num());
                    DRM_flush_directory(directory);
                    if (result < 0) return result - 10;
                    else if (result == 0 || result == 1 || result == 2) {
                        return 1;
                    }
                }
            }
        }

        DRM_flush_directory(directory);
    }

    if (table->header->dir_count + 1 > DIRECTORIES_PER_TABLE) return -1;

    // Create new empty directory and append data.
    // If we overfill directory by data, save size of data,
    // that we should save.
    directory_t* new_directory = DRM_create_empty_directory();
    if (new_directory == NULL) return -1;

    table->append_offset = table->header->dir_count;
    int append_result = DRM_append_content(new_directory, data_pointer, size4append);
    if (append_result < 0) {
        DRM_free_directory(new_directory);
        return append_result - 10;
    }

    TBM_link_dir2table(table, new_directory);

    // Save directory to DDT
    CHC_add_entry(
        new_directory, new_directory->header->name, 
        DIRECTORY_CACHE, (void*)DRM_free_directory, (void*)DRM_save_directory
    );
    
    DRM_flush_directory(new_directory);
    return 1;
}

int TBM_insert_content(table_t* __restrict table, int offset, unsigned char* __restrict data, size_t data_size) {
#ifndef NO_UPDATE_COMMAND
    if (table->header->dir_count == 0) return -3;

    unsigned char* data_pointer = data;
    int size4insert = (int)data_size;

    // Iterate existed directories. Maybe we can insert data here?
    for (int i = 0; i < table->header->dir_count; i++) {
        // Load directory to memory
        directory_t* directory = DRM_load_directory(table->dir_names[i]);
        if (directory == NULL) return -1;

        if (THR_require_lock(&directory->lock, omp_get_thread_num()) == 1) {
            int result = DRM_insert_content(directory, offset, data_pointer, size4insert);
            THR_release_lock(&directory->lock, omp_get_thread_num());
            DRM_flush_directory(directory);

            if (result == -1) return -1;
            else if (result == 1 || result == 2) {
                return result;
            }

            // Move append data pointer.
            int loaded_data = size4insert - result;
            data_pointer += loaded_data;
            size4insert = result;
        }
    }

    // If we reach end, return error code.
    // Check docs why we return error, instead dir creation.
#endif
    return -2;
}

int TBM_delete_content(table_t* table, int offset, size_t size) {
#ifndef NO_DELETE_COMMAND
    int size4delete = (int)size;

    // Iterate existed directories. Maybe we can insert data here?
    for (int i = 0; i < table->header->dir_count; i++) {
        // Load directory to memory
        directory_t* directory = DRM_load_directory(table->dir_names[i]);
        if (directory == NULL) return -1;
        if (THR_require_lock(&directory->lock, omp_get_thread_num()) == 1) {
            table->append_offset = i;

            size4delete = DRM_delete_content(directory, offset, size4delete);
            THR_release_lock(&directory->lock, omp_get_thread_num());
            DRM_flush_directory(directory);

            if (size4delete < 0) return -1;
            else if (size4delete >= 0) return size4delete;
        }
    }

    // If we reach end, return error code.
#endif
    return -2;
}

int TBM_cleanup_dirs(table_t* table) {
#ifndef NO_DELETE_COMMAND
    #pragma omp parallel for schedule(dynamic, 1)
    for (int i = 0; i < table->header->dir_count; i++) {
        directory_t* directory = DRM_load_directory(table->dir_names[i]);
        DRM_cleanup_pages(directory);
        if (directory->header->page_count == 0) {
            TBM_unlink_dir_from_table(table, directory->header->name);
            DRM_delete_directory(directory, 0);
        }
        else DRM_flush_directory(directory);
    }

#endif
    return 1;
}

int TBM_find_content(table_t* __restrict table, int offset, unsigned char* __restrict data, size_t data_size) {
    unsigned char* data_pointer = data;
    int directory_offset    = 0;
    int size4seach          = (int)data_size;
    int target_global_index = -1;

    for (; directory_offset < table->header->dir_count && size4seach > 0; directory_offset++) {
        // We load current page to memory
        directory_t* directory = DRM_load_directory(table->dir_names[directory_offset]);
        if (directory == NULL) return -2;

        // We search part of data in this page, save index and unload page.
        if (THR_require_lock(&directory->lock, omp_get_thread_num()) == 1) {
            int result = DRM_find_content(directory, offset, data_pointer, size4seach);
            THR_release_lock(&directory->lock, omp_get_thread_num());

            // If TGI is -1, we know that we start seacrhing from start.
            // Save current TGI of find part of data.
            if (target_global_index == -1) target_global_index = result;
            if (result == -1) {
                // We don`t find any entry of data part.
                // This indicates, that we don`t find any data.
                // Restore size4search and datapointer, we go to start
                size4seach   = (int)data_size;
                data_pointer = data;
            } 
            else {
                // Move pointer to next position
                size4seach   -= directory->header->page_count * PAGE_CONTENT_SIZE - result;
                data_pointer += directory->header->page_count * PAGE_CONTENT_SIZE - result;
            }
        }

        DRM_flush_directory(directory);
    }

    if (target_global_index != -1) {
        for (int i = 0; i < MAX(directory_offset - 1, 0); i++) {
            directory_t* directory = DRM_load_directory(table->dir_names[i]);
            if (directory != NULL) {
                target_global_index += directory->header->page_count * PAGE_CONTENT_SIZE;
                DRM_flush_directory(directory);
            }
        }
    }

    return target_global_index;
}

int TBM_link_dir2table(table_t* __restrict table, directory_t* __restrict directory) {
    #pragma omp critical (link_dir2table)
    strncpy(table->dir_names[table->header->dir_count++], directory->header->name, DIRECTORY_NAME_SIZE);
    return 1;
}

int TBM_unlink_dir_from_table(table_t* table, const char* dir_name) {
    int status = 0;
    #pragma omp critical (unlink_dir_from_table)
    {
        for (int i = 0; i < table->header->dir_count; i++) {
            if (strncmp(table->dir_names[i], dir_name, DIRECTORY_NAME_SIZE) == 0) {
                for (int j = i; j < table->header->dir_count - 1; j++) {
                    memcpy(table->dir_names[j], table->dir_names[j + 1], DIRECTORY_NAME_SIZE);
                }

                table->header->dir_count--;
                status = 1;
                break;
            }
        }
    }

    return status;
}

int TBM_migrate_table(table_t* __restrict src, table_t* __restrict dst, int* __restrict querry, size_t querry_size) {
#ifndef NO_MIGRATE_COMMAND
    if (THR_require_lock(&src->lock, omp_get_thread_num()) == 1 && THR_require_lock(&dst->lock, omp_get_thread_num()) == 1) {
        table_column_t** src_columns = src->columns;
        table_column_t** dst_columns = dst->columns;

        int offset = 0;
        unsigned char* data = (unsigned char*)" ";

        while (*data != '\0') {
            SOFT_FREE(data);
            data = TBM_get_content(src, offset, src->row_size);
            unsigned char* new_row = (unsigned char*)malloc(dst->row_size);
            if (!new_row || !data) {
                SOFT_FREE(new_row);
                SOFT_FREE(data);

                THR_release_lock(&src->lock, omp_get_thread_num());
                THR_release_lock(&dst->lock, omp_get_thread_num());

                return -2;
            }

            memset(new_row, '0', dst->row_size);
            for (size_t i = 0; i < querry_size; i += 2) {
                memcpy(
                    new_row + TBM_get_column_offset(dst, dst_columns[querry[i + 1]]->name),
                    data + TBM_get_column_offset(src, src_columns[querry[i]]->name),
                    src_columns[querry[i]]->size
                );
            }

            TBM_append_content(dst, new_row, dst->row_size);

            SOFT_FREE(new_row);
            offset += src->row_size;
        }

        SOFT_FREE(data);
        THR_release_lock(&src->lock, omp_get_thread_num());
        THR_release_lock(&dst->lock, omp_get_thread_num());
        return 1;
    }
    else return -1;
#endif
    return 1;
}

int TBM_invoke_modules(table_t* __restrict table, unsigned char* __restrict data, unsigned char type) {
    int module_offset = 0;
    for (int i = 0; i < table->header->column_count; i++) {
        if (GET_COLUMN_DATA_TYPE(table->columns[i]->type) == COLUMN_TYPE_MODULE) {
            if (table->columns[i]->module_params == type || table->columns[i]->module_params == COLUMN_MODULE_BOTH) {
                char* formula = table->columns[i]->module_querry;
                char* output_querry = (char*)malloc(COLUMN_MODULE_SIZE);
                if (!output_querry) return -1;
                strncpy(output_querry, formula, COLUMN_MODULE_SIZE);

                int content_offset = 0;
                for (int j = 0; j < table->header->column_count; j++) {
                    unsigned char* content_pointer = data + content_offset;
                    char* content_part = (char*)malloc(table->columns[j]->size + 1);
                    if (!content_part) {
                        free(output_querry);
                        return -2;
                    }

                    strncpy(content_part, (char*)content_pointer, table->columns[j]->size);
                    char* next_output_querry = strrep(output_querry, table->columns[j]->name, content_part);

                    free(content_part);
                    if (!next_output_querry) continue;
                    free(output_querry);

                    output_querry = (char*)next_output_querry;
                    content_offset += table->columns[j]->size;
                }

                MDL_launch_module(table->columns[i]->module_name, output_querry, data + module_offset, table->columns[i]->size);
            }
        }

        module_offset += table->columns[i]->size;
    }

    return 1;
}