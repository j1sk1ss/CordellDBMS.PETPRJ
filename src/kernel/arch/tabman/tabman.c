#include "../../include/tabman.h"


uint8_t* TBM_get_content(table_t* table, int offset, size_t size) {
    int global_page_offset = offset / PAGE_CONTENT_SIZE;
    int directory_index    = -1;
    int current_page       = 0;

    // Find directory that has first page of data (first page).
    directory_t* directory = NULL;
    for (int i = 0; i < table->header->dir_count; i++) {
        // Load directory from disk to memory
        directory = DRM_load_directory(NULL, (char*)table->dir_names[i]);
        if (directory == NULL) continue;

        // Check:
        // If current directory include page, that we want, we save this directory.
        // Else we unload directory and go to the next dir. name.
        int page_count = directory->header->page_count;
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
    int content2get_size = (int)size;

    // Allocate data for output
    uint8_t* output_content = (uint8_t*)malloc(size);
    uint8_t* output_content_pointer = output_content;

    // Iterate from all directories in table
    for (int i = directory_index; i < table->header->dir_count && content2get_size > 0; i++) {
        // Load directory to memory
        directory = DRM_load_directory(NULL, (char*)table->dir_names[i]);
        if (DRM_lock_directory(directory, omp_get_thread_num()) == -1) {
            print_warn("Can't lock directory [%s] while get_content operation!", (char*)table->dir_names[i]);
            continue;
        }

        if (directory == NULL) continue;

        // Get data from directory
        // After getting data, copy it to allocated output
        int current_size = MIN(directory->header->page_count * PAGE_CONTENT_SIZE, content2get_size);
        uint8_t* directory_content = DRM_get_content(directory, offset, current_size);
        DRM_release_directory(directory, omp_get_thread_num());
        if (directory_content == NULL) continue;

        memcpy(output_content_pointer, directory_content, current_size);

        // Realise data
        free(directory_content);

        // Set offset to 0, because we go to next directory
        // Update size of getcontent
        offset = 0;
        content2get_size       -= current_size;
        output_content_pointer += current_size;
    }

    return output_content;
}

int TBM_append_content(table_t* table, uint8_t* data, size_t data_size) {
    uint8_t* data_pointer = data;
    int size4append = (int)data_size;

    // Iterate existed directories. Maybe we can store data here?
    for (int i = 0; i < table->header->dir_count; i++) {
        // Load directory to memory
        directory_t* directory = DRM_load_directory(NULL, (char*)table->dir_names[i]);
        if (DRM_lock_directory(directory, omp_get_thread_num()) == -1) return -2;
        if (directory == NULL) return -1;
        if (directory->header->page_count + size4append > PAGES_PER_DIRECTORY) continue;

        int result = DRM_append_content(directory, data_pointer, size4append);
        DRM_release_directory(directory, omp_get_thread_num());
        if (result < 0) return result - 10;
        else if (result == 0 || result == 1 || result == 2) {
            return 1;
        }
    }

    if (table->header->dir_count + 1 > DIRECTORIES_PER_TABLE) return -1;

    // Create new empty directory and append data.
    // If we overfill directory by data, save size of data,
    // that we should save.
    directory_t* new_directory = DRM_create_empty_directory();
    if (new_directory == NULL) return -1;

    int append_result = DRM_append_content(new_directory, data_pointer, size4append);
    if (append_result < 0) return append_result - 10;

    TBM_link_dir2table(table, new_directory);

    // Save directory to DDT
    DRM_DDT_add_directory(new_directory);
    return 1;
}

int TBM_insert_content(table_t* table, int offset, uint8_t* data, size_t data_size) {
    if (table->header->dir_count == 0) return -3;

    uint8_t* data_pointer = data;
    int size4insert = (int)data_size;

    // Iterate existed directories. Maybe we can insert data here?
    for (int i = 0; i < table->header->dir_count; i++) {
        // Load directory to memory
        directory_t* directory = DRM_load_directory(NULL, (char*)table->dir_names[i]);
        if (directory == NULL) return -1;

        if (DRM_lock_directory(directory, omp_get_thread_num()) == -1) return -4;
        int result = DRM_insert_content(directory, offset, data_pointer, size4insert);
        DRM_release_directory(directory, omp_get_thread_num());

        if (result == -1) return -1;
        else if (result == 1 || result == 2) {
            return result;
        }

        // Move append data pointer.
        int loaded_data = size4insert - result;
        data_pointer += loaded_data;
        size4insert = result;
    }

    // If we reach end, return error code.
    // Check docs why we return error, instead dir creation.
    return -2;
}

int TBM_delete_content(table_t* table, int offset, size_t size) {
    int size4delete = (int)size;

    // Iterate existed directories. Maybe we can insert data here?
    for (int i = 0; i < table->header->dir_count; i++) {
        // Load directory to memory
        directory_t* directory = DRM_load_directory(NULL, (char*)table->dir_names[i]);

        if (DRM_lock_directory(directory, omp_get_thread_num()) == -1) return -4;
        size4delete = DRM_delete_content(directory, offset, size4delete);
        DRM_release_directory(directory, omp_get_thread_num());

        if (size4delete == -1) return -1;
        else if (size4delete == 1 || size4delete == 2) return size4delete;
    }

    // If we reach end, return error code.
    return -2;
}

int TBM_cleanup_dirs(table_t* table) {
    for (int i = 0; i < table->header->dir_count; i++) {
        directory_t* directory = DRM_load_directory(NULL, (char*)table->dir_names[i]);
        DRM_cleanup_pages(directory);
        if (directory->header->page_count == 0) {
            TBM_unlink_dir_from_table(table, (char*)directory->header->name);
            DRM_delete_directory(directory, 0);
        }
    }

    return 1;
}

int TBM_find_content(table_t* table, int offset, uint8_t* data, size_t data_size) {
    int directory_offset    = 0;
    int size4seach          = (int)data_size;
    int target_global_index = -1;

    uint8_t* data_pointer = data;
    for (directory_offset = 0; directory_offset < table->header->dir_count && size4seach > 0; directory_offset++) {
        // We load current page to memory
        directory_t* directory = DRM_load_directory(NULL, (char*)table->dir_names[directory_offset]);
        if (directory == NULL) return -2;

        // We search part of data in this page, save index and unload page.
        int result = DRM_find_content(directory, offset, data_pointer, size4seach);

        // If TGI is -1, we know that we start seacrhing from start.
        // Save current TGI of find part of data.
        if (target_global_index == -1) {
            target_global_index = result;
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
            size4seach   -= directory->header->page_count * PAGE_CONTENT_SIZE - result;
            data_pointer += directory->header->page_count * PAGE_CONTENT_SIZE - result;
        }
    }

    if (target_global_index == -1) return -1;
    for (int i = 0; i < MAX(directory_offset - 1, 0); i++) {
        directory_t* directory = DRM_load_directory(NULL, (char*)table->dir_names[i]);
        if (directory == NULL) return -2;
        target_global_index += directory->header->page_count * PAGE_CONTENT_SIZE;
    }

    return target_global_index;
}

int TBM_link_dir2table(table_t* table, directory_t* directory) {
    if (table->header->dir_count + 1 >= DIRECTORIES_PER_TABLE)
        return -1;

    #pragma omp critical (link_dir2table)
    memcpy(table->dir_names[table->header->dir_count++], directory->header->name, DIRECTORY_NAME_SIZE);
    return 1;
}

int TBM_unlink_dir_from_table(table_t* table, const char* dir_name) {
    int status = 0;
    #pragma omp critical (unlink_dir_from_table)
    {
        for (int i = 0; i < table->header->dir_count; i++) {
            if (strncmp((char*)table->dir_names[i], dir_name, DIRECTORY_NAME_SIZE) == 0) {
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

#pragma endregion

#pragma region [Column]

int TBM_link_column2column(table_t* master, char* master_column_name, table_t* slave, char* slave_column_name, uint8_t type) {
    master->column_links = (table_column_link_t**)realloc(master->column_links, (master->header->column_link_count + 1) * sizeof(table_column_link_t*));
    master->column_links[master->header->column_link_count] = (table_column_link_t*)malloc(sizeof(table_column_link_t));
    memcpy(
        master->column_links[master->header->column_link_count]->master_column_name,
        master_column_name, COLUMN_NAME_SIZE
    );

    memcpy(
        master->column_links[master->header->column_link_count]->slave_table_name,
        slave->header->name, TABLE_NAME_SIZE
    );

    memcpy(
        master->column_links[master->header->column_link_count]->slave_column_name,
        slave_column_name, COLUMN_NAME_SIZE
    );

    master->column_links[master->header->column_link_count]->type = type;
    master->header->column_link_count++;
    return 1;
}

int TBM_unlink_column_from_column(table_t* master, char* master_column_name, table_t* slave, char* slave_column_name) {
    for (int i = 0; i < master->header->column_link_count; i++) {
        if (
            memcmp(master->column_links[i]->master_column_name, master_column_name, COLUMN_NAME_SIZE) == 0 &&
            memcmp(master->column_links[i]->slave_column_name, slave_column_name, COLUMN_NAME_SIZE) == 0 &&
            memcmp(master->column_links[i]->slave_table_name, slave->header->name, TABLE_NAME_SIZE) == 0
        ) {
            free(master->column_links[i]);

            for (int j = i; j < master->header->column_link_count - 1; j++)
                master->column_links[j] = master->column_links[j + 1];

            table_column_link_t** new_links = (table_column_link_t**)realloc(
                master->column_links, (master->header->column_link_count - 1) * sizeof(table_column_link_t*)
            );

            if (new_links || master->header->column_link_count - 1 == 0) master->column_links = new_links;
            else return -1;

            master->header->column_link_count--;
            return 1;
        }
    }

    return 0;
}

int TBM_update_column_in_table(table_t* table, table_column_t* column, int by_index) {
    if (by_index != -1) {
        if (by_index > table->header->column_count) return -1;
        if (table->columns[by_index]->size != column->size && table->header->dir_count != 0) return -2;

        SOFT_FREE(table->columns[by_index]);
        table->columns[by_index] = column;

        return 1;
    }

    for (int i = 0; i < table->header->column_count; i++) {
        if (memcmp(table->columns[i]->name, column->name, COLUMN_NAME_SIZE) == 0) {
            if (table->columns[i]->size != column->size && table->header->dir_count != 0) return -2;

            SOFT_FREE(table->columns[i]);
            table->columns[i] = column;
            return 1;
        }
    }

    return -1;
}

table_column_t* TBM_create_column(uint8_t type, uint8_t size, char* name) {
    table_column_t* column = (table_column_t*)malloc(sizeof(table_column_t));

    column->magic = COLUMN_MAGIC;
    strncpy((char*)column->name, name, COLUMN_NAME_SIZE);
    column->type = type;
    column->size = size;

    return column;
}

int TBM_check_signature(table_t* table, uint8_t* data) {
    uint8_t* data_pointer = data;
    for (int i = 0; i < table->header->column_count; i++) {
        char value[COLUMN_MAX_SIZE] = { '\0' };
        memcpy(value, data_pointer, table->columns[i]->size);
        data_pointer += table->columns[i]->size;

        uint8_t data_type = GET_COLUMN_DATA_TYPE(table->columns[i]->type);
        switch (data_type) {
            case COLUMN_TYPE_STRING:
                break;
            case COLUMN_TYPE_INT:
                if (!is_integer(value)) return -2;
                break;
            case COLUMN_TYPE_ANY:
                break;
            case COLUMN_TYPE_MODULE:
                break;
            default:
                return -4;
        }
    }

    return 1;
}

int TBM_invoke_modules(table_t* table, uint8_t* data, uint8_t type) {
    for (int i = 0; i < table->header->column_count; i++) {
        int content_offset = 0;
        int module_offset  = 0;

        if (GET_COLUMN_DATA_TYPE(table->columns[i]->type) == COLUMN_TYPE_MODULE) {
            if (table->columns[i]->module_params != type) continue;
            char* formula = (char*)table->columns[i]->module_querry;
            char* output_querry = (char*)malloc(COLUMN_MODULE_SIZE);
            memcpy(output_querry, formula, COLUMN_MODULE_SIZE);

            for (int j = 0; j < table->header->column_count; j++) {
                if (i != j) {
                    uint8_t* content_pointer = data + content_offset;
                    char content_part[table->columns[j]->size];
                    memcpy(content_part, content_pointer, table->columns[j]->size);

                    size_t next_output_querry_size[1];
                    uint8_t* next_output_querry = memrep(
                        (uint8_t*)output_querry, COLUMN_MODULE_SIZE,
                        table->columns[j]->name, strlen((char*)table->columns[j]->name),
                        (uint8_t*)content_part, table->columns[j]->size,
                        next_output_querry_size
                    );
                    
                    free(output_querry);
                    output_querry = (char*)next_output_querry;
                }

                content_offset += table->columns[j]->size;
            }

            uint8_t* module_content_answer = data + module_offset;
            MDL_launch_module((char*)table->columns[i]->module_name, output_querry, module_content_answer, table->columns[i]->size);
        }

        module_offset += table->columns[i]->size;
    }

    return 1;
}