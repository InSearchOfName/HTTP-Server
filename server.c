#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include "server.h"

int server_sockfd = -1;  // Global to track the socket

int start_server(int port){
    // Create socket
    server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sockfd < 0) {
        perror("Socket creation failed");
        return -1;
    }
    
    // Configure server address
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    // Bind socket to address
    if (bind(server_sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_sockfd);
        server_sockfd = -1;
        return -1;
    }
    
    // Listen for connections
    if (listen(server_sockfd, 5) < 0) {
        perror("Listen failed");
        close(server_sockfd);
        server_sockfd = -1;
        return -1;
    }
    
    printf("server started on port: %d \n", port);
    return 0;
}

void stop_server(){
    if (server_sockfd >= 0) {
        close(server_sockfd);
        server_sockfd = -1;
        printf("\nServer stopped\n");
    }
}

void accept_client(){
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    int client_sockfd = accept(server_sockfd, (struct sockaddr*)&client_addr, &client_len);

    if (client_sockfd < 0) {
        perror("Failed to accept client\n");
        return;
    }
    printf("Client connected!\n");
    handle_client(client_sockfd);
    close(client_sockfd); 
}

void handle_client(int client_sockfd){
    char buffer[8192] = {0};
    read(client_sockfd, buffer, sizeof(buffer) - 1);
    //Logging 
    printf("\n%s\n", buffer);


    char method[16], path[256], version[16];
    sscanf(buffer, "%15s %255s %15s", method, path, version);

    char *response = handle_response(path);
    write(client_sockfd, response, strlen(response));
}

char* handle_response(char *path){
    char filepath[512] = ".";
    
    if (strcmp(path, "/") == 0){
        strcat(filepath, "/index.html");
    } else {
        strcat(filepath, path);
    }
    
    if (file_exists(filepath)) {
        FILE *file = fopen(filepath, "r");
        if (file == NULL) {
            return "HTTP/1.1 500 Internal Server Error\r\n\r\n";
        }
        
        // Get file size
        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        fseek(file, 0, SEEK_SET);
        
        // Allocate buffer for headers + content
        char *response = malloc(512 + file_size + 1);
        
        // Write headers
        sprintf(response, 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: %ld\r\n"
            "\r\n", file_size);
        
        // Read file content after headers
        int header_len = strlen(response);
        fread(response + header_len, 1, file_size, file);
        response[header_len + file_size] = '\0';
        
        fclose(file);
        return response;
    } else {
        return "HTTP/1.1 404 Not Found\r\n"
               "Content-Type: text/html\r\n\r\n"
               "<h1>404 - File Not Found</h1>";
    }
}

int file_exists(const char *path)
{
    struct stat buffer;
    return stat(path, &buffer) == 0 ? 1 : 0;
}