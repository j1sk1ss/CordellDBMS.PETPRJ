#include "../../include/tabman.h"


static int _link_dir2table(table_t* __restrict table, directory_t* __restrict directory) {
    #pragma omp critical (link_dir2table)
    strncpy(table->dir_names[table->header->dir_count++], directory->header->name, DIRECTORY_NAME_SIZE);
    return 1;
}

static int _unlink_dir_from_table(table_t* table, const char* dir_name) {
    int status = 0;
    #pragma omp critical (unlink_dir_from_table)
    {
        for (int i = 0; i < table->header->dir_count; i++) {
            if (strncmp(table->dir_names[i], dir_name, DIRECTORY_NAME_SIZE) == 0) {
                for (int j = i; j < table->header->dir_count - 1; j++) {
                    memcpy(table->dir_names[j], table->dir_names[j + 1], DIRECTORY_NAME_SIZE);
                }

                table->header->dir_count--;
                table->append_offset = MAX(table->append_offset - 1, 0);
                status = 1;
                break;
            }
        }
    }

    return status;
}

#pragma region [CRUD]

int TBM_append_content(table_t* __restrict table, unsigned char* __restrict data, size_t data_size) {
    unsigned char* data_pointer = data;
    int size4append = (int)data_size;

    // Iterate existed directories. Maybe we can store data here?
    for (int i = table->append_offset; i < table->header->dir_count; i++) { // O(n)
        // Load directory to memory
        directory_t* directory = DRM_load_directory(table->dir_names[i]);
        if (!directory) continue;
        if (THR_require_lock(&directory->lock, omp_get_thread_num()) == 1) {
            int result = DRM_append_content(directory, data_pointer, size4append);
            THR_release_lock(&directory->lock, omp_get_thread_num());
            DRM_flush_directory(directory);
            if (result < 0) return result - 10;
            else if (result == 0 || result == 1 || result == 2) {
                return 1;
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

    _link_dir2table(table, new_directory);

    // Save directory to DDT
    CHC_add_entry(
        new_directory, new_directory->header->name, DIRECTORY_BASE_PATH,
        DIRECTORY_CACHE, (void*)DRM_free_directory, (void*)DRM_save_directory
    );
    
    DRM_flush_directory(new_directory);
    return 1;
}

int TBM_get_content(table_t* __restrict table, int offset,  unsigned char* __restrict buffer, size_t size) {
    // Data for DRM
    directory_t* directory = NULL;
    int content2get_size = (int)size;

    // Allocate data for output
    unsigned char* output_content_pointer = buffer;
    memset(output_content_pointer, 0, size);

    // Iterate from all directories in table
    int current_index = 0;
    for (int i = 0; i < table->header->dir_count && content2get_size > 0; i++) {
        // Load directory to memory
        directory = DRM_load_directory(table->dir_names[i]);
        if (!directory) continue;
        if (current_index + directory->header->page_count * PAGE_CONTENT_SIZE <= offset) {
            current_index += directory->header->page_count * PAGE_CONTENT_SIZE;
            continue;
        }

        if (THR_require_lock(&directory->lock, omp_get_thread_num()) == 1) {
            // Get data from directory
            // After getting data, copy it to allocated output
            int current_size = MIN(directory->header->page_count * PAGE_CONTENT_SIZE, content2get_size);
            DRM_get_content(directory, MAX(offset - current_index, 0), output_content_pointer, current_size);
            THR_release_lock(&directory->lock, omp_get_thread_num());
            
            // Set offset to 0, because we go to next directory
            // Update size of getcontent
            offset = 0;
            current_index = 0;

            content2get_size -= current_size;
            output_content_pointer += current_size;
        }

        DRM_flush_directory(directory);
    }

    return 1;
}

int TBM_insert_content(table_t* __restrict table, int offset, unsigned char* __restrict data, size_t data_size) {
#ifndef NO_UPDATE_COMMAND
    unsigned char* data_pointer = data;
    int size4insert = (int)data_size;

    int current_index = 0;
    for (int i = 0; i < table->header->dir_count && size4insert > 0; i++) {
        // Load directory to memory
        directory_t* directory = DRM_load_directory(table->dir_names[i]);
        if (!directory) return -1;
        if (current_index + directory->header->page_count * PAGE_CONTENT_SIZE <= offset) {
            current_index += directory->header->page_count * PAGE_CONTENT_SIZE;
            continue;
        }

        if (THR_require_lock(&directory->lock, omp_get_thread_num()) == 1) {
            int result = DRM_insert_content(directory, MAX(offset - current_index, 0), data_pointer, size4insert);
            THR_release_lock(&directory->lock, omp_get_thread_num());

            if (result == -1) return -1;
            else if (result == 1 || result == 2) size4insert = 0;
            else {
                data_pointer += size4insert - result;
                size4insert = result;
            }
        }

        DRM_flush_directory(directory);
    }

    return 1;
#endif
    return -2;
}

int TBM_delete_content(table_t* table, int offset, size_t size) {
#ifndef NO_DELETE_COMMAND
    int size4delete = (int)size;

    int current_index = 0;
    int deleted_data = 0;
    for (int i = 0; i < table->header->dir_count && size4delete > 0; i++) {
        // Load directory to memory
        directory_t* directory = DRM_load_directory(table->dir_names[i]);
        if (!directory) return -1;
        if (current_index + directory->header->page_count * PAGE_CONTENT_SIZE <= offset) {
            current_index += directory->header->page_count * PAGE_CONTENT_SIZE;
            continue;
        }

        if (THR_require_lock(&directory->lock, omp_get_thread_num()) == 1) {
            int result = DRM_delete_content(directory, MAX(offset - current_index, 0), size4delete);
            table->append_offset = MIN(table->append_offset, i);

            offset = 0;
            size4delete -= result;
            deleted_data += result;

            THR_release_lock(&directory->lock, omp_get_thread_num());
        }

        DRM_flush_directory(directory);
    }

    return deleted_data;
#endif
    return -2;
}

#pragma endregion

int TBM_cleanup_dirs(table_t* table) {
#ifndef NO_DELETE_COMMAND
    int temp_count = table->header->dir_count;
    char** temp_names = copy_array2array((char**)table->dir_names, DIRECTORY_NAME_SIZE, temp_count, DIRECTORY_NAME_SIZE);
    if (!temp_names) return -1;

    #pragma omp parallel for schedule(dynamic, 4)
    for (int i = 0; i < temp_count; i++) {
        char dir_path[DEFAULT_PATH_SIZE] = { 0 };
        get_load_path(temp_names[i], DIRECTORY_NAME_SIZE, dir_path, DIRECTORY_BASE_PATH, DIRECTORY_EXTENSION);

        directory_t* directory = DRM_load_directory(temp_names[i]);
        if (!directory) continue;
        if (THR_require_lock(&directory->lock, omp_get_thread_num()) == 1) {
            DRM_cleanup_pages(directory);
            if (directory->header->page_count == 0) {
                _unlink_dir_from_table(table, directory->header->name);
                if (CHC_flush_entry(directory, DIRECTORY_CACHE) == -2) DRM_flush_directory(directory);
                print_debug("Directory [%s] was deleted with result [%i]", temp_names[i], remove(dir_path));
                continue;
            }
            else {
                THR_release_lock(&directory->lock, omp_get_thread_num());
            }
        }

        DRM_flush_directory(directory);
    }

    ARRAY_SOFT_FREE(temp_names, temp_count);
#endif
    return 1;
}

int TBM_find_content(table_t* __restrict table, int offset, unsigned char* __restrict data, size_t data_size) {
    unsigned char* data_pointer = data;
    size_t temp_data_size = data_size;
    int target_global_index = -1;
    int current_index = 0;
 
    for (int i = 0; i < table->header->dir_count && temp_data_size > 0; i++) {
        // We load current page to memory
        directory_t* directory = DRM_load_directory(table->dir_names[i]);
        if (!directory) return -2;
        if (current_index + directory->header->page_count * PAGE_CONTENT_SIZE <= offset) {
            current_index += directory->header->page_count * PAGE_CONTENT_SIZE;
            continue;
        }

        // We search part of data in this directory, save index and unload directory.
        int current_size = MIN((directory->header->page_count * PAGE_CONTENT_SIZE) - MAX(offset - current_index, 0), (int)temp_data_size);
        if (THR_require_lock(&directory->lock, omp_get_thread_num()) == 1) {
            int result = DRM_find_content(directory, MAX(offset - current_index, 0), data_pointer, current_size);
            THR_release_lock(&directory->lock, omp_get_thread_num());

            // If TGI is -1, we know that we start seacrhing from start.
            // Save current TGI of find part of data.
            if (target_global_index == -1) target_global_index = result;
            if (result == -1) {
                // We don`t find any entry of data part.
                // This indicates, that we don`t find any data.
                // Restore size4search and datapointer, we go to start
                temp_data_size = data_size;
                data_pointer = data;
            } 
            else {
                // Move pointer to next position
                temp_data_size -= current_size;
                data_pointer += current_size;
            }
        }

        DRM_flush_directory(directory);
    }

    if (target_global_index != -1) target_global_index += current_index;
    return target_global_index;
}

int TBM_migrate_table(table_t* __restrict src, table_t* __restrict dst, char* __restrict querry[], size_t querry_size) {
#ifndef NO_MIGRATE_COMMAND
    if (THR_require_lock(&src->lock, omp_get_thread_num()) == 1 && THR_require_lock(&dst->lock, omp_get_thread_num()) == 1) {
        int offset = 0;
        unsigned char* data = (unsigned char*)" ";

        while (*data != '\0') {
            SOFT_FREE(data);
            data = (unsigned char*)malloc(src->row_size);
            TBM_get_content(src, offset, data, src->row_size);
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
                table_columns_info_t fquerry;
                table_columns_info_t squerry;
                TBM_get_column_info(dst, querry[i + 1], &fquerry);
                TBM_get_column_info(src, querry[i], &squerry);
                memcpy(new_row + fquerry.offset, data + squerry.offset, squerry.size);
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