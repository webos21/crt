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

The machine-readable manifest is:

```text
third_party/bionic/import_manifest.json
```

The JSON schema is:

```text
third_party/bionic/import_manifest.schema.json
```

The human-readable manifest table in `third_party/bionic/README.md` is still
useful for browsing, but the JSON manifest is the source of truth for automated
checks. Run this after adding or changing imported sources:

```sh
cmake --build --preset macos-host-ninja-debug --target check-import-manifest
```

or directly:

```sh
python3 tools/check_import_manifest.py
```

The first import baseline is:

```text
Repository: https://android.googlesource.com/platform/bionic
Branch: main
Observed main commit: 731631f300090436d7f5df80d50b6275c8c60a93
```

## Main-First Source Policy

The default source policy is **Bionic main first**. New imports should first look
at `refs/heads/main` and should use current Bionic public headers, current
Bionic FreeBSD/msun sources, current Bionic builtins policy, and current Bionic
kernel-header flow whenever those sources can be used cleanly in this project's
C99/freestanding PAL environment.

Older Bionic refs such as `ics-mr0` or `884e4f8` are legacy exceptions, not the
baseline. A legacy exception is allowed only when at least one of these applies:

- current Bionic no longer carries a small portable C implementation for the
  function;
- current Bionic routes the function through architecture-specific assembly,
  optimized routines, Android-private C++ runtime code, fortify hooks, or other
  dependencies that would pull in a larger Android runtime tranche too early;
- current Bionic's available implementation is derived from a source family this
  project has decided not to use as a primary base for that tranche, such as
  musl-derived code in an otherwise fdlibm-oriented import;
- the imported source is explicitly temporary bootstrap code and has a documented
  path back to current Bionic or to a project-owned PAL implementation.

Every legacy exception must be recorded in `third_party/bionic/README.md` with
the upstream ref, original path, local adaptation status, and a short reason why
`main` was not used. Legacy imports should be treated as review targets during
future cleanup, especially once the relevant PAL, pthread, fenv, or architecture
support exists.

Legacy exceptions must also be classified in
`third_party/bionic/import_manifest.json` using one of these `review_class`
values:

- `bootstrap_keep`
  - keep temporarily because replacement would pull in a larger tranche or a
    premature ABI policy.
- `main_replace_candidate`
  - re-check against Bionic `main` before performance or architecture work.
- `project_owned_transition`
  - either replace with a cleaner current-main source or freeze as
    project-owned behavior while preserving attribution.

The current legacy review is summarized in
`third_party/bionic/legacy_import_review.md`.

Some first-tranche portable string/memory files are currently taken from the
older `ics-mr0` Bionic tree because modern Bionic uses architecture-specific
assembly or reorganized upstream source for several of these functions. Some
early fdlibm functions are taken from Bionic `884e4f8` because current Bionic
`main` no longer lists direct portable C sources for those exact double-precision
entry points. These choices are compatibility/bootstrap choices, not a statement
that `ics-mr0` or `884e4f8` is the project baseline.

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

The second broad string/memory tranche adds:

- `memccpy`
- `stpcpy`
- `stpncpy`
- `strcasecmp`
- `strncasecmp`
- `strcoll`
- `strsignal`
- `strxfrm`

`strcoll` and `strxfrm` currently implement the C/POSIX locale bootstrap policy:
collation is bytewise and transformation is a bounded copy of the input string.
This keeps configure probes and basic libraries moving, but full locale-aware
collation remains deferred until the locale subsystem exists. `strsignal` uses
the project's current signal number surface and returns generic text for unknown
signals.

## Locale Tranche

The first locale tranche adds the C/POSIX locale surface needed by the existing
ctype, string collation, numeric conversion, and time-formatting code:

- `locale.h`
- `LC_ALL`
- `LC_COLLATE`
- `LC_CTYPE`
- `LC_MONETARY`
- `LC_NUMERIC`
- `LC_TIME`
- `LC_MESSAGES`
- `struct lconv`
- `setlocale`
- `localeconv`

Only the built-in `C`/`POSIX` locale is supported. `setlocale(category, NULL)`
queries the current locale and returns `"C"`. `setlocale(category, "")` is
accepted as a request for the best available runtime locale, which is also `"C"`
for now. Host locale import, locale archives, `LANG`/`LC_*` environment parsing,
thread-local locale objects, `_l` variants, and locale-aware collation remain
deferred.

`localeconv` returns a static C-locale `struct lconv`: decimal point is `"."`,
grouping and currency strings are empty, and unavailable numeric fields use
`CHAR_MAX`. This makes the locale contract explicit while keeping the runtime
independent from host libc locale state.

## Wide Character And Multibyte Tranche

The first `wchar`/`mbstate` tranche adds:

