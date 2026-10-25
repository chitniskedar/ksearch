/* Must come before any system header: enables POSIX extensions
 * (strcasecmp) on Linux/macOS regardless of how this file is compiled —
 * not every build invocation picks up the Makefile's -D flags. */
#if defined(_WIN32)
  #define strcasecmp _stricmp /* MSVCRT names it with a leading underscore */
  #define strdup     _strdup
#else
  #define _POSIX_C_SOURCE 200809L
  #define _DEFAULT_SOURCE
#endif

#include "cli.h"
#include "document.h"
#include "scanner.h"
#include "storage.h"
#include "hashmap.h"
#include "index.h"
#include "tokenizer.h"
#include "ranking.h"
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>

#if defined(_WIN32)
  #include <windows.h>
  #include <io.h> /* _isatty, _fileno */
#else
  #include <unistd.h> /* isatty */
#endif

#define MAX_QUERY_TOKENS 32
#define INDEX_BUCKETS    4096
/* Portable monotonic clock in milliseconds — CLOCK_MONOTONIC isn't available
 * on plain MinGW, so this branches per platform instead of relying on it. */
static double now_ms(void)
{
#if defined(_WIN32)
    static LARGE_INTEGER freq = {0};
    if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart * 1000.0 / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
#endif
}

static int stdout_is_terminal(void)
{
#if defined(_WIN32)
    return _isatty(_fileno(stdout));
#else
    return isatty(fileno(stdout));
#endif
}

/* Converts a filesystem path (POSIX "/a/b" or Windows "D:\a\b" / "D:\a/b")
 * into a "file://" URI so terminals can turn it into a clickable hyperlink.
 * Backslashes become forward slashes; spaces and '#' are percent-encoded
 * (the two characters most likely to actually appear and break a URI). */
static int uri_unreserved(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~';
}

/* Result links deliberately target the containing folder: file:// links to a
 * file invoke its associated application, whereas folder links open Explorer. */
static void print_folder_link(const char *path, const char *text)
{
    static const char hex[] = "0123456789ABCDEF";
    const char *end = path + strlen(path);
    const char *slash = strrchr(path, '/');
    const char *backslash = strrchr(path, '\\');
    if (backslash && (!slash || backslash > slash)) slash = backslash;
    if (slash && slash != path) end = slash;
    if (!stdout_is_terminal())
    { fputs(text, stdout); return; }
    fputs("\x1b]8;;file://", stdout);
    if (path[0] != '/' && path[0] != '\\') fputc('/', stdout);
    for (const unsigned char *p = (const unsigned char *)path;
         p < (const unsigned char *)end; p++)
    {
        unsigned char c = *p == '\\' ? '/' : *p;
        size_t offset = (size_t)(p - (const unsigned char *)path);
        /* The drive colon is part of the Windows file URI structure:
         * file:///D:/folder. Encoding it prevents some terminals from
         * recognizing the hyperlink. */
        if (uri_unreserved(c) || c == '/' || (offset == 1 && c == ':')) fputc(c, stdout);
        else
        { fputc('%', stdout); fputc(hex[c >> 4], stdout); fputc(hex[c & 15], stdout); }
    }
    printf("\x1b\\%s\x1b]8;;\x1b\\", text);
}

static void print_usage(void)
{
    printf(
        "KSearch - native local search engine\n\n"
        "Usage:\n"
        "  ksearch init                         Set up ~/.ksearch\n"
        "  ksearch index <path>                 Scan and index a directory\n"
        "  ksearch \"<query>\" [flags]             Search the index\n"
        "  ksearch remove <path>                Drop indexed files under <path>\n"
        "  ksearch rebuild                       Re-scan the last indexed directory\n"
        "  ksearch open <N>                     Open a result from the last search\n"
        "  ksearch reveal <N>                   Reveal a result in the file manager\n"
        "  ksearch --stats                       Show index statistics\n\n"
        "Search flags:\n"
        "  --type <ext>       Only files with this extension (e.g. --type c)\n"
        "  --path <substring>  Only files whose path contains this substring\n"
        "  --recent <days>     Only files modified within the last <days> days\n"
        "  --sort <mode>       recent (default) or relevance\n"
        "  --limit <N>         Show at most N results (default 20)\n"
        "  --all               Show all results\n"
    );
}

