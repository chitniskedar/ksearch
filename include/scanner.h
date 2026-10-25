#ifndef KSEARCH_SCANNER_H
#define KSEARCH_SCANNER_H

#include "document.h"

/* Recursively walks `root`, adding every regular file found to `table`.
 * Skips hidden dot-directories (e.g. .git) and follows no symlinks.
 * Returns the number of files added, or -1 on failure to open `root`. */
int scanner_walk(const char *root, DocumentTable *table);

#endif