- `wchar.h`
- `wctype.h`
- `wint_t`
- `mbstate_t`
- `WEOF`
- `MB_CUR_MAX`
- `btowc`
- `wctob`
- `mbrtowc`
- `wcrtomb`
- `mbsrtowcs`
- `wcsrtombs`
- `mbstowcs`
- `wcstombs`
- legacy `mblen`, `mbtowc`, and `wctomb`
- basic wide string and wide memory helpers
- ASCII/C-locale `isw*`, `tow*`, `wctype`, and `wctrans`

`wchar_t` is part of the project ABI rather than the host OS ABI. The build
uses `-Xclang -fwchar-type=int` so Windows, Linux, and macOS all expose a signed
32-bit `wchar_t`, matching the Linux/Bionic-style libc surface instead of
Windows' native 16-bit `wchar_t`. The public typedef still comes from the
compiler's `__WCHAR_TYPE__`, but the project build flags force that compiler
type to the runtime ABI we want.

The bootstrap multibyte encoding is UTF-8. Full Unicode classification, case
mapping beyond ASCII, locale-specific multibyte encodings, Windows UTF-16 host
API adapters, and xlocale-aware `_l` variants remain deferred.

## Libm Bootstrap Tranche

The first `libm` tranche adds a separate `libm.a` and public `math.h` while
keeping the traditional `-lm` link boundary distinct from `libc.a`.

The current surface includes:

- `math.h`
- `HUGE_VAL`, `HUGE_VALF`, `HUGE_VALL`
- `INFINITY`
- `NAN`
- `fpclassify`, `isfinite`, `isinf`, `isnan`, `isnormal`, and `signbit`
- `fabs`, `fabsf`, `fabsl`
- `copysign`, `copysignf`, `copysignl`
- `fmin`, `fminf`, `fminl`
- `fmax`, `fmaxf`, `fmaxl`
- `floor`, `floorf`, `floorl`
- `ceil`, `ceilf`, `ceill`
- `trunc`, `truncf`, `truncl`
- `round`, `roundf`, `roundl`
- `sqrt`, `sqrtf`, `sqrtl`
- `exp`, `expf`, `expl`
- `log`, `logf`, `logl`
- `scalbn`, `scalbnf`, `scalbnl`
- `ldexp`, `ldexpf`, `ldexpl`
- `pow`, `powf`, `powl`
- `sin`, `sinf`, `sinl`
- `cos`, `cosf`, `cosl`
- `tan`, `tanf`, `tanl`
- `log10`, `log10f`, `log10l`
- `log2`, `log2f`, `log2l`
- `expm1`, `expm1f`, `expm1l`
- `log1p`, `log1pf`, `log1pl`
- `frexp`, `frexpf`, `frexpl`
- `modf`, `modff`, `modfl`
- `fmod`, `fmodf`, `fmodl`
- `remainder`, `remainderf`, `remainderl`
- `remquo`, `remquof`, `remquol`
- `fenv.h` architecture-backed environment surface for x86_64 and AArch64

The first Bionic/FreeBSD msun import replaces the double-precision
`floor`/`ceil`/`trunc`/`round` and `fmin`/`fmax` implementations with curated
sources from Bionic's FreeBSD math tree. Local private headers are adapted to
avoid host libc and endian dependencies while preserving the upstream copyright
notices.

The `sqrt` and `sqrtf` implementations follow current Bionic `main` policy and
use compiler builtins rather than importing the older portable fdlibm
`e_sqrt.c`. The project uses Clang's elementwise sqrt builtin so Debug/O0
Windows builds lower directly to hardware sqrt instructions instead of producing
a recursive `sqrt`/`sqrtf` libcall back into the same exported function.

The `fabs*`, `copysign*`, `fmin`, `fmax`, `fminf`, and `fmaxf` functions also
use Clang builtins. These were checked at Debug/O0 for the supported Windows and
Linux target families to avoid the recursive libcall issue seen with ordinary
`__builtin_sqrt`.

The exponential tranche imports `exp` and native `expf` from older Bionic fdlibm
because current Bionic `main` no longer lists matching direct portable
`e_exp.c`/`e_expf.c` sources in the same fdlibm source family. `expl` remains a
bootstrap wrapper around the imported double implementation until the
long-double ABI tranche is opened.

The logarithm tranche follows the same policy for `log` and native `logf`.
Current Bionic `main` does not list the same direct fdlibm source files; this
project imports the older Bionic fdlibm sources and keeps `logl` as a bootstrap
wrapper for now.

The scaling and power tranche imports `scalbn`, `scalbnf`, and `pow` from the
same older Bionic fdlibm source set used for `exp` and `log`, and then imports
native `powf` from the same source family. Current Bionic `main` has `scalbn*`
sources derived from musl; this project intentionally uses the older fdlibm
versions here to keep the early libm tranche aligned with the project's
Bionic/fdlibm provenance policy. `ldexp` and `ldexpf` are thin aliases
implemented as wrappers, while `scalbnl`, `ldexpl`, and `powl` remain bootstrap
wrappers over the imported double implementations.

