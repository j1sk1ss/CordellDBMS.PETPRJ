#include "include/kentry.h"


static database_t* _connections[MAX_CONNECTIONS] = { NULL };


#pragma region [Private]

    static inline int _flush_tables() {
        CHC_free();
        return 1;
    }

    static table_t* _get_table(database_t* database, char* table_name) {
        table_t* table = DB_get_table(database, table_name);
        if (!table) { print_error("Table [%s] not found in database [%.*s]", table_name, DATABASE_NAME_SIZE, database->header->name); }
        return table;
    }

    static int _compare_data(char* expression, char* fdata, size_t fdata_size, char* sdata, size_t sdata_size) {
        char* temp_fdata = (char*)malloc(fdata_size + 1);
        if (!temp_fdata) return 0;

        memcpy(temp_fdata, fdata, fdata_size);
        temp_fdata[fdata_size] = '\0';
        char* mv_fdata = temp_fdata + strspn(temp_fdata, " ");
        
        char* temp_sdata = (char*)malloc(sdata_size + 1);
        if (!temp_sdata) return 0;

        memcpy(temp_sdata, sdata, sdata_size);
        temp_sdata[sdata_size] = '\0';
        char* mv_sdata = temp_sdata + strspn(temp_sdata, " ");

        int comparison = 0;
        if (strcmp(expression, STR_EQUALS) == 0) comparison = strcmp(mv_fdata, mv_sdata) == 0;
        else if (strcmp(expression, STR_NEQUALS) == 0) comparison = strcmp(mv_fdata, mv_sdata) != 0;
        else {
            int first = atoi(mv_fdata);
            int second = atoi(mv_sdata);
            if (strcmp(expression, NEQUALS) == 0) comparison = first != second;
            else if (strcmp(expression, EQUALS) == 0) comparison = first == second;
            else if (strcmp(expression, LESS_THAN) == 0) comparison = first < second;
            else if (strcmp(expression, MORE_THAN) == 0) comparison = first > second;
        }

        free(temp_fdata);
        free(temp_sdata);
        return comparison;
    }

    static int _create_expression(table_t* table, char* commands[], int current_command, int argc, expression_t* expression) {
        expression->condition_count = 0;
        expression->operator_count = 0;
        expression->limit = -1;
        expression->offset = 0;

        while (1) {
            char* operator = SAFE_GET_VALUE_PRE_INC(commands, argc, current_command);
            if (!operator) break;
            if (strcmp(operator, COLUMN) == 0) {
                TBM_get_column_info(table, SAFE_GET_VALUE_PRE_INC(commands, argc, current_command), &expression->conditions[expression->condition_count].col_info);
                expression->conditions[expression->condition_count].expression = SAFE_GET_VALUE_PRE_INC(commands, argc, current_command);
                expression->conditions[expression->condition_count].value = SAFE_GET_VALUE_PRE_INC(commands, argc, current_command);
                expression->condition_count++;
            } 
            else if (strcmp(operator, OR) == 0 || strcmp(operator, AND) == 0) {
                expression->operators[expression->operator_count++] = operator;
            } 
            else if (strcmp(operator, OFFSET) == 0) {
                expression->offset = atoi(SAFE_GET_VALUE_PRE_INC_S(commands, argc, current_command));
            } 
            else if (strcmp(operator, LIMIT) == 0) {
                expression->limit = atoi(SAFE_GET_VALUE_PRE_INC_S(commands, argc, current_command));
            }
            else break;
        }  
        
        return 1;
    }

    static int _evaluate_expression(unsigned char* row_data, expression_t* expression) {
        int results[MAX_STATEMENTS] = { 0 };
        #pragma omp parallel for schedule(dynamic, 2)
        for (int i = 0; i < expression->condition_count; i++) {
            results[i] = _compare_data(
                expression->conditions[i].expression, (char*)(row_data + expression->conditions[i].col_info.offset), 
                expression->conditions[i].col_info.size, expression->conditions[i].value, strlen(expression->conditions[i].value)
            );
        }

        int match = results[0];
        for (int i = 0; i < expression->operator_count; i++) {
            if (strcmp(expression->operators[i], AND) == 0) match &= results[i + 1];
            else if (strcmp(expression->operators[i], OR) == 0) match |= results[i + 1];
        }

        return match;
    }

    static int __delete_logic(
        database_t* database, char* table_name, int index, unsigned char* data, 
        size_t data_size, unsigned char access, kernel_answer_t* answer
    ) {
        answer->answer_code = DB_insert_row(database, table_name, index, data, data_size, access);
        return 1;
    }

    static int __insert_logic(
        database_t* database, char* table_name, int index, unsigned char* data,
        size_t data_size, unsigned char access, kernel_answer_t* answer
    ) {
        answer->answer_code = DB_delete_row(database, table_name, index, access);
        return 1;
    }

    static int __get_logic(
        database_t* database, char* table_name, int index, unsigned char* data, 
        size_t data_size, unsigned char access, kernel_answer_t* answer
    ) {
        int data_start = answer->answer_size;
        answer->answer_size += data_size;
        answer->answer_body = (unsigned char*)realloc(answer->answer_body, answer->answer_size);
        memcpy(answer->answer_body + data_start, data, data_size);
        return 1;
    }

    static int _process_table(
        database_t* database, table_t* table, kernel_answer_t* answer, expression_t* exp, unsigned char access, 
        int (*logic)(database_t*, char*, int, unsigned char*, size_t, unsigned char, kernel_answer_t*)
    ) {
        int index = exp->offset;
        int processed_rows = 0;
        while (1) {
            unsigned char* row_data = (unsigned char*)malloc(table->row_size);
            if (!row_data) return -1;

            int get_result = DB_get_row(database, table->header->name, index, access, row_data, table->row_size);
            if (!get_result) {
                free(row_data);
                break;
            }
            
            if (*row_data != PAGE_EMPTY) {
                if (_evaluate_expression(row_data, exp)) {
                    if (exp->limit != -1 && processed_rows++ >= exp->limit) {
                        free(row_data);
                        break;
                    }
                    
                    logic(database, table->header->name, index, row_data, table->row_size, access, answer);
                }
            }
            
            index++;
            free(row_data);
        }

        return 1;
    }

