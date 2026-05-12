#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "response.h"

static int file_exists(const char *path)
{
    struct stat buffer;
    return stat(path, &buffer) == 0 ? 1 : 0;
}

static char *build_error_response(const char *status_line, const char *body, const char *content_type, size_t *response_len)
{
    size_t body_len = strlen(body);
    int header_size = snprintf(NULL,
                               0,
                               "%s\r\n"
                               "Content-Type: %s\r\n"
                               "Content-Length: %zu\r\n"
                               "\r\n",
                               status_line,
                               content_type,
                               body_len);

    if (header_size < 0)
    {
        return NULL;
    }

    size_t header_len = (size_t)header_size;

    char *response = malloc(header_len + body_len + 1);

    if (!response)
    {
        return NULL;
    }

    size_t written = (size_t)snprintf(response,
                                      header_len + 1,
                                      "%s\r\n"
                                      "Content-Type: %s\r\n"
                                      "Content-Length: %zu\r\n"
                                      "\r\n",
                                      status_line,
                                      content_type,
                                      body_len);

    memcpy(response + written, body, body_len);
    response[written + body_len] = '\0';
    *response_len = written + body_len;

    return response;
}

static int resolve_filepath(const char *path, char *filepath, size_t filepath_size)
{
    const char *search_roots[] = {"./dist", "."};

    for (size_t i = 0; i < sizeof(search_roots) / sizeof(search_roots[0]); i++)
    {
        const char *root = search_roots[i];
        int written;

        if (strcmp(path, "/") == 0)
        {
            written = snprintf(filepath, filepath_size, "%s/index.html", root);
        }
        else
        {
            written = snprintf(filepath, filepath_size, "%s%s", root, path);
        }

        if (written < 0 || (size_t)written >= filepath_size)
        {
            continue;
        }

        if (file_exists(filepath))
        {
            return 0;
        }
    }

    return -1;
}

static const char *content_type_for_path(const char *path)
{
    const char *extension = strrchr(path, '.');

    if (!extension)
    {
        return "text/html";
    }

    if (strcmp(extension, ".html") == 0 || strcmp(extension, ".htm") == 0)
    {
        return "text/html";
    }

    if (strcmp(extension, ".js") == 0)
    {
        return "application/javascript";
    }

    if (strcmp(extension, ".css") == 0)
    {
        return "text/css";
    }

    if (strcmp(extension, ".json") == 0 || strcmp(extension, ".webmanifest") == 0)
    {
        return "application/json";
    }

    if (strcmp(extension, ".png") == 0)
    {
        return "image/png";
    }

    if (strcmp(extension, ".jpg") == 0 || strcmp(extension, ".jpeg") == 0)
    {
        return "image/jpeg";
    }

    if (strcmp(extension, ".svg") == 0)
    {
        return "image/svg+xml";
    }

    if (strcmp(extension, ".ico") == 0)
    {
        return "image/x-icon";
    }

    if (strcmp(extension, ".txt") == 0)
    {
        return "text/plain";
    }

    return "application/octet-stream";
}

char *handle_response(const char *path, size_t *response_len)
{
    char filepath[512] = ".";
    const char *content_type;

    if (resolve_filepath(path, filepath, sizeof(filepath)) != 0)
    {
        return build_error_response("HTTP/1.1 404 Not Found",
                                    "<h1>404 - File Not Found</h1>",
                                    "text/html",
                                    response_len);
    }

    content_type = content_type_for_path(filepath);

    FILE *file = fopen(filepath, "rb");
    if (file == NULL)
    {
        return build_error_response("HTTP/1.1 500 Internal Server Error",
                                    "<h1>500 - Internal Server Error</h1>",
                                    "text/html",
                                    response_len);
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);

    if (file_size < 0)
    {
        fclose(file);
        return build_error_response("HTTP/1.1 500 Internal Server Error",
                                    "<h1>500 - Internal Server Error</h1>",
                                    "text/html",
                                    response_len);
    }

    fseek(file, 0, SEEK_SET);

    char *response = malloc(512 + file_size + 1);
    if (!response)
    {
        fclose(file);
        return NULL;
    }

    size_t header_len = (size_t)snprintf(response,
                                         512 + file_size + 1,
                                         "HTTP/1.1 200 OK\r\n"
                                         "Content-Type: %s\r\n"
                                         "Content-Length: %ld\r\n"
                                         "\r\n",
                                         content_type,
                                         file_size);

    if (header_len >= (size_t)(512 + file_size + 1))
    {
        fclose(file);
        free(response);
        return NULL;
    }

    fread(response + header_len, 1, file_size, file);
    response[header_len + file_size] = '\0';
    *response_len = header_len + (size_t)file_size;

    fclose(file);
    return response;
}