The trigonometric tranche imports current Bionic FreeBSD/msun `sin`, `cos`,
`tan`, their shared kernel functions, and the double-precision argument
reduction path. The public double functions use the original FreeBSD structure:
`e_rem_pio2.c` is included into each public wrapper with `INLINE_REM_PIO2`,
while `k_rem_pio2.c`, `k_sin.c`, `k_cos.c`, and `k_tan.c` are compiled once as
shared internal helpers. The native float `tanf` path is also imported from
current Bionic FreeBSD/msun; it includes `e_rem_pio2f.c` and `k_tanf.c` inline.
For `sinf` and `cosf`, the observed Bionic main tree provides the float kernels
`k_sinf.c` and `k_cosf.c` plus `e_rem_pio2f.c`, but not standalone public
`s_sinf.c` or `s_cosf.c` wrappers. The project therefore owns a small
`s_sincosf.c` wrapper while preserving Bionic main provenance for the imported
float reduction and kernel helpers. `sinl`, `cosl`, and `tanl` remain bootstrap
wrappers for now.

The configure-friendly accuracy tranche replaces the earlier project-owned
bootstrap implementations for `log10`, `log10f`, `expm1`, `expm1f`, `log1p`,
`log1pf`, `frexp`, `frexpf`, `modf`, `modff`, `fmod`, and `fmodf` with current
Bionic FreeBSD/msun sources. The long-double variants remain wrappers over the
double implementations until a long-double ABI and fenv policy tranche is
opened. `log2` and native `log2f` use explicit FreeBSD upstream msun sources as
a documented source-family exception because neither the observed Bionic main
tree nor the older fdlibm ref used here exposed direct `e_log2.c`/`e_log2f.c`
files. `log2l` remains a bootstrap wrapper.

The remainder tranche replaces `remainder`, `remainderf`, `remquo`, and
`remquof` with current Bionic FreeBSD/msun sources. `remainderl` and `remquol`
remain wrappers over the double implementations. The local `math_private.h`
adds a small `nan_mix_op` compatibility macro for these current FreeBSD/msun
sources so the imported files can remain close to upstream while the project
keeps a reduced freestanding private header.

The first hardware `fenv.h` tranche exposes the C99 exception and rounding
constants, `FE_DFL_ENV`, and the core `fe*` functions. The implementation is
architecture-backed for the supported 64-bit CPU families: x86_64 stores and
restores MXCSR plus the x87 control/status words, while AArch64 stores and
restores FPCR/FPSR. The x86_64 path keeps rounding mode synchronized between
MXCSR and x87 so host-native 80-bit `long double` code can observe the same
rounding policy as SSE code. This is not a full `fxsave`/`fldenv` environment
image: x87 tag word, instruction/data pointers, and trap-enable behavior remain
deferred.

`math_errhandling` is still `0`: imported libm functions are not required to set
`errno`, and project-owned wrappers should not add per-function `errno` side
effects. The fenv API itself exposes observable exception flags, and imported
math sources may naturally raise hardware flags, but the project does not yet
require every libm function to raise exactly the POSIX/IEEE exception set for
every edge case.

Most common float variants now use native float sources or project-owned
wrappers over native float kernels. The first long-double tranche moves the
public `*l` APIs out of double wrappers and into a project-owned portable
implementation in `libm/src/long_double.c`. This is still a bootstrap accuracy
layer, not a correctly-rounded fdlibm/msun import.

The project long-double ABI follows the active compiler target default and does
not use `-mlong-double-64`, `-mlong-double-80`, or `-mlong-double-128` to force a
single representation. Bionic 64-bit Android uses 128-bit long double, but that
cannot be forced uniformly across this project's Windows/macOS/Linux target
matrix. Future source imports should therefore be selected by the observed
`LDBL_MANT_DIG`: double-sized/fake long double for `53`, ld80-style sources for
`64`, and ld128-style sources for `113`. Per-function `errno` policy and full
edge-case accuracy coverage remain deferred to later libm import tranches. Their
planned source dependencies are recorded in `docs/libm_dependency_map.md`.

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

`errno`, `pthread_self()`, pthread key storage, and thread-name state are routed
through the private `crt_tls` adapter. The public ABI remains Bionic-shaped, but
each host backend can choose the mechanism that works with its startup model.

Linux currently uses a project-owned current-thread registry behind the adapter
until the runtime grows a Bionic-style TCB and `CLONE_SETTLS` setup. macOS may
use compiler TLS behind the adapter because libSystem-created threads already
have native TLS state. Windows uses Win32 TLS slots rather than compiler PE TLS
so the freestanding startup path does not require `_tls_index` or the MSVC CRT
startup model.

## Bootstrap Allocator Tranche

The next allocator tranche adds:

- `malloc`
- `free`
- `calloc`
- `realloc`

This allocator is not imported from Bionic yet. Bionic's production allocator
stack has more dependencies and should be evaluated separately. The current
allocator is a small VM-backed bootstrap heap with an internal spinlock-protected
free list, intended only to support early libc/PAL tests across Linux, Windows,
and macOS.

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

