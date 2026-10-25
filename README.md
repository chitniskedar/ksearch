# KSearch

KSearch is a native command-line filename search tool written in C. Index a
directory once, then search its saved file list without walking the filesystem
again for every query.

It is designed for Windows PowerShell with MinGW GCC, and also has portable
POSIX code paths.

## Build

Windows PowerShell:

```powershell
mingw32-make clean
mingw32-make all
mingw32-make test
```

Development build with AddressSanitizer and UndefinedBehaviorSanitizer:

```powershell
mingw32-make debug
```

The MinGW GCC originally used for this workspace is too old to build the
sanitizer configuration; use a current MinGW-w64 GCC for that command.

On Linux or macOS:

```sh
make clean all test
```

## Quick start

```powershell
ksearch init
ksearch index D:\Documents
ksearch scheduler
ksearch "process scheduling" --sort relevance
ksearch --stats
```

`index` scans regular files beneath a directory and persists their path, size,
and modified time under `~/.ksearch` (`USERPROFILE/.ksearch` on Windows when
`HOME` is not set). Searches rebuild the in-memory filename index from the
saved document list; they do not rescan the indexed directory.

For automated testing, `KSEARCH_HOME` overrides the parent directory of
`.ksearch`.

## Commands

```text
ksearch init
ksearch index <path>
ksearch rebuild
ksearch remove <path>
ksearch <query> [flags]
ksearch open <N>
ksearch reveal <N>
ksearch --stats
ksearch --help
```

- `init` creates the KSearch data directory.
- `index <path>` creates a filename index for one directory tree. At present,
  indexing a new path replaces the previous index.
- `rebuild` scans the last indexed root again.
- `remove <path>` removes indexed files whose paths contain `<path>`.
- `open <N>` opens result `N` from the most recent search with its default
  application.
- `reveal <N>` opens the file manager and selects result `N` on Windows.
- `--stats` reports the indexed root, document count, term count, saved index
  size, and timestamp.

## Search

The query is tokenized into lowercase alphanumeric filename terms. KSearch
first looks for files matching all terms; if there are no such files, it shows
partial matches instead. The normal sort order is newest modified file first.

```powershell
ksearch storage
ksearch storage --sort relevance
ksearch storage --type c
ksearch storage --type c --path src
ksearch storage --recent 30
ksearch storage --limit 5
ksearch storage --all
```

Available flags:

| Flag | Meaning |
| --- | --- |
| `--type <ext>` | Restrict results to an extension, such as `c` or `txt`. |
| `--path <text>` | Restrict results to paths containing the text. |
| `--recent <days>` | Restrict results to files modified in the past number of days. |
| `--sort recent` | Sort newest to oldest. This is the default. |
| `--sort relevance` | Sort by filename-term relevance. |
| `--limit <N>` | Display at most `N` results. Default: 20. |
| `--all` | Display every result. |

When output goes to an interactive terminal, each result is an OSC-8 link to
its containing folder. This intentionally opens File Explorer rather than the
file's associated application. Use `ksearch reveal <N>` to select the exact
file in Explorer. Piped output is plain text with no terminal escape codes.

## Data files

KSearch currently writes:

```text
~/.ksearch/documents.dat    Indexed document records
~/.ksearch/meta.dat         Indexed root and timestamp
~/.ksearch/last_results.dat Last displayed result paths
```

The document and result paths are hex encoded in the current text format, so
paths containing tabs and newlines round-trip safely. This is an implementation
format, not a stable public index format.

## Current scope and limitations

- Searches index filenames only; file contents are not read or indexed yet.
- The index tracks one root at a time.
- Indexing is single-threaded and full rebuilds rescan the entire root.
- Hidden dot entries are skipped by the current scanner.
- The Windows recursive scanner still needs the planned wide Win32 API pass
  for long paths, reparse-point handling, and OneDrive cloud-only safeguards.
- Boolean query syntax, fuzzy matching, TF-IDF, persistent inverted indexes,
  file watching, and semantic search are not implemented yet.

## Project layout

```text
include/     Public module headers
src/         C implementation
tests/       Dependency-free test harness
docs/        Platform testing notes
```

## Testing

`make test` builds and runs the dependency-free tests for tokenization,
hashmap/ranking behavior, and lossless storage round trips.

See [Windows testing notes](docs/windows-testing.md) for the Windows manual
checklist.
