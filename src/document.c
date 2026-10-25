#include "document.h"
#include "platform.h"
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAPACITY 256

void doc_table_init(DocumentTable *table)
{
    table->items = malloc(sizeof(Document) * INITIAL_CAPACITY);
    table->count = 0;
    table->capacity = table->items ? INITIAL_CAPACITY : 0;
}

void doc_table_free(DocumentTable *table)
{
    for (size_t i = 0; i < table->count; i++)
    {
        free(table->items[i].path);
    }
    free(table->items);
    table->items = NULL;
    table->count = 0;
    table->capacity = 0;
}

static const char *basename_ptr(const char *path)
{
    const char *slash = strrchr(path, '/');
    const char *backslash = strrchr(path, '\\');
    const char *last = (backslash && (!slash || backslash > slash)) ? backslash : slash;
    return last ? last + 1 : path;
}

int doc_table_add(DocumentTable *table, const char *path, long size, time_t mtime)
{
    if (table->count == table->capacity)
    {
        size_t new_cap = table->capacity == 0 ? INITIAL_CAPACITY : table->capacity * 2;
        Document *grown = realloc(table->items, sizeof(Document) * new_cap);
        if (!grown) return -1;
        table->items = grown;
        table->capacity = new_cap;
    }

    Document *d = &table->items[table->count];
    d->id = (int)table->count;
    d->path = platform_strdup(path);
    if (!d->path) return -1;
    d->filename = (char *)basename_ptr(d->path);
    d->size = size;
    d->mtime = mtime;

    table->count++;
    return d->id;
}