## Header ABI And Bits Tranche

The first sysroot/header ABI tranche introduces `include/bits/crt_types.h` as a
small shared public ABI layer. The goal is not to clone Bionic's full `bits/`
tree yet, but to centralize types that appear in multiple public headers and
must remain stable across Linux, Windows, and macOS.

The fixed public type policy is Linux/Bionic-shaped where portability pressure
is highest: `off_t` and `time_t` are signed 64-bit types, device/inode/link
counts are 64-bit, `ssize_t` is pointer-sized, `pid_t` and `socklen_t` are
32-bit, and network address family/port/address types keep their POSIX-sized
forms. Windows LLP64 is absorbed here: Windows `long` can remain 32-bit, but
public runtime types must not shrink just because the host C data model differs.

`tests/header_abi_test.c` includes the public header set together and verifies
core type sizes, selected structure fields, and include-order compatibility.
`tests/data_model_test.c` continues to check wider target data-model choices,
including the target-native `long double` policy.

## Time Tranche

The current time tranche adds:

- `time_t`
- `clock_t`
- `clockid_t`
- `struct timespec`
- `struct timeval`
- `struct tm`
- `clock`
- `time`
- `clock_gettime`
- `gettimeofday`
- `nanosleep`
- `timespec_get`
- `gmtime`
- `gmtime_r`
- `localtime`
- `localtime_r`
- `asctime`
- `asctime_r`
- `ctime`
- `ctime_r`
- `mktime`
- `strftime`

Linux uses direct syscall wrappers for `gettimeofday` and sleep. Windows maps
wall-clock time to `GetSystemTimeAsFileTime` and sleep to `Sleep`. macOS maps
wall-clock time through Mach's calendar clock service and uses a
`poll(NULL, 0, timeout_ms)` based bootstrap implementation for `nanosleep`.

`CLOCK_REALTIME` is backed by wall-clock time on all hosts. `CLOCK_MONOTONIC`
uses a real monotonic backend: Linux `clock_gettime`, macOS Mach absolute time,
and Windows QPC.

The calendar conversion layer is still intentionally small. `gmtime_r`,
`ctime_r`, `asctime_r`, `mktime`, and the static-buffer variants are implemented
with a project-owned Gregorian UTC conversion. `localtime` and `localtime_r`
currently follow the same UTC conversion because timezone database loading,
`TZ`, daylight-saving rules, and locale-aware formatting are not implemented
yet. `strftime` supports the common bootstrap specifiers used by configure and
early library tests: `%Y`, `%m`, `%d`, `%H`, `%M`, `%S`, `%a`, `%A`, `%b`, `%h`,
`%B`, `%F`, `%T`, and `%%`.

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

The spinlock still uses `sched_yield` while waiting, so it depends on the
scheduler primitive tranche. The once-state helper now sleeps through the
private wait/futex primitive while another thread runs the initializer, then
wakes all waiters when initialization completes. Wider atomics and public C11
atomics are deferred.

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
attributes, recursive/error-checking mutexes, and key destructors have follow-up
tranches. Cancellation and robust mutexes are still deferred.

The exposed pthread type layout is still provisional and may change before ABI
stabilization.

## Pthread TLS Key Tranche

The pthread TLS key tranche extends the provisional pthread subset with:

- `pthread_key_t`
- `pthread_key_create`
- `pthread_key_delete`
- `pthread_getspecific`
- `pthread_setspecific`

This tranche covers key allocation and per-thread value storage. Destructor
registration is stored in the project key table and destructors are run when a
project-created thread returns from its start routine or calls `pthread_exit`.
The runtime follows the usual bounded repeated destructor pass model so a
destructor may set a non-null value again for a later pass.

pthread key values are stored through the private `crt_tls` adapter. Linux uses
the project-owned current-thread registry behind that adapter for now, macOS can
use compiler TLS behind libSystem-created native thread state, and Windows maps
through Win32 TLS slots. This keeps Windows free of compiler-emitted
`_tls_index` dependencies while preserving the pthread API shape used by
portable libraries.

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

The current project-owned implementation is still intentionally narrow. Detach
state, stack size, guard size, and caller-supplied stack addresses are represented
in `pthread_attr_t`. The attribute object accepts caller-supplied stacks on every
host, matching Bionic's attr-object behavior. Linux applies project-owned stacks
directly through `clone`, and macOS forwards stack attributes to libSystem when
native pthread attr entry points are available. Windows preserves the stack
address in the attr object but returns `ENOTSUP` from `pthread_create` if that
caller-owned stack must be applied, because `CreateThread` has no API for using
an arbitrary caller-provided stack. Priority, scheduling backend application,
cancellation, and robust synchronization are deferred.

