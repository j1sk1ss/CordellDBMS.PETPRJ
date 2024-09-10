#include "../include/file_manager.h"


part_data_t* load_part(char* name) {
    FILE* file = fopen(name, "r");
    uint8_t* data = (uint8_t*)malloc(sizeof(part_data_t));
    
    fgets(data, sizeof(part_data_t), file);
    part_data_t* part = (part_data_t*)data;

    fclose(file);

    return part;
}

void free_part(part_data_t* part) {
    free(part);
}