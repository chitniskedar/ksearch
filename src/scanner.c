#if !defined(_WIN32)
  #define _DEFAULT_SOURCE /* for lstat, regardless of build flags */
#endif

#include "scanner.h"
#include "platform.h"
#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if defined(_WIN32)
  /* Windows has no separate "don't follow symlinks" stat call, and this
   * scanner doesn't follow symlinks anyway — plain stat() is equivalent here. */
  #define lstat stat
#endif

static int walk_recursive(const char *path, DocumentTable *table, int *count)
{
    DIR *dir = opendir(path);
    if (!dir) return -1;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        /* Skip hidden entries (.git, .ksearch, etc.) */
        if (entry->d_name[0] == '.')
            continue;

        char *child = platform_path_join(path, entry->d_name);
        if (!child)
        { closedir(dir); return -1; }

        struct stat st;
        if (lstat(child, &st) != 0)
        {
            free(child);
            continue; /* unreadable entry, skip it */
        }

        if (S_ISDIR(st.st_mode))
        {
            (void)walk_recursive(child, table, count);
        }
else if (S_ISREG(st.st_mode))
{
            if (doc_table_add(table, child, (long)st.st_size, st.st_mtime) >= 0)
            {
                (*count)++;
            }
        }
        /* symlinks and other types are ignored for now */
        free(child);
    }

    closedir(dir);
    return 0;
}

int scanner_walk(const char *root, DocumentTable *table)
{
    int count = 0;
    if (walk_recursive(root, table, &count) != 0)
    {
        return -1;
    }
    return count;
}
