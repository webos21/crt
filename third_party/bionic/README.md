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

## Rules

- Preserve original copyright and license headers.
- Record every imported file in this manifest.
- Keep pristine snapshots, if added later, separate from project-adapted runtime
  sources.
- Do not import broad Bionic internals until a specific tranche requires them.
