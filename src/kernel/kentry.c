#include "include/kentry.h"


static database_t* connections[MAX_CONNECTIONS] = { NULL };


kernel_answer_t* kernel_process_command(int argc, char* argv[], unsigned char access, int connection) {
    kernel_answer_t* answer = (kernel_answer_t*)malloc(sizeof(kernel_answer_t));
    memset(answer, 0, sizeof(kernel_answer_t));

    int current_start = 1;
    database_t* database = NULL;
    char* db_name = SAFE_GET_VALUE_POST_INC_S(argv, argc, current_start);

    while (1) {
        if (connections[connection] == NULL) {
            database = DB_load_database(db_name);
            if (database == NULL) {
                print_warn("Database wasn`t found. Create a new one with [create database <name>].");
                current_start = 1;
            }

            connections[connection] = database;
            break;
        }
        else {
            if (strncmp(connections[connection]->header->name, db_name, DATABASE_NAME_SIZE) == 0) {
                database = connections[connection];
                break;
            }
            else {
                DB_free_database(connections[connection]);
                connections[connection] = NULL;
            }
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
        Handle flush command. Init transaction start. Check docs.
        Command syntax: flush
        */
        if (strcmp(command, SYNC) == 0) {
            answer->answer_code = DB_init_transaction(database);
        }
        /*
        Handle rollback command.
        Command syntax: rollback
        */
        else if (strcmp(command, ROLLBACK) == 0) {
            answer->answer_code = DB_rollback(&connections[connection]);
        }
        /*
        Handle info command about cdbms kernel version.
        Command syntax: version
        */
        else if (strcmp(command, VERSION) == 0) {
            answer->answer_body = (unsigned char*)malloc(strlen(KERNEL_VERSION));
            memcpy(answer->answer_body, KERNEL_VERSION, strlen(KERNEL_VERSION));
            answer->answer_size = strlen(KERNEL_VERSION);
        }
        /*
        Handle creation.
        Command syntax: create <option>
        */
#ifndef NO_CREATE_COMMAND
        else if (strcmp(command, CREATE) == 0) {
            /*
            Handle database creation.
            Command syntax: create database <name>
            */
            command_index++;
            if (strcmp(SAFE_GET_VALUE_S(commands, argc, command_index), DATABASE) == 0) {
                char* database_name = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                if (database_name == NULL) return answer;

                database_t* new_database = DB_create_database(database_name);
                int result = DB_save_database(new_database);

                print_log("Database [%.*s.%s] create succes!", DATABASE_NAME_SIZE, new_database->header->name, DATABASE_EXTENSION);
                DB_free_database(new_database);

                answer->answer_code = result;
                answer->answer_size = -1;
            }
            /*
            Handle table creation.
            Command syntax: create table <name> <rwd/same> columns ( name size <int/str/"<module>=args,<mpre/mpost/both>"/any> <is_primary/np> <auto_increment/na> )
            Errors:
            - Return -1 if table already exists in database.
            */
            else if (strcmp(SAFE_GET_VALUE_S(commands, argc, command_index), TABLE) == 0) {
                char* table_name = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                if (table_name == NULL) return answer;

                table_t* table = DB_get_table(database, table_name);
                if (table == NULL) {
                    char* table_access = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                    if (table_access != NULL) {
                        unsigned char access_byte = access;
                        if (strcmp(table_access, ACCESS_SAME) != 0) {
                            access_byte = CREATE_ACCESS_BYTE(
                                (table_access[0] - '0'), (table_access[1] - '0'), (table_access[2] - '0')
                            );

                            if (access_byte < access) {
                                access_byte = access;
                            }
                        }

                        int column_count = 0;
                        table_column_t** columns = NULL;
                        if (strcmp(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), COLUMNS) == 0) {
                            if (*(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index)) == OPEN_BRACKET) {
                                int column_stack_index = 0;
                                char* column_stack[512] = { NULL };
                                while (*(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index)) != CLOSE_BRACKET) {
                                    column_stack[column_stack_index++] = SAFE_GET_VALUE(commands, argc, command_index);
                                }

                                for (int j = 0; j < 512; j += 5) {
                                    if (column_stack[j] == NULL) break;

                                    // Get column data type
                                    unsigned char data_type = COLUMN_TYPE_MODULE;
                                    char* column_data_type = column_stack[j + 2];
                                    if (strcmp(column_data_type, TYPE_INT) == 0) data_type = COLUMN_TYPE_INT;
                                    else if (strcmp(column_data_type, TYPE_ANY) == 0) data_type = COLUMN_TYPE_ANY;
                                    else if (strcmp(column_data_type, TYPE_STRING) == 0) data_type = COLUMN_TYPE_STRING;

                                    // Get column primary status
                                    unsigned char primary_status = COLUMN_NOT_PRIMARY;
                                    char* column_primary = column_stack[j + 3];
                                    if (strcmp(column_primary, PRIMARY) == 0) primary_status = COLUMN_PRIMARY;

                                    // Get column increment status
                                    unsigned char increment_status = COLUMN_NO_AUTO_INC;
                                    char* column_increment = column_stack[j + 4];
                                    if (strcmp(column_increment, AUTO_INC) == 0) increment_status = COLUMN_AUTO_INCREMENT;

                                    columns = (table_column_t**)realloc(columns, sizeof(table_column_t*) * (column_count + 1));
                                    columns[column_count] = TBM_create_column(
                                        CREATE_COLUMN_TYPE_BYTE(primary_status, data_type, increment_status),
                                        atoi(column_stack[j + 1]), column_stack[j]
                                    );

                                    if (data_type == COLUMN_TYPE_MODULE) {
                                        char* equals_pos = strchr(column_data_type, '=');
                                        char* comma_pos = strchr(column_data_type, ',');

                                        columns[column_count]->module_params = COLUMN_MODULE_POSTLOAD;
                                        if (strncmp(comma_pos + 1, MODULE_PRELOAD, strlen(MODULE_PRELOAD)) == 0)
                                            columns[column_count]->module_params = COLUMN_MODULE_PRELOAD;
                                        else if (strncmp(comma_pos + 1, MODULE_BOTH_LOAD, strlen(MODULE_PRELOAD)) == 0)
                                            columns[column_count]->module_params = COLUMN_MODULE_BOTH;

                                        size_t module_name_len = equals_pos - column_data_type;
                                        size_t module_query_len = comma_pos - equals_pos - 1;

                                        strncpy(columns[column_count]->module_name, column_data_type, MIN(module_name_len, MODULE_NAME_SIZE));
                                        if (equals_pos) {
                                            strncpy(columns[column_count]->module_querry, equals_pos + 1, MIN(module_query_len, COLUMN_MODULE_SIZE));
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
                            answer->answer_code = 6;
                            TBM_flush_table(new_table);
                            return answer;
                        }

                        DB_link_table2database(database, new_table);
                        CHC_add_entry(
                            new_table, new_table->header->name, TABLE_CACHE, (void*)TBM_free_table, (void*)TBM_save_table
                        );

                        print_log("Table [%.*s] create success!", TABLE_NAME_SIZE, new_table->header->name);

                        answer->answer_size = -1;
                        answer->answer_code = 1;
                        TBM_flush_table(new_table);
                    }
                }
                else TBM_flush_table(table);
            }
        }
#endif
        /*
        Handle data append.
        Command syntax: append row <table_name> values <data>
        Note: Command don`t care about spacing, data separations, padding and other stuff. This is your work.
        Errors:
        - Return -1 error if table not found.
        - Return -2 error if data size not equals row size.
        */
        else if (strcmp(command, APPEND) == 0) {
            if (strcmp(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), ROW) == 0) {
                char* table_name = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                if (strcmp(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), VALUES) == 0) {
                    char* input_data = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                    int result = DB_append_row(database, table_name, (unsigned char*)input_data, strlen(input_data), access);
                    if (result >= 0) print_log("Row [%s] successfully added to [%.*s] database!", input_data, DATABASE_NAME_SIZE, database->header->name);
                    else {
                        print_error(
                            "Error code: %i, Params: [%.*s] [%s] [%s] [%i]", 
                            result, DATABASE_NAME_SIZE, database->header->name, table_name, input_data, access
                        );
                    }

                    answer->answer_size = -1;
                    answer->answer_code = result;
                }
            }
        }
        /*
        Handle delete command.
        Command syntax: delete <option>
        */
#ifndef NO_DELETE_COMMAND
        else if (strcmp(command, DELETE) == 0) {
            /*
            Command syntax: delete database
            */
            command_index++;
            if (strcmp(SAFE_GET_VALUE_S(commands, argc, command_index), DATABASE) == 0) {
                if (DB_delete_database(database, 1) != 1) print_error("Error code 1 during deleting current database!");
                else print_log("Current database was delete successfully.");

                database = NULL;
                connections[connection] = NULL;

                answer->answer_code = 1;
                answer->answer_size = -1;
            }
            /*
            Command syntax: delete table <name>
            */
            else if (strcmp(SAFE_GET_VALUE_S(commands, argc, command_index), TABLE) == 0) {
                char* table_name = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                if (DB_delete_table(database, table_name, 1) != 1) print_error("Error code 1 during deleting %s", table_name);
                else print_log("Table [%s] was delete successfully.", table_name);

                answer->answer_code = 1;
                answer->answer_size = -1;
            }

            /*
            Command syntax: delete row <table_name> <operation_type> <options>
            */
            else if (strcmp(SAFE_GET_VALUE_S(commands, argc, command_index), ROW) == 0) {
                char* table_name = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);

                /*
                Note: Will delete entire row. (If table has links, it will cause CASCADE operations).
                Command syntax: delete row <table_name> by_index <index>
                */
                command_index++;
                if (strcmp(SAFE_GET_VALUE_S(commands, argc, command_index), BY_INDEX) == 0) {
                    answer->answer_code = DB_delete_row(
                        database, table_name, atoi(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index)), access
                    );
                }
                /*
                Note: will delete all rows, where will find value in provided column.
                Command syntax: delete row <table_name> by_value column <column_name> value <value>
                */
                else if (strcmp(SAFE_GET_VALUE_S(commands, argc, command_index), BY_VALUE) == 0) {
                    if (strcmp(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), COLUMN) == 0) {
                        char* column_name = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);

                        if (strcmp(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), VALUE) == 0) {
                            char* value = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                            while (1) {
                                int index = DB_find_data_row(database, table_name, column_name, 0, (unsigned char*)value, strlen(value), access);
                                if (index == -1) break;
                                answer->answer_code = DB_delete_row(database, table_name, index, access);
                            }
                        }
                    }
                }

                answer->answer_size = -1;
            }
        }