Windows uses `CreateThread`, `WaitForSingleObject`, `CloseHandle`, and
`ExitThread` from `kernel32`. Linux uses a raw `clone` wrapper with
`CLONE_THREAD`, parent/child tid setup, child tid clearing, a project-owned
stack, futex-backed join on the tid word, and a permanent reaper thread for
detached project-owned stack/control reclamation. See
`docs/linux_pthread_lifecycle.md`.

macOS keeps the same public pthread ABI as Linux and Windows. Basic mutex,
once, self, and key APIs are provided by the project implementation. Thread
creation and join are implemented through an adaptation layer that resolves
libSystem's native `pthread_create`, `pthread_join`, and `pthread_exit` with
`dlsym(RTLD_NEXT, ...)`, stores the native Darwin thread handle inside the
project control block, and exposes only the project-owned `pthread_t` value to
callers.

## Pthread Attribute Tranche

The pthread attribute tranche adds the first `pthread_attr_t` API subset:

- `pthread_attr_init`
- `pthread_attr_destroy`
- `pthread_attr_getdetachstate`
- `pthread_attr_setdetachstate`
- `pthread_attr_getstacksize`
- `pthread_attr_setstacksize`
- `pthread_attr_getguardsize`
- `pthread_attr_setguardsize`
- `pthread_attr_getstack`
- `pthread_attr_setstack`
- `PTHREAD_STACK_MIN`

The runtime now consumes detach state and stack size during `pthread_create`.
Caller-supplied stacks are stored by `pthread_attr_setstack` on all hosts.
Linux applies them directly, macOS attempts to pass them through native
libSystem pthread attributes, and Windows reports `ENOTSUP` from
`pthread_create` when such a stack must be applied. Joinable remains the default.
Detached creation is accepted and releases the project control block when the
worker returns or calls `pthread_exit`; Linux performs that release from its
detached reaper once the kernel clears the child tid. Explicit `pthread_detach`
is implemented. Scheduling and priority attributes are stored, while host
backend scheduler application is deferred.

## Pthread Detach Tranche

The pthread detach tranche adds:

- `pthread_detach`

Windows detaches by closing the retained thread handle. macOS detaches the
hidden native libSystem pthread handle through the adaptation layer. Linux marks
the project control block as detached and queues it to the detached reaper when
the worker exits.

Calling `pthread_join` on a detached project thread returns `EINVAL`.

## Pthread Mutex Attribute Tranche

The pthread mutex attribute tranche adds:

- `pthread_mutex_trylock`
- `pthread_mutexattr_t`
- `pthread_mutexattr_init`
- `pthread_mutexattr_destroy`
- `pthread_mutexattr_gettype`
- `pthread_mutexattr_settype`
- `pthread_mutexattr_getpshared`
- `pthread_mutexattr_setpshared`

The runtime accepts `PTHREAD_MUTEX_NORMAL`, `PTHREAD_MUTEX_RECURSIVE`, and
`PTHREAD_MUTEX_ERRORCHECK`. Mutexes keep lock state, type, recursion count, and
owner identity inside the Bionic-shaped `pthread_mutex_t.__private[]` storage.
Contended mutexes now sleep through the private wait/futex primitive instead of
spinning until release. Error-checking mutexes return `EDEADLK` on self-lock and
`EPERM` on unlock by a non-owner.

The process-sharing attribute surface is exposed. `PTHREAD_PROCESS_PRIVATE` is
supported; `PTHREAD_PROCESS_SHARED` returns `ENOTSUP` until shared-memory
synchronization policy is defined.

## Pthread Read/Write Lock Tranche

The pthread read/write lock tranche adds:

- `pthread_rwlock_t`
- `pthread_rwlockattr_t`
- `PTHREAD_RWLOCK_INITIALIZER`
- `pthread_rwlock_init`
- `pthread_rwlock_destroy`
- `pthread_rwlock_rdlock`
- `pthread_rwlock_tryrdlock`
- `pthread_rwlock_wrlock`
- `pthread_rwlock_trywrlock`
- `pthread_rwlock_unlock`
- `pthread_rwlockattr_init`
- `pthread_rwlockattr_destroy`
- `pthread_rwlockattr_getpshared`
- `pthread_rwlockattr_setpshared`

The current implementation stores a compact reader-count/writer-state word in
the Bionic-shaped `pthread_rwlock_t.__private[]` storage. Contended readers and
writers sleep through the private wait/futex primitive and wake all waiters when
the lock transitions back to the unlocked state. The process-sharing attribute
surface is exposed: `PTHREAD_PROCESS_PRIVATE` is supported and
`PTHREAD_PROCESS_SHARED` returns `ENOTSUP`. Writer preference is deferred.

## Pthread Spin Lock Tranche

The pthread spin lock tranche adds:

- `pthread_spinlock_t`
- `PTHREAD_PROCESS_PRIVATE`
- `PTHREAD_PROCESS_SHARED`
- `pthread_spin_init`
- `pthread_spin_destroy`
- `pthread_spin_lock`
- `pthread_spin_trylock`
- `pthread_spin_unlock`

