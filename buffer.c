#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "buffer.h"

static int buffer_grow(Buffer *b, size_t needed);

int buffer_init(Buffer *b, size_t initial_size)
{
    b->data = malloc(initial_size);
    if (!b->data)
    {
        return -1;
    }
    b->len = 0;
    b->cap = initial_size;

    return 0;
}

void buffer_free(Buffer *b)
{
    if (!b)
    {
        return;
    }
    
    free(b->data);
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

int buffer_append(Buffer *b, const char *data, size_t len)
{
    if (buffer_grow(b,len) < 0)
    {
        return -1;
    }
    
    memcpy(b->data + b->len, data, len);
    b->len += len;

    return 0;
}

void buffer_consume(Buffer *b, size_t len)
{
    if (len >= b->len)
    {
        b->len = 0;
        return;
    }

    memmove(b->data, b->data + len, b->len - len);
    b->len -= len;
}

ssize_t buffer_find(Buffer *b, const char *pattern, size_t pattern_len)
{
    if (pattern_len > b->len)
    {
        return -1;
    }

    for (size_t i = 0; i <= b->len - pattern_len; i++)
    {
        if (memcmp(b->data + i, pattern, pattern_len) == 0)
            return (ssize_t)i;
    }

    return -1;
}

static int buffer_grow(Buffer *b, size_t needed)
{
    if (b->len + needed <= b->cap)
    {
        return 0;
    }
    
    size_t new_cap = b->cap * 2;
    while (new_cap < b->len + needed)
    {
        new_cap *= 2;
    }

    char *new_data = realloc(b->data, new_cap);
    if (!new_data)
    {
        return -1;
    }
    
    b->data = new_data;
    b->cap = new_cap;

    return 0;
}