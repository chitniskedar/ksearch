#include "ranking.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define CONTENT_MATCH_SCORE  5.0   /* score per matching token (filename-only for now) */
#define PHRASE_MATCH_BONUS   8.0   /* full query appears as a substring of the filename */

typedef struct
{
    int   doc_id;
    int   matches;
} Candidate;

/* Two-pointer intersection of two ascending, deduplicated posting lists. */
static size_t intersect(const int *a, size_t alen, const int *b, size_t blen, int *out)
{
    size_t i = 0, j = 0, k = 0;
    while (i < alen && j < blen)
    {
        if (a[i] == b[j])
        { out[k++] = a[i]; i++; j++; }
else if (a[i] < b[j]) i++;
        else j++;
    }
    return k;
}

static int cmp_result(const void *a, const void *b)
{
    const SearchResult *ra = a, *rb = b;
    if (rb->score > ra->score) return 1;
    if (rb->score < ra->score) return -1;
    /* Equal relevance: most recently modified first. */
    if (rb->mtime > ra->mtime) return 1;
    if (rb->mtime < ra->mtime) return -1;
    return ra->doc_id - rb->doc_id;
}

static void lower_copy(char *dst, const char *src, size_t dst_size)
{
    size_t i = 0;
    for (; src[i] && i + 1 < dst_size; i++) dst[i] = (char)tolower((unsigned char)src[i]);
    dst[i] = '\0';
}

size_t rank_search(const HashMap *map, const DocumentTable *table,
                    char **tokens, size_t token_count,
                    const char *raw_query,
                    SearchResult **out_results)
                    {
    *out_results = NULL;
    if (token_count == 0) return 0;

    /* Gather posting lists for each token that actually exists in the index. */
    const PostingList **lists = malloc(sizeof(PostingList *) * token_count);
    size_t found = 0;
    for (size_t i = 0; i < token_count; i++)
    {
        const PostingList *pl = hashmap_get(map, tokens[i]);
        if (pl && pl->count > 0) lists[found++] = pl;
    }

    if (found == 0)
    { free(lists); return 0; }

    /* AND: intersect all found lists together. */
    int *inter = malloc(sizeof(int) * lists[0]->count);
    size_t inter_len = lists[0]->count;
    memcpy(inter, lists[0]->ids, sizeof(int) * inter_len);

    for (size_t i = 1; i < found && inter_len > 0; i++)
    {
        int *tmp = malloc(sizeof(int) * inter_len);
        size_t tmp_len = intersect(inter, inter_len, lists[i]->ids, lists[i]->count, tmp);
        free(inter);
        inter = tmp;
        inter_len = tmp_len;
    }

    int use_and = inter_len > 0;
    Candidate *cands;
    size_t cand_count = 0;

    if (use_and)
    {
        cands = malloc(sizeof(Candidate) * inter_len);
        for (size_t i = 0; i < inter_len; i++)
        {
            cands[cand_count].doc_id = inter[i];
            cands[cand_count].matches = (int)found; /* matched every token */
            cand_count++;
        }
    }
else
{
        /* OR fallback: union all ids, counting how many token-lists include each. */
        size_t cap = 0;
        for (size_t i = 0; i < found; i++) cap += lists[i]->count;
        cands = malloc(sizeof(Candidate) * (cap ? cap : 1));

        for (size_t i = 0; i < found; i++)
        {
            for (size_t j = 0; j < lists[i]->count; j++)
            {
                int id = lists[i]->ids[j];
                int existing = -1;
                for (size_t k = 0; k < cand_count; k++)
                {
                    if (cands[k].doc_id == id)
                    { existing = (int)k; break; }
                }
                if (existing >= 0) cands[existing].matches++;
                else
                { cands[cand_count].doc_id = id; cands[cand_count].matches = 1; cand_count++; }
            }
        }
    }
    free(inter);
    free(lists);

    if (cand_count == 0)
    { free(cands); return 0; }

    char query_lower[512];
    lower_copy(query_lower, raw_query, sizeof(query_lower));

    SearchResult *results = malloc(sizeof(SearchResult) * cand_count);
    for (size_t i = 0; i < cand_count; i++)
    {
        double score = (double)cands[i].matches * CONTENT_MATCH_SCORE;

        const Document *doc = &table->items[cands[i].doc_id];
        char fname_lower[512];
        lower_copy(fname_lower, doc->filename, sizeof(fname_lower));
        if (query_lower[0] != '\0' && strstr(fname_lower, query_lower) != NULL)
        {
            score += PHRASE_MATCH_BONUS;
        }

        results[i].doc_id = cands[i].doc_id;
        results[i].score = score;
        results[i].mtime = doc->mtime;
    }
    free(cands);

    qsort(results, cand_count, sizeof(SearchResult), cmp_result);
    *out_results = results;
    return cand_count;
}
