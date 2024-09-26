/*
 * Kmain file. In future here will be placed some kernel init stuff.
 * Also, this file handle commands, and work with database.
 *
 * Cordell DBMS is a light weight data base manager studio. Main idea
 * that we can work with big data by using very light weighten app.
 *
 * Anyway, that fact, that we try to simplify DBMS, create some points:
 * - We don`t have rollbacks, hosting and user managment. This is work for
 * higher abstractions.
 * - We have few fixed enviroment variable (paths for saving databases,
 * tables, directories and pages), that can`t be changed by default user.
 *
 * ! Building this file without server side, will disable API communication of third-part applications !
 * building without OMP: gcc -Wall .\kentry.c .\std\* .\arch\* -Wunknown-pragmas -fpermissive -o cdbms.exe
 * buildinc with OMP: gcc -Wall .\kentry.c .\std\* .\arch\* -fopenmp -fpermissive -o cdbms.exe
*/

#ifndef KENTRY_H_
#define KENTRY_H_

#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "common.h"
#include "traceback.h"
#include "database.h"


#define MAX_COMMANDS    100

#pragma region [Commands]

    #define HELP            "help"
    #define TRANSACTION     "transaction-start"

    #define CREATE          "create"    // Example: db.db create table table_1 columns ( col1 10 str is_primary na col2 10 any np na )
    #define LINK            "link"      // Example: db.db link master_table master_column slave_table slave_column ( cdel cupd capp cfnd )
    #define DELETE          "delete"    // Example: db.db delete table table_1
    #define APPEND          "append"    // Example: db.db append table_1 columns "hello     second col" 000
    #define UPDATE          "update"    // Example: db.db update row table_1 by_index 0 "goodbye   hello  bye" 000
    #define GET_INFO        "info"      // Example: db.db info table table_1
    #define GET             "get"       // Example: db.db get row table_1 by_value "hello" column "col2" 000

    #define TABLE           "table"
    #define DATABASE        "database"

    #define COLUMNS         "columns"
    #define COLUMN          "column"
    #define ROW             "row"

    #define BY_INDEX        "by_index"
    #define BY_VALUE        "by_value"

    #define OPEN_BRACKET    "("
    #define CLOSE_BRACKET   ")"

    #pragma region [Types]

        #define INT             "int"
        #define DOUBLE          "dob"
        #define STRING          "str"
        #define ANY             "any"

        #define CASCADE_DEL     "cdel"
        #define CASCADE_UPD     "cupd"
        #define CASCADE_APP     "capp"
        #define CASCADE_FND     "cfnd"

    #pragma endregion

    #define PRIMARY         "is_primary"
    #define NPRIMART        "np"
    #define AUTO_INC        "auto_increment"
    #define NAUTO_INC       "na"

#pragma endregion


struct kernel_answer {
    uint16_t commands_processed;
    uint8_t answer_code;
    uint16_t answer_size;
    uint8_t* answer_body;
} typedef kernel_answer_t;


/*
Entry syntax: data_base_path<*.db> <commands>.

Params:
- argc - args count.
- argv - args body.

Return -1 or 1.
*/
int kmain(int argc, char* argv[]);

/*
Process commands and return answer structure.

Params:
- argc - args count.
- argv - args body.

Return NULL or answer.
*/
kernel_answer_t* kernel_process_command(int argc, char* argv[]);

/*
Free answer structure.

Params:
- answer - answer pointer.

Return -1 or 1.
*/
int kernel_free_answer(kernel_answer_t* answer);

#endif
