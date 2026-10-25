#ifndef KSEARCH_TOKENIZER_H
#define KSEARCH_TOKENIZER_H

/* Splits `text` on any run of non-alphanumeric characters, lowercases each
 * resulting token, and calls emit(token, ctx) for every non-empty token.
 * Tokens of length 1 are skipped (too noisy to be useful for search). */
void tokenize(const char *text, void (*emit)(const char *token, void *ctx), void *ctx);

#endif
