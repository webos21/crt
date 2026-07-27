# Bionic Import Policy

This document defines how Android Bionic headers and source files are imported
into this project.

## Import Location

Bionic source should be handled in two different locations:

- `third_party/bionic/`
  - provenance records, import manifests, license notes, and optional pristine
    source snapshots.
- project runtime directories such as `libc/string/`, `libm/`, `libdl/`, and
  `linker/`
  - curated files that are actually compiled by this project.

The default policy is curated import. Do not import the full Bionic tree until a
specific milestone requires it.

## Source Commit Recording

Every import tranche must record:

- upstream repository URL;
- upstream branch or tag;
- upstream commit hash;
- original file path;
- local file path;
- license family;
- whether the local file is pristine or adapted;
- a short note explaining any local adaptation.

The first import baseline is:

```text
Repository: https://android.googlesource.com/platform/bionic
Branch: main
Observed main commit: 731631f300090436d7f5df80d50b6275c8c60a93
```

Some first-tranche portable string/memory files are taken from the older
`ics-mr0` Bionic tree because modern Bionic uses architecture-specific assembly
or reorganized upstream source for several of these functions. Those files must
still be recorded individually in `third_party/bionic/README.md`.

## License And Provenance Policy

Bionic is used because it is Android's real libc/libm/libdl/linker stack and
because its libc code is permissively licensed. Imported files must preserve
their original copyright and license headers.

For each import:

- preserve the original file license header;
- preserve the upstream path and commit in a local provenance comment or manifest;
- do not remove attribution;
- keep license-relevant files and notes under `third_party/bionic/`;
- avoid importing Linux internal kernel headers;
- prefer Bionic cleaned kernel headers when kernel UAPI headers are needed.

## No Direct Edits To Pristine Sources

If a pristine Bionic snapshot is imported under `third_party/bionic/`, do not edit
it directly.

Local changes should live in project-owned runtime directories, such as
`libc/string/`, and must be recorded as adaptations. This keeps the upstream
source auditable and makes future updates easier.

## Patch Location Policy

Local adaptations should be small and explicit. Examples:

- include path adjustments;
- C99/freestanding compatibility fixes;
- Windows LLP64 portability fixes;
- replacing Bionic private dependencies with project PAL interfaces;
- disabling Android-only fortify or platform hooks until their tranche exists.

For larger changes, prefer a project wrapper around the imported implementation
rather than rewriting the imported file in place.

## First Import Tranche

The first tranche imports only low-dependency string/memory functions:

- `memcpy`
- `memmove`
- `memset`
- `strlen`
- `strcmp`

The goal is to establish the import and test workflow while keeping the existing
`write`/`_exit` hello bring-up intact.

The second string/memory tranche expands the same low-dependency area with:

- `memchr`
- `memcmp`
- `strcat`
- `strchr`
- `strcpy`
- `strncmp`
- `strncpy`
- `strrchr`

These are byte/string primitives only. Locale-sensitive collation and tokenizing
APIs are deferred.

The third string tranche adds common search/span and allocation-backed helpers:

- `strcspn`
- `strdup`
- `strndup`
- `strnlen`
- `strpbrk`
- `strspn`
- `strstr`

`strdup` and `strndup` depend on the bootstrap allocator. `strndup` and
`strnlen` are project-owned implementations for now; the rest follows Bionic's
BSD-derived portable C sources. Stateful tokenization such as `strtok` is
deferred until thread/TLS policy is clearer.

## Next Runtime Boundary Tranche

After the first string/memory import, the next implemented runtime boundary is:

- `errno`
- `read`
- `write`
- `open`
- `close`
- `lseek`

Linux and macOS use direct syscall wrappers. Windows uses a small POSIX-like fd
table over Win32 APIs. This keeps the public C surface stable while allowing each
host OS to provide its own PAL backend.

## Errno TLS Tranche

`errno` is thread-local on Linux and macOS through compiler TLS. On Windows it
uses a Win32 TLS slot rather than compiler PE TLS so the freestanding startup
path does not require `_tls_index` or the MSVC CRT startup model.

The Windows implementation lazily allocates a TLS index and a per-thread `int`
storage cell with `VirtualAlloc`. Cleanup hooks are deferred until the full
thread lifecycle and pthread key destructor policy exists.

The TLS index initialization uses compiler `__atomic` builtins instead of
Windows `Interlocked*` imports so the implementation works consistently on
Windows ARM64 freestanding links.

## Bootstrap Allocator Tranche

The next allocator tranche adds:

