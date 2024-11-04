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

#include "userland/include/user.h"
#include "kernel/include/kentry.h"
#include "kernel/include/logging.h"
#include "kernel/include/cache.h"

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
    #include <pthread.h>
#endif


#define CDBMS_SERVER_PORT   ENV_GET("CDBMS_SERVER_PORT", "7777")
#define MESSAGE_BUFFER      2048
#define COMMANDS_BUFFER     256
#define MAX_SESSION_COUNT   5


void cleanup();
int send2destination(int destination, void* data, size_t data_size);
int send2destination_byte(int destination, int byte);
void* handle_client(void* client_socket_fd);
void start_kernel_session(int source, int destination, int session);
int setup_server();


static int sessions[MAX_SESSION_COUNT] = { 0 };


void cleanup() {
    #ifdef _WIN32
        WSACleanup();
    #endif
}

#pragma region [Send functions]

    int send2destination(int destination, void* data, size_t data_size) {
        #ifdef _WIN32
            send(destination, (const char*)data, data_size, 0);
        #else
            write(destination, data, data_size);
        #endif

        return 1;
    }

    int send2destination_byte(int destination, int byte) {
        return send2destination(destination, &byte, 1);
    }

#pragma endregion

void* handle_client(void* client_socket_fd) {
    int socket_fd = ((int*)client_socket_fd)[0];
    int session = ((int*)client_socket_fd)[1];

    start_kernel_session(socket_fd, socket_fd, session);
    close_connection(session);
    close(socket_fd);

    sessions[session] = 0;
    print_info("Session [%i] closed", session);
    free(client_socket_fd);

    #ifndef _WIN32
    pthread_exit(NULL);
    close(socket_fd);
    #else
    closesocket(client_socket_fd);
    #endif

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
void start_kernel_session(int source, int destination, int session) {
    user_t* user = NULL;
    int count = 0;
    uint8_t buffer[MESSAGE_BUFFER];

    #ifdef _WIN32
    while ((count = recv(source, (char*)buffer, MESSAGE_BUFFER, 0)) > 0)
    #else
    while ((count = read(source, buffer, MESSAGE_BUFFER)) > 0)
    #endif
    {
        buffer[count - 1] = '\0';
        print_info("Session [%i]: [%s]", session, buffer);

        if (user == NULL) {
            char username[USERNAME_SIZE];
            char password[128];
            sscanf((char*)buffer, "%[^:]:%s", username, password);

            #ifndef USER_DEBUG
                user = USR_auth(username, password);
                if (user == NULL) {
                    print_error("Wrong password [%s] for user [%s] at session [%i]", user->name, password, session);
                    send2destination_byte(destination, 0);
                }
                else {
                    print_info("User [%s] auth succes in session [%i]", user->name, session);
                    send2destination_byte(destination, 1);
                }
            #else
                user = USR_create(username, password, CREATE_ACCESS_BYTE(0, 0, 0));
                USR_save(user, NULL);
                send2destination_byte(destination, 1);
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

        if (current_arg) argv[argc++] = current_arg;
        kernel_answer_t* result = kernel_process_command(argc, argv, 0, user->access, session);
        if (result->answer_body != NULL) {
            send2destination(destination, result->answer_body, result->answer_size);
            print_log("Answer body: [%.*s]", result->answer_size, result->answer_body);
        }
        else {
            send2destination_byte(destination, result->answer_code);
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
#ifdef DESKTOP
int main(int argc, char* argv[])
#else
int main()
#endif
{
    /*
    Enable traceback for current session.
    */
    print_info("Cordell Database Manager Studio server-side.");
    print_info("Current version of kernel is [%s].", KERNEL_VERSION);

    TB_enable();
    CL_enable();
    CHC_init();

    #ifdef DESKTOP

        kernel_answer_t* result = kernel_process_command(argc, argv, 1, CREATE_ACCESS_BYTE(3, 3, 3));
        if (result->answer_body != NULL) print_info("Answer body: [%s]\nAnswer code: [%i]", result->answer_body, result->answer_code);
        else print_info("Answer code: [%i]", result->answer_code);

    #else

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

        struct sockaddr_in server_address;
        struct sockaddr_in client_address;

        int server_port = atoi(CDBMS_SERVER_PORT);
        server_address.sin_port = htons(server_port);
        server_address.sin_family = AF_INET;
        server_address.sin_addr.s_addr = htonl(INADDR_ANY);

        if (bind(server_socket, (struct sockaddr*) &server_address, sizeof(server_address)) < 0) {
            print_error("bind() call failed");
            return -2;
        }

        if (listen(server_socket, 5) < 0) {
            print_error("listen() call failed");
            return -3;
        }

        print_info("DB server started on %s:%d", inet_ntoa(server_address.sin_addr), ntohs(server_address.sin_port));

        while (1) {
            int client_socket_fd   = -1;
            int client_address_len = -1;

            client_address_len = sizeof(client_address);
            client_socket_fd   = accept(server_socket, (struct sockaddr *)&client_address, (socklen_t*)&client_address_len);
            if (client_socket_fd < 0) {
                print_error("accept() call failed");
                continue;
            }

            int* client_socket_fd_ptr = malloc(sizeof(int) * 3);
            client_socket_fd_ptr[0] = client_socket_fd;

            int session = 0;
            while (sessions[session] == 1) {
                if (session++ >= MAX_SESSION_COUNT) session = 0;
            }

            sessions[session] = 1;
            client_socket_fd_ptr[1] = session;

            print_info("Client connected from %s:%d", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
            if (THR_create_thread(handle_client, client_socket_fd_ptr) != 1) {
                print_error("Error while server try to create thread for [%i] session", session);
            }
        }

        cleanup();

    #endif

    return 1;
}
