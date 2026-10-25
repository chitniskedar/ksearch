#include "hashmap.h"
#include "platform.h"
#include <stdlib.h>
#include <string.h>

static unsigned long fnv1a(const char *s)
{
    unsigned long hash = 2166136261UL;
    while (*s)
    {
        hash ^= (unsigned char)(*s++);
        hash *= 16777619UL;
    }
    return hash;
}

void hashmap_init(HashMap *map, size_t bucket_count)
{
    map->buckets = calloc(bucket_count, sizeof(HashEntry *));
    map->bucket_count = map->buckets ? bucket_count : 0;
    map->entry_count = 0;
}

void hashmap_free(HashMap *map)
{
    for (size_t i = 0; i < map->bucket_count; i++)
    {
        HashEntry *e = map->buckets[i];
        while (e)
        {
            HashEntry *next = e->next;
            free(e->key);
            free(e->postings.ids);
            free(e);
            e = next;
        }
    }
    free(map->buckets);
    map->buckets = NULL;
    map->bucket_count = 0;
    map->entry_count = 0;
}

static HashEntry *find_entry(const HashMap *map, const char *term)
{
    if (map->bucket_count == 0) return NULL;
    unsigned long h = fnv1a(term) % map->bucket_count;
    for (HashEntry *e = map->buckets[h]; e; e = e->next)
    {
        if (strcmp(e->key, term) == 0) return e;
    }
    return NULL;
}

static void posting_append(PostingList *list, int doc_id)
{
    /* Skip exact duplicate of the last id (same doc indexed twice in a row). */
    if (list->count > 0 && list->ids[list->count - 1] == doc_id) return;

    if (list->count == list->capacity)
    {
        size_t new_cap = list->capacity == 0 ? 4 : list->capacity * 2;
        int *grown = realloc(list->ids, sizeof(int) * new_cap);
        if (!grown) return;
        list->ids = grown;
        list->capacity = new_cap;
    }
    list->ids[list->count++] = doc_id;
}

void hashmap_add_posting(HashMap *map, const char *term, int doc_id)
{
    if (map->bucket_count == 0) return;
    unsigned long h = fnv1a(term) % map->bucket_count;

    HashEntry *e = find_entry(map, term);
    if (!e)
    {
        e = malloc(sizeof(HashEntry));
        if (!e) return;
        e->key = platform_strdup(term);
        if (!e->key)
        { free(e); return; }
        e->postings.ids = NULL;
        e->postings.count = 0;
        e->postings.capacity = 0;
        e->next = map->buckets[h];
        map->buckets[h] = e;
        map->entry_count++;
    }
    posting_append(&e->postings, doc_id);
}

const PostingList *hashmap_get(const HashMap *map, const char *term)
{
    HashEntry *e = find_entry(map, term);
    return e ? &e->postings : NULL;
}

void hashmap_foreach(const HashMap *map,
                      void (*fn)(const char *key, const PostingList *postings, void *ctx),
                      void *ctx)
                      {
    for (size_t i = 0; i < map->bucket_count; i++)
    {
        for (HashEntry *e = map->buckets[i]; e; e = e->next)
        {
            fn(e->key, &e->postings, ctx);
        }
    }
}
