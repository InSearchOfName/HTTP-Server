#ifndef SERVER_H
#define SERVER_H

extern int server_sockfd;

int start_server(int port);
void stop_server();
void accept_client();
void handle_client(int client_sockfd);
char* handle_response(char *path);
char* handle_response(char *path);
int file_exists(const char *path);

#endif