- `malloc`
- `free`
- `calloc`
- `realloc`

This allocator is not imported from Bionic yet. Bionic's production allocator
stack has more dependencies and should be evaluated separately. The current
allocator is a small VM-backed bootstrap heap with a free list, intended only to
support early libc/PAL tests across Linux, Windows, and macOS.

## Stdlib Numeric Conversion Tranche

The current stdlib numeric conversion tranche adds:

- `atoi`
- `atol`
- `strtol`
- `strtoul`

The implementation is adapted from Bionic's BSD-derived conversion routines and
uses the current C-locale `ctype` tranche. It supports bases 2 through 36, base
0 prefix detection, end-pointer reporting, and `ERANGE` on overflow. Wider
integer conversions such as `strtoll`, `strtoull`, and `strtoimax` are deferred.

## C99 Base Header Tranche

The current C99 base header tranche adds:

- `stdint.h`
- `stdbool.h`
- `stddef.h`
- `stdarg.h`

`stdint.h` is adapted from Bionic's public header shape but uses compiler
predefined integer type macros so that LP64 Unix targets and LLP64 Windows
targets both expose correct `intptr_t`, `uintptr_t`, `intmax_t`, and limit
macros. This keeps the sysroot self-contained for code that includes standard
C99 integer headers before the fuller Bionic header set is imported.

`stddef.h` and `stdarg.h` are project-owned freestanding wrappers over compiler
predefined types and builtins. Bionic normally relies on compiler-provided forms
for this layer, so keeping these wrappers small makes the sysroot more explicit
without importing host libc headers.

## Time Tranche

The current time tranche adds:

- `time_t`
- `clockid_t`
- `struct timespec`
- `struct timeval`
- `time`
- `clock_gettime`
- `gettimeofday`
- `nanosleep`

Linux uses direct syscall wrappers for `gettimeofday` and sleep. Windows maps
wall-clock time to `GetSystemTimeAsFileTime` and sleep to `Sleep`. macOS uses
the direct `gettimeofday` syscall and a `poll(NULL, 0, timeout_ms)` based
bootstrap implementation for `nanosleep`.

`CLOCK_REALTIME` is backed by wall-clock time on all hosts. `CLOCK_MONOTONIC`
uses a real monotonic backend: Linux `clock_gettime`, macOS Mach absolute time,
and Windows QPC.

## Scheduler Primitive Tranche

The current scheduler primitive tranche adds:

- `sched_yield`

Linux uses the direct `sched_yield` syscall. Windows maps it to `Sleep(0)`.
macOS uses `poll(NULL, 0, 0)` as a bootstrap yield syscall because there is no
stable public `sched_yield` syscall entry in the Darwin syscall table.

This tranche also changes macOS `nanosleep` from a busy-wait loop to
`poll(NULL, 0, timeout_ms)`. The resolution is millisecond-level for now, but it
is a blocking sleep primitive and is suitable as a stepping stone toward pthread
condition waits.

## Internal Atomic And Lock Tranche

The current internal atomic and lock tranche adds a private libc header:

- `libc/include/private/crt_atomic.h`

This is not a public C11 `<stdatomic.h>` import. It is a small internal layer
over compiler `__atomic` builtins, currently limited to `int` atomics, a
spinlock, and a once-state helper. The goal is to provide a stable foundation
for future pthread mutex, pthread_once, TLS-key bookkeeping, and allocator lock
work without committing to public atomic ABI yet.

The spinlock uses `sched_yield` while waiting, so it depends on the scheduler
primitive tranche. Wider atomics, futex-backed waiting, and public C11 atomics
are deferred.

On Linux AArch64, the build disables outlined atomics with
`-mno-outline-atomics`. Otherwise Clang/GCC can pull in libgcc's LSE atomic
initializer, which depends on glibc's `__getauxval`; that dependency is outside
this freestanding runtime boundary.

## Pthread Basic Tranche

The current pthread basic tranche adds a first public `pthread.h` subset:

- `pthread_t`
- `pthread_mutex_t`
- `pthread_once_t`
- `PTHREAD_MUTEX_INITIALIZER`
- `PTHREAD_ONCE_INIT`
- `pthread_mutex_init`
- `pthread_mutex_destroy`
- `pthread_mutex_lock`
- `pthread_mutex_unlock`
- `pthread_once`
- `pthread_self`
- `pthread_equal`

This is not a complete pthread implementation yet. It provides bootstrap mutex
and once behavior over the internal atomic/lock layer, plus a host thread-id
query for `pthread_self`. Thread creation, join/detach, condition variables,
attributes, cancellation, robust/recursive mutexes, and key destructors are
deferred.