The public spin lock is represented as a single `int`, matching the compact
POSIX/Bionic-style surface. It uses compiler atomics and `sched_yield` while
spinning. `PTHREAD_PROCESS_PRIVATE` is supported; `PTHREAD_PROCESS_SHARED`
returns `ENOTSUP` until process-shared synchronization and shared-memory ABI
policy are defined.

## Pthread Condition Variable Tranche

The pthread condition variable tranche adds:

- `pthread_cond_t`
- `pthread_condattr_t`
- `PTHREAD_COND_INITIALIZER`
- `pthread_cond_init`
- `pthread_cond_destroy`
- `pthread_cond_signal`
- `pthread_cond_broadcast`
- `pthread_cond_wait`
- `pthread_cond_timedwait`
- `pthread_condattr_init`
- `pthread_condattr_destroy`
- `pthread_condattr_getclock`
- `pthread_condattr_setclock`

The condition variable uses a sequence counter in the Bionic-shaped
`pthread_cond_t.__private[]` storage. Waiting threads unlock the supplied mutex,
wait on the sequence address through the private wait/futex primitive, then lock
the mutex again. This preserves the public API shape and basic predicate-loop
usage while keeping the OS wait backend private. `pthread_cond_timedwait` uses
absolute `CLOCK_REALTIME` timeouts for now. The condition clock attribute surface
is exposed in `pthread_condattr_t`, but condition objects do not yet store a
selected clock internally.

## Private Wait/Futex Tranche

The private wait/futex tranche adds a small internal wait-address primitive:

- `__crt_wait32`
- `__crt_wait32_timed`
- `__crt_wake32_one`
- `__crt_wake32_all`

Linux maps this to the raw `futex` syscall with private wait/wake operations.
Windows maps it to `WaitOnAddress`, `WakeByAddressSingle`, and
`WakeByAddressAll`. macOS maps it to libSystem's public wait-by-address API:
`os_sync_wait_on_address`, `os_sync_wait_on_address_with_timeout`,
`os_sync_wake_by_address_any`, and `os_sync_wake_by_address_all`.

Timed waits use Linux futex relative timeouts, Windows `WaitOnAddress`
millisecond timeouts, and macOS `os_sync_wait_on_address_with_timeout`
nanosecond timeouts. Because the public project errno values follow the
Bionic/Linux numbering, the macOS backend translates Darwin's timeout errno to
the project `ETIMEDOUT` value before returning to libc code.

`pthread_once`, `pthread_mutex_lock`, `pthread_mutex_unlock`,
`pthread_rwlock_rdlock`, `pthread_rwlock_wrlock`, `pthread_rwlock_unlock`,
`pthread_cond_signal`, `pthread_cond_broadcast`, `pthread_cond_wait`, and
`pthread_cond_timedwait` now use this private primitive instead of pure
spin/yield polling.

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
- `feof`
- `ferror`
- `clearerr`
- `remove`
- `rename`
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

## Formatting, Stdio, File/Path, and Libc Surface Tranche

The formatter tranche expands `vsnprintf`, `snprintf`, `printf`, and `fprintf`
with enough surface for more configure-style probes and basic library tests:

- field width;
- precision for strings and integer zero padding;
- `-`, `0`, `+`, space, and `#` flags;
- `%o`;
- `%zu` and `%zd`.

The formatter is still integer/string-only. Floating-point formatting, dynamic
`*` width/precision, positional arguments, locale grouping, and the wider
`printf` family remain deferred.

The stdio surface now also exposes:

- `getc`, `fgetc`, and `getchar`;
- `putc`;
- `ungetc`;
- `setbuf` and `setvbuf`;
- `fileno`;
- `fdopen`;
- `freopen`;
- `fgets`;
- `fseeko`;
- `ftello`;
- `fgetpos`;
- `fsetpos`;
- `rewind`;
- `perror`;
- `tmpfile`.

The stdio implementation now has a small real buffering engine. `_IONBF` keeps
direct I/O, `_IOFBF` buffers reads and writes until the buffer fills or
`fflush`/`fclose`/`fseek` forces a flush, and `_IOLBF` flushes writes on
newline. `setvbuf` can attach a caller-provided buffer, while streams without a
provided buffer allocate a bootstrap `BUFSIZ` buffer lazily. The implementation
still keeps a private bootstrap `FILE` layout and intentionally does not expose
or freeze a Bionic-compatible `FILE` ABI yet.

The startup objects now route a returned `main` status through `exit`, and
`exit` runs a small LIFO `atexit` handler stack and flushes the bootstrap
standard streams before calling the host exit adapter. This keeps newly buffered
stdout/stderr behavior usable while the broader process lifecycle surface is
still being built out.

`fdopen` currently creates an owned `FILE` wrapper around an existing project fd
and does not yet validate that the requested mode is compatible with the fd's
original access mode. `freopen` flushes the existing stream, opens the new path,
then replaces the stream fd while preserving the caller's `FILE*` identity.
`tmpfile` is a bootstrap implementation: it creates a private counter-based
temporary name with `O_CREAT | O_EXCL`, unlinks it immediately after a successful
open where the host permits that behavior, and keeps the resulting stream open.
It is collision-resistant enough for early tests, but it is not yet a final
secure temporary-file policy.

