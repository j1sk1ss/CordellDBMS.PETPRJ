/*
Code took from: https://devhops.ru/code/c/sockets.php
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#ifndef _WIN32
    #include <unistd.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
#endif


#define SERVER_PORT 9002


void processing_message(unsigned char *s, int n) {
    unsigned char *p;
    for (p=s; p < s + n; p++) {
        if (islower(*p)) {
            *p += 13;
            if(*p > 'z') *p -= 26;
        }
    }
}  


void processing_message_service(int in, int out) {
    unsigned char buf[1024];
    int count;

    while  ((count = read(in, buf, 1024)) > 0) {
        processing_message(buf, count);
        write(out, buf, count);
    }

}  


int main() {
    #ifndef _WIN32
        int rc;
        char server_message[1024];
        int server_socket;

        server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket == -1) {
            perror("HH_ERROR: error in calling socket()");
            exit(1);
        };

        // define the server address
        // htons means host to network short and is related to
        // network byte order
        // htonl host to network long
        struct sockaddr_in server_address;
        struct sockaddr_in client_address;

        server_address.sin_family = AF_INET;           // IPv4
        server_address.sin_port   = htons(SERVER_PORT);
        // port is 16 bit so htons is enough.
        // htons returns host_uint16 converted to network byte order
        server_address.sin_addr.s_addr = htonl(INADDR_ANY);
        // returns host_uint32 converted to network byte order

        // bind the socket to our specified IP and port
        // int bind(int sockfd, const struct  sockaddr *addr,
        // socklen_t addrlen);
        // sockfd argument is a file descriptor obtained from a
        // previous call to socket()

        // addr agrument is a pointer to a struckture specifying
        // the address to which this socket is to be bound

        rc = bind(server_socket, (struct sockaddr*) &server_address, sizeof(server_address));

        // bind should return 0;
        if (rc < 0) {
            perror("HH_ERROR: bind() call failed");
            exit(1);
        }

        // second agrument is a backlog - how many connections
        // can be waiting for this socket simultaneously
        rc = listen(server_socket, 5);

        if (rc < 0) {
            perror("HH_ERROR: listen() call failed");
            exit(1);
        }

        printf("DB server started ...\n");

        int keep_socket = 1;
        while (keep_socket) {

            int client_socket_fd;
            int client_address_len;

            // int accept(int sockfd, struct sockaddr *addr,
            // socklen_t *addrlen);
            // the application that called listen() then accepts
            // the connection using accept()
            // accept() creates a new socket and this new socket
            // is connected to the peer socket that performed the
            // connect()
            // a file descriptor for the connected socket
            // is returned as the function result of the accept() call
            // client_socket = accept(server_socket, NULL, NULL);

            client_address_len = sizeof(client_address);
            client_socket_fd   = accept(server_socket, (struct sockaddr *)&client_address, &client_address_len);
            if (client_socket_fd < 0) {
                perror("HH_ERROR: accept() call failed");
                continue;
            }

            // send the message
            // send(client_socket_fd, server_message, sizeof(server_message), 0);

            processing_message_service(client_socket_fd, client_socket_fd);
            close(client_socket_fd);
        };

        // close the socket

    #endif
	return 1;
}  