The exposed pthread type layout is still provisional and may change before ABI
stabilization.

## Pthread TLS Key Tranche

The pthread TLS key tranche extends the provisional pthread subset with:

- `pthread_key_t`
- `pthread_key_create`
- `pthread_key_delete`
- `pthread_getspecific`
- `pthread_setspecific`

This tranche intentionally covers only key allocation and per-thread value
storage. Destructor registration is accepted by the API but deferred until
thread exit and pthread lifecycle management are implemented.

Linux and macOS currently use compiler TLS storage for the key value array.
Windows maps each pthread key to a Win32 TLS slot. This keeps Windows free of
compiler-emitted `_tls_index` dependencies while preserving the pthread API
shape used by portable libraries.

## Pthread Thread Lifecycle Tranche

The pthread thread lifecycle tranche adds the first public thread lifecycle API:

- `pthread_attr_t`
- `pthread_create`
- `pthread_join`
- `pthread_exit`

The public pthread ABI is intentionally project-owned and OS-independent. The
current type layout follows Bionic's 64-bit Linux-facing shape:
`pthread_key_t` and `pthread_once_t` are `int`, `pthread_attr_t` is an inline
attribute record, and `pthread_mutex_t` is an inline opaque
`int32_t __private[10]` storage object. `pthread_t` is a pointer-width signed
integer (`intptr_t`): this matches Bionic's LP64 `long` size on Linux/macOS and
avoids truncating internal thread handles on Windows LLP64, where C `long` is
only 32 bits. macOS does not expose Darwin's native opaque pthread types through
this runtime.

The current project-owned implementation is still intentionally narrow.
Attributes are accepted only as a placeholder and ignored. Joinable threads are
supported as the first policy on Windows and Linux; detach, cancellation,
priority, scheduling attributes, guard size, stack attributes, and destructor
execution at thread exit are deferred.

Windows uses `CreateThread`, `WaitForSingleObject`, `CloseHandle`, and
`ExitThread` from `kernel32`. Linux uses a raw `clone` wrapper with a
project-owned stack plus `wait4` for the first joinable-thread path. The Linux
backend deliberately avoids `CLONE_THREAD` for now so that `wait4` can provide a
simple join primitive while the runtime does not yet have futexes.

macOS keeps the same public pthread ABI as Linux and Windows. Basic mutex,
once, self, and key APIs are provided by the project implementation. Thread
creation and join are implemented through an adaptation layer that resolves
libSystem's native `pthread_create`, `pthread_join`, and `pthread_exit` with
`dlsym(RTLD_NEXT, ...)`, stores the native Darwin thread handle inside the
project control block, and exposes only the project-owned `pthread_t` value to
callers.

## VM Memory Tranche

The VM memory tranche adds:

- `mmap`
- `munmap`

The first implementation supports anonymous private mappings only. Linux and
macOS use direct syscall wrappers. Windows maps this subset to
`VirtualAlloc`/`VirtualFree`. The bootstrap allocator now uses this VM subset.
File-backed mappings, protection changes, and `mprotect` are deferred.

## Ctype Tranche

The current ctype tranche adds C-locale ASCII classification and conversion:

- `isalnum`
- `isalpha`
- `isascii`
- `isblank`
- `iscntrl`
- `isdigit`
- `isgraph`
- `islower`
- `isprint`
- `ispunct`
- `isspace`
- `isupper`
- `isxdigit`
- `toascii`
- `tolower`
- `toupper`

This is adapted from Bionic's ctype inline logic into out-of-line C99 functions.
Locale-aware `_l` variants are deferred until locale/xlocale policy is defined.

## Minimal Stdio Tranche

The current stdio tranche adds:

- standard streams: `stdin`, `stdout`, `stderr`
- `fopen`
- `fclose`
- `fseek`
- `ftell`
- bootstrap `printf`
- bootstrap `fprintf`
- bootstrap `snprintf`
- bootstrap `vsnprintf`
- `fputc`
- `fputs`
- `puts`
- `putchar`
- `fread`
- `fwrite`
- `fflush`

This is not a final Bionic stdio import. The `FILE` ABI, buffering model, and
complete `printf` family are intentionally deferred until the file/path,
allocator, and formatting policies are more stable. The current formatter only
supports a small subset useful for early bring-up: `%s`, `%c`, `%d`, `%i`, `%u`,
`%x`, `%X`, `%p`, `%%`, and `l`/`ll` integer length modifiers.