#pragma endregion


kernel_answer_t* kernel_process_command(int argc, char* argv[], unsigned char access, int connection) {
    kernel_answer_t* answer = (kernel_answer_t*)malloc(sizeof(kernel_answer_t));
    if (!answer) return NULL;
    memset(answer, 0, sizeof(kernel_answer_t));

    int current_start = 1;
    char* db_name = SAFE_GET_VALUE_POST_INC_S(argv, argc, current_start);
    while (1) {
        if (_connections[connection] == NULL) {
            _connections[connection] = DB_load_database(db_name);
            if (_connections[connection] == NULL) current_start = 1;
            break;
        }
        else {
            if (strncmp(_connections[connection]->header->name, db_name, DATABASE_NAME_SIZE) == 0) break;
            else {
                DB_free_database(_connections[connection]);
                _connections[connection] = NULL;
            }
        }
    }

    database_t* database = _connections[connection];

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
                    char* nav_stack[128] = { NULL };
                    while (*(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index)) != CLOSE_BRACKET) {
                        nav_stack[nav_stack_index++] = SAFE_GET_VALUE_S(commands, argc, command_index);
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

                print_log("Database [%s.%s] create success!", new_database->header->name, DATABASE_EXTENSION);
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
                table_t* table = _get_table(database, table_name);
                if (table) { // Table already exist
                    TBM_flush_table(table);
                    return answer;
                }

                unsigned char access_byte = access;
                char* table_access = SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index);                
                if (strcmp(table_access, ACCESS_SAME) != 0) {
                    access_byte = CREATE_ACCESS_BYTE((table_access[0] - '0'), (table_access[1] - '0'), (table_access[2] - '0'));
                    access_byte = (access_byte < access) ? access : access_byte;
                }

                int column_count = 0;
                table_column_t** columns = NULL;
                if (strcmp(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), COLUMNS) == 0) {
                    if (*(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index)) == OPEN_BRACKET) {
                        int current_stack_pointer = 0;
                        char* column_stack[512] = { NULL };
                        while (*(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index)) != CLOSE_BRACKET) {
                            column_stack[current_stack_pointer++] = SAFE_GET_VALUE(commands, argc, command_index);
                        }

                        column_count = current_stack_pointer / 5;
                        columns = (table_column_t**)malloc(column_count * sizeof(table_column_t*));
                        if (!columns) return answer;
                        memset(columns, 0, column_count * sizeof(table_column_t*));

                        for (int j = 0, k = 0; j < 512; j += 5, k++) {
                            if (column_stack[j] == NULL) break;

                            // Get column data type
                            char* column_data_type = column_stack[j + 2];
                            unsigned char data_type = COLUMN_TYPE_MODULE;
                            if (strcmp(column_data_type, TYPE_INT) == 0) data_type = COLUMN_TYPE_INT;
                            else if (strcmp(column_data_type, TYPE_ANY) == 0) data_type = COLUMN_TYPE_ANY;
                            else if (strcmp(column_data_type, TYPE_STRING) == 0) data_type = COLUMN_TYPE_STRING;

                            // Get column primary status
                            unsigned char primary_status = COLUMN_NOT_PRIMARY;
                            if (strcmp(column_stack[j + 3], PRIMARY) == 0) primary_status = COLUMN_PRIMARY;

                            // Get column increment status
                            unsigned char increment_status = COLUMN_NO_AUTO_INC;
                            if (strcmp(column_stack[j + 4], AUTO_INC) == 0) increment_status = COLUMN_AUTO_INCREMENT;

                            columns[k] = TBM_create_column(
                                CREATE_COLUMN_TYPE_BYTE(primary_status, data_type, increment_status), atoi(column_stack[j + 1]), column_stack[j]
                            );

                            if (data_type == COLUMN_TYPE_MODULE) {
                                char* equals_pos = strchr(column_data_type, '=');
                                char* comma_pos  = strchr(column_data_type, ',');

                                if (equals_pos && comma_pos) {
                                    columns[k]->module_params = COLUMN_MODULE_POSTLOAD;
                                    if (strncmp(comma_pos + 1, MODULE_PRELOAD, strlen(MODULE_PRELOAD)) == 0) columns[k]->module_params = COLUMN_MODULE_PRELOAD;
                                    else if (strncmp(comma_pos + 1, MODULE_BOTH_LOAD, strlen(MODULE_PRELOAD)) == 0) columns[k]->module_params = COLUMN_MODULE_BOTH;

                                    strncpy(columns[k]->module_name, column_data_type, MIN(equals_pos - column_data_type, MODULE_NAME_SIZE));
                                    strncpy(columns[k]->module_querry, equals_pos + 1, MIN(comma_pos - equals_pos - 1, COLUMN_MODULE_SIZE));
                                    print_debug(
                                        "Module [%s] registered with [%s] querry", columns[k]->module_name, columns[column_count]->module_querry
                                    );
                                }
                            }

                            print_debug("%i) Column [%s] with args: [%i], created success!", k, column_stack[j], columns[k]->type);
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
                CHC_add_entry(new_table, new_table->header->name, TABLE_BASE_PATH, TABLE_CACHE, (void*)TBM_free_table, (void*)TBM_save_table);
                print_log("Table [%s] create success!", new_table->header->name);

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
                    if (result >= 0) { print_log("Row [%s] successfully added to [%s] database!", input_data, database->header->name); }
                    else {
                        print_error("Error code: %i, Params: [%s] [%s] [%s] [%i]", result, database->header->name, table_name, input_data, access);
                    }

                    answer->answer_size = -1;
                    answer->answer_code = result;
                }
            }
        }
        /*
        Handle get command.
        Command syntax: get <option>
        */
        else if (strcmp(command, GET) == 0) {
            /*
            Command syntax: get row <table_name> <operation_type> <options>
            */
            if (strcmp(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), ROW) == 0) {
                char* table_name = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                table_t* table = _get_table(database, table_name);
                if (!table) return answer;

                unsigned char* answer_data = NULL;
                int answer_size = 0;
                int index = -1;

                /*
                Note: Will get entire row.
                Command syntax: get row <table_name> by_index <index>
                */
                command_index++;
                if (strcmp(SAFE_GET_VALUE_S(commands, argc, command_index), BY_INDEX) == 0) {
                    int index = atoi(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index));
                    answer->answer_body = (unsigned char*)malloc(table->row_size);
                    if (!answer->answer_body) {
                        return answer;
                    }

                    if (!DB_get_row(database, table_name, index, access, answer->answer_body, table->row_size)) {
                        print_error("Something goes wrong! Params: [%.*s] [%s] [%i] [%i]", DATABASE_NAME_SIZE, database->header->name, table_name, index, access);
                        answer->answer_code = 8;
                        return answer;
                    }

                    answer->answer_size = table->row_size;
                    answer->answer_code = (char)index;
                }
                /*
                Note: will get line of rows, that equals expression.
                Command syntax: get row table <table_name> by_exp column <column_name> <</>/!=/=/eq/neq> <value> <or/and> ... limit <limit>
                */
                else if (strcmp(SAFE_GET_VALUE_S(commands, argc, command_index), BY_EXPRESSION) == 0) {      
                    expression_t exp;
                    _create_expression(table, commands, command_index, argc, &exp);
                    _process_table(database, table, answer, &exp, access, __get_logic);
                }

                TBM_flush_table(table);
            }
        }
        /*
        Handle update command.
        Command syntax: update <option>
        */
