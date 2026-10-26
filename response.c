#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include "response.h"

#define DOC_ROOT "./public"
#define MAX_FILE_SIZE (64L * 1024 * 1024)

enum
{
    PATH_OK = 0,
    PATH_NOT_FOUND = -1,
    PATH_FORBIDDEN = -2
};

static int is_regular_file(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static int hex_value(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

/* Strips the query/fragment and percent-decodes the request path.
 * Returns -1 on malformed escapes, an embedded NUL, or if it does not fit. */
static int decode_path(const char *path, char *out, size_t out_size)
{
    size_t n = 0;

    size_t i = 0;

    while (path[i] && path[i] != '?' && path[i] != '#')
    {
        char c = path[i];
        size_t consumed = 1;

        if (c == '%')
        {
            int hi = hex_value(path[i + 1]);
            int lo = hi < 0 ? -1 : hex_value(path[i + 2]);

            if (hi < 0 || lo < 0)
                return -1;

            c = (char)(hi * 16 + lo);
            consumed = 3;

            if (c == '\0')
                return -1;
        }

        if (n + 1 >= out_size)
            return -1;

        out[n++] = c;
        i += consumed;
    }

    out[n] = '\0';
    return 0;
}

/* Rejects backslashes and any ".." or dot-prefixed segment. */
static int path_is_safe(const char *path)
{
    if (path[0] != '/' || strchr(path, '\\'))
        return 0;

    for (const char *seg = path; seg && *seg; seg = strchr(seg + 1, '/'))
    {
        if (seg[1] == '.')
            return 0;
    }

    return 1;
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
    char decoded[256];
    char candidate[512];
    char real_root[PATH_MAX];
    char real_file[PATH_MAX];

    if (decode_path(path, decoded, sizeof(decoded)) != 0 || !path_is_safe(decoded))
    {
        return PATH_FORBIDDEN;
    }

    int written;
    if (strcmp(decoded, "/") == 0)
    {
        written = snprintf(candidate, sizeof(candidate), "%s/index.html", DOC_ROOT);
    }
    else
    {
        written = snprintf(candidate, sizeof(candidate), "%s%s", DOC_ROOT, decoded);
    }

    if (written < 0 || (size_t)written >= sizeof(candidate))
    {
        return PATH_FORBIDDEN;
    }

    if (!realpath(DOC_ROOT, real_root) || !realpath(candidate, real_file))
    {
        return PATH_NOT_FOUND;
    }

    /* Symlinks must not lead out of the document root. */
    size_t root_len = strlen(real_root);
    if (strncmp(real_file, real_root, root_len) != 0 || real_file[root_len] != '/')
    {
        return PATH_FORBIDDEN;
    }

    if (!is_regular_file(real_file))
    {
        return PATH_NOT_FOUND;
    }

    if (strlen(real_file) >= filepath_size)
    {
        return PATH_FORBIDDEN;
    }

    strcpy(filepath, real_file);
    return PATH_OK;
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
    char filepath[PATH_MAX];
    const char *content_type;

    int resolved = resolve_filepath(path, filepath, sizeof(filepath));

    if (resolved == PATH_FORBIDDEN)
    {
        return build_error_response("HTTP/1.1 403 Forbidden",
                                    "<h1>403 - Forbidden</h1>",
                                    "text/html",
                                    response_len);
    }

    if (resolved != PATH_OK)
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
    if (fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        return build_error_response("HTTP/1.1 500 Internal Server Error",
                                    "<h1>500 - Internal Server Error</h1>",
                                    "text/html",
                                    response_len);
    }

    long file_size = ftell(file);

    if (file_size < 0 || file_size > MAX_FILE_SIZE || fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        return build_error_response("HTTP/1.1 500 Internal Server Error",
                                    "<h1>500 - Internal Server Error</h1>",
                                    "text/html",
                                    response_len);
    }

    size_t body_len = (size_t)file_size;
    char header[256];
    int header_size = snprintf(header,
                               sizeof(header),
                               "HTTP/1.1 200 OK\r\n"
                               "Content-Type: %s\r\n"
                               "Content-Length: %zu\r\n"
                               "\r\n",
                               content_type,
                               body_len);

    if (header_size < 0 || (size_t)header_size >= sizeof(header))
    {
        fclose(file);
        return NULL;
    }

    size_t header_len = (size_t)header_size;
    char *response = malloc(header_len + body_len);
    if (!response)
    {
        fclose(file);
        return NULL;
    }

    memcpy(response, header, header_len);

    if (fread(response + header_len, 1, body_len, file) != body_len)
    {
        fclose(file);
        free(response);
        return build_error_response("HTTP/1.1 500 Internal Server Error",
                                    "<h1>500 - Internal Server Error</h1>",
                                    "text/html",
                                    response_len);
    }

    *response_len = header_len + body_len;

    fclose(file);
    return response;
}
