#include "include/kentry.h"


static database_t* _connections[MAX_CONNECTIONS] = { NULL };


static table_t* _get_table(database_t* database, const char* table_name) {
    table_t* table = DB_get_table(database, table_name);
    if (!table) print_error("Table [%s] not found in database [%.*s]", table_name, DATABASE_NAME_SIZE, database->header->name);
    return table;
}

static int _compare_data(char* expression, char* fdata, char* sdata) {
    int comparison = 0;
    if (strcmp(expression, STR_EQUALS) == 0) comparison = (strcmp((char*)fdata, sdata) == 0);
    else if (strcmp(expression, STR_NEQUALS) == 0) comparison = (strcmp((char*)fdata, sdata) != 0);
    else {
        int first = atoi(fdata);
        int second = atoi(sdata);
        if (strcmp(expression, NEQUALS) == 0) comparison = first != second;
        else if (strcmp(expression, EQUALS) == 0) comparison = first == second;
        else if (strcmp(expression, LESS_THAN) == 0) comparison = first < second;
        else if (strcmp(expression, MORE_THAN) == 0) comparison = first > second;
    }

    return comparison;
}


kernel_answer_t* kernel_process_command(int argc, char* argv[], unsigned char access, int connection) {
    kernel_answer_t* answer = (kernel_answer_t*)malloc(sizeof(kernel_answer_t));
    if (!answer) return NULL;
    
    memset(answer, 0, sizeof(kernel_answer_t));

    int current_start = 1;
    database_t* database = NULL;
    char* db_name = SAFE_GET_VALUE_POST_INC_S(argv, argc, current_start);

    while (1) {
        if (_connections[connection] == NULL) {
            database = DB_load_database(db_name);
            if (database == NULL) {
                print_warn("Database wasn`t found. Create a new one with [create database <name>].");
                current_start = 1;
            }

            _connections[connection] = database;
            break;
        }
        else {
            if (strncmp(_connections[connection]->header->name, db_name, DATABASE_NAME_SIZE) == 0) {
                database = _connections[connection];
                break;
            }
            else {
                DB_free_database(_connections[connection]);
                _connections[connection] = NULL;
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
            answer->answer_code = DB_rollback(&_connections[connection]);
        }
        /*
        Handle info command about cdbms kernel version.
        Command syntax: version
        */
#ifndef NO_VERSION_COMMAND
        else if (strcmp(command, VERSION) == 0) {
            answer->answer_body = (unsigned char*)malloc(strlen(KERNEL_VERSION));
            if (!answer->answer_body) return answer;
            memcpy(answer->answer_body, KERNEL_VERSION, strlen(KERNEL_VERSION));
            answer->answer_size = strlen(KERNEL_VERSION);
        }
#endif
        /*
        Handle migration.
        Command syntax: migrate <src_table_name> <dst_table_name> nav ( ... )
        */
#ifndef NO_MIGRATE_COMMAND
        else if (strcmp(command, MIGRATE) == 0) {
            char* src_table_name = SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index);
            char* dst_table_name = SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index);
            if (strcmp(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), NAV) == 0) {
                if (*(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index)) == OPEN_BRACKET) {
                    int nav_stack_index = 0;
                    int nav_stack[128] = { 0 };
                    while (*(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index)) != CLOSE_BRACKET) {
                        nav_stack[nav_stack_index++] = atoi(SAFE_GET_VALUE_S(commands, argc, command_index));
                    }

                    table_t* src_table = _get_table(database, src_table_name);
                    table_t* dst_table = _get_table(database, dst_table_name);
                    if (!src_table || !dst_table) return answer;
                    TBM_migrate_table(src_table, dst_table, nav_stack, nav_stack_index);

                    TBM_flush_table(src_table);
                    TBM_flush_table(dst_table);
                }
            }
        }