The file/path tranche adds:

- `access`;
- `mkdir`;
- `rmdir`;
- `getcwd`;
- `chdir`;
- `dup`;
- `dup2`;
- `pipe`;
- `isatty`;
- `poll`;
- `select`;
- bootstrap `fcntl`;
- `realpath`;
- `readlink`;
- `symlink`;
- `opendir`;
- `readdir`;
- `closedir`;
- `dirent.h`;
- `stat`;
- `fstat`;
- `lstat`;
- `sys/stat.h`.

Linux uses raw syscalls. Windows maps the subset to Kernel32 APIs and the
project fd table; `lstat` follows a bootstrap policy that reports reparse
points as `S_IFLNK` while ordinary files and directories retain their `stat`
mode. macOS uses direct syscalls where practical. Its `getcwd`
adapter opens `"."` and calls Darwin `fcntl(F_GETPATH)` through a private
syscall wrapper, avoiding a libc-level dependency on libSystem's `getcwd`.
The current `fcntl` subset supports `F_DUPFD`, `F_GETFD`, `F_SETFD`, `F_GETFL`,
and `F_SETFL` as bootstrap probes; close-on-exec and nonblocking behavior are
recorded only as accepted surface for now because `exec` and full descriptor
status flag tracking are not implemented yet.
The first readiness tranche adds `poll.h` and `sys/select.h`. Linux routes
`poll` through `ppoll` so x86_64 and AArch64 share the same public C code.
macOS uses the host `poll` syscall directly. Windows maps readiness over the
project fd table: regular files and console-like handles are treated as ready,
pipe reads use `PeekNamedPipe`, and writes are treated as ready for valid
handles. `select` is implemented over `poll`, so the policy remains centralized.
This is enough for common configure probes and pipe readiness tests, but it is
not yet a full socket/network event backend.
`realpath` currently validates that the path exists, returns an absolute
normalized path, and supports the common `resolved_path == NULL` allocation
extension; it does not yet walk and expand every symlink component. Linux and
macOS implement `readlink` and `symlink` through host syscalls. Windows returns
`ENOSYS` for `readlink`/`symlink` until the project defines a reparse-point
policy that is robust without requiring Developer Mode or elevated privileges.
Directory iteration is exposed through a bootstrap `dirent.h` and
`opendir`/`readdir`/`closedir`. Linux uses `getdents64`, macOS uses
`getdirentries64`, and Windows maps to `FindFirstFileA`/`FindNextFileA`.

The first socket/network tranche adds:

- `sys/socket.h`;
- `netinet/in.h`;
- `arpa/inet.h`;
- `netdb.h`;
- IPv4 `sockaddr_in`, `in_addr`, and byte-order helpers;
- `inet_pton`, `inet_ntop`, and `inet_addr` for IPv4 numeric addresses;
- numeric IPv4 `getaddrinfo`, `freeaddrinfo`, and `gai_strerror`;
- `socket`, `bind`, `listen`, `accept`, `connect`;
- `send`, `recv`, `sendto`, `recvfrom`;
- `getsockname`, `setsockopt`, and `shutdown`.

Linux uses direct socket syscalls for x86_64 and AArch64. macOS keeps the
public Linux/Bionic-shaped `sockaddr_in` ABI and translates to Darwin's
`sin_len` sockaddr layout inside the socket adapter before entering the kernel.
Windows maps project fds to Winsock sockets, initializes Winsock lazily by
loading `ws2_32.dll` at runtime, and routes socket close/read/write/poll
operations through the socket-aware fd table. The build intentionally avoids
linking `ws2_32.lib`, because this libc exports POSIX socket names such as
`socket`, `send`, and `recv`; importing the same names from Winsock would create
duplicate symbols. The current network resolver is intentionally numeric only;
DNS lookup, IPv6, nonblocking connect readiness, socketpair, getsockopt, and
full socket `poll` semantics remain later tranches.

## Libdl Bootstrap Tranche

The first `libdl` tranche adds:

- `dlfcn.h`;
- `dlopen`;
- `dlsym`;
- `dlclose`;
- `dlerror`;
- `RTLD_LAZY`, `RTLD_NOW`, `RTLD_LOCAL`, `RTLD_GLOBAL`;
- `RTLD_DEFAULT` and `RTLD_NEXT`.

This is a project-owned host adapter, not a direct Bionic source import.
Bionic's real `libdl` is a frontend to Android's dynamic linker and brings
Android linker namespaces, ELF loader state, soname/search policy, relocation,
dependency loading, and Android-specific extension APIs. Those belong to the
future `linker/` tranche rather than this first host-adapter step.

