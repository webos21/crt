# Android Bionic Provenance

This directory records Android Bionic provenance and import policy.

The project currently uses curated imports rather than a full pristine Bionic
source snapshot.

## Upstream

```text
Repository: https://android.googlesource.com/platform/bionic
Primary branch: main
Observed main commit: 731631f300090436d7f5df80d50b6275c8c60a93
License family: BSD-style/permissive for imported libc files
```

## Imported Files

### String/Memory Tranche 1

The first runtime import uses portable C string/memory implementations from the
Android Bionic `ics-mr0` tree.

| Local file | Upstream path | Upstream ref | Status | Notes |
| --- | --- | --- | --- | --- |
| `libc/string/bcopy.c` | `libc/string/bcopy.c` | `ics-mr0` | adapted | Uses `uintptr_t` instead of `long` for pointer-width arithmetic across Windows LLP64. |
| `libc/string/memcpy.c` | `libc/string/memcpy.c` | `ics-mr0` | adapted | Keeps Bionic wrapper shape; includes local `bcopy.c`. |
| `libc/string/memmove.c` | `libc/string/bcopy.c` | `ics-mr0` | adapted | Generated from the same Bionic `bcopy.c` implementation using `MEMMOVE`. |
| `libc/string/memset.c` | `libc/string/memset.c` | `ics-mr0` | adapted | Formatting only; license header preserved. |
| `libc/string/strlen.c` | `libc/string/strlen.c` | `ics-mr0` | adapted | Removes kernel/standalone include branch for this hosted public header environment. |
| `libc/string/strcmp.c` | `libc/string/strcmp.c` | `ics-mr0` | adapted | Removes kernel/standalone include branch for this hosted public header environment. |
| `libc/string/memchr.c` | `libc/string/memchr.c` | `ics-mr0` | adapted | Formatting only; license header preserved. |
| `libc/string/memcmp.c` | `libc/string/memcmp.c` | `ics-mr0` | adapted | Formatting only; license header preserved. |
| `libc/string/strcat.c` | `libc/string/strcat.c` | `ics-mr0` | adapted | Removes Android API warning hook. |
| `libc/string/strchr.c` | `libc/string/index.c` | `ics-mr0` | adapted | Keeps Bionic/OpenBSD implementation under the standard `strchr` name. |
| `libc/string/strcpy.c` | `libc/string/strcpy.c` | `ics-mr0` | adapted | Formatting only; license header preserved. |
| `libc/string/strncmp.c` | `libc/string/strncmp.c` | `ics-mr0` | adapted | Removes kernel/standalone include branch for this hosted public header environment. |
| `libc/string/strncpy.c` | `libc/string/strncpy.c` | `ics-mr0` | adapted | Removes kernel/standalone include branch for this hosted public header environment. |
| `libc/string/strrchr.c` | `libc/string/rindex.c` | `ics-mr0` | adapted | Keeps Bionic/OpenBSD implementation under the standard `strrchr` name. |
| `libc/string/strcspn.c` | `libc/string/strcspn.c` | `ics-mr0` | adapted | Formatting only; license header preserved. |
| `libc/string/strdup.c` | `libc/string/strdup.c` | `ics-mr0` | adapted | Removes unused `sys/types.h` dependency for this project. |
| `libc/string/strndup.c` | none | project-owned | new | Allocator-backed bounded duplicate helper using local `strnlen`. |
| `libc/string/strnlen.c` | none | project-owned | new | Bounded length helper using local `memchr`. |
| `libc/string/strpbrk.c` | `libc/string/strpbrk.c` | `ics-mr0` | adapted | Formatting only; license header preserved. |
| `libc/string/strspn.c` | `libc/string/strspn.c` | `ics-mr0` | adapted | Formatting only; license header preserved. |
| `libc/string/strstr.c` | `libc/string/strstr.c` | `ics-mr0` | adapted | Formatting only; license header preserved. |

### Ctype Tranche

The ctype tranche adapts Bionic's ASCII/C-locale inline logic into project-owned
out-of-line C99 functions.

| Local file | Upstream path | Upstream ref | Status | Notes |
| --- | --- | --- | --- | --- |
| `libc/src/ctype.c` | `libc/include/ctype.h` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | adapted | Converts Bionic inline predicates to exported C99 functions and defers locale-aware `_l` variants. |
| `include/ctype.h` | `libc/include/ctype.h` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | adapted | Keeps only public prototypes for the current C-locale tranche. |

### Stdlib Numeric Conversion Tranche

| Local file | Upstream path | Upstream ref | Status | Notes |
| --- | --- | --- | --- | --- |
| `libc/src/atoi.c` | `libc/stdlib/atoi.c` | `ics-mr0` | adapted | Uses local `strtol` instead of Bionic's `strtoimax` dependency. |
| `libc/src/atol.c` | `libc/stdlib/atol.c` | `ics-mr0` | adapted | Uses local `strtol` instead of Bionic's `strtoimax` dependency. |
| `libc/src/strtol.c` | `libc/stdlib/strtol.c` | `ics-mr0` | adapted | Keeps BSD-derived conversion logic; adds invalid-base handling for this project. |
| `libc/src/strtoul.c` | `libc/stdlib/strtoul.c` | `ics-mr0` | adapted | Keeps BSD-derived conversion logic; adds invalid-base handling for this project. |
| `include/limits.h` | `libc/include/limits.h` | project-owned | new | Minimal C99 limit macros needed by numeric conversion tests. |
| `include/stdlib.h` | `libc/include/stdlib.h` | project-owned | adapted | Adds prototypes for the current allocator and numeric conversion tranche. |

### C99 Base Header Tranche

| Local file | Upstream path | Upstream ref | Status | Notes |
| --- | --- | --- | --- | --- |
| `include/stdint.h` | `libc/include/stdint.h` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | adapted | Uses compiler predefined type macros instead of Bionic's `__LP64__` branch to support Windows LLP64. |
| `include/stdbool.h` | `libc/include/stdbool.h` | project-owned | new | Minimal C99 boolean macro header. |
| `include/stddef.h` | compiler-provided header layer | project-owned | new | Defines `ptrdiff_t`, `size_t`, `wchar_t`, `NULL`, and `offsetof` from compiler builtins. |
| `include/stdarg.h` | compiler-provided header layer | project-owned | new | Defines `va_list` and `va_*` macros from compiler builtins. |

### Time Tranche

| Local file | Upstream path | Upstream ref | Status | Notes |
| --- | --- | --- | --- | --- |
| `include/time.h` | `libc/include/time.h` | project-owned | new | Minimal C/POSIX time declarations for pthread preparation. |
| `include/sys/time.h` | `libc/include/sys/time.h` | project-owned | new | Minimal `timeval` and `gettimeofday` declaration. |
| `libc/src/time.c` | mixed Bionic/POSIX surface | project-owned | new | Cross-host bootstrap implementation over project syscall/PAL hooks. |

## Rules

- Preserve original copyright and license headers.
- Record every imported file in this manifest.
- Keep pristine snapshots, if added later, separate from project-adapted runtime
  sources.
- Do not import broad Bionic internals until a specific tranche requires them.
