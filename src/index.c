#include "index.h"
#include "tokenizer.h"

typedef struct
{
    HashMap *map;
    int      doc_id;
} BuildCtx;

static void emit_token(const char *token, void *ctx)
{
    BuildCtx *bc = (BuildCtx *)ctx;
    hashmap_add_posting(bc->map, token, bc->doc_id);
}

void index_build(const DocumentTable *table, HashMap *map)
{
    for (size_t i = 0; i < table->count; i++)
    {
        BuildCtx ctx = { map, table->items[i].id };
        tokenize(table->items[i].filename, emit_token, &ctx);
    }
}
