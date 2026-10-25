#include "cli.h"

#if defined(_WIN32)
#include <windows.h>

/* MinGW's bundled headers don't always define this Windows-10-era constant
 * (it depends on _WIN32_WINNT), so define it ourselves if missing rather
 * than fight header versioning. This is its fixed, documented value. */
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

/* Older conhost.exe windows don't interpret ANSI/VT escape codes (used for
 * clickable hyperlinks) unless a program explicitly asks for it. Windows
 * Terminal already has this on by default, but enabling it unconditionally
 * is harmless there and required in legacy console windows. */
static void enable_windows_vt_processing(void) {
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (out != INVALID_HANDLE_VALUE && GetConsoleMode(out, &mode)) {
        SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
}
#endif

int main(int argc, char **argv) 
{
#if defined(_WIN32)
    enable_windows_vt_processing();
#endif
    return cli_run(argc, argv);
}