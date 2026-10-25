#include "storage.h"
#include "platform.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

/* The text format hex-encodes paths, so tabs and newlines remain lossless. */
static char *directory;

const char *storage_dir(void)
{
    const char *home;
    if (directory) return directory;
    home = getenv("KSEARCH_HOME");
    if (!home || !*home) home = getenv("HOME");
#if defined(_WIN32)
    if (!home || !*home) home = getenv("USERPROFILE");
#endif
    if (!home || !*home) home = ".";
    directory = platform_path_join(home, ".ksearch");
    if (!directory) return ".";
    if (platform_mkdir(directory) != 0 && errno != EEXIST)
    {
        free(directory);
        directory = NULL;
        return ".";
    }
    return directory;
}

static char *path_for(const char *name)
{ return platform_path_join(storage_dir(), name); }

static char *hex_encode(const char *text)
{
    static const char digits[] = "0123456789ABCDEF";
    size_t len = strlen(text), i;
    char *out;
    if (len > (SIZE_MAX - 1) / 2) return NULL;
    out = malloc(len * 2 + 1);
    if (!out) return NULL;
    for (i = 0; i < len; i++)
    {
        unsigned char c = (unsigned char)text[i];
        out[i * 2] = digits[c >> 4];
        out[i * 2 + 1] = digits[c & 15];
    }
    out[len * 2] = '\0';
    return out;
}

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static char *hex_decode(const char *text)
{
    size_t len = strlen(text), i;
    char *out;
    if ((len & 1) != 0) return NULL;
    out = malloc(len / 2 + 1);
    if (!out) return NULL;
    for (i = 0; i < len; i += 2)
    {
        int high = hex_value(text[i]), low = hex_value(text[i + 1]);
        if (high < 0 || low < 0)
        { free(out); return NULL; }
        out[i / 2] = (char)((high << 4) | low);
    }
    out[len / 2] = '\0';
    return out;
}

static char *read_line(FILE *file)
{
    size_t length = 0, capacity = 128;
    int c;
    char *line = malloc(capacity);
    if (!line) return NULL;
    while ((c = fgetc(file)) != EOF && c != '\n')
    {
        if (length + 1 >= capacity)
        {
            char *grown;
            if (capacity > SIZE_MAX / 2)
            { free(line); return NULL; }
            capacity *= 2;
            grown = realloc(line, capacity);
            if (!grown)
            { free(line); return NULL; }
            line = grown;
        }
        line[length++] = (char)c;
    }
    if (c == EOF && length == 0)
    { free(line); return NULL; }
    line[length] = '\0';
    return line;
}

int storage_save(const char *indexed_root, const DocumentTable *table)
{
    char *docs_path = path_for("documents.dat");
    char *meta_path = path_for("meta.dat");
    FILE *docs;
    FILE *meta;
    char *root_hex;
    time_t now;
    char timebuf[64];
    if (!docs_path || !meta_path)
    { free(docs_path); free(meta_path); return -1; }
    docs = fopen(docs_path, "wb");
    free(docs_path);
    if (!docs)
    { free(meta_path); return -1; }
    if (fprintf(docs, "KSEARCH-DOCS-2\n") < 0)
    { fclose(docs); free(meta_path); return -1; }
    for (size_t i = 0; i < table->count; i++)
    {
        const Document *doc = &table->items[i];
        char *path_hex = hex_encode(doc->path);
        if (!path_hex || fprintf(docs, "%d\t%ld\t%ld\t%s\n", doc->id, doc->size,
                                 (long)doc->mtime, path_hex) < 0)
                                 {
            free(path_hex); fclose(docs); free(meta_path); return -1;
        }
        free(path_hex);
    }
    if (fclose(docs) != 0)
    { free(meta_path); return -1; }
    root_hex = hex_encode(indexed_root ? indexed_root : "");
    if (!root_hex)
    { free(meta_path); return -1; }
    meta = fopen(meta_path, "wb");
    free(meta_path);
    if (!meta)
    { free(root_hex); return -1; }
    now = time(NULL);
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M", localtime(&now));
    fprintf(meta, "KSEARCH-META-2\nroot_hex\t%s\nindexed_at\t%s\nfile_count\t%lu\n",
            root_hex, timebuf, (unsigned long)table->count);
    free(root_hex);
    return fclose(meta) == 0 ? 0 : -1;
}

