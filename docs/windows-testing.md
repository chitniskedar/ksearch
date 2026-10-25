# Windows manual testing

Build with a current MinGW GCC from PowerShell:

```powershell
mingw32-make clean
mingw32-make all test
mingw32-make debug
```

The MinGW GCC installed in the original workspace is too old to compile this
project with AddressSanitizer: its compiler crashes internally. Use a current
MinGW-w64 GCC for the sanitizer command and report the compiler version if it
still fails.

Check the following on Windows 10 or 11:

- Run `ksearch init`, index a normal folder, and search for a filename.
- Test paths with spaces, non-ASCII characters, `#`, `%`, `?`, tabs, and a very long nested path.
- Confirm a junction or symlink is not recursively followed.
- Confirm OneDrive cloud-only files are not hydrated while indexing.
- Pipe search output to a file and verify it contains no terminal escape sequences.
- Run a search in Windows Terminal and click both the filename and path hyperlinks.