#ifndef NO_UPDATE_COMMAND
        else if (strcmp(command, UPDATE) == 0) {
            /*
            Command syntax: update row <table_name> <new_data> <option>
            */
            if (strcmp(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), ROW) == 0) {
                int index = -1;
                char* table_name = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                char* data = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);

                /*
                Command syntax: update row <table_name> <new_data> by_index <index>
                */
                command_index++;
                if (strcmp(SAFE_GET_VALUE_S(commands, argc, command_index), BY_INDEX) == 0) {
                    index = atoi(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index));
                    answer->answer_code = DB_insert_row(database, table_name, index, (unsigned char*)data, strlen(data), access);
                }
                /*
                Command syntax: update row <table_name> <new_data> by_exp column <column_name> <</>/!=/=/eq/neq> <value> values <data>
                */
                else if (strcmp(SAFE_GET_VALUE_S(commands, argc, command_index), BY_EXPRESSION) == 0) {
                    table_t* table = _get_table(database, table_name);
                    if (!table) return answer;
                                        
                    expression_t exp;
                    _create_expression(table, commands, command_index, argc, &exp);
                    _process_table(database, table, answer, &exp, access, __insert_logic);
                }
            }
        }
#endif
        /*
        Handle delete command.
        Command syntax: delete <option>
        */