/* ---------- small helpers ---------- */

static double elapsed_ms(double start, double end)
{
    return end - start;
}

static const char *file_extension(const char *filename)
{
    const char *dot = strrchr(filename, '.');
    return (dot && dot != filename) ? dot + 1 : "";
}

static int ascii_case_equal(const char *left, const char *right)
{
    while (*left && *right)
    {
        unsigned char a = (unsigned char)*left++;
        unsigned char b = (unsigned char)*right++;
        if (tolower(a) != tolower(b)) return 0;
    }
    return *left == '\0' && *right == '\0';
}

static int cmp_recent_result(const void *a, const void *b)
{
    const SearchResult *left = a, *right = b;
    if (right->mtime > left->mtime) return 1;
    if (right->mtime < left->mtime) return -1;
    return left->doc_id - right->doc_id;
}

typedef struct
{ char **tokens; size_t count; } TokenCollector;

static void collect_token(const char *token, void *ctx)
{
    TokenCollector *tc = (TokenCollector *)ctx;
    if (tc->count >= MAX_QUERY_TOKENS) return;
    tc->tokens[tc->count++] = strdup(token);
}

/* ---------- commands ---------- */

static int cmd_init(void)
{
    storage_dir(); /* creates it as a side effect */
    printf("Initialized KSearch at %s\n", storage_dir());
    return 0;
}

static int cmd_index(const char *root)
{
    printf("Scanning...\n\n");

    double t0 = now_ms();

    DocumentTable table;
    doc_table_init(&table);

    char *absolute_root = platform_absolute_path(root);
    if (!absolute_root)
    {
        fprintf(stderr, "ksearch: out of memory\n");
        doc_table_free(&table);
        return 1;
    }

    int count = scanner_walk(absolute_root, &table);
    if (count < 0)
    {
        fprintf(stderr, "ksearch: could not open directory: %s\n", root);
        free(absolute_root);
        doc_table_free(&table);
        return 1;
    }

    if (storage_save(absolute_root, &table) != 0)
    {
        fprintf(stderr, "ksearch: failed to save index\n");
        free(absolute_root);
        doc_table_free(&table);
        return 1;
    }

    double t1 = now_ms();

    printf("Files indexed:       %d\n", count);
    printf("Completed in %.1f seconds.\n", elapsed_ms(t0, t1) / 1000.0);

    doc_table_free(&table);
    free(absolute_root);
    return 0;
}

static int cmd_rebuild(void)
{
    char root[900] = {0};
    char indexed_at[64] = {0};
    if (storage_get_meta(root, sizeof(root), indexed_at) != 0 || root[0] == '\0')
    {
        fprintf(stderr, "ksearch: nothing indexed yet. Run: ksearch index <path>\n");
        return 1;
    }
    printf("Rebuilding index for %s\n", root);
    return cmd_index(root);
}

static int cmd_remove(const char *path_substring)
{
    DocumentTable old_table;
    doc_table_init(&old_table);
    if (storage_load(&old_table) < 0)
    {
        fprintf(stderr, "ksearch: no index found. Run: ksearch index <path>\n");
        doc_table_free(&old_table);
        return 1;
    }

    char root[900] = {0};
    char indexed_at[64] = {0};
    storage_get_meta(root, sizeof(root), indexed_at);

    DocumentTable kept;
    doc_table_init(&kept);

    int removed = 0;
    for (size_t i = 0; i < old_table.count; i++)
    {
        Document *d = &old_table.items[i];
        if (strstr(d->path, path_substring) != NULL)
        {
            removed++;
        }
else
{
            doc_table_add(&kept, d->path, d->size, d->mtime);
        }
    }

    storage_save(root, &kept);

    printf("Removed %d file%s from the index.\n", removed, removed == 1 ? "" : "s");

    doc_table_free(&old_table);
    doc_table_free(&kept);
    return 0;
}

