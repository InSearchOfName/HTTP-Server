#ifndef BUFFER_H
#define BUFFER_H

#include <stddef.h>

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} Buffer;

int buffer_init(Buffer *b, size_t initial_size);
void buffer_free(Buffer *b);

int buffer_append(Buffer *b, const char *data, size_t len);
void buffer_consume(Buffer *b, size_t len);

char *buffer_find(Buffer *b, const char *pattern, size_t pattern_len);

#endif