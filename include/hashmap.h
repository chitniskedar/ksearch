#ifndef KSEARCH_HASHMAP_H
#define KSEARCH_HASHMAP_H

#include <stddef.h>

/* A posting list: the set of document ids containing a given term. */
typedef struct
{
    int    *ids;
    size_t  count;
    size_t  capacity;
} PostingList;

typedef struct HashEntry
{
    char             *key;    /* owned */
    PostingList       postings;
    struct HashEntry *next;   /* separate chaining */
} HashEntry;

typedef struct
{
    HashEntry **buckets;
    size_t      bucket_count;
    size_t      entry_count;
} HashMap;

void hashmap_init(HashMap *map, size_t bucket_count);
void hashmap_free(HashMap *map);

/* Appends doc_id to the posting list for `term`, creating the entry if needed.
 * Avoids duplicate consecutive ids so repeated words in one file only add once
 * per call site (caller is expected to call this once per (term, doc) pair). */
void hashmap_add_posting(HashMap *map, const char *term, int doc_id);

/* Returns the posting list for `term`, or NULL if the term is unindexed. */
const PostingList *hashmap_get(const HashMap *map, const char *term);

/* Iterates all entries, calling fn(key, postings, ctx) for each. Used by storage. */
void hashmap_foreach(const HashMap *map,
                      void (*fn)(const char *key, const PostingList *postings, void *ctx),
                      void *ctx);

#endif