static int cmd_stats(void)
{
    char root[900] = {0};
    char indexed_at[64] = {0};
    if (storage_get_meta(root, sizeof(root), indexed_at) != 0)
    {
        fprintf(stderr, "ksearch: nothing indexed yet. Run: ksearch index <path>\n");
        return 1;
    }

    DocumentTable table;
    doc_table_init(&table);
    int loaded = storage_load(&table);
    if (loaded < 0) loaded = 0;

    HashMap map;
    hashmap_init(&map, INDEX_BUCKETS);
    index_build(&table, &map);

    char docs_path[1100];
    snprintf(docs_path, sizeof(docs_path), "%s/documents.dat", storage_dir());
    struct stat st;
    long index_bytes = (stat(docs_path, &st) == 0) ? (long)st.st_size : 0;

    printf("KSearch Statistics\n");
    printf("-----------------------------\n\n");
    printf("Indexed root:         %s\n", root[0] ? root : "(unknown)");
    printf("Indexed files:        %d\n", loaded);
    printf("Indexed terms:        %lu\n", (unsigned long)map.entry_count);
    printf("Index size:           %.1f KB\n", index_bytes / 1024.0);
    printf("Last update:          %s\n", indexed_at[0] ? indexed_at : "(unknown)");

    hashmap_free(&map);
    doc_table_free(&table);
    return 0;
}

static int cmd_search(const char *raw_query, const char *type_filter,
                       const char *path_filter, int recent_days,
                       int recent_sort, size_t limit)
                       {
    double t0 = now_ms();

    DocumentTable full_table;
    doc_table_init(&full_table);
    if (storage_load(&full_table) < 0)
    {
        fprintf(stderr, "ksearch: no index found. Run: ksearch index <path>\n");
        doc_table_free(&full_table);
        return 1;
    }

    /* Apply --type / --path / --recent filters by building a filtered table
     * with fresh, contiguous ids (ranking indexes doc_id directly into the table). */
    DocumentTable table;
    doc_table_init(&table);

    time_t cutoff = 0;
    if (recent_days > 0) cutoff = time(NULL) - (time_t)recent_days * 86400;

    for (size_t i = 0; i < full_table.count; i++)
    {
        Document *d = &full_table.items[i];
        if (type_filter && !ascii_case_equal(file_extension(d->filename), type_filter)) continue;
        if (path_filter && strstr(d->path, path_filter) == NULL) continue;
        if (recent_days > 0 && d->mtime < cutoff) continue;
        doc_table_add(&table, d->path, d->size, d->mtime);
    }
    doc_table_free(&full_table);

    HashMap map;
    hashmap_init(&map, INDEX_BUCKETS);
    index_build(&table, &map);

    char *tokens[MAX_QUERY_TOKENS];
    TokenCollector tc = { tokens, 0 };
    tokenize(raw_query, collect_token, &tc);

    if (tc.count == 0)
    {
        printf("Enter a search term, e.g. ksearch \"process scheduling\"\n");
        hashmap_free(&map);
        doc_table_free(&table);
        return 1;
    }

    SearchResult *results = NULL;
    size_t result_count = rank_search(&map, &table, tokens, tc.count, raw_query, &results);

    if (recent_sort && result_count > 1)
        qsort(results, result_count, sizeof(*results), cmp_recent_result);

    double t1 = now_ms();

    printf("KSearch - %lu result%s\n\n", (unsigned long)result_count, result_count == 1 ? "" : "s");

    size_t shown = result_count < limit ? result_count : limit;
    int *last_ids = shown ? malloc(shown * sizeof(*last_ids)) : NULL;
    for (size_t i = 0; i < shown; i++)
    {
        Document *d = &table.items[results[i].doc_id];

        int name_len = (int)strlen(d->filename);
        int pad = 38 - name_len;
        if (pad < 1) pad = 1;

        printf("%lu. ", (unsigned long)(i + 1));
        print_folder_link(d->path, d->filename);
        {
            char modified[32];
            struct tm *tm_value = localtime(&d->mtime);
            if (tm_value) strftime(modified, sizeof(modified), "%Y-%m-%d", tm_value);
            else snprintf(modified, sizeof(modified), "unknown");
            printf("%*s%.2f  %s\n", pad, "", results[i].score, modified);
        }

        printf("   ");
        print_folder_link(d->path, d->path);
        printf("\n\n");
        if (last_ids) last_ids[i] = results[i].doc_id;
    }
    if (storage_save_last_results(&table, last_ids, shown) != 0)
        fprintf(stderr, "ksearch: could not save last search results\n");
    free(last_ids);

    printf("-----------------------------------------------\n");
    printf("%lu result%s - %.1f ms\n", (unsigned long)result_count, result_count == 1 ? "" : "s",
           elapsed_ms(t0, t1));

    for (size_t i = 0; i < tc.count; i++) free(tokens[i]);
    free(results);
    hashmap_free(&map);
    doc_table_free(&table);
    return 0;
}

