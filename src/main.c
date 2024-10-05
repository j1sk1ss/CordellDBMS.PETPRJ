/*
 * Main file. This is main Cordell Database Manager Studio File, where
 * placed init stuff with server side.
 *
 * Cordell DBMS is a light weight data base manager studio. Main idea
 * that we can work with big data by using very light weighten app.
 *
 * <DEPRECATED | USE MAKEFILE INSTEAD MANUAL COMMAND BUILD>
 * Unix:
 * building without OMP: ...
 * building with OMP: ...
 *
 * Win10/Win11:
 * building without OMP: ...
 * building with OMP: ...
 *
 * Base code of sockets took from: https://devhops.ru/code/c/sockets.php
*/

#include "kernel/include/tabman.h"
#include "userland/include/user.h"
#include "kernel/include/kentry.h"
#include "kernel/include/logging.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <unistd.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
#endif


#define CDBMS_SERVER_PORT   getenv("CDBMS_SERVER_PORT") == NULL ? 1010 : atoi(getenv("CDBMS_SERVER_PORT"))
#define MESSAGE_BUFFER      2048
#define COMMANDS_BUFFER     256


user_t* user = NULL;


void cleanup();
int setup_server();
void send2kernel(int source, int destination);


void cleanup() {
    #ifdef _WIN32
        WSACleanup();
    #endif
}

/*
 * This function takes source data and destination address for kernel answer.
 * In source should be provided correct command for kernel.
 *
 * Params:
 * - source - source FD with commands.
 * - destination - destination FD for kernel answer.
*/
void send2kernel(int source, int destination) {
    int count = 0;
    uint8_t buffer[MESSAGE_BUFFER];

    #ifdef _WIN32
    while ((count = recv(source, (char*)buffer, MESSAGE_BUFFER, 0)) > 0)
    #else
    while ((count = read(source, buffer, MESSAGE_BUFFER)) > 0)
    #endif
    {
        buffer[count - 1] = '\0';
        if (user == NULL) {
            #ifndef USER_DEBUG
                print_info("Authorization required!");
                char username[USERNAME_SIZE];
                char password[128];

                sscanf((char*)buffer, "%[^:]:%s", username, password);
                user = USR_auth(username, password);
            #else
                char username[USERNAME_SIZE];
                char password[128];

                sscanf((char*)buffer, "%[^:]:%s", username, password);
                user = USR_create(username, password, CREATE_ACCESS_BYTE(0, 0, 0));
                USR_save(user, NULL);
            #endif
            continue;
        }

        char* argv[COMMANDS_BUFFER];
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

        if (current_arg) {
            argv[argc++] = current_arg;
        }

        kernel_answer_t* result = kernel_process_command(argc, argv, 0, user->access);
        if (result->answer_body != NULL) {
            #ifdef _WIN32
                send(destination, (const char*)result->answer_body, result->answer_size, 0);
            #else
                write(destination, result->answer_body, result->answer_size);
            #endif

            print_log("Answer body: [%s]", result->answer_body);
        }
        else {
            #ifdef _WIN32
                uint8_t buffer[1] = { result->answer_code };
                send(destination, (const char*)buffer, sizeof(uint8_t), 0);
            #else
                uint8_t buffer[1] = { result->answer_code };
                write(destination, buffer, sizeof(uint8_t));
            #endif

            print_log("Answer code: %i", result->answer_code);
        }

        kernel_free_answer(result);
    }
}

/*
 * Server setup function
*/
int main() {
    /*
    Enable traceback for current session.
    */
    TB_enable();

    #ifdef DESKTOP

        kernel_answer_t* result = kernel_process_command(argc, argv, 1);
        if (result->answer_body != NULL) {
            printf("%s\nCode: %i\n", result->answer_body, result->answer_code);
        }
        else {
            printf("Code: %i\n", result->answer_code);
        }

    #else

        #ifdef _WIN32
            WSADATA wsa_data;
            if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
                print_error("HH_ERROR: WSAStartup() failed");
                return -1;
            }
        #endif

        int server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket == -1) {
            print_error("HH_ERROR: error in calling socket()");
            return -1;
        }

        struct sockaddr_in server_address;
        struct sockaddr_in client_address;

        server_address.sin_family = AF_INET;
        server_address.sin_port = htons(CDBMS_SERVER_PORT);
        server_address.sin_addr.s_addr = htonl(INADDR_ANY);

        if (bind(server_socket, (struct sockaddr*) &server_address, sizeof(server_address)) < 0) {
            print_error("HH_ERROR: bind() call failed");
            return -1;
        }

        if (listen(server_socket, 5) < 0) {
            print_error("HH_ERROR: listen() call failed");
            return -1;
        }

        print_info("DB server started on %s:%d", inet_ntoa(server_address.sin_addr), ntohs(server_address.sin_port));

        while (1) {
            int client_socket_fd   = -1;
            int client_address_len = -1;

            client_address_len = sizeof(client_address);
            client_socket_fd   = accept(server_socket, (struct sockaddr *)&client_address, (socklen_t*)&client_address_len);
            if (client_socket_fd < 0) {
                print_error("HH_ERROR: accept() call failed");
                continue;
            }

            print_info("Client connected from %s:%d", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
            send2kernel(client_socket_fd, client_socket_fd);

            #ifdef _WIN32
                closesocket(client_socket_fd);
            #else
                close(client_socket_fd);
            #endif
        }

        cleanup();

    #endif

	return 1;
}
