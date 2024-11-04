#include "include/kentry.h"


static database_t* connections[MAX_CONNECTIONS] = { NULL };


kernel_answer_t* kernel_process_command(int argc, char* argv[], int auto_sync, uint8_t access, int connection) {
    kernel_answer_t* answer = (kernel_answer_t*)malloc(sizeof(kernel_answer_t));

    answer->answer_body = NULL;
    answer->answer_code = 255;
    answer->answer_size = 0;
    answer->commands_processed = 0;

    int current_start = 1;
    database_t* database;
    char* db_name = SAFE_GET_VALUE_POST_INC_S(argv, argc, current_start);

    while (1) {
        if (connections[connection] == NULL) {
            database = DB_load_database(NULL, db_name);
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
            answer->answer_body = (uint8_t*)malloc(strlen(KERNEL_VERSION));
            memcpy(answer->answer_body, KERNEL_VERSION, strlen(KERNEL_VERSION));
            answer->answer_size = strlen(KERNEL_VERSION);
        }
        /*
        Handle creation.
        Command syntax: create <option>
        */
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
                int result = DB_save_database(new_database, NULL);

                print_log("Database [%.*s.%s] create succes!", DATABASE_NAME_SIZE, new_database->header->name, DATABASE_EXTENSION);
                DB_free_database(new_database);

                answer->answer_code = result;
                answer->answer_size = -1;
                answer->commands_processed = command_index;
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

                if (DB_get_table(database, table_name) != NULL) return answer;
                char* table_access = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                if (table_access == NULL) {
                    answer->answer_code = 5;
                    return answer;
                }

                uint8_t access_byte = access;
                if (strcmp(table_access, ACCESS_SAME) != 0) {
                    uint8_t rd  = table_access[0] - '0';
                    uint8_t wr  = table_access[1] - '0';
                    uint8_t del = table_access[2] - '0';
                    access_byte = CREATE_ACCESS_BYTE(rd, wr, del);
                    if (access_byte < access) {
                        answer->answer_code = 6;
                        return answer;
                    }
                }

                int column_count = 0;
                table_column_t** columns = NULL;
                if (strcmp(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), COLUMNS) == 0) {
                    if (strcmp(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), OPEN_BRACKET) == 0) {
                        int column_stack_index = 0;
                        char* column_stack[256] = { NULL };
                        while (strcmp(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), CLOSE_BRACKET) != 0) {
                            column_stack[column_stack_index++] = SAFE_GET_VALUE(commands, argc, command_index);
                        }

                        for (int j = 0; j < 256; j += 5) {
                            if (column_stack[j] == NULL) break;

                            // Get column data type
                            uint8_t data_type = COLUMN_TYPE_MODULE;
                            char* column_data_type = column_stack[j + 2];
                            if (strcmp(column_data_type, TYPE_INT) == 0) data_type = COLUMN_TYPE_INT;
                            else if (strcmp(column_data_type, TYPE_ANY) == 0) data_type = COLUMN_TYPE_ANY;
                            else if (strcmp(column_data_type, TYPE_STRING) == 0) data_type = COLUMN_TYPE_STRING;

                            // Get column primary status
                            uint8_t primary_status = COLUMN_NOT_PRIMARY;
                            char* column_primary = column_stack[j + 3];
                            if (strcmp(column_primary, PRIMARY) == 0) primary_status = COLUMN_PRIMARY;

                            // Get column increment status
                            uint8_t increment_status = COLUMN_NO_AUTO_INC;
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

                                memset(columns[column_count]->module_name, 0, MODULE_NAME_SIZE);
                                strncpy(columns[column_count]->module_name, column_data_type, MIN(module_name_len, MODULE_NAME_SIZE));
                                if (equals_pos != NULL) {
                                    strncpy(columns[column_count]->module_querry, equals_pos + 1, module_query_len);
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
                    return answer;
                }

                DB_link_table2database(database, new_table);
                TBM_TDT_add_table(new_table);

                print_log("Table [%.*s] create success!", TABLE_NAME_SIZE, new_table->header->name);

                answer->answer_size = -1;
                answer->answer_code = 1;
                answer->commands_processed = command_index;
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
        else if (strcmp(command, APPEND) == 0) {
            if (strcmp(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), ROW) == 0) {
                char* table_name = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);

                if (strcmp(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), VALUES) == 0) {
                    char* input_data = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                    if (input_data == NULL || table_name == NULL) {
                        answer->answer_code = 5;
                        return answer;
                    }

                    int result = DB_append_row(database, table_name, (uint8_t*)input_data, strlen(input_data), access);
                    if (result >= 0) print_log("Row [%s] successfully added to [%.*s] database!", input_data, DATABASE_NAME_SIZE, database->header->name);
                    else {
                        print_error(
                            "Error code: %i, Params: [%.*s] [%s] [%s] [%i]", 
                            result, DATABASE_NAME_SIZE, database->header->name, table_name, input_data, access
                        );
                    }

                    answer->answer_size = -1;
                    answer->answer_code = result;
                    answer->commands_processed = command_index;
                }
            }
        }
        /*
        Handle delete command.
        Command syntax: delete <option>
        */
        else if (strcmp(command, DELETE) == 0) {
            /*
            Command syntax: delete database <name>
            */
            command_index++;
            if (strcmp(SAFE_GET_VALUE_S(commands, argc, command_index), DATABASE) == 0) {
                char* database_name = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                if (database_name == NULL) {
                    answer->answer_code = 5;
                    return answer;
                }

                if (DB_delete_database(database, 1) != 1) print_error("Error code 1 during deleting %s", database_name);
                else print_log("Database [%s] was delete successfully.", database_name);

                database = NULL;
                connections[connection] = NULL;

                answer->answer_code = 1;
                answer->answer_size = -1;
                answer->commands_processed = command_index;
            }
            /*
            Command syntax: delete table <name>
            */
            else if (strcmp(SAFE_GET_VALUE_S(commands, argc, command_index), TABLE) == 0) {
                char* table_name = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                if (table_name == NULL) {
                    answer->answer_code = 5;
                    return answer;
                }

                if (DB_delete_table(database, table_name, 1) != 1) print_error("Error code 1 during deleting %s", table_name);
                else print_log("Table [%s] was delete successfully.", table_name);

                answer->answer_code = 1;
                answer->answer_size = -1;
                answer->commands_processed = command_index;
            }

            /*
            Command syntax: delete row <table_name> <operation_type> <options>
            */
            else if (strcmp(SAFE_GET_VALUE_S(commands, argc, command_index), ROW) == 0) {
                int index = -1;
                char* table_name = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);

                /*
                Note: Will delete entire row. (If table has links, it will cause CASCADE operations).
                Command syntax: delete row <table_name> by_index <index>
                */
                command_index++;
                if (strcmp(SAFE_GET_VALUE_S(commands, argc, command_index), BY_INDEX) == 0) {
                    index = atoi(SAFE_GET_VALUE_PRE_INC(commands, argc, command_index));
                }
                /*
                Note: will delete first row, where will find value in provided column.
                Command syntax: delete row <table_name> by_value column <column_name> value <value>
                */
                else if (strcmp(SAFE_GET_VALUE_S(commands, argc, command_index), BY_VALUE) == 0) {
                    if (strcmp(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), COLUMN) == 0) {
                        char* column_name = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);

                        if (strcmp(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), VALUE) == 0) {
                            char* value = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                            if (value == NULL || column_name == NULL || table_name == NULL) {
                                answer->answer_code = 5;
                                return answer;
                            }

                            index = DB_find_data_row(database, table_name, column_name, 0, (uint8_t*)value, strlen(value), access);
                            if (index == -1) {
                                print_error("Value not presented in table [%s].", table_name);
                                return answer;
                            }
                        }
                    }
                }

                answer->answer_size = -1;
                answer->answer_code = DB_delete_row(database, table_name, index, access);
                answer->commands_processed = command_index;
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
                int index = -1;
                int answer_size = 0;
                uint8_t* answer_data = NULL;

                char* table_name = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                if (table_name == NULL) {
                    answer->answer_code = 5;
                    return answer;
                }

                /*
                Note: Will get entire row. (If table has links, it will cause CASCADE operations).
                Command syntax: get row <table_name> by_index <index>
                */
                command_index++;
                if (strcmp(SAFE_GET_VALUE_S(commands, argc, command_index), BY_INDEX) == 0) {
                    index = atoi(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index));
                    if (index == -1) {
                        answer->answer_code = 7;
                        return answer;
                    }

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
                }
                /*
                Note: will get first row, where will find value in provided column.
                Command syntax: get row table <table_name> by_value column <column_name> value <value>
                */
                else if (strcmp(SAFE_GET_VALUE_S(commands, argc, command_index), BY_VALUE) == 0) {
                    if (strcmp(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), COLUMN) == 0) {
                        char* column_name = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);

                        if (strcmp(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), VALUE) == 0) {
                            char* value = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                            if (value == NULL || column_name == NULL) {
                                answer->answer_code = 5;
                                return answer;
                            }

                            table_t* table = DB_get_table(database, table_name);
                            int offset = 0;

                            while (index != -1) {
                                index = DB_find_data_row(database, table_name, column_name, offset, (uint8_t*)value, strlen(value), access);
                                if (index == -1) break;
                                
                                uint8_t* row_data = DB_get_row(database, table_name, index, access);
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

                                answer_data = (uint8_t*)realloc(answer_data, answer_size);
                                uint8_t* copy_pointer = answer_data + data_start;

                                memcpy(copy_pointer, row_data, table->row_size);
                            }
                        }
                    }
                }

                answer->answer_body = answer_data;
                answer->answer_code = index;
                answer->answer_size = answer_size;
                answer->commands_processed = command_index;
            }
        }
        /*
        Handle update command.
        Command syntax: update <option>
        */
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
                    index = atoi(SAFE_GET_VALUE_PRE_INC(commands, argc, command_index));
                    if (strcmp(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), VALUE) == 0) {
                        data = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                        if (data == NULL) {
                            answer->answer_code = 5;
                            return answer;
                        }
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
                                if (data == NULL || value == NULL || col_name == NULL) {
                                    answer->answer_code = 5;
                                    return answer;
                                }

                                index = DB_find_data_row(database, table_name, col_name, 0, (uint8_t*)value, strlen(value), access);
                                if (index == -1) {
                                    print_error("Value [%s] not presented in table [%s]", data, table_name);
                                    return answer;
                                }
                            }
                        }
                    }
                }
                
                answer->answer_size = -1;
                answer->answer_code = DB_insert_row(database, table_name, index, (uint8_t*)data, strlen(data), access);
                answer->commands_processed = command_index;
            }

        }
        /*
        Handle link command.
        Command syntax: link master <master_table> <master_column> to_slave <slave_table> <slave_column> ( flags )
        */
        else if (strcmp(command, LINK) == 0) {
            if (strcmp(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), MASTER) == 0) {
                char* master_table  = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                char* master_column = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);

                if (strcmp(SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index), TO_SLAVE) == 0) {
                    char* slave_table   = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                    char* slave_column  = SAFE_GET_VALUE_PRE_INC(commands, argc, command_index);
                    if (master_table == NULL || master_column == NULL || slave_table == NULL || slave_column == NULL) {
                        answer->answer_code = 5;
                        return answer;
                    }

                    uint8_t delete_link = LINK_NOTHING;
                    uint8_t update_link = LINK_NOTHING;
                    uint8_t append_link = LINK_NOTHING;
                    uint8_t find_link   = LINK_NOTHING;

                    table_t* master = DB_get_table(database, master_table);
                    table_t* slave = DB_get_table(database, slave_table);
                    if (slave == NULL || master == NULL) {
                        print_error("Table [%s] not found in database!", slave_table);
                        return answer;
                    }

                    if (strcmp(OPEN_BRACKET, SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index)) == 0) {
                        while (strcmp(CLOSE_BRACKET, SAFE_GET_VALUE_PRE_INC_S(commands, argc, command_index)) != 0) {
                            if (strcmp(CASCADE_DEL, SAFE_GET_VALUE_S(commands, argc, command_index)) == 0) delete_link = LINK_CASCADE_DELETE;
                            else if (strcmp(CASCADE_UPD, SAFE_GET_VALUE_S(commands, argc, command_index)) == 0) update_link = LINK_CASCADE_UPDATE;
                            else if (strcmp(CASCADE_APP, SAFE_GET_VALUE_S(commands, argc, command_index)) == 0) append_link = LINK_CASCADE_APPEND;
                            else if (strcmp(CASCADE_FND, SAFE_GET_VALUE_S(commands, argc, command_index)) == 0) find_link = LINK_CASCADE_FIND;
                        }
                    }

                    int result = TBM_link_column2column(
                        master, master_column, slave, slave_column, CREATE_LINK_TYPE_BYTE(find_link, append_link, update_link, delete_link)
                    );

                    print_log("Result [%i] of linking table [%s] with table [%s]", result, master_table, slave_table);

                    answer->answer_size = -1;
                    answer->answer_code = result;
                    answer->commands_processed = command_index;
                }
            }
        }
    }

    if (auto_sync == 1) DB_init_transaction(database);
    return answer;
}

int flush_tables() {
    TBM_TDT_free();
    DRM_DDT_free();
    PGM_PDT_free();
    return 1;
}

int close_connection(int connection) {
    flush_tables();
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
    flush_tables();
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (connections[i] == NULL) continue;
        DB_free_database(connections[i]);
    }
}
