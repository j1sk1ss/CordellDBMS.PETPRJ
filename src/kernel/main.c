/*
 * Main file. This is main Cordell Database Manager Studio File, where
 * placed init stuff with server side.
 *
 * Cordell DBMS is a light weight data base manager studio. Main idea
 * that we can work with big data by using very light weighten app.
 *
 * Unix:
 * building without OMP: gcc-14 -Wall main.c kentry.c std\* arch\* -Wunknown-pragmas -fpermissive -o cdbms.bin
 * building with OMP: gcc-14 -Wall main.c kentry.c std\* arch\* -fopenmp -fpermissive -o cdbms.bin
 *
 * Win10/Win11:
 * building without OMP: gcc -Wall .\main.c .\kentry.c .\std\* .\arch\* -Wunknown-pragmas -lws2_32 -fpermissive -o cdbms.exe
 * building with OMP: gcc -Wall .\main.c .\kentry.c .\std\* .\arch\* -fopenmp -lws2_32 -fpermissive -o cdbms.exe
 *
 * Base code of sockets took from: https://devhops.ru/code/c/sockets.php
*/

#include "include/kentry.h"
#include "include/logging.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <unistd.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
#endif

#define SERVER_PORT     8888
#define MESSAGE_BUFFER  2048
#define COMMANDS_BUFFER 256

#define COMMAND_DELIMITER   0x1F


int setup_server();
void cleanup();
void send2kernel(int source, int destination);


void cleanup() {
    #ifdef _WIN32
        WSACleanup();
    #endif
}

/*
This function takes source data and destination address for kernel answer.
In source should be provided correct command for kernel.

Params:
- source - source FD with commands.
- destination - destination FD for kernel answer.
*/
void send2kernel(int source, int destination) {
    unsigned char buffer[MESSAGE_BUFFER];

    int count = 0;

    #ifdef _WIN32
        while ((count = recv(source, (char*)buffer, MESSAGE_BUFFER, 0)) > 0) {
    #else
        while ((count = read(source, buffer, MESSAGE_BUFFER)) > 0) {
    #endif

        buffer[count - 1] = '\0';

        char* argv[COMMANDS_BUFFER];
        int argc = 1;

        int in_quotes = 0;
        char* current_arg = NULL;

        for (char *p = (char *)buffer; *p != '\0'; p++) {
            if (*p == '"') {
                in_quotes = !in_quotes;
                if (in_quotes) {
                    current_arg = p + 1;
                } else {
                    *p = '\0';
                    argv[argc++] = current_arg;
                    current_arg = NULL;
                }
            } else if (!in_quotes && (*p == ' ' || *p == '\t')) {
                *p = '\0';
                if (current_arg) {
                    argv[argc++] = current_arg;
                    current_arg = NULL;
                }
            } else if (!current_arg) {
                current_arg = p;
            }
        }

        if (current_arg) {
            argv[argc++] = current_arg;
        }

        kernel_answer_t* result = kernel_process_command(argc, argv);
        if (result->answer_body != NULL) {
            #ifdef _WIN32
                send(destination, (const char*)result->answer_body, result->answer_size, 0);
            #else
                write(destination, result->answer_body, result->answer_size);
            #endif
        }
        
        kernel_free_answer(result);
    }
}

/*
Server setup function
*/
int main(int argc, char* argv[]) {
    int rc = -1;
    int server_socket = -1;
    char server_message[MESSAGE_BUFFER];

    #ifdef _WIN32
        WSADATA wsa_data;
        rc = WSAStartup(MAKEWORD(2, 2), &wsa_data);
        if (rc != 0) {
            print_error("HH_ERROR: WSAStartup() failed");
            return -1;
        }
    #endif

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        print_error("HH_ERROR: error in calling socket()");
        return -1;
    }

    struct sockaddr_in server_address;
    struct sockaddr_in client_address;

    server_address.sin_family = AF_INET;
    server_address.sin_port   = htons(SERVER_PORT);

    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    rc = bind(server_socket, (struct sockaddr*) &server_address, sizeof(server_address));

    if (rc < 0) {
        print_error("HH_ERROR: bind() call failed");
        return -1;
    }

    rc = listen(server_socket, 5);
    if (rc < 0) {
        print_error("HH_ERROR: listen() call failed");
        return -1;
    }

    char *server_ip = inet_ntoa(server_address.sin_addr);
    int server_port = ntohs(server_address.sin_port);
    print_info("DB server started on IP %s and port %d\n", server_ip, server_port);

    int keep_socket = 1;
    while (keep_socket) {
        int client_socket_fd = -1;
        int client_address_len = -1;

        client_address_len = sizeof(client_address);
        client_socket_fd   = accept(server_socket, (struct sockaddr *)&client_address, (socklen_t*)&client_address_len);
        if (client_socket_fd < 0) {
            print_error("HH_ERROR: accept() call failed");
            continue;
        }

        char *client_ip = inet_ntoa(client_address.sin_addr);
        int client_port = ntohs(client_address.sin_port);
        print_info("Client connected from IP %s and port %d\n", client_ip, client_port);

        send2kernel(client_socket_fd, client_socket_fd);
        send(client_socket_fd, server_message, sizeof(server_message), 0);

        #ifdef _WIN32
            closesocket(client_socket_fd);
        #else
            close(client_socket_fd);
        #endif
    }

    cleanup();
	return 1;
}