#endif
        /*
        Handle get command.
        Command syntax: get <option>
        */
        else if (strcmp(command, GET) == 0) {
            /*
            Command syntax: get row <table_name> <operation_type> <options>
            */
            if (strcmp(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), ROW) == 0) {
                int index = -1;
                int answer_size = 0;
                unsigned char* answer_data = NULL;
                char* table_name = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);

                /*
                Note: Will get entire row. (If table has links, it will cause CASCADE operations).
                Command syntax: get row <table_name> by_index <index>
                */
                command_index++;
                if (strcmp(SAFE_GET_VALUE_S(commands, argc, command_index), BY_INDEX) == 0) {
                    index = atoi(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index));
                    answer_data = DB_get_row(database, table_name, index, access);
                    if (answer_data == NULL) {
                        print_error(
                            "Something goes wrong! Params: [%.*s] [%s] [%i] [%i]", 
                            DATABASE_NAME_SIZE, database->header->name, table_name, index, access
                        );

                        answer->answer_code = 8;
                        return answer;
                    }

                    table_t* table = DB_get_table(database, table_name);
                    answer_size = table->row_size;
                    TBM_flush_table(table);
                }
                /*
                Note: will get list of rows, where will find value in provided column.
                Command syntax: get row table <table_name> by_value column <column_name> value <value>
                */
