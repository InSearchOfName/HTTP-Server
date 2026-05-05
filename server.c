#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include "server.h"

int server_sockfd = -1; // Global to track the socket

int start_server(int port)
{
    // Create socket
    server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sockfd < 0)
    {
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
    if (bind(server_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Bind failed");
        close(server_sockfd);
        server_sockfd = -1;
        return -1;
    }

    // Listen for connections
    if (listen(server_sockfd, 5) < 0)
    {
        perror("Listen failed");
        close(server_sockfd);
        server_sockfd = -1;
        return -1;
    }

    printf("server started on port: %d \n", port);
    return 0;
}

void stop_server()
{
    if (server_sockfd >= 0)
    {
        close(server_sockfd);
        server_sockfd = -1;
        printf("\nServer stopped\n");
    }
}

void accept_client()
{
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    int client_sockfd = accept(server_sockfd, (struct sockaddr *)&client_addr, &client_len);

    if (client_sockfd < 0)
    {
        perror("Failed to accept client\n");
        return;
    }
    printf("Client connected!\n");
    handle_client(client_sockfd);
    close(client_sockfd);
}

void handle_client(int client_sockfd)
{
    Buffer buf;
    buffer_init(&buf, 4096);

    char tmp[4096];

    while (1)
    {
        ssize_t n = read(client_sockfd, tmp, sizeof(tmp));
        if (n <= 0)
            break;

        buffer_append(&buf, tmp, (size_t)n);

        ssize_t req_end;

        // process all complete HTTP requests in buffer
        while ((req_end = buffer_find(&buf, "\r\n\r\n", 4)) >= 0)
        {
            char request_line[1024];

            // SAFE boundary handling (fixes signed/unsigned issue)
            size_t max_copy = sizeof(request_line) - 1;

            size_t copy_len = ((size_t)req_end < max_copy)
                ? (size_t)req_end
                : max_copy;

            memcpy(request_line, buf.data, copy_len);
            request_line[copy_len] = '\0';

            char method[16], path[256], version[16];

            if (sscanf(request_line, "%15s %255s %15s", method, path, version) != 3)
            {
                buffer_consume(&buf, (size_t)req_end + 4);
                continue;
            }

            char *response = handle_response(path);

            // SAFE write loop (handles partial writes)
            size_t total = strlen(response);
            size_t sent = 0;

            while (sent < total)
            {
                ssize_t w = write(client_sockfd, response + sent, total - sent);

                if (w <= 0)
                {
                    perror("write");
                    break;
                }

                sent += (size_t)w;
            }

            free(response);

            // consume full request including "\r\n\r\n"
            buffer_consume(&buf, (size_t)req_end + 4);
        }
    }

    buffer_free(&buf);
}

char *handle_response(char *path)
{
    char filepath[512] = ".";

    if (strcmp(path, "/") == 0)
    {
        strcat(filepath, "/index.html");
    }
    else
    {
        strcat(filepath, path);
    }

    if (file_exists(filepath))
    {
        FILE *file = fopen(filepath, "r");
        if (file == NULL)
        {
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
                "\r\n",
                file_size);

        // Read file content after headers
        int header_len = strlen(response);
        fread(response + header_len, 1, file_size, file);
        response[header_len + file_size] = '\0';

        fclose(file);
        return response;
    }
    else
    {
        char *response = strdup(
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/html\r\n\r\n"
            "<h1>404 - File Not Found</h1>");
        return response;
    }
}

int file_exists(const char *path)
{
    struct stat buffer;
    return stat(path, &buffer) == 0 ? 1 : 0;
}