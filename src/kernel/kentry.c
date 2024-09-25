#include "include/kentry.h"


int main(int argc, char* argv[]) {
    /*
    Enable traceback for current session.
    */
    TB_enable();

    /*
    Check transaction status.
    Syntax: <db path> transaction-start <command count>
    */
    int transaction_size = 1;
    char** commands_pointer = argv;
    if (strcmp(argv[0], TRANSACTION) == 0)
        transaction_size = atoi(argv[1]);

    /*
    Commands executing and pointer moving.
    */
    int arg_count = 0;
    for (int i = 0; i < transaction_size; i++) {
        kernel_answer_t* answer = kernel_process_command(argc - arg_count, commands_pointer);
        arg_count += answer->commands_processed;
        commands_pointer += arg_count;
    }
}

kernel_answer_t* kernel_process_command(int argc, char* argv[]) {
    kernel_answer_t* answer = (kernel_answer_t*)malloc(sizeof(kernel_answer_t));

    uint8_t* answer_body = NULL;
    uint8_t answer_code = -1;
    uint8_t answer_size = -1;

    int current_start = 1;
    database_t* database = DB_load_database(argv[current_start++]);
    if (database == NULL) {
        print_warn("Database wasn`t found. Create a new one with CREATE DATABASE.");
        current_start = 1;
    }

    /*
    Save commands into RAM.
    */
    char* commands[MAX_COMMANDS] = { NULL };
    for (int i = current_start; i < argc; i++) commands[i - current_start] = argv[i];

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
        if (strcmp(command, CREATE) == 0) {
            /*
            Handle database creation.
            Command syntax: create database <name>
            */
            command_index++;
            if (strcmp(commands[command_index], DATABASE) == 0) {
                char* database_name = commands[++command_index];

                char save_path[DEFAULT_PATH_SIZE];
                sprintf(save_path, "%s%.8s.%s", DATABASE_BASE_PATH, database_name, DATABASE_EXTENSION);

                database_t* new_database = DB_create_database(database_name);
                DB_save_database(new_database, save_path);

                print_info("Database [%s] create succes!", save_path);
                answer->answer_code = 1;
                answer->answer_size = -1;

                return answer;
            }
            /*
            Handle table creation.
            Command syntax: create table <name> <rwd> columns ( name size <int/str/dob/any> <is_primary/np> <auto_increment/na> )
            Errors:
            - Return -1 if table already exists in database.
            */
            else if (strcmp(commands[command_index], TABLE) == 0) {
                char* table_name = commands[++command_index];
                if (DB_get_table(database, table_name) != NULL) return NULL;

                char* access = commands[++command_index];
                uint8_t rd  = access[0] - '0';
                uint8_t wr  = access[1] - '0';
                uint8_t del = access[2] - '0';

                int column_count = 0;
                table_column_t** columns = NULL;
                if (strcmp(commands[++command_index], COLUMNS) == 0) {
                    if (strcmp(commands[++command_index], OPEN_BRACKET) == 0) {
                        int column_stack_index = 0;
                        char* column_stack[256] = { NULL };
                        while (strcmp(commands[++command_index], CLOSE_BRACKET) != 0) {
                            column_stack[column_stack_index++] = commands[command_index];
                        }

                        for (int j = 0; j < 256; j += 5) {
                            if (column_stack[j] == NULL) break;

                            // Get column data type
                            uint8_t data_type = COLUMN_TYPE_ANY;
                            char* column_data_type = column_stack[j + 2];
                            if (strcmp(column_data_type, INT) == 0) data_type = COLUMN_TYPE_INT;
                            else if (strcmp(column_data_type, DOUBLE) == 0) data_type = COLUMN_TYPE_FLOAT;
                            else if (strcmp(column_data_type, STRING) == 0) data_type = COLUMN_TYPE_STRING;

                            // Get column primary status
                            uint8_t primary_status = COLUMN_NOT_PRIMARY;
                            char* column_primary = column_stack[j + 3];
                            if (strcmp(column_primary, PRIMARY) == 0) primary_status = COLUMN_PRIMARY;

                            // Get column increment status
                            uint8_t increment_status = COLUMN_NO_AUTO_INC;
                            char* column_increment = column_stack[j + 4];
                            if (strcmp(column_increment, AUTO_INC) == 0) increment_status = COLUMN_AUTO_INCREMENT;

                            columns = (table_column_t**)realloc(columns, sizeof(table_column_t*) * (column_count + 1));
                            columns[column_count++] = TBM_create_column(
                                CREATE_COLUMN_TYPE_BYTE(primary_status, data_type, increment_status),
                                atoi(column_stack[j + 1]), column_stack[j]
                            );

                            print_info("%i) Column [%s] with args: [%i], created success!", column_count, column_stack[j], columns[column_count - 1]->type);
                        }
                    }
                }

                table_t* new_table = TBM_create_table(table_name, columns, column_count, CREATE_ACCESS_BYTE(rd, wr, del));
                DB_link_table2database(database, new_table);

                char tb_save_path[DEFAULT_PATH_SIZE];
                sprintf(tb_save_path, "%s%.8s.%s", TABLE_BASE_PATH, new_table->header->name, TABLE_EXTENSION);
                TBM_save_table(new_table, tb_save_path);
                TBM_free_table(new_table);

                char db_save_path[DEFAULT_PATH_SIZE];
                sprintf(db_save_path, "%s%.8s.%s", DATABASE_BASE_PATH, database->header->name, DATABASE_EXTENSION);
                int result = DB_save_database(database, db_save_path);

                print_info("Table [%s] create success!", table_name);
                answer->answer_code = result;
                answer->answer_size = -1;

                return answer;
            }
        }
        /*
        Handle data append.
        Command syntax: append <table_name> columns <data> <rwd>.
        Note: Command don`t care about spacing, data separations, padding and other stuff. This is your work.
        Errors:
        - Return -1 error if table not found.
        - Return -2 error if data size not equals row size.
        */
        else if (strcmp(command, APPEND) == 0) {
            char* table_name = commands[++command_index];
            if (strcmp(commands[++command_index], COLUMNS) == 0) {
                char* input_data = commands[++command_index];
                char* access = commands[++command_index];
                uint8_t rd  = access[0] - '0';
                uint8_t wr  = access[1] - '0';
                uint8_t del = access[2] - '0';

                int result = DB_append_row(database, table_name, (uint8_t*)input_data, strlen(input_data), CREATE_ACCESS_BYTE(rd, wr, del));
                if (result >= 0) print_info("Row [%s] successfully added to [%s] database!", input_data, database->header->name);
                else {
                    print_error("Error code: %i\n", result);
                }

                answer->answer_code = result;
                answer->answer_size = -1;

                return answer;
            }
        }
        /*
        Handle delete command.
        Command syntax: delete <option>
        */
        else if (strcmp(command, DELETE) == 0) {

            /*
            Command syntax: delete table <name> <rwd>
            */
            command_index++;
            if (strcmp(commands[command_index], TABLE) == 0) {
                char* table_name = commands[++command_index];
                if (DB_delete_table(database, table_name, atoi(commands[++command_index])) != 1) print_error("Error code 1 during deleting %s", table_name);
                else print_info("Table [%s] was delete successfully.", table_name);

                answer->answer_code = 1;
                answer->answer_size = -1;

                return answer;
            }

            /*
            Command syntax: delete row <table_name> <operation_type> <options>
            */
            else if (strcmp(commands[command_index], ROW) == 0) {

                char* table_name = commands[++command_index];

                /*
                Note: Will delete entire row. (If table has links, it will cause CASCADE operations).
                Command syntax: delete row <table_name> by_index <index> <rwd>
                */
                command_index++;
                if (strcmp(commands[command_index], BY_INDEX) == 0) {
                    int index = atoi(commands[++command_index]);
                    char* access = commands[++command_index];
                    uint8_t rd  = access[0] - '0';
                    uint8_t wr  = access[1] - '0';
                    uint8_t del = access[2] - '0';

                    int result = DB_delete_row(database, table_name, index, CREATE_ACCESS_BYTE(rd, wr, del));
                    answer->answer_code = result;
                    answer->answer_size = -1;

                    return answer;
                }

                /*
                Note: will delete first row, where will find value in provided column.
                Command syntax: delete row <table_name> by_value <value> column <column_name> <rwd>
                */
                else if (strcmp(commands[command_index], BY_VALUE) == 0) {
                    char* value = commands[++command_index];
                    if (strcmp(commands[++command_index], COLUMN) == 0) {
                        char* column_name = commands[++command_index];
                        char* access = commands[++command_index];
                        uint8_t rd  = access[0] - '0';
                        uint8_t wr  = access[1] - '0';
                        uint8_t del = access[2] - '0';

                        int row2delete = DB_find_data_row(database, table_name, column_name, 0, (uint8_t*)value, strlen(value), CREATE_ACCESS_BYTE(rd, wr, del));
                        if (row2delete == -1) {
                            print_error("Value not presented in table [%s].", table_name);
                            return NULL;
                        }

                        int result = DB_delete_row(database, table_name, row2delete, CREATE_ACCESS_BYTE(rd, wr, del));
                        if (result == 1) printf("Row %i was deleted succesfully from %s\n", row2delete, table_name);
                        else {
                            print_error("Error code: %i", result);
                            return NULL;
                        }

                        answer->answer_code = result;
                        answer->answer_size = -1;

                        return answer;
                    }
                }
            }

            return answer;
        }
        /*
        Handle get command.
        Command syntax: get <option>
        */
        else if (strcmp(command, GET) == 0) {

            /*
            Command syntax: get row <table_name> <operation_type> <options>
            */
            if (strcmp(commands[++command_index], ROW) == 0) {
                char* table_name = commands[++command_index];

                /*
                Note: Will get entire row. (If table has links, it will cause CASCADE operations).
                Command syntax: get row <table_name> by_index <index> <rwd>
                */
                command_index++;
                if (strcmp(commands[command_index], BY_INDEX) == 0) {
                    int index = atoi(commands[++command_index]);
                    char* access = commands[++command_index];
                    uint8_t rd  = access[0] - '0';
                    uint8_t wr  = access[1] - '0';
                    uint8_t del = access[2] - '0';

                    uint8_t* data = DB_get_row(database, table_name, index, CREATE_ACCESS_BYTE(rd, wr, del));
                    if (data == NULL) {
                        print_error("Something goes wrong!");
                        return NULL;
                    }

                    int row_size = 0;
                    table_t* table = DB_get_table(database, table_name);
                    for (int j = 0; j < table->header->column_count; j++) row_size += table->columns[j]->size;

                    answer->answer_body = data;
                    answer->answer_code = index;
                    answer->answer_size = row_size;

                    return answer;
                }

                /*
                Note: will get first row, where will find value in provided column.
                Command syntax: get row <table_name> by_value <value> column <column_name> <rwd>
                */
                else if (strcmp(commands[command_index], BY_VALUE) == 0) {
                    char* value = commands[++command_index];
                    if (strcmp(commands[++command_index], COLUMN) == 0) {
                        char* column_name = commands[++command_index];
                        char* access = commands[++command_index];
                        uint8_t rd  = access[0] - '0';
                        uint8_t wr  = access[1] - '0';
                        uint8_t del = access[2] - '0';

                        int row2get = DB_find_data_row(database, table_name, column_name, 0, (uint8_t*)value, strlen(value), CREATE_ACCESS_BYTE(rd, wr, del));
                        if (row2get == -1) {
                            print_error("Value not presented in table [%s].", table_name);
                            return NULL;
                        }

                        uint8_t* data = DB_get_row(database, table_name, row2get, CREATE_ACCESS_BYTE(rd, wr, del));
                        if (data == NULL) {
                            print_error("Something goes wrong!");
                            return NULL;
                        }

                        int row_size = 0;
                        table_t* table = DB_get_table(database, table_name);
                        for (int j = 0; j < table->header->column_count; j++) row_size += table->columns[j]->size;

                        answer->answer_body = data;
                        answer->answer_code = 1;
                        answer->answer_size = row_size;

                        return answer;
                    }
                }
            }

            return answer;
        }
        /*
        Handle info command.
        Command syntax: update <option>
        */
        else if (strcmp(command, UPDATE) == 0) {
            /*
            Command syntax: update row <table_name> <option>
            */
            if (strcmp(commands[++command_index], ROW) == 0) {
                char* table_name = commands[++command_index];
                command_index++;

                /*
                Command syntax: update row <table_name> by_index <index> <new_data> <rwd>
                */
                if (strcmp(commands[command_index], BY_INDEX) == 0) {
                    int index = atoi(commands[++command_index]);
                    char* data = commands[++command_index];
                    char* access = commands[++command_index];
                    uint8_t rd  = access[0] - '0';
                    uint8_t wr  = access[1] - '0';
                    uint8_t del = access[2] - '0';

                    int result = DB_insert_row(database, table_name, index, (uint8_t*)data, strlen(data), CREATE_ACCESS_BYTE(rd, wr, del));
                    if (result >= 0) print_info("Success update of row [%i] in [%s]", index, table_name);
                    else print_error("Error code: %i", result);

                    answer->answer_code = result;
                    answer->answer_size = -1;

                    return answer;
                }

                /*
                Command syntax: update row <table_name> by_value <value> column <column_name> <new_data> <rwd>
                */
                else if (strcmp(commands[command_index], BY_VALUE) == 0) {
                    char* value = commands[++command_index];
                    if (strcmp(commands[++command_index], COLUMN) == 0) {
                        char* col_name = commands[++command_index];
                        char* data = commands[++command_index];
                        char* access = commands[++command_index];
                        uint8_t rd  = access[0] - '0';
                        uint8_t wr  = access[1] - '0';
                        uint8_t del = access[2] - '0';

                        int index = DB_find_data_row(database, table_name, col_name, 0, (uint8_t*)value, strlen(value), CREATE_ACCESS_BYTE(rd, wr, del));
                        if (index == -1) {
                            print_error("Value [%s] not presented in table [%s]", data, table_name);
                            return NULL;
                        }

                        int result = DB_insert_row(database, table_name, index, (uint8_t*)data, strlen(data), CREATE_ACCESS_BYTE(rd, wr, del));
                        if (result >= 0) print_info("Success update of row [%i] in [%s]", index, table_name);
                        else print_error("Error code: %i", result);

                        answer->answer_code = result;
                        answer->answer_size = -1;

                        return answer;
                    }
                }
            }

        }
        /*
        Handle info command.
        Command syntax: info table
        */
        else if (strcmp(command, GET_INFO) == 0) {
            if (strcmp(commands[++command_index], TABLE) == 0) {
                char* table_name = commands[++command_index];
                table_t* table = DB_get_table(database, table_name);
                if (table == NULL) {
                    print_error("Table [%s] not found in database!", table_name);
                    return NULL;
                }

                printf("Table [%s]:\n", table_name);
                printf("Table CC [%i].\n", table->header->column_count);
                printf("Table DC [%i].\n", table->header->dir_count);
                printf("Table M [%i].\n", table->header->magic);
                printf("Table CLC [%i].\n", table->header->column_link_count);

                printf("\n");
                printf("- COLUMNS: \n");
                for (int i = 0; i < table->header->column_count; i++)
                    printf("\tColumn [%s], type [%i], size [%i].\n", table->columns[i]->name, table->columns[i]->type, table->columns[i]->size);

                printf("\n");
                printf("- LINKS: \n");
                for (int i = 0; i < table->header->column_link_count; i++)
                    printf(
                        "\tLink [%s] column with [%s] column from [%s] table.\n",
                        table->column_links[i]->master_column_name,
                        table->column_links[i]->slave_column_name,
                        table->column_links[i]->slave_table_name
                    );
            }
        }
        /*
        Handle info command.
        Command syntax: link <master_table> <naster_column> <slave_table> <slave_volumn> ( flags )
        */
        else if (strcmp(command, LINK) == 0) {
            char* master_table  = commands[++command_index];
            char* master_column = commands[++command_index];
            char* slave_table   = commands[++command_index];
            char* slave_column  = commands[++command_index];

            uint8_t delete_link = LINK_NOTHING;
            uint8_t update_link = LINK_NOTHING;
            uint8_t append_link = LINK_NOTHING;
            uint8_t find_link   = LINK_NOTHING;

            table_t* master = DB_get_table(database, master_table);
            if (master == NULL) {
                print_error("Table [%s] not found in database!", master_table);
                return NULL;
            }

            table_t* slave  = DB_get_table(database, slave_table);
            if (slave == NULL) {
                print_error("Table [%s] not found in database!", slave_table);
                return NULL;
            }

            if (strcmp(OPEN_BRACKET, commands[++command_index]) == 0) {
                while (strcmp(CLOSE_BRACKET, commands[++command_index]) != 0) {
                    if (strcmp(CASCADE_DEL, commands[command_index]) == 0) delete_link = LINK_CASCADE_DELETE;
                    else if (strcmp(CASCADE_UPD, commands[command_index]) == 0) update_link = LINK_CASCADE_UPDATE;
                    else if (strcmp(CASCADE_APP, commands[command_index]) == 0) append_link = LINK_CASCADE_APPEND;
                    else if (strcmp(CASCADE_FND, commands[command_index]) == 0) find_link = LINK_CASCADE_FIND;
                }
            }
            int result = TBM_link_column2column(
                master, master_column, slave, slave_column,
                CREATE_LINK_TYPE_BYTE(find_link, append_link, update_link, delete_link)
            );

            char save_path[DEFAULT_PATH_SIZE];
            sprintf(save_path, "%s%.8s.%s", TABLE_BASE_PATH, master_table, TABLE_EXTENSION);
            int save_result = TBM_save_table(master, save_path);

            print_info("Result [%i %i] of linking table [%s] with table [%s]", save_result, result, master_table, slave_table);

            answer->answer_code = save_result;
            answer->answer_size = -1;
            return answer;
        }
        /*
        Handle info command.
        Command syntax: help
        */
        else if (strcmp(command, HELP) == 0) {
            printf("Examples:\n");
            printf("- CREATE:\n");
            printf("\tExample: db.db create database db1\n");
            printf("\tExample: db.db create table table_1 columns ( col1 10 str is_primary na col2 10 any np na )\n");

            printf("- APPEND:\n");
            printf("\tExample: db.db append table_1 columns 'hello     second col' 000\n");

            printf("- UPDATE:\n");
            printf("\tExample: db.db update row table_1 by_index 0 'goodbye   hello  bye' 000\n");
            printf("\tExample: db.db update row table_1 by_value value column col1 'goodbye   hello  bye' 000\n");

            printf("- GET:\n");
            printf("\tExample: db.db get row table_1 by_value hello column col2 000\n");
            printf("\tExample: db.db get row table_1 by_index 0 000\n");

            printf("- DELETE:\n");
            printf("\tExample: db.db delete table table_1\n");
            printf("\tExample: db.db delete row table_1 by_index 0 000\n");
            printf("\tExample: db.db delete row table_1 by_value value column col1 000\n");

            printf("- LINK:\n");
            printf("\tExample: db.db link table1 col1 table2 col1 ( capp cdel )\n");

            printf("- INFO:\n");
            printf("\tExample: db.db info table table_1\n");

            return NULL;
        }
    }

    return NULL;
}

int kernel_free_answer(kernel_answer_t* answer) {
    SOFT_FREE(answer->answer_body);
    SOFT_FREE(answer);

    return 1;
}
