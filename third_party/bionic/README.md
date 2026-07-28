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

### Scheduler Primitive Tranche

| Local file | Upstream path | Upstream ref | Status | Notes |
| --- | --- | --- | --- | --- |
| `include/sched.h` | `libc/include/sched.h` | project-owned | new | Minimal `sched_yield` declaration. |
| `libc/src/sched.c` | `libc/bionic/sched_yield.cpp` | project-owned | new | Cross-host wrapper over project syscall/PAL hook. |

### Internal Atomic And Lock Tranche

| Local file | Upstream path | Upstream ref | Status | Notes |
| --- | --- | --- | --- | --- |
| `libc/include/private/crt_atomic.h` | none | project-owned | new | Private compiler-builtin atomic, spinlock, and wait-backed once helpers for pthread preparation. |

### Pthread Basic Tranche

| Local file | Upstream path | Upstream ref | Status | Notes |
| --- | --- | --- | --- | --- |
| `include/pthread.h` | `libc/include/pthread.h` | project-owned | new | Minimal provisional pthread subset for mutex, once, self, and equal. |
| `libc/src/pthread.c` | mixed Bionic/POSIX surface | project-owned | new | Bootstrap pthread primitives over the private atomic layer and host thread-id hooks. |

### Pthread TLS Key Tranche

| Local file | Upstream path | Upstream ref | Status | Notes |
| --- | --- | --- | --- | --- |
| `include/pthread.h` | `libc/include/pthread.h` | project-owned | extended | Adds provisional pthread key APIs. |
| `libc/src/pthread.c` | mixed Bionic/POSIX surface | project-owned | extended | Adds key allocation, per-thread specific value storage, and bounded destructor passes at project thread exit. |

### Pthread Thread Lifecycle Tranche

| Local file | Upstream path | Upstream ref | Status | Notes |
| --- | --- | --- | --- | --- |
| `include/pthread.h` | `libc/include/pthread.h` and `libc/include/bits/pthread_types.h` | project-owned, Bionic-shaped | extended | Uses one Bionic/Linux-style pthread type layout across all target OSes. |
| `libc/src/pthread.c` | mixed Bionic/POSIX surface | project-owned | extended | Adds thread backends for Windows, Linux, and macOS; Linux uses CLONE_THREAD plus child-tid futex join and a detached reaper, and macOS adapts libSystem pthreads beneath the project ABI. |
| `libc/arch/linux/x86_64/syscall.S` | mixed Bionic/Linux syscall surface | project-owned | extended | Adds raw clone, futex, and thread exit syscall wrappers. |
| `libc/arch/linux/aarch64/syscall.S` | mixed Bionic/Linux syscall surface | project-owned | extended | Adds raw clone, futex, and thread exit syscall wrappers. |

### Pthread Condition Variable Tranche

| Local file | Upstream path | Upstream ref | Status | Notes |
| --- | --- | --- | --- | --- |
| `include/pthread.h` | `libc/include/pthread.h` and `libc/include/bits/pthread_types.h` | project-owned, Bionic-shaped | extended | Adds condition variable public type and API subset, including timed wait. |
| `libc/src/pthread.c` | mixed Bionic/POSIX surface | project-owned | extended | Adds sequence-counter condition variables backed by the private wait/futex layer, including absolute realtime timed waits. |

### Pthread Read/Write Lock Tranche

| Local file | Upstream path | Upstream ref | Status | Notes |
| --- | --- | --- | --- | --- |
| `include/pthread.h` | `libc/include/pthread.h` and `libc/include/bits/pthread_types.h` | project-owned, Bionic-shaped | extended | Adds read/write lock public type and API subset. |
| `libc/src/pthread.c` | mixed Bionic/POSIX surface | project-owned | extended | Adds wait-backed reader/writer lock operations over inline private storage. |

### Pthread Spin Lock Tranche

| Local file | Upstream path | Upstream ref | Status | Notes |
| --- | --- | --- | --- | --- |
| `include/pthread.h` | `libc/include/pthread.h` and `libc/include/bits/pthread_types.h` | project-owned, Bionic-shaped | extended | Adds public spin lock type, process-sharing constants, and API subset. |
| `libc/src/pthread.c` | mixed Bionic/POSIX surface | project-owned | extended | Adds compiler-atomic spin lock operations for process-private use. |

### Private Wait/Futex Tranche

| Local file | Upstream path | Upstream ref | Status | Notes |
| --- | --- | --- | --- | --- |
| `libc/include/private/crt_wait.h` | none | project-owned | new | Private 32-bit wait/wake API used by pthread primitives, including relative timed waits. |
| `libc/src/wait.c` | mixed Bionic/Linux futex and host wait surface | project-owned | new | Maps wait/wake to Linux futex, Windows WaitOnAddress, and macOS libSystem os_sync wait-by-address APIs with timed wait support. |
| `libc/arch/linux/x86_64/syscall.S` | mixed Bionic/Linux syscall surface | project-owned | extended | Adds raw futex syscall wrapper. |
| `libc/arch/linux/aarch64/syscall.S` | mixed Bionic/Linux syscall surface | project-owned | extended | Adds raw futex syscall wrapper. |

## Rules

- Preserve original copyright and license headers.
- Record every imported file in this manifest.
- Keep pristine snapshots, if added later, separate from project-adapted runtime
  sources.
- Do not import broad Bionic internals until a specific tranche requires them.