#endif
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
                if (!database_name) return answer;

                database_t* new_database = DB_create_database(database_name);
                if (!new_database) return answer;
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

                table_t* table = _get_table(database, table_name);
                if (table) {
                    TBM_flush_table(table);
                    return answer;
                }

                char* table_access = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                if (!table_access) return answer;
                
                unsigned char access_byte = access;
                if (strcmp(table_access, ACCESS_SAME) != 0) {
                    access_byte = CREATE_ACCESS_BYTE((table_access[0] - '0'), (table_access[1] - '0'), (table_access[2] - '0'));
                    access_byte = (access_byte < access) ? access : access_byte;
                }

                int column_count = 0;
                table_column_t** columns = NULL;
                if (strcmp(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), COLUMNS) == 0) {
                    if (*(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index)) == OPEN_BRACKET) {
                        char* column_stack[512] = { NULL };
                        while (*(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index)) != CLOSE_BRACKET) {
                            column_stack[column_count++] = SAFE_GET_VALUE(commands, argc, command_index);
                        }

                        columns = (table_column_t**)malloc(column_count * sizeof(table_column_t*));
                        if (!columns) return answer;

                        for (int j = 0, k = 0; j < 512; j += 5, k++) {
                            if (column_stack[j] == NULL) break;

                            char* column_data_type = column_stack[j + 2];
                            char* column_primary   = column_stack[j + 3];
                            char* column_increment = column_stack[j + 4];

                            // Get column data type
                            unsigned char data_type = COLUMN_TYPE_MODULE;
                            if (strcmp(column_data_type, TYPE_INT) == 0) data_type = COLUMN_TYPE_INT;
                            else if (strcmp(column_data_type, TYPE_ANY) == 0) data_type = COLUMN_TYPE_ANY;
                            else if (strcmp(column_data_type, TYPE_STRING) == 0) data_type = COLUMN_TYPE_STRING;

                            // Get column primary status
                            unsigned char primary_status = COLUMN_NOT_PRIMARY;
                            if (strcmp(column_primary, PRIMARY) == 0) primary_status = COLUMN_PRIMARY;

                            // Get column increment status
                            unsigned char increment_status = COLUMN_NO_AUTO_INC;
                            if (strcmp(column_increment, AUTO_INC) == 0) increment_status = COLUMN_AUTO_INCREMENT;

                            columns[k] = TBM_create_column(
                                CREATE_COLUMN_TYPE_BYTE(primary_status, data_type, increment_status), atoi(column_stack[j + 1]), column_stack[j]
                            );

                            if (data_type == COLUMN_TYPE_MODULE) {
                                char* equals_pos = strchr(column_data_type, '=');
                                char* comma_pos  = strchr(column_data_type, ',');

                                columns[k]->module_params = COLUMN_MODULE_POSTLOAD;
                                if (strncmp(comma_pos + 1, MODULE_PRELOAD, strlen(MODULE_PRELOAD)) == 0) columns[k]->module_params = COLUMN_MODULE_PRELOAD;
                                else if (strncmp(comma_pos + 1, MODULE_BOTH_LOAD, strlen(MODULE_PRELOAD)) == 0) columns[k]->module_params = COLUMN_MODULE_BOTH;

                                if (equals_pos) {
                                    strncpy(columns[k]->module_name, column_data_type, MIN(equals_pos - column_data_type, MODULE_NAME_SIZE));
                                    strncpy(columns[k]->module_querry, equals_pos + 1, MIN(comma_pos - equals_pos - 1, COLUMN_MODULE_SIZE));

                                    print_log(
                                        "Module [%.*s] registered with [%s] querry", MODULE_NAME_SIZE, columns[k]->module_name, columns[column_count]->module_querry
                                    );
                                }
                            }

                            print_log("%i) Column [%s] with args: [%i], created success!", k, column_stack[j], columns[k]->type);
                        }
                    }
                }

                table_t* new_table = TBM_create_table(table_name, columns, column_count, access_byte);
                if (!new_table) {
                    answer->answer_code = 6;
                    ARRAY_SOFT_FREE(columns, column_count);
                    return answer;
                }

                DB_link_table2database(database, new_table);
                CHC_add_entry(new_table, new_table->header->name, TABLE_CACHE, (void*)TBM_free_table, (void*)TBM_save_table);
                print_log("Table [%.*s] create success!", TABLE_NAME_SIZE, new_table->header->name);

                answer->answer_size = -1;
                answer->answer_code = 1;
                TBM_flush_table(new_table);
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
                        print_error("Error code: %i, Params: [%.*s] [%s] [%s] [%i]", result, DATABASE_NAME_SIZE, database->header->name, table_name, input_data, access);
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
                else { print_log("Current database was delete successfully."); }

                database = NULL;
                _connections[connection] = NULL;

                answer->answer_code = 1;
                answer->answer_size = -1;
            }
            /*
            Command syntax: delete table <name>
            */
            else if (strcmp(SAFE_GET_VALUE_S(commands, argc, command_index), TABLE) == 0) {
                char* table_name = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                if (DB_delete_table(database, table_name, 1) != 1) print_error("Error code 1 during deleting %s", table_name);
                else { print_log("Table [%s] was delete successfully.", table_name); }

                answer->answer_code = 1;
                answer->answer_size = -1;
            }

            /*
            Command syntax: delete row <table_name> <operation_type> <options>
            */
            else if (strcmp(SAFE_GET_VALUE_S(commands, argc, command_index), ROW) == 0) {
                char* table_name = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);

                /*
                Note: Will delete entire row.
                Command syntax: delete row <table_name> by_index <index>
                */
                command_index++;
                if (strcmp(SAFE_GET_VALUE_S(commands, argc, command_index), BY_INDEX) == 0) {
                    answer->answer_code = DB_delete_row(database, table_name, atoi(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index)), access);
                }
                /*
                Note: will delete all rows, where will find value in provided column.
                Command syntax: delete row <table_name> by_exp column <column_name> <</>/!=/=/eq/neq> <value>
                */
                else if (strcmp(SAFE_GET_VALUE_S(commands, argc, command_index), BY_EXPRESSION) == 0) {
                    if (strcmp(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), COLUMN) == 0) {
                        table_t* table = _get_table(database, table_name);
                        if (!table) return answer;
                        
                        char* column_name = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                        char* expression = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                        char* value = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);

                        int index = 0;
                        unsigned char* row_data = (unsigned char*)" ";
                        while (*row_data != '\0') {
                            row_data = DB_get_row(database, table_name, index++, access);
                            if (!row_data) break;

                            unsigned char* column_data = row_data + TBM_get_column_offset(table, column_name);
                            if (_compare_data(expression, value, column_data)) {
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
                Note: Will get entire row.
                Command syntax: get row <table_name> by_index <index>
                */
                command_index++;
                if (strcmp(SAFE_GET_VALUE_S(commands, argc, command_index), BY_INDEX) == 0) {
                    index = atoi(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index));
                    answer_data = DB_get_row(database, table_name, index, access);
                    if (answer_data == NULL) {
                        print_error("Something goes wrong! Params: [%.*s] [%s] [%i] [%i]", DATABASE_NAME_SIZE, database->header->name, table_name, index, access);
                        answer->answer_code = 8;
                        return answer;
                    }

                    table_t* table = _get_table(database, table_name);
                    answer_size = table->row_size;
                    TBM_flush_table(table);
                }
                /*
                Note: will get line of rows, that equals expression.
                Command syntax: get row table <table_name> by_exp column <column_name> <</>/!=/=/eq/neq> <value>
                */
#ifndef NO_GET_EXPRESSION_COMMAND
                else if (strcmp(SAFE_GET_VALUE_S(commands, argc, command_index), BY_EXPRESSION) == 0) {
                    if (strcmp(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), COLUMN) == 0) {  // TODO: Incapsulate
                        char* column_name = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                        char* expression  = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                        char* value       = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);

                        table_t* table = _get_table(database, table_name);
                        if (!table) return answer;

                        int index = 0;
                        int offset = 0;
                        unsigned char* row_data = (unsigned char*)" ";

                        while (*row_data != '\0') {
                            row_data = DB_get_row(database, table_name, index++, access);
                            if (row_data == NULL) break;
                            
                            unsigned char* column_data = row_data + TBM_get_column_offset(table, column_name);
                            if (_compare_data(expression, value, column_data)) {
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
                Command syntax: update row <table_name> by_exp column <column_name> <</>/!=/=/eq/neq> <value> values <data>
                */
                else if (strcmp(SAFE_GET_VALUE_S(commands, argc, command_index), BY_EXPRESSION) == 0) {
                    if (strcmp(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), COLUMN) == 0) {
                        table_t* table = _get_table(database, table_name);
                        if (!table) return answer;
                        
                        char* column_name = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                        char* expression = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                        char* value = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                        
                        if (strcmp(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), VALUES) == 0) {
                            data = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                            unsigned char* row_data = (unsigned char*)" ";
                            while (*row_data != '\0') {
                                row_data = DB_get_row(database, table_name, index, access);
                                if (!row_data) break;

                                unsigned char* column_data = row_data + TBM_get_column_offset(table, column_name);
                                if (_compare_data(expression, value, column_data)) {
                                    answer->answer_code = DB_insert_row(database, table_name, index, (unsigned char*)data, strlen(data), access);
                                }

                                index++;
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
    if (_connections[connection] == NULL) return -2;
    DB_free_database(_connections[connection]);
    _connections[connection] = NULL;
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
        if (_connections[i] == NULL) continue;
        DB_free_database(_connections[i]);
    }
}
