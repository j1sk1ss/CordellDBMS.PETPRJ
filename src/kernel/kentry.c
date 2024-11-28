#include "include/kentry.h"


static database_t* database = NULL;


#ifdef DEBUG
int main(int argc, char* argv[]) { return 1; }
#endif

commands_t* process_command(char* buffer) {
    commands_t* commands = (commands_t*)malloc(sizeof(commands_t));
    commands->argv = (char**)malloc(sizeof(char*) * MAX_COMMANDS);
    commands->argc = 1;

    char* current_arg = NULL;
    int in_quotes = 0;

    for (char *p = (char*)buffer; *p != '\0'; p++) {
        if (*p == '"') {
            in_quotes = !in_quotes;
            if (in_quotes) current_arg = p + 1;
            else {
                *p = '\0';
                commands->argv[commands->argc++] = current_arg;
                current_arg  = NULL;
            }
        }
        else if (!in_quotes && (*p == ' ' || *p == '\t')) {
            *p = '\0';
            if (current_arg) {
                commands->argv[commands->argc++] = current_arg;
                current_arg  = NULL;
            }
        }
        else if (!current_arg) current_arg = p;
    }

    if (current_arg) commands->argv[commands->argc++] = current_arg;
    return commands;
}

int kernel(char* querry) {
    commands_t* input_commands = process_command(querry);
    char** argv = input_commands->argv;
    int argc = input_commands->argc;

    int current_start = 1;
    char* db_name = SAFE_GET_VALUE_POST_INC_S(argv, argc, current_start);

    if (database == NULL) {
        database = DB_load_database(NULL, db_name);
        if (database == NULL) {
            print_warn("Database wasn`t found. Create a new one with [create database <name>].");
            current_start = 1;
        }
    }

    /*
    Save commands into RAM.
    */
    char* commands[MAX_COMMANDS] = { NULL };
    for (int i = current_start; i < argc; i++) {
        commands[i - current_start] = argv[i];
    }

    /*
    Handle command.
    */
    for (int i = 0; i < MAX_COMMANDS; i++) {
        if (commands[i] == NULL) break;
        char* command = commands[i];
        int command_index = i;

        /*
        Handle creation.
        Command syntax: create <option>
        */
        else if (strcmp_s(command, CREATE) == 0) {
            /*
            Handle database creation.
            Command syntax: create database <name>
            */
            command_index++;
            if (strcmp_s(SAFE_GET_VALUE_S(commands, argc, command_index), DATABASE) == 0) {
                char* database_name = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                if (database_name == NULL) return -1;

                database_t* new_database = DB_create_database(database_name);
                int result = DB_save_database(new_database, NULL);

                print_log("Database [%.*s.%s] create succes!", DATABASE_NAME_SIZE, new_database->header->name, DATABASE_EXTENSION);
                DB_free_database(new_database);
                return result;
            }
            /*
            Handle table creation.
            Command syntax: create table <name> <rwd/same> columns ( name size <int/str/"<module>=args,<mpre/mpost/both>"/any> <is_primary/np> <auto_increment/na> )
            Errors:
            - Return -1 if table already exists in database.
            */
            else if (strcmp_s(SAFE_GET_VALUE_S(commands, argc, command_index), TABLE) == 0) {
                char* table_name = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                if (table_name == NULL) return -1;

                if (DB_get_table(database, table_name) != NULL) return -1;
                char* table_access = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                if (table_access == NULL) {
                    return 5;
                }

                unsigned char access_byte = access;
                if (strcmp(table_access, ACCESS_SAME) != 0) {
                    unsigned char rd  = table_access[0] - '0';
                    unsigned char wr  = table_access[1] - '0';
                    unsigned char del = table_access[2] - '0';
                    access_byte = CREATE_ACCESS_BYTE(rd, wr, del);
                    if (access_byte < access) {
                        return 6;
                    }
                }

                        int column_count = 0;
                        table_column_t** columns = NULL;
                        if (strcmp_s(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), COLUMNS) == 0) {
                            if (strcmp_s(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), OPEN_BRACKET) == 0) {
                                int column_stack_index = 0;
                                char* column_stack[256] = { NULL };
                                while (strcmp_s(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), CLOSE_BRACKET) != 0) {
                                    column_stack[column_stack_index++] = SAFE_GET_VALUE(commands, argc, command_index);
                                }

                                for (int j = 0; j < 256; j += 5) {
                                    if (column_stack[j] == NULL) break;

                                    // Get column data type
                                    unsigned char data_type = COLUMN_TYPE_MODULE;
                                    char* column_data_type = column_stack[j + 2];
                                    if (strcmp_s(column_data_type, TYPE_INT) == 0) data_type = COLUMN_TYPE_INT;
                                    else if (strcmp_s(column_data_type, TYPE_ANY) == 0) data_type = COLUMN_TYPE_ANY;
                                    else if (strcmp_s(column_data_type, TYPE_STRING) == 0) data_type = COLUMN_TYPE_STRING;

                                    // Get column primary status
                                    unsigned char primary_status = COLUMN_NOT_PRIMARY;
                                    char* column_primary = column_stack[j + 3];
                                    if (strcmp_s(column_primary, PRIMARY) == 0) primary_status = COLUMN_PRIMARY;

                                    // Get column increment status
                                    unsigned char increment_status = COLUMN_NO_AUTO_INC;
                                    char* column_increment = column_stack[j + 4];
                                    if (strcmp_s(column_increment, AUTO_INC) == 0) increment_status = COLUMN_AUTO_INCREMENT;

                                    columns = (table_column_t**)realloc(columns, sizeof(table_column_t*) * (column_count + 1));
                                    columns[column_count] = TBM_create_column(
                                        CREATE_COLUMN_TYPE_BYTE(primary_status, data_type, increment_status),
                                        atoi(column_stack[j + 1]), column_stack[j]
                                    );

                                    if (data_type == COLUMN_TYPE_MODULE) {
                                        char* equals_pos = strchr_s(column_data_type, '=');
                                        char* comma_pos = strchr_s(column_data_type, ',');

                                        columns[column_count]->module_params = COLUMN_MODULE_POSTLOAD;
                                        if (strncmp_s(comma_pos + 1, MODULE_PRELOAD, strlen_s(MODULE_PRELOAD)) == 0)
                                            columns[column_count]->module_params = COLUMN_MODULE_PRELOAD;
                                        else if (strncmp_s(comma_pos + 1, MODULE_BOTH_LOAD, strlen_s(MODULE_PRELOAD)) == 0)
                                            columns[column_count]->module_params = COLUMN_MODULE_BOTH;

                                        size_t module_name_len = equals_pos - column_data_type;
                                        size_t module_query_len = comma_pos - equals_pos - 1;

                                        memset_s(columns[column_count]->module_name, 0, MODULE_NAME_SIZE);
                                        strncpy_s(columns[column_count]->module_name, column_data_type, MIN(module_name_len, MODULE_NAME_SIZE));
                                        if (equals_pos != NULL) {
                                            strncpy_s(columns[column_count]->module_querry, equals_pos + 1, module_query_len);
                                            print_log(
                                                "Module [%.*s] registered with [%s] querry", 
                                                MODULE_NAME_SIZE, columns[column_count]->module_name, columns[column_count]->module_querry
                                            );
                                        }
                                    }

                                    print_log("%i) Column [%s] with args: [%i], created success!", column_count, column_stack[j], columns[column_count]->type);
                                    column_count++;
                                }
                            }
                        }

                table_t* new_table = TBM_create_table(table_name, columns, column_count, access_byte);
                if (new_table == NULL) {
                    return 6;
                }

                        DB_link_table2database(database, new_table);
                        CHC_add_entry(new_table, new_table->header->name, TABLE_CACHE, TBM_free_table, TBM_save_table);

                print_log("Table [%.*s] create success!", TABLE_NAME_SIZE, new_table->header->name);
                return 1;
            }
        }
        /*
        Handle data append.
        Command syntax: append row <table_name> values <data>
        Note: Command don`t care about spacing, data separations, padding and other stuff. This is your work.
        Errors:
        - Return -1 error if table not found.
        - Return -2 error if data size not equals row size.
        */
        else if (strcmp_s(command, APPEND) == 0) {
            if (strcmp_s(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), ROW) == 0) {
                char* table_name = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);

                if (strcmp_s(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), VALUES) == 0) {
                    char* input_data = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                    if (input_data == NULL || table_name == NULL) {
                        return 5;
                    }

                    int result = DB_append_row(database, table_name, (unsigned char*)input_data, strlen_s(input_data), access);
                    if (result >= 0) print_log("Row [%s] successfully added to [%.*s] database!", input_data, DATABASE_NAME_SIZE, database->header->name);
                    else {
                        print_error(
                            "Error code: %i, Params: [%.*s] [%s] [%s] [%i]", 
                            result, DATABASE_NAME_SIZE, database->header->name, table_name, input_data, access
                        );
                    }
                    
                    return result;
                }
            }
        }
        /*
        Handle delete command.
        Command syntax: delete <option>
        */
        else if (strcmp_s(command, DELETE) == 0) {
            /*
            Command syntax: delete database <name>
            */
            command_index++;
            if (strcmp_s(SAFE_GET_VALUE_S(commands, argc, command_index), DATABASE) == 0) {
                char* database_name = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                if (database_name == NULL) {
                    return 5;
                }

                if (DB_delete_database(database, 1) != 1) print_error("Error code 1 during deleting %s", database_name);
                else print_log("Database [%s] was delete successfully.", database_name);

                database = NULL;
                return 1;
            }
            /*
            Command syntax: delete table <name>
            */
            else if (strcmp_s(SAFE_GET_VALUE_S(commands, argc, command_index), TABLE) == 0) {
                char* table_name = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                if (table_name == NULL) {
                    return 5;
                }

                if (DB_delete_table(database, table_name, 1) != 1) print_error("Error code 1 during deleting %s", table_name);
                else print_log("Table [%s] was delete successfully.", table_name);
                return 1;
            }

            /*
            Command syntax: delete row <table_name> <operation_type> <options>
            */
            else if (strcmp_s(SAFE_GET_VALUE_S(commands, argc, command_index), ROW) == 0) {
                int index = -1;
                char* table_name = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);

                /*
                Note: Will delete entire row. (If table has links, it will cause CASCADE operations).
                Command syntax: delete row <table_name> by_index <index>
                */
                command_index++;
                if (strcmp_s(SAFE_GET_VALUE_S(commands, argc, command_index), BY_INDEX) == 0) {
                    index = atoi(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index));
                }
                /*
                Note: will delete first row, where will find value in provided column.
                Command syntax: delete row <table_name> by_value column <column_name> value <value>
                */
                else if (strcmp_s(SAFE_GET_VALUE_S(commands, argc, command_index), BY_VALUE) == 0) {
                    if (strcmp_s(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), COLUMN) == 0) {
                        char* column_name = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);

                        if (strcmp_s(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), VALUE) == 0) {
                            char* value = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                            if (value == NULL || column_name == NULL || table_name == NULL) {
                                return 5;
                            }

                            index = DB_find_data_row(database, table_name, column_name, 0, (unsigned char*)value, strlen_s(value), access);
                            if (index == -1) {
                                return 2;
                            }
                        }
                    }
                }

                return DB_delete_row(database, table_name, index, access);
            }
        }
        /*
        Handle get command.
        Command syntax: get <option>
        */
        else if (strcmp_s(command, GET) == 0) {
            /*
            Command syntax: get row <table_name> <operation_type> <options>
            */
            if (strcmp_s(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), ROW) == 0) {
                int index = -1;
                int answer_size = 0;
                unsigned char* answer_data = NULL;

                char* table_name = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                if (table_name == NULL) {
                    return 5;
                }

                /*
                Note: Will get entire row. (If table has links, it will cause CASCADE operations).
                Command syntax: get row <table_name> by_index <index>
                */
                command_index++;
                if (strcmp_s(SAFE_GET_VALUE_S(commands, argc, command_index), BY_INDEX) == 0) {
                    index = atoi(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index));
                    if (index == -1) {
                        return 7;
                    }

                    answer_data = DB_get_row(database, table_name, index, access);
                    if (answer_data == NULL) {
                        print_error(
                            "Something goes wrong! Params: [%.*s] [%s] [%i] [%i]", 
                            DATABASE_NAME_SIZE, database->header->name, table_name, index, access
                        );

                        return 8;
                    }

                    table_t* table = DB_get_table(database, table_name);
                    answer_size = table->row_size;
                    TBM_flush_table(table);
                }
                /*
                Note: will get first row, where will find value in provided column.
                Command syntax: get row table <table_name> by_value column <column_name> value <value>
                */
                else if (strcmp_s(SAFE_GET_VALUE_S(commands, argc, command_index), BY_VALUE) == 0) {
                    if (strcmp_s(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), COLUMN) == 0) {
                        char* column_name = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);

                        if (strcmp_s(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), VALUE) == 0) {
                            char* value = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                            if (value == NULL || column_name == NULL) {
                                return 5;
                            }

                            table_t* table = DB_get_table(database, table_name);
                            int offset = 0;

                            while (index != -1) {
                                index = DB_find_data_row(database, table_name, column_name, offset, (unsigned char*)value, strlen_s(value), access);
                                if (index == -1) break;
                                
                                unsigned char* row_data = DB_get_row(database, table_name, index, access);
                                if (row_data == NULL) {
                                    print_error(
                                        "Something goes wrong! Params: [%.*s] [%s] [%i] [%i]", 
                                        DATABASE_NAME_SIZE, database->header->name, table_name, index, access
                                    );
                                    
                                    return -1;
                                }

                                int data_start = answer_size;
                                answer_size += table->row_size;
                                offset += (index + 1) * table->row_size;

                                answer_data = (unsigned char*)realloc(answer_data, answer_size);
                                unsigned char* copy_pointer = answer_data + data_start;

                                memcpy_s(copy_pointer, row_data, table->row_size);
                            }

                            TBM_flush_table(table);
                        }
                    }
                }

                print_info("%.*s", answer_size, answer_data);
                return 1;
            }
        }
        /*
        Handle update command.
        Command syntax: update <option>
        */
        else if (strcmp_s(command, UPDATE) == 0) {
            /*
            Command syntax: update row <table_name> <option>
            */
            if (strcmp_s(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), ROW) == 0) {
                int index = -1;
                char* data = NULL;
                char* table_name = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                if (table_name == NULL) {
                    return 5;
                }

                /*
                Command syntax: update row <table_name> by_index <index> value <new_data>
                */
                command_index++;
                if (strcmp_s(SAFE_GET_VALUE_S(commands, argc, command_index), BY_INDEX) == 0) {
                    index = atoi(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index));
                    if (strcmp_s(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), VALUE) == 0) {
                        data = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                        if (data == NULL) {
                            return 5;
                        }
                    }
                }
                /*
                Command syntax: update row <table_name> by_value column <column_name> value <value> values <data>
                */
                else if (strcmp_s(SAFE_GET_VALUE_S(commands, argc, command_index), BY_VALUE) == 0) {
                    if (strcmp_s(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), COLUMN) == 0) {
                        char* col_name  = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);

                        if (strcmp_s(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), VALUE) == 0) {
                            char* value = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);

                            if (strcmp_s(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), VALUES) == 0) {
                                data = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                                if (data == NULL || value == NULL || col_name == NULL) {
                                    return 5;
                                }

                                index = DB_find_data_row(database, table_name, col_name, 0, (unsigned char*)value, strlen_s(value), access);
                                if (index == -1) {
                                    print_error("Value [%s] not presented in table [%s]", data, table_name);
                                    return -1;
                                }
                            }
                        }
                    }
                }
                
                return DB_insert_row(database, table_name, index, (unsigned char*)data, strlen(data), access);
            }

        }
    }

    return -1;
}
