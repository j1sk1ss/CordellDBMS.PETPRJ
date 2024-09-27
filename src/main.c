/*
 * Main file. This is main Cordell Database Manager Studio File, where
 * placed init stuff with server side.
 *
 * Cordell DBMS is a light weight data base manager studio. Main idea
 * that we can work with big data by using very light weighten app.
 *
 * Unix:
 * building without OMP: gcc-14 -Wall main.c kernel\kentry.c kernel\std\* kernel\arch\* -Wunknown-pragmas -fpermissive -o cdbms.bin
 * building with OMP: gcc-14 -Wall main.c kernel\kentry.c kernel\std\* kernel\arch\* -fopenmp -fpermissive -o cdbms.bin
 *
 * Win10/Win11:
 * building without OMP: .\main.c .\kernel\kentry.c .\kernel\std\* .\kernel\arch\* -Wunknown-pragmas -lws2_32 -fopenmp -fpermissive -o cdbms_win_x86-64_omp.exe
 * building with OMP: .\main.c .\kernel\kentry.c .\kernel\std\* .\kernel\arch\* -Wunknown-pragmas -lws2_32 -fpermissive -o cdbms_win_x86-64_omp.exe
 *
 * Base code of sockets took from: https://devhops.ru/code/c/sockets.php
*/

#include "kernel/include/kentry.h"
#include "kernel/include/logging.h"

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

#define CDBMS_SERVER_PORT   getenv("CDBMS_SERVER_PORT") == NULL ? 1010 : atoi(getenv("CDBMS_SERVER_PORT"))
#define MESSAGE_BUFFER      2048
#define COMMANDS_BUFFER     256


void cleanup();
int setup_server();
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
    int count = 0;
    uint8_t buffer[MESSAGE_BUFFER];

    #ifdef _WIN32
    while ((count = recv(source, (char*)buffer, MESSAGE_BUFFER, 0)) > 0)
    #else
    while ((count = read(source, buffer, MESSAGE_BUFFER)) > 0)
    #endif
    {

        buffer[count - 1] = '\0';

        char* argv[COMMANDS_BUFFER];
        char* current_arg = NULL;

        int argc = 1;
        int in_quotes = 0;

        for (char *p = (char *)buffer; *p != '\0'; p++) {
            if (*p == '"') {
                in_quotes = !in_quotes;
                if (in_quotes) {
                    current_arg = p + 1;
                } 
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
            else if (!current_arg) {
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
        else {
            #ifdef _WIN32
                uint8_t buffer[1] = { result->answer_code };
                send(destination, (const char*)buffer, sizeof(uint8_t), 0);
            #else
                write(destination, result->answer_code, sizeof(uint8_t));
            #endif

            print_log("Answer code: %i", result->answer_code);
        }

        kernel_free_answer(result);
    }
}

/*
Server setup function
*/
int main(int argc, char* argv[]) {
    int reciever        = -1;
    int server_socket   = -1;
    char server_message[MESSAGE_BUFFER];

    #ifdef _WIN32
        WSADATA wsa_data;
        reciever = WSAStartup(MAKEWORD(2, 2), &wsa_data);
        if (reciever != 0) {
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
    server_address.sin_port   = htons(CDBMS_SERVER_PORT);
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);

    reciever = bind(server_socket, (struct sockaddr*) &server_address, sizeof(server_address));
    if (reciever < 0) {
        print_error("HH_ERROR: bind() call failed");
        return -1;
    }

    reciever = listen(server_socket, 5);
    if (reciever < 0) {
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

        char* client_ip = inet_ntoa(client_address.sin_addr);
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