#ifndef NO_DELETE_COMMAND
        else if (strcmp(command, DELETE) == 0) {
            answer->answer_code = 1;

            /*
            Command syntax: delete database
            */
            command_index++;
            if (strcmp(SAFE_GET_VALUE_S(commands, argc, command_index), DATABASE) == 0) {
                if (DB_delete_database(database, 1)) {
                    print_log("Current database was delete successfully.");
                    _connections[connection] = NULL;
                } 
                else { 
                    print_error("Error code 1 during deleting current database!");
                    answer->answer_code = -1;
                }
            }
            /*
            Command syntax: delete table <name>
            */
            else if (strcmp(SAFE_GET_VALUE_S(commands, argc, command_index), TABLE) == 0) {
                char* table_name = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                if (DB_delete_table(database, table_name, 1)) print_log("Table [%s] was delete successfully.", table_name);
                else {
                    print_error("Error code 1 during deleting %s", table_name);
                    answer->answer_code = -1;
                }
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
                    table_t* table = _get_table(database, table_name);
                    if (!table) return answer;
                    
                    expression_t exp;
                    _create_expression(table, commands, command_index, argc, &exp);
                    _process_table(database, table, answer, &exp, access, __delete_logic);
                }
            }

            answer->answer_size = -1;
        }
#endif
    }

    return answer;
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
