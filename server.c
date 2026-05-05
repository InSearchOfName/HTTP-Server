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

static int send_all(int fd, const char *data, size_t total)
{
    size_t sent = 0;

    while (sent < total)
    {
        ssize_t written = write(fd, data + sent, total - sent);

        if (written <= 0)
        {
            perror("write");
            return -1;
        }

        sent += (size_t)written;
    }

    return 0;
}

static void process_buffered_requests(int client_sockfd, Buffer *buf)
{
    ssize_t req_end;

    while ((req_end = buffer_find(buf, "\r\n\r\n", 4)) >= 0)
    {
        char request_line[1024];
        size_t max_copy = sizeof(request_line) - 1;
        size_t copy_len = ((size_t)req_end < max_copy)
                              ? (size_t)req_end
                              : max_copy;

        memcpy(request_line, buf->data, copy_len);
        request_line[copy_len] = '\0';

        char method[16], path[256], version[16];
        char *response;

        if (sscanf(request_line, "%15s %255s %15s", method, path, version) != 3)
        {
            buffer_consume(buf, (size_t)req_end + 4);
            continue;
        }

        size_t response_len = 0;

        response = handle_response(path, &response_len);
        if (!response)
        {
            buffer_consume(buf, (size_t)req_end + 4);
            continue;
        }

        if (send_all(client_sockfd, response, response_len) < 0)
        {
            free(response);
            buffer_consume(buf, (size_t)req_end + 4);
            continue;
        }

        free(response);
        buffer_consume(buf, (size_t)req_end + 4);
    }
}

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
    if (buffer_init(&buf, 4096) != 0)
    {
        fprintf(stderr, "Failed to initialize request buffer\n");
        return;
    }

    char tmp[4096];

    while (1)
    {
        ssize_t n = read(client_sockfd, tmp, sizeof(tmp));
        if (n <= 0)
            break;

        if (buffer_append(&buf, tmp, (size_t)n) != 0)
        {
            fprintf(stderr, "Failed to append to request buffer\n");
            break;
        }

        process_buffered_requests(client_sockfd, &buf);
    }

    buffer_free(&buf);
}

static char *duplicate_response(const char *text, size_t text_len, size_t *response_len)
{
    char *response = malloc(text_len + 1);

    if (!response)
    {
        return NULL;
    }

    memcpy(response, text, text_len);
    response[text_len] = '\0';
    *response_len = text_len;

    return response;
}

char *handle_response(const char *path, size_t *response_len)
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

    if (!file_exists(filepath))
    {
        return duplicate_response(
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/html\r\n\r\n"
            "<h1>404 - File Not Found</h1>",
            sizeof("HTTP/1.1 404 Not Found\r\n"
                   "Content-Type: text/html\r\n\r\n"
                   "<h1>404 - File Not Found</h1>") -
                1,
            response_len);
    }

    FILE *file = fopen(filepath, "r");
    if (file == NULL)
    {
        return duplicate_response(
            "HTTP/1.1 500 Internal Server Error\r\n\r\n",
            sizeof("HTTP/1.1 500 Internal Server Error\r\n\r\n") - 1,
            response_len);
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Allocate buffer for headers + content
    char *response = malloc(512 + file_size + 1);
    if (!response)
    {
        fclose(file);
        return NULL;
    }

    // Write headers
    size_t header_len = (size_t)snprintf(response,
                                         512 + file_size + 1,
                                         "HTTP/1.1 200 OK\r\n"
                                         "Content-Type: text/html\r\n"
                                         "Content-Length: %ld\r\n"
                                         "\r\n",
                                         file_size);

    if (header_len >= (size_t)(512 + file_size + 1))
    {
        fclose(file);
        free(response);
        return NULL;
    }

    // Read file content after headers
    fread(response + header_len, 1, file_size, file);
    response[header_len + file_size] = '\0';
    *response_len = header_len + (size_t)file_size;

    fclose(file);
    return response;
}

int file_exists(const char *path)
{
    struct stat buffer;
    return stat(path, &buffer) == 0 ? 1 : 0;
}