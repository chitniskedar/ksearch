#ifndef KSEARCH_RANKING_H
#define KSEARCH_RANKING_H

#include <stddef.h>
#include <time.h>
#include "document.h"
#include "hashmap.h"

typedef struct
{
    int    doc_id;
    double score;
    time_t mtime;
} SearchResult;

/* Runs `tokens` (already lowercased/normalized) against `map`.
 * Tries an AND match across all tokens first; if that yields nothing,
 * falls back to an OR match so a query still returns partial matches.
 * Filename matches are scored higher, and an exact phrase hit in the
 * filename gets an additional boost. Results are sorted by relevance score
 * descending, and ties are broken by recency — most recently modified first.
 * Caller must free() *out_results. Returns the number of results. */
size_t rank_search(const HashMap *map, const DocumentTable *table,
                    char **tokens, size_t token_count,
                    const char *raw_query,
                    SearchResult **out_results);

#endif