#ifndef NO_GET_VALUE_COMMAND
                else if (strcmp(SAFE_GET_VALUE_S(commands, argc, command_index), BY_VALUE) == 0) {
                    if (strcmp(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), COLUMN) == 0) {
                        char* column_name = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                        
                        if (strcmp(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), VALUE) == 0) {
                            char* value = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                            table_t* table = DB_get_table(database, table_name);
                            int offset = 0;

                            while (index != -1) {
                                index = DB_find_data_row(database, table_name, column_name, offset, (unsigned char*)value, strlen(value), access);
                                if (index == -1) break;
                                
                                unsigned char* row_data = DB_get_row(database, table_name, index, access);
                                if (row_data == NULL) {
                                    print_error(
                                        "Something goes wrong! Params: [%.*s] [%s] [%i] [%i]", 
                                        DATABASE_NAME_SIZE, database->header->name, table_name, index, access
                                    );
                                    
                                    return answer;
                                }

                                int data_start = answer_size;
                                answer_size += table->row_size;
                                offset += (index + 1) * table->row_size;
                                answer_data = (unsigned char*)realloc(answer_data, answer_size);

                                memcpy(answer_data + data_start, row_data, table->row_size);
                            }

                            TBM_flush_table(table);
                        }
                    }
                }
#endif
                /*
                Note: will get line of rows, that equals expression.
                Command syntax: get row table <table_name> by_exp column <column_name> <</>/!=> <value>
                */
