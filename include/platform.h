#ifndef KSEARCH_PLATFORM_H
#define KSEARCH_PLATFORM_H

#include <stddef.h>

/* Small cross-platform helpers. Returned strings are owned by the caller. */
char *platform_strdup(const char *text);
char *platform_path_join(const char *left, const char *right);
char *platform_absolute_path(const char *path);
int platform_mkdir(const char *path);
double platform_now_ms(void);
int platform_stdout_is_terminal(void);
int platform_open_file(const char *utf8_path);
int platform_reveal_file(const char *utf8_path);

#endif
