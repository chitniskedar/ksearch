#include "document.h"
#include "hashmap.h"
#include "index.h"
#include "ranking.h"
#include "storage.h"
#include "tokenizer.h"
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); failures++; } } while (0)

typedef struct
{
    char values[8][32];
    size_t count;
} Tokens;
static void save_token(const char *token, void *ctx) {
    Tokens *tokens = ctx;
    if (tokens->count < 8) snprintf(tokens->values[tokens->count++], 32, "%s", token);
}

static void test_tokenizer(void) {
    Tokens tokens = { { { 0 } }, 0 };
    tokenize("A Hello, WORLD_42 x", save_token, &tokens);
    CHECK(tokens.count == 3);
    CHECK(strcmp(tokens.values[0], "hello") == 0);
    CHECK(strcmp(tokens.values[1], "world") == 0);
    CHECK(strcmp(tokens.values[2], "42") == 0);
}

static void test_hashmap_and_ranking(void) {
    DocumentTable table;
    HashMap map;
    SearchResult *results = NULL;
    char *terms[] = { "alpha", "beta" };
    doc_table_init(&table);
    CHECK(doc_table_add(&table, "/tmp/alpha-beta.txt", 1, 100) == 0);
    CHECK(doc_table_add(&table, "/tmp/alpha.txt", 1, 200) == 1);
    hashmap_init(&map, 2);
    index_build(&table, &map);
    CHECK(hashmap_get(&map, "alpha")->count == 2);
    CHECK(rank_search(&map, &table, terms, 2, "alpha beta", &results) == 1);
    CHECK(results && results[0].doc_id == 0);
    free(results);
    hashmap_free(&map);
    doc_table_free(&table);
}

static void test_storage_round_trip(void) {
    DocumentTable written, loaded;
    char root[128] = { 0 }, indexed_at[64] = { 0 };
#if defined(_WIN32)
    (void)_putenv("KSEARCH_HOME=tests/.testdata");
#else
    setenv("KSEARCH_HOME", "tests/.testdata", 1);
#endif
    (void)platform_mkdir("tests/.testdata");
    doc_table_init(&written);
    doc_table_init(&loaded);
    CHECK(doc_table_add(&written, "dir/a\tb\nfile.txt", 12, 34) == 0);
    CHECK(storage_save("root\twith\nlines", &written) == 0);
    CHECK(storage_load(&loaded) == 1);
    CHECK(loaded.count == 1 && strcmp(loaded.items[0].path, "dir/a\tb\nfile.txt") == 0);
    CHECK(storage_get_meta(root, sizeof(root), indexed_at) == 0);
    CHECK(strcmp(root, "root\twith\nlines") == 0);
    doc_table_free(&written);
    doc_table_free(&loaded);
}

int main(void) {
    test_tokenizer();
    test_hashmap_and_ranking();
    test_storage_round_trip();
    if (failures) return 1;
    puts("All tests passed.");
    return 0;
}
