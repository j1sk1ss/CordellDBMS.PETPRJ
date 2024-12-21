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

#include "userland/include/user.h"
#include "kernel/include/kentry.h"
#include "kernel/include/logging.h"
#include "kernel/include/cache.h"
#include "kernel/include/threading.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
#else
    #include <unistd.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
#endif


#define CDBMS_SERVER_PORT   ENV_GET("CDBMS_SERVER_PORT", "7777")
#define MESSAGE_BUFFER      2048
#define COMMANDS_BUFFER     256
#define MAX_SESSION_COUNT   5


void _cleanup();
int _send2destination(int destination, void* data, size_t data_size);
int _send2destination_byte(int destination, int byte);
void* _handle_client(void* client_socket_fd);
void _start_kernel_session(int source, int destination, int session);


static int sessions[MAX_SESSION_COUNT] = { 0 };


void _cleanup() {
    #ifdef _WIN32
        WSACleanup();
    #endif
}

#pragma region [Send functions]

    int _send2destination(int destination, void* data, size_t data_size) {
        #ifdef _WIN32
            send(destination, (const char*)data, data_size, 0);
        #else
            if (write(destination, data, data_size) != (ssize_t)data_size) print_warn("Data send size != data write");
        #endif

        return 1;
    }

    int _send2destination_byte(int destination, int byte) {
        return _send2destination(destination, byte, 1);
    }

#pragma endregion

void* _handle_client(void* client_socket_fd) {
    int socket_fd = ((int*)client_socket_fd)[0];
    int session = ((int*)client_socket_fd)[1];

    _start_kernel_session(socket_fd, socket_fd, session);
    close_connection(session);
    close(socket_fd);

    sessions[session] = 0;
    print_info("Session [%i] closed", session);
    free(client_socket_fd);

    THR_kill_thread();
    return NULL;
}

/*
 * This function takes source data and destination address for kernel answer.
 * In source should be provided correct command for kernel.
 *
 * Params:
 * - source - source FD with commands.
 * - destination - destination FD for kernel answer.
*/
void _start_kernel_session(int source, int destination, int session) {
    user_t* user = NULL;
    int count = 0;
    unsigned char buffer[MESSAGE_BUFFER] = { 0 };

    #ifdef _WIN32
    while ((count = recv(source, (char*)buffer, MESSAGE_BUFFER, 0)) > 0)
    #else
    while ((count = read(source, buffer, MESSAGE_BUFFER)) > 0)
    #endif
    {
        buffer[count - 1] = '\0';
        print_info("Session [%i]: [%s]", session, buffer);

        if (user == NULL) {
            char username[USERNAME_SIZE] = { 0 };
            char password[128] = { 0 };
            sscanf((char*)buffer, "%[^:]:%s", username, password);

            user = USR_auth(username, password);
            if (user == NULL) {
                print_error("Wrong password [%s] for user [%s] at session [%i]", password, username, session);
                _send2destination_byte(destination, 0);
            }
            else {
                print_info("User [%s] auth succes in session [%i]", user->name, session);
                _send2destination_byte(destination, 1);
            }

            continue;
        }

        char* argv[COMMANDS_BUFFER] = { NULL };
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
        kernel_answer_t* result = kernel_process_command(argc, argv, user->access, session);
        if (result->answer_body != NULL) {
            _send2destination(destination, result->answer_body, result->answer_size);
            print_log("Answer body: [%.*s]", result->answer_size, result->answer_body);
        }
        else {
            _send2destination_byte(destination, result->answer_code);
            print_log("Answer code: %i", result->answer_code);
        }

        kernel_free_answer(result);
    }

    free(user);
    user = NULL;
}

/*
 * Server setup function
*/
int main() {
    /*
    Enable traceback for current session.
    */
    print_info("Cordell Database Manager Studio server-side.");
    print_info("Current version of kernel is [%s].", KERNEL_VERSION);

    TB_enable();
    CL_enable();
    CHC_init();

    #ifdef _WIN32
        WSADATA wsa_data;
        if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
            print_error("WSAStartup() failed");
            return -1;
        }
    #endif

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        print_error("Error in calling socket()");
        return -1;
    }

    int server_port = atoi(CDBMS_SERVER_PORT);
    struct sockaddr_in client_address;
    struct sockaddr_in server_address = {
        .sin_port = htons(server_port),
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };

    int bind_result = bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address));
    if (bind_result < 0) {
        print_error("bind() call failed. Code: %i", bind_result);
        return -2;
    }

    int listen_result = listen(server_socket, 5);
    if (listen_result < 0) {
        print_error("listen() call failed. Code: %i", listen_result);
        return -3;
    }

    print_info("DB server started on %s:%d", inet_ntoa(server_address.sin_addr), ntohs(server_address.sin_port));

    while (1) {
        int client_socket_fd   = -1;
        int client_address_len = -1;

        client_address_len = sizeof(client_address);
        client_socket_fd   = accept(server_socket, (struct sockaddr*)&client_address, (socklen_t*)&client_address_len);
        if (client_socket_fd < 0) {
            print_error("accept() call failed. Code: %i", client_socket_fd);
            continue;
        }

        int* client_socket_fd_ptr = (int*)malloc(sizeof(int) * 3);
        client_socket_fd_ptr[0] = client_socket_fd;

        int session = 0;
        while (sessions[session] == 1) {
            if (session++ >= MAX_SESSION_COUNT) session = 0;
        }

        sessions[session] = 1;
        client_socket_fd_ptr[1] = session;

        print_info("Client connected from %s:%d", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
        if (THR_create_thread(_handle_client, client_socket_fd_ptr) != 1) {
            print_error("Error while server try to create thread for [%i] session", session);
        }
    }

    _cleanup();
    return 1;
}
