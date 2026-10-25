#include "tokenizer.h"
#include <ctype.h>
#include <string.h>

#define MAX_TOKEN_LEN 128

void tokenize(const char *text, void (*emit)(const char *token, void *ctx), void *ctx)
{
    char buf[MAX_TOKEN_LEN];
    size_t len = 0;

    for (const char *p = text; ; p++)
    {
        unsigned char c = (unsigned char)*p;
        if (c != '\0' && isalnum(c))
        {
            if (len < MAX_TOKEN_LEN - 1)
            {
                buf[len++] = (char)tolower(c);
            }
            continue;
        }

        /* End of a token (or end of string). */
        if (len > 1)
        {
            buf[len] = '\0';
            emit(buf, ctx);
        }
        len = 0;

        if (c == '\0') break;
    }
}
