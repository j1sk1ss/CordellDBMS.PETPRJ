/*
 *  License:
 *  Copyright (C) 2024 Nikolaj Fot
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of 
 *  the GNU General Public License as published by the Free Software Foundation, version 3.
 *  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  See the GNU General Public License for more details.
 *  You should have received a copy of the GNU General Public License along with this program. 
 *  If not, see https://www.gnu.org/licenses/.
 *
 *  Description:
 *  Kmain file. In future here will be placed some kernel init stuff.
 *  Also, this file handle commands, and work with database.
 *
 *  Cordell DBMS is a light weight data base manager studio. Main idea
 *  that we can work with big data by using very light weighten app.
 *
 *  Anyway, that fact, that we try to simplify DBMS, create some points:
 *  - We don`t have rollbacks, hosting and user managment. This is work for
 *  higher abstractions.
 *  - We have few fixed enviroment variable (paths for saving databases,
 *  tables, directories and pages), that can`t be changed by default user.
 *
 *  ! Building this file without server side, will disable API communication of third-part applications !
 *  building without OMP: gcc -Wall .\kentry.c .\std\* .\arch\* -Wunknown-pragmas -fpermissive -o cdbms.exe
 *  buildinc with OMP: gcc -Wall .\kentry.c .\std\* .\arch\* -fopenmp -fpermissive -o cdbms.exe
*/

#ifndef KENTRY_H_
#define KENTRY_H_

#include <string.h>
#include <stdio.h>

#include "common.h"
#include "sighandler.h"
#include "dataman.h"
#include "cache.h"


#define SAFE_GET_VALUE(argv, max, index)            index >= max ? NULL : argv[index]
#define SAFE_GET_VALUE_PRE_INC(argv, max, index)    index + 1 >= max ? NULL : argv[++index]
#define SAFE_GET_VALUE_POST_INC(argv, max, index)   index >= max ? NULL : argv[index++]

#define SAFE_GET_VALUE_S(argv, max, index)            index >= max ? "-1" : argv[index]
#define SAFE_GET_VALUE_PRE_INC_S(argv, max, index)    index + 1 >= max ? "-1" : argv[++index]
#define SAFE_GET_VALUE_POST_INC_S(argv, max, index)   index >= max ? "-1" : argv[index++]

#define MAX_COMMANDS    100
#define MAX_STATEMENTS  20
#define MAX_CONNECTIONS 5

#pragma region [Commands]

    #define ROLLBACK        "rollback"
    #define SYNC            "sync"

    #define CREATE          "create"
    #define MIGRATE         "migrate"
    #define DELETE          "delete"
    #define APPEND          "append"
    #define UPDATE          "update"
    #define GET             "get"

    #define TABLE           "table"
    #define DATABASE        "database"
    #define VERSION         "version"

    #define NAV             "nav"
    #define COLUMNS         "columns"
    #define COLUMN          "column"
    #define VALUES          "values"
    #define VALUE           "value"
    #define EXPRESSION      "exp"
    #define ROW             "row"
    #define OFFSET          "offset"
    #define LIMIT           "limit"

    #define BY_INDEX        "by_index"
    #define BY_EXPRESSION   "by_exp"

    #define OPEN_BRACKET    '('
    #define CLOSE_BRACKET   ')'

    #pragma region [Types]

        #define ACCESS_SAME "same"

        #define TYPE_INT    "int"
        #define TYPE_MODULE "mod"
        #define TYPE_STRING "str"
        #define TYPE_ANY    "any"

        #define MODULE_PRELOAD   "mpre"
        #define MODULE_POSTLOAD  "mpost"
        #define MODULE_BOTH_LOAD "both"

    #pragma endregion

    #pragma region [Expressions]

        #define OR  "or"
        #define AND "and"

        #define MORE_THAN   ">"
        #define LESS_THAN   "<"
        #define NEQUALS     "!="
        #define EQUALS      "="
        #define STR_NEQUALS "neq"
        #define STR_EQUALS  "eq"

    #pragma endregion

    #define PRIMARY         "p"
    #define NPRIMART        "np"
    #define AUTO_INC        "a"
    #define NAUTO_INC       "na"

#pragma endregion

#define KERNEL_VERSION     "v2.7 (main)"


typedef struct {
    signed char answer_code;
    unsigned short answer_size;
    unsigned char* answer_body;
} kernel_answer_t;

typedef struct {
    char* column_name;
    char* expression;
    char* value;
} condition_t;

typedef struct {
    condition_t conditions[MAX_STATEMENTS];
    int condition_count;
    char* operators[MAX_STATEMENTS];
    int operator_count;
    int offset;
    int limit;
} expression_t;


/*
Process commands and return answer structure.

Params:
- argc - args count.
- argv - args body.
- auto_sync - flag for auto sync command

Return NULL or answer.
*/
kernel_answer_t* kernel_process_command(int argc, char* argv[], unsigned char access, int connection);

/*
Close connection by index.

Params:
- connection - Connection index.

Return 1 if success.
*/
int close_connection(int connection);

/*
Free answer structure.

Params:
- answer - answer pointer.

Return -1 or 1.
*/
int kernel_free_answer(kernel_answer_t* answer);

/*
Cleanup kernel will free all entries from GCT.
Also cleanup function free all database connections.
Note: don't invoke this function. 
*/
void cleanup_kernel();

#endif