Windows maps `dlopen`/`dlsym`/`dlclose` to
`LoadLibraryA`/`GetProcAddress`/`FreeLibrary`. macOS maps to dyld image APIs and
performs Mach-O symbol lookup with the required leading underscore. Linux is
currently explicit bootstrap behavior only: `dlopen(NULL)` returns a private
main-program handle, but real object loading and symbol lookup report a
`dlerror` diagnostic. The Linux limitation is intentional because this project
links CRT executables with `-nostdlib`, owns its public libc ABI boundary, and
should not silently mix host glibc/libdl state into the freestanding runtime.

The detailed policy is documented in `docs/dynamic_loading.md`.

## C++ Runtime Bootstrap Tranche

The first C++ runtime tranche adds a project-owned ABI support archive installed
as `libc++.a` from the `libstdc++/` source directory. The directory name follows
the historical Bionic layout for small C++ support, but the project policy is
LLVM libc++/libc++abi/libunwind rather than GNU libstdc++.

The first symbol surface is:

- `__cxa_guard_acquire`;
- `__cxa_guard_release`;
- `__cxa_guard_abort`;
- `__cxa_atexit`;
- `__cxa_finalize`;
- `__cxa_pure_virtual`;
- `__cxa_deleted_virtual`;
- `__dso_handle`.

This is not a direct Bionic source import. It is a small Itanium C++ ABI shaped
bootstrap implementation so the project can test guard variables and destructor
registration before importing libc++abi. `exit()` weakly calls
`__cxa_finalize(NULL)` when the C++ runtime archive is linked, so plain C
programs do not gain a hard dependency on C++ runtime symbols.

Exceptions, RTTI, `__gxx_personality_v0`, full unwind behavior, demangling,
operator new/delete, libc++abi, libunwind, and libc++ are explicitly deferred.
Windows needs a separate C++ frontend ABI decision because the current Clang
MSVC target normally emits MSVC C++ ABI hooks rather than Bionic/Itanium
`__cxa_*` hooks.

The detailed policy is documented in `docs/cxx_runtime.md`.

The metadata tranche moves the supported OS backends beyond the first
regular-file fallback. Linux uses the raw `statx` syscall and converts the
kernel `statx` record into the project `struct stat`. Windows uses
`GetFileInformationByHandle` through either an fd-table handle or a temporary
metadata handle opened with `FILE_FLAG_BACKUP_SEMANTICS`, then fills file type,
size, link count, file id, device id, block count, and timestamps. macOS uses
the Darwin `stat64`, `fstat64`, and `lstat64` syscalls with a private
Darwin-layout metadata record, then converts that record into the project
`struct stat` without exposing Darwin's public `struct stat` ABI.

`lstat` currently differs from `stat` on Linux and macOS. Linux passes
`AT_SYMLINK_NOFOLLOW` to `statx`, and macOS calls `lstat64`.

Windows `access` is still intentionally small, but it now distinguishes writable
regular files from read-only regular files using `FILE_ATTRIBUTE_READONLY` for
`W_OK` checks. `R_OK` and `X_OK` remain existence-oriented bootstrap checks.

The string/stdlib tranche adds:

- `strtok_r`;
- `strerror`;
- POSIX-style `strerror_r`;
- `strtoll`;
- `strtoull`;
- `qsort`;
- `bsearch`;
- `getenv`;
- `setenv`;
- `unsetenv`.

The environment store is process-local and runtime-owned. Startup captures the
initial host environment pointer before `main`, and the first environment API
call copies it into the runtime store: Linux and macOS retain the startup
`envp`, while Windows lazily imports the process environment block with
`GetEnvironmentStringsA`. `getenv`, `setenv`, and `unsetenv` operate on the
copied runtime store and expose it through `environ`; they still do not
synchronize later changes back to host-specific environment APIs.

The process/signal tranche adds:

- `getpid`;
- `getppid`;
- `kill`;
- `signal`;
- `raise`;
- `abort`;
- `atexit`;
- `signal.h`.

This is still a bootstrap signal model. `signal` and `raise` support in-process
handlers and ignored signals, `kill(pid, 0)` can probe the current process, and
Linux/macOS forward other `kill` calls to the host syscall. Windows currently
supports the current-process probe and returns `ENOSYS` for broader host signal
delivery. Full asynchronous signal delivery, signal masks, `sigaction`, and
process-group semantics remain deferred.

Windows builds now include a small project-owned `__chkstk` helper in `libc.a`
for x86_64 and aarch64. Clang may emit this symbol for functions with larger
stack frames when building with the MSVC ABI, and the freestanding link cannot
depend on the MSVC runtime to provide it. The x86_64 helper performs page
probing while preserving the allocation size register. The aarch64 helper
follows the Windows ARM64 stack-probe convention used by LLVM compiler-rt:
`x15` carries the allocation size in 16-byte units, the helper probes pages
below `sp`, clobbers only scratch registers `x16`/`x17`, and leaves `sp`
unchanged for the caller's prologue to adjust.