int storage_load(DocumentTable *table)
{
    char *docs_path = path_for("documents.dat");
    FILE *file;
    char *line;
    int loaded = 0;
    if (!docs_path) return -1;
    file = fopen(docs_path, "rb");
    free(docs_path);
    if (!file) return -1;
    line = read_line(file);
    if (!line || strcmp(line, "KSEARCH-DOCS-2") != 0)
    {
        free(line); fclose(file); return -1;
    }
    free(line);
    while ((line = read_line(file)) != NULL)
    {
        char *a = strchr(line, '\t'), *b, *c, *path;
        long size, mtime;
        char *decoded;
        if (!a)
        { free(line); continue; }
        *a++ = '\0'; b = strchr(a, '\t'); if (!b)
        { free(line); continue; }
        *b++ = '\0'; c = strchr(b, '\t'); if (!c)
        { free(line); continue; }
        *c++ = '\0'; path = c;
        size = strtol(a, NULL, 10); mtime = strtol(b, NULL, 10);
        decoded = hex_decode(path);
        if (decoded)
        {
            if (doc_table_add(table, decoded, size, (time_t)mtime) < 0)
            {
                free(decoded); free(line); fclose(file); return -1;
            }
            free(decoded); loaded++;
        }
        free(line);
    }
    fclose(file);
    return loaded;
}

int storage_get_meta(char *root_out, size_t root_out_size, char *indexed_at_out)
{
    char *meta_path = path_for("meta.dat");
    FILE *file;
    char *line;
    int valid = 0;
    if (!meta_path) return -1;
    file = fopen(meta_path, "rb");
    free(meta_path);
    if (!file) return -1;
    line = read_line(file);
    if (line && strcmp(line, "KSEARCH-META-2") == 0) valid = 1;
    free(line);
    while (valid && (line = read_line(file)) != NULL)
    {
        char *tab = strchr(line, '\t');
        if (tab)
        {
            *tab++ = '\0';
            if (strcmp(line, "root_hex") == 0 && root_out && root_out_size)
            {
                char *root = hex_decode(tab);
                if (root)
                { snprintf(root_out, root_out_size, "%s", root); free(root); }
            }
else if (strcmp(line, "indexed_at") == 0 && indexed_at_out)
{
                snprintf(indexed_at_out, 64, "%s", tab);
            }
        }
        free(line);
    }
    fclose(file);
    return valid ? 0 : -1;
}

int storage_save_last_results(const DocumentTable *table, const int *ids, size_t count)
{
    char *file_path = path_for("last_results.dat");
    FILE *file;
    if (!file_path) return -1;
    file = fopen(file_path, "wb");
    free(file_path);
    if (!file) return -1;
    for (size_t i = 0; i < count; i++)
    {
        const Document *doc;
        char *encoded;
        if (ids[i] < 0 || (size_t)ids[i] >= table->count) continue;
        doc = &table->items[ids[i]];
        encoded = hex_encode(doc->path);
        if (!encoded || fprintf(file, "%s\n", encoded) < 0)
        {
            free(encoded); fclose(file); return -1;
        }
        free(encoded);
    }
    return fclose(file) == 0 ? 0 : -1;
}

int storage_load_last_result(size_t number, char **path_out)
{
    char *file_path = path_for("last_results.dat");
    FILE *file;
    char *line = NULL;
    if (!path_out || number == 0 || !file_path) return -1;
    *path_out = NULL;
    file = fopen(file_path, "rb");
    free(file_path);
    if (!file) return -1;
    for (size_t i = 0; i < number; i++)
    {
        free(line);
        line = read_line(file);
        if (!line)
        { fclose(file); return -1; }
    }
    *path_out = hex_decode(line);
    free(line);
    fclose(file);
    return *path_out ? 0 : -1;
}
