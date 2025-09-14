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
 *  Main file. This is main Cordell Database Manager Studio File, where
 *  placed init stuff with server side.
 * 
 *  Cordell DBMS is a light weight data base manager studio. Main idea
 *  that we can work with big data by using very light weighten app.
 * 
 *  Base code of sockets took from: https://devhops.ru/code/c/sockets.php
*/

#include "kernel/include/mm.h"
#include "kernel/include/cache.h"
#include "kernel/include/kentry.h"
#include "kernel/include/logging.h"
#include "kernel/include/threading.h"

#define COMMANDS_BUFFER     256
#define MAX_SESSION_COUNT   5

static int _process_quotes(unsigned char* buffer, char* argv[]) {
    char* current_arg = NULL;

    int argc = 1;
    int in_quotes = 0;

    for (char *p = (char*)buffer; *p != '\0'; p++) {
        if (*p == '"') {
            in_quotes = !in_quotes;
            if (in_quotes) current_arg = p + 1;
            else {
                *p = '\0';
                argv[argc++] = current_arg;
                current_arg  = NULL;
            }
        }
        else if (!in_quotes && (*p == ' ' || *p == '\t')) {
            *p = '\0';
            if (current_arg) {
                argv[argc++] = current_arg;
                current_arg  = NULL;
            }
        }
        else if (!current_arg) current_arg = p;
    }

    if (current_arg) argv[argc++] = current_arg;
    return argc;
}

/*
 * This function takes source data and destination address for kernel answer.
 * In source should be provided correct command for kernel.
 *
 * Params:
 * - source - source FD with commands.
 * - destination - destination FD for kernel answer.
*/
#define NO_SERVER
#define NO_USER
static void _start_kernel_session(int source, int destination, int session) {
}

static void* _handle_client(void* client_socket_fd) {
    return NULL;
}

kernel_answer_t* entry(char* command) {
#ifdef NO_SERVER
    char* argv[MAX_COMMANDS] = { NULL };
    int argc = _process_quotes(buffer, argv);
    return kernel_process_command(argc, argv);
#endif
    return NULL;
}

int main() {

    return 1;
}
