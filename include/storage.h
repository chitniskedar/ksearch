#ifndef KSEARCH_STORAGE_H
#define KSEARCH_STORAGE_H

#include "document.h"

/* Saves `table` plus bookkeeping (indexed root, timestamp) under ~/.ksearch/.
 * Returns 0 on success, -1 on failure. */
int storage_save(const char *indexed_root, const DocumentTable *table);

/* Loads a previously saved document table from ~/.ksearch/ into `table`
 * (which must already be initialized via doc_table_init). Returns the number
 * of documents loaded, or -1 if no index exists yet / it can't be read. */
int storage_load(DocumentTable *table);

/* Fills `root_out` (size root_out_size) with the last-indexed root path and
 * `indexed_at_out` with the ISO-ish timestamp string (size 64), for --stats.
 * Returns 0 on success, -1 if no metadata is available. */
int storage_get_meta(char *root_out, size_t root_out_size, char *indexed_at_out);

/* Stores the paths displayed by the most recent search. */
int storage_save_last_results(const DocumentTable *table, const int *ids, size_t count);
/* Allocates the one-based result path. Caller owns *path_out. */
int storage_load_last_result(size_t number, char **path_out);

/* Returns the ~/.ksearch directory path in a static buffer (not thread-safe,
 * fine for this single-threaded CLI). Creates the directory if missing. */
const char *storage_dir(void);

#endif
