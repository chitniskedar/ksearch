#include "platform.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#include <direct.h>
#include <io.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#endif

char *platform_strdup(const char *text)
{
    size_t length;
    char *copy;
    if (!text) return NULL;
    length = strlen(text) + 1;
    copy = malloc(length);
    if (copy) memcpy(copy, text, length);
    return copy;
}

char *platform_path_join(const char *left, const char *right)
{
    size_t left_len, right_len;
    int slash;
    char *out;
    if (!left || !right) return NULL;
    left_len = strlen(left);
    right_len = strlen(right);
    slash = left_len != 0 && left[left_len - 1] != '/' && left[left_len - 1] != '\\';
    if (left_len > SIZE_MAX - right_len - (size_t)slash - 1) return NULL;
    out = malloc(left_len + right_len + (size_t)slash + 1);
    if (!out) return NULL;
    memcpy(out, left, left_len);
    if (slash) out[left_len++] = '/';
    memcpy(out + left_len, right, right_len + 1);
    return out;
}

char *platform_absolute_path(const char *path)
{
    char *out;
    if (!path || !*path) return NULL;
#if defined(_WIN32)
    DWORD needed;
    if ((path[0] && path[1] == ':') || path[0] == '/' || path[0] == '\\')
        return platform_strdup(path);
    needed = GetCurrentDirectoryA(0, NULL);
    if (needed == 0) return platform_strdup(path);
    out = malloc((size_t)needed);
    if (!out) return NULL;
    if (GetCurrentDirectoryA(needed, out) == 0) { free(out); return platform_strdup(path); }
    {
        char *joined = platform_path_join(out, path);
        free(out);
        return joined;
    }
#else
    size_t size = 256;
    if (path[0] == '/') return platform_strdup(path);
    for (;;) {
        out = malloc(size);
        if (!out) return NULL;
        if (getcwd(out, size)) break;
        free(out);
        if (size > SIZE_MAX / 2) return platform_strdup(path);
        size *= 2;
    }
    {
        char *joined = platform_path_join(out, path);
        free(out);
        return joined;
    }
#endif
}

int platform_mkdir(const char *path)
{
#if defined(_WIN32)
    return _mkdir(path);
#else
    return mkdir(path, 0755);
#endif
}

double platform_now_ms(void)
{
#if defined(_WIN32)
    static LARGE_INTEGER frequency;
    LARGE_INTEGER counter;
    if (frequency.QuadPart == 0) QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart * 1000.0 / (double)frequency.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
#endif
}

int platform_stdout_is_terminal(void)
{
#if defined(_WIN32)
    return _isatty(_fileno(stdout));
#else
    return isatty(fileno(stdout));
#endif
}

#if defined(_WIN32)
static wchar_t *utf8_to_wide(const char *text)
{
    int length;
    wchar_t *wide;
    if (!text) return NULL;
    length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, NULL, 0);
    if (length <= 0) return NULL;
    wide = malloc((size_t)length * sizeof(*wide));
    if (!wide) return NULL;
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, wide, length))
    {
        free(wide); return NULL;
    }
    return wide;
}

int platform_open_file(const char *utf8_path)
{
    wchar_t *path = utf8_to_wide(utf8_path);
    INT_PTR result;
    if (!path) return -1;
    result = (INT_PTR)ShellExecuteW(NULL, L"open", path, NULL, NULL, SW_SHOWNORMAL);
    free(path);
    return result > 32 ? 0 : -1;
}

int platform_reveal_file(const char *utf8_path)
{
    wchar_t *path = utf8_to_wide(utf8_path);
    const wchar_t prefix[] = L"/select,\"";
    size_t length;
    wchar_t *args;
    INT_PTR result;
    if (!path) return -1;
    length = wcslen(prefix) + wcslen(path) + 2;
    args = malloc(length * sizeof(*args));
    if (!args)
    { free(path); return -1; }
    wcscpy(args, prefix);
    wcscat(args, path);
    wcscat(args, L"\"");
    result = (INT_PTR)ShellExecuteW(NULL, L"open", L"explorer.exe", args, NULL, SW_SHOWNORMAL);
    free(args); free(path);
    return result > 32 ? 0 : -1;
}
#elif defined(__APPLE__)
#include <unistd.h>
int platform_open_file(const char *path)
{ return execlp("open", "open", path, (char *)NULL) == -1 ? -1 : 0; }
int platform_reveal_file(const char *path)
{ return execlp("open", "open", "-R", path, (char *)NULL) == -1 ? -1 : 0; }
#else
#include <unistd.h>
int platform_open_file(const char *path)
{ return execlp("xdg-open", "xdg-open", path, (char *)NULL) == -1 ? -1 : 0; }
int platform_reveal_file(const char *path)
{
    char *copy = platform_strdup(path);
    char *slash;
    if (!copy) return -1;
    slash = strrchr(copy, '/');
    if (slash) *slash = '\0'; else strcpy(copy, ".");
    return execlp("xdg-open", "xdg-open", copy, (char *)NULL) == -1 ? -1 : 0;
}
#endif
