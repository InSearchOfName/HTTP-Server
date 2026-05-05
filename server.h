#ifndef SERVER_H
#define SERVER_H
#include "buffer.h"

typedef struct {
    int fd;
    Buffer buffer;
    int should_close;
} Connection;

extern int server_sockfd;

int start_server(int port);
void stop_server();
void accept_client();
void handle_client(int client_sockfd);
char* handle_response(const char *path);
int file_exists(const char *path);

#endif