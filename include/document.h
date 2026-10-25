#ifndef KSEARCH_DOCUMENT_H
#define KSEARCH_DOCUMENT_H

#include <stddef.h>
#include <time.h>

/* One indexed file. */
typedef struct
{
    int   id;
    char *path;      /* full path, owned */
    char *filename;  /* pointer into path, NOT owned */
    long  size;
    time_t mtime;
} Document;

/* Growable table of documents. */
typedef struct
{
    Document *items;
    size_t    count;
    size_t    capacity;
} DocumentTable;

void doc_table_init(DocumentTable *table);
void doc_table_free(DocumentTable *table);

/* Adds a document, taking ownership of a copy of `path`. Returns its id. */
int doc_table_add(DocumentTable *table, const char *path, long size, time_t mtime);

#endif