static int cmd_open_or_reveal(const char *number_text, int reveal)
{
    char *end = NULL, *path = NULL;
    unsigned long number = strtoul(number_text, &end, 10);
    if (!number_text[0] || !end || *end || number == 0 ||
        storage_load_last_result((size_t)number, &path) != 0)
        {
        fprintf(stderr, "ksearch: no saved result %s\n", number_text);
        free(path); return 1;
    }
    if ((reveal ? platform_reveal_file(path) : platform_open_file(path)) != 0)
    {
        fprintf(stderr, "ksearch: could not %s %s\n", reveal ? "reveal" : "open", path);
        free(path); return 1;
    }
    free(path);
    return 0;
}

/* ---------- argument parsing ---------- */

int cli_run(int argc, char **argv)
{
    if (argc < 2)
    {
        print_usage();
        return 1;
    }

    if (strcmp(argv[1], "init") == 0) return cmd_init();
    if (strcmp(argv[1], "rebuild") == 0) return cmd_rebuild();
    if (strcmp(argv[1], "--stats") == 0) return cmd_stats();
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)
    { print_usage(); return 0; }
    if (strcmp(argv[1], "open") == 0)
    {
        if (argc != 3)
        { fprintf(stderr, "ksearch: usage: ksearch open <N>\n"); return 1; }
        return cmd_open_or_reveal(argv[2], 0);
    }
    if (strcmp(argv[1], "reveal") == 0)
    {
        if (argc != 3)
        { fprintf(stderr, "ksearch: usage: ksearch reveal <N>\n"); return 1; }
        return cmd_open_or_reveal(argv[2], 1);
    }

    if (strcmp(argv[1], "index") == 0)
    {
        if (argc < 3)
        { fprintf(stderr, "ksearch: usage: ksearch index <path>\n"); return 1; }
        return cmd_index(argv[2]);
    }

    if (strcmp(argv[1], "remove") == 0)
    {
        if (argc < 3)
        { fprintf(stderr, "ksearch: usage: ksearch remove <path>\n"); return 1; }
        return cmd_remove(argv[2]);
    }

    /* Otherwise: search. Collect non-flag words into the query, and parse
     * --type / --path / --recent wherever they appear. */
    char query[1024] = {0};
    const char *type_filter = NULL;
    const char *path_filter = NULL;
    int recent_days = 0;
    int recent_sort = 1;
    size_t limit = 20;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--type") == 0 && i + 1 < argc)
        {
            type_filter = argv[++i];
        }
else if (strcmp(argv[i], "--path") == 0 && i + 1 < argc)
{
            path_filter = argv[++i];
        }
else if (strcmp(argv[i], "--recent") == 0 && i + 1 < argc)
{
            recent_days = atoi(argv[++i]);
        }
else if (strcmp(argv[i], "--sort") == 0 && i + 1 < argc)
{
            const char *sort = argv[++i];
            if (strcmp(sort, "recent") == 0) recent_sort = 1;
            else if (strcmp(sort, "relevance") == 0) recent_sort = 0;
            else
            { fprintf(stderr, "ksearch: --sort must be recent or relevance\n"); return 1; }
        }
else if (strcmp(argv[i], "--limit") == 0 && i + 1 < argc)
{
            char *end = NULL;
            unsigned long parsed = strtoul(argv[++i], &end, 10);
            if (!end || *end || parsed == 0)
            { fprintf(stderr, "ksearch: --limit must be positive\n"); return 1; }
            limit = (size_t)parsed;
        }
else if (strcmp(argv[i], "--all") == 0)
{
            limit = (size_t)-1;
        }
else
{
            if (query[0] != '\0') strncat(query, " ", sizeof(query) - strlen(query) - 1);
            strncat(query, argv[i], sizeof(query) - strlen(query) - 1);
        }
    }

    if (query[0] == '\0')
    {
        print_usage();
        return 1;
    }

    return cmd_search(query, type_filter, path_filter, recent_days, recent_sort, limit);
}