#ifndef NO_GET_EXPRESSION_COMMAND
                else if (strcmp(SAFE_GET_VALUE_S(commands, argc, command_index), BY_EXPRESSION) == 0) {
                    if (strcmp(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), COLUMN) == 0) {
                        char* column_name = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);

                        if (strcmp(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), VALUE) == 0) {
                            char* expression = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                            char* value = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);

                            table_t* table = DB_get_table(database, table_name);
                            if (table == NULL) {
                                print_error("Table [%s] not found in database [%.*s]", table_name, DATABASE_NAME_SIZE, database->header->name);
                                return answer;
                            }

                            int index = 0;
                            int offset = 0;
                            unsigned char* row_data = NULL;

                            while (*row_data != '\0') {
                                row_data = DB_get_row(database, table_name, index++, access);
                                if (row_data == NULL) break;
                                
                                unsigned char* column_data = row_data + TBM_get_column_offset(table, column_name);
                                int comparison = strcmp((char*)column_data, value);
                                if (strcmp(expression, NEQUALS) == 0) comparison = comparison != 0;
                                else if (strcmp(expression, LESS_THAN) == 0) comparison = comparison < 0;
                                else if (strcmp(expression, MORE_THAN) == 0) comparison = comparison > 0;
                                
                                if (comparison == 1) {
                                    int data_start = answer_size;
                                    answer_size += table->row_size;
                                    answer_data = (unsigned char*)realloc(answer_data, answer_size);
                                    memcpy(answer_data + data_start, row_data, table->row_size);
                                }

                                offset += (index + 1) * table->row_size;
                            }

                            TBM_flush_table(table);
                        }
                    }
                }
#endif

                answer->answer_body = answer_data;
                answer->answer_code = index;
                answer->answer_size = answer_size;
            }
        }
        /*
        Handle update command.
        Command syntax: update <option>
        */
#ifndef NO_UPDATE_COMMAND
        else if (strcmp(command, UPDATE) == 0) {
            /*
            Command syntax: update row <table_name> <option>
            */
            if (strcmp(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), ROW) == 0) {
                int index = -1;
                char* data = NULL;
                char* table_name = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                if (table_name == NULL) {
                    answer->answer_code = 5;
                    return answer;
                }

                /*
                Command syntax: update row <table_name> by_index <index> value <new_data>
                */
                command_index++;
                if (strcmp(SAFE_GET_VALUE_S(commands, argc, command_index), BY_INDEX) == 0) {
                    index = atoi(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index));
                    if (strcmp(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), VALUE) == 0) {
                        data = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                        DB_insert_row(database, table_name, index, (unsigned char*)data, strlen(data), access);
                    }
                }
                /*
                Command syntax: update row <table_name> by_value column <column_name> value <value> values <data>
                */
                else if (strcmp(SAFE_GET_VALUE_S(commands, argc, command_index), BY_VALUE) == 0) {
                    if (strcmp(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), COLUMN) == 0) {
                        char* col_name  = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                        
                        if (strcmp(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), VALUE) == 0) {
                            char* value = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);

                            if (strcmp(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), VALUES) == 0) {
                                data = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                                index = DB_find_data_row(database, table_name, col_name, 0, (unsigned char*)value, strlen(value), access);
                                if (index == -1) {
                                    print_error("Value [%s] not presented in table [%s]", data, table_name);
                                    return answer;
                                }
                            }
                        }
                    }
                }
                
                answer->answer_size = -1;
                answer->answer_code = DB_insert_row(database, table_name, index, (unsigned char*)data, strlen(data), access);
            }
        }
#endif
    }

    return answer;
}

static int _flush_tables() {
    CHC_free();
    return 1;
}

int close_connection(int connection) {
    _flush_tables();
    if (connections[connection] == NULL) return -2;
    DB_free_database(connections[connection]);
    connections[connection] = NULL;
    return 1;
}

int kernel_free_answer(kernel_answer_t* answer) {
    if (answer->answer_body != NULL) free(answer->answer_body);
    free(answer);
    return 1;
}

void cleanup_kernel() {
    _flush_tables();
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (connections[i] == NULL) continue;
        DB_free_database(connections[i]);
    }
}
