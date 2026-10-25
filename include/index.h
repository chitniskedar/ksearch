#ifndef KSEARCH_INDEX_H
#define KSEARCH_INDEX_H

#include "document.h"
#include "hashmap.h"

/* Tokenizes every document's filename and populates `map` with
 * term -> posting list of document ids. (Content indexing is a later phase;
 * this milestone is filename-only, as scoped in the project plan.) */
void index_build(const DocumentTable *table, HashMap *map);

#endif
