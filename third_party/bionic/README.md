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

## Main-First And Legacy Exceptions

The project baseline is Bionic `main`. Legacy refs are used only as curated
exceptions when current Bionic has no small portable C source for the requested
function, when current code is tied to Android-private runtime machinery, or
when current code would force a source-family switch that conflicts with the
tranche policy.

Current legacy exceptions are:

- `ics-mr0` for early string/memory and numeric-conversion bootstrap sources.
  These files are small BSD-style C implementations and were easier to adapt to
  the cross-OS C99/freestanding PAL than current architecture-tuned Bionic
  variants.
- Bionic `884e4f8` for selected fdlibm sources such as `exp`, `log`, `pow`,
  native `expf`/`logf`/`powf`, and `scalbn*`. Current Bionic `main` either no
  longer lists those exact portable sources or, for `scalbn*`, carries a
  musl-derived implementation. The project keeps these early libm imports on an
  fdlibm-oriented path until a native Bionic-main import map is explicit.
- FreeBSD upstream msun for the narrow `log2`/`log2f` exception. The observed
  Bionic source trees used by this tranche do not expose direct `e_log2.c` and
  `e_log2f.c` files, so the project records the source-family exception
  explicitly instead of pretending it came through Bionic.

Every row below is authoritative for local provenance. If a legacy source is
replaced by a current-main source later, update the row rather than leaving both
as implicit alternatives.

The machine-readable manifest is `third_party/bionic/import_manifest.json`, with
schema `third_party/bionic/import_manifest.schema.json`. The manifest adds a
`review_class` and `next_action` for every tracked source so legacy imports can
be audited mechanically. The current legacy decision summary is in
`third_party/bionic/legacy_import_review.md`.

Use:

```sh
python3 tools/check_import_manifest.py
```

or the CMake target `check-import-manifest` when Python is available.

## Imported Files

### String/Memory Tranche 1

The first runtime import uses portable C string/memory implementations from the
Android Bionic `ics-mr0` tree as a legacy bootstrap exception.

| Local file | Upstream path | Upstream ref | Status | Notes |
| --- | --- | --- | --- | --- |
| `libc/src/string/bcopy.c` | `libc/string/bcopy.c` | `ics-mr0` | adapted | Uses `uintptr_t` instead of `long` for pointer-width arithmetic across Windows LLP64. |
| `libc/src/string/memcpy.c` | `libc/string/memcpy.c` | `ics-mr0` | adapted | Keeps Bionic wrapper shape; includes local `bcopy.c`. |
| `libc/src/string/memmove.c` | `libc/string/bcopy.c` | `ics-mr0` | adapted | Generated from the same Bionic `bcopy.c` implementation using `MEMMOVE`. |
| `libc/src/string/memset.c` | `libc/string/memset.c` | `ics-mr0` | adapted | Formatting only; license header preserved. |
| `libc/src/string/strlen.c` | `libc/string/strlen.c` | `ics-mr0` | adapted | Removes kernel/standalone include branch for this hosted public header environment. |
| `libc/src/string/strcmp.c` | `libc/string/strcmp.c` | `ics-mr0` | adapted | Removes kernel/standalone include branch for this hosted public header environment. |
| `libc/src/string/memchr.c` | `libc/string/memchr.c` | `ics-mr0` | adapted | Formatting only; license header preserved. |
| `libc/src/string/memcmp.c` | `libc/string/memcmp.c` | `ics-mr0` | adapted | Formatting only; license header preserved. |
| `libc/src/string/strcat.c` | `libc/string/strcat.c` | `ics-mr0` | adapted | Removes Android API warning hook. |
| `libc/src/string/strchr.c` | `libc/string/index.c` | `ics-mr0` | adapted | Keeps Bionic/OpenBSD implementation under the standard `strchr` name. |
| `libc/src/string/strcpy.c` | `libc/string/strcpy.c` | `ics-mr0` | adapted | Formatting only; license header preserved. |
| `libc/src/string/strncmp.c` | `libc/string/strncmp.c` | `ics-mr0` | adapted | Removes kernel/standalone include branch for this hosted public header environment. |
| `libc/src/string/strncpy.c` | `libc/string/strncpy.c` | `ics-mr0` | adapted | Removes kernel/standalone include branch for this hosted public header environment. |
| `libc/src/string/strrchr.c` | `libc/string/rindex.c` | `ics-mr0` | adapted | Keeps Bionic/OpenBSD implementation under the standard `strrchr` name. |
| `libc/src/string/strcspn.c` | `libc/string/strcspn.c` | `ics-mr0` | adapted | Formatting only; license header preserved. |
| `libc/src/string/strdup.c` | `libc/string/strdup.c` | `ics-mr0` | adapted | Removes unused `sys/types.h` dependency for this project. |
| `libc/src/string/strndup.c` | none | project-owned | new | Allocator-backed bounded duplicate helper using local `strnlen`. |
| `libc/src/string/strnlen.c` | none | project-owned | new | Bounded length helper using local `memchr`. |
| `libc/src/string/strpbrk.c` | `libc/string/strpbrk.c` | `ics-mr0` | adapted | Formatting only; license header preserved. |
| `libc/src/string/strspn.c` | `libc/string/strspn.c` | `ics-mr0` | adapted | Formatting only; license header preserved. |
| `libc/src/string/strstr.c` | `libc/string/strstr.c` | `ics-mr0` | adapted | Formatting only; license header preserved. |

### Ctype Tranche

The ctype tranche adapts Bionic's ASCII/C-locale inline logic into project-owned
out-of-line C99 functions.

| Local file | Upstream path | Upstream ref | Status | Notes |
| --- | --- | --- | --- | --- |
| `libc/src/ctype.c` | `libc/include/ctype.h` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | adapted | Converts Bionic inline predicates to exported C99 functions and defers locale-aware `_l` variants. |
| `include/ctype.h` | `libc/include/ctype.h` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | adapted | Keeps only public prototypes for the current C-locale tranche. |

### Locale Tranche

| Local file | Upstream path | Upstream ref | Status | Notes |
| --- | --- | --- | --- | --- |
| `include/locale.h` | `libc/include/locale.h` | project-owned | new | Minimal C/POSIX locale category declarations and `struct lconv` for the bootstrap C locale. |
| `libc/src/locale.c` | mixed Bionic/POSIX surface | project-owned | new | Supports the `C`/`POSIX` locale names, empty locale requests, `setlocale`, and `localeconv` without importing host locale state. |

### Stdlib Numeric Conversion Tranche

| Local file | Upstream path | Upstream ref | Status | Notes |
| --- | --- | --- | --- | --- |
| `libc/src/atoi.c` | `libc/stdlib/atoi.c` | `ics-mr0` | adapted | Uses local `strtol` instead of Bionic's `strtoimax` dependency. |
| `libc/src/atol.c` | `libc/stdlib/atol.c` | `ics-mr0` | adapted | Uses local `strtol` instead of Bionic's `strtoimax` dependency. |
| `libc/src/strtol.c` | `libc/stdlib/strtol.c` | `ics-mr0` | adapted | Keeps BSD-derived conversion logic; adds invalid-base handling for this project. |
| `libc/src/strtoul.c` | `libc/stdlib/strtoul.c` | `ics-mr0` | adapted | Keeps BSD-derived conversion logic; adds invalid-base handling for this project. |
| `include/limits.h` | `libc/include/limits.h` | project-owned | new | Minimal C99 limit macros needed by numeric conversion tests. |
| `include/stdlib.h` | `libc/include/stdlib.h` | project-owned | adapted | Adds prototypes for the current allocator and numeric conversion tranche. |

### Gdtoa Decimal Conversion Tranche

This tranche imports the Bionic/OpenBSD gdtoa decimal-to-binary conversion path
from Bionic `main` at `731631f300090436d7f5df80d50b6275c8c60a93`. It replaces
the project-owned bootstrap `strtod`/`strtof` parser with the same gdtoa source
family Bionic uses for accurate floating-point input conversion.

The current compile set includes input conversion plus gdtoa output conversion:
`__dtoa` for double decimal printf output, `__ldtoa` for long-double decimal
printf output, and `__hdtoa`/`__hldtoa` for hexadecimal `%a`/`%A` output. The
local `machine/ieee.h` compatibility header supplies the IEEE layout structures
expected by OpenBSD gdtoa while keeping host SDK headers out of the CRT build.

`strtold` follows the project long-double ABI policy: if the compiler target has
128-bit IEEE quad long double (`LDBL_MANT_DIG == 113`), it dispatches through
Bionic's `__strtorQ`; otherwise it remains a `strtod` wrapper until a native x87
80-bit source policy is selected.

| Local file | Upstream path | Upstream ref | Status | Notes |
| --- | --- | --- | --- | --- |
| `libc/src/gdtoa/arith.h` | `libc/upstream-openbsd/android/include/arith.h` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | adapted | Bionic Android gdtoa target configuration for IEEE little-endian 64-bit targets. |
| `libc/src/gdtoa/gd_qnan.h` | `libc/upstream-openbsd/android/include/gd_qnan.h` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | imported | Quiet-NaN word definitions used by gdtoa NaN parsing. |
| `libc/src/gdtoa/openbsd-compat.h` | `libc/upstream-openbsd/android/include/openbsd-compat.h` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | adapted | Trimmed to this project's freestanding header set while preserving gdtoa visibility and alias macros. |
| `libc/src/gdtoa/dmisc.c` | `libc/upstream-openbsd/lib/libc/gdtoa/dmisc.c` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | imported/compiled | `__freedtoa` support for the active gdtoa-backed printf decimal output path. |
| `libc/src/gdtoa/dtoa.c` | `libc/upstream-openbsd/lib/libc/gdtoa/dtoa.c` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | imported/compiled | Active `__dtoa` decimal output source used by printf `%f`/`%e`/`%g`. |
| `libc/src/gdtoa/gdtoa.c` | `libc/upstream-openbsd/lib/libc/gdtoa/gdtoa.c` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | imported/compiled | General binary-to-decimal conversion engine used by `__ldtoa` for long-double printf output. |
| `libc/src/gdtoa/gdtoa.h` | `libc/upstream-openbsd/lib/libc/gdtoa/gdtoa.h` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | imported | Public/internal gdtoa declarations used by the imported conversion files. |
| `libc/src/gdtoa/gdtoa_fltrnds.h` | `libc/upstream-openbsd/lib/libc/gdtoa/gdtoa_fltrnds.h` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | imported | Rounding-mode helper included by `strtof.c`. |
| `libc/src/gdtoa/gdtoaimp.h` | `libc/upstream-openbsd/lib/libc/gdtoa/gdtoaimp.h` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | adapted | Built with local OpenBSD compatibility and thread-private lock adapter. |
| `libc/src/gdtoa/gethex.c` | `libc/upstream-openbsd/lib/libc/gdtoa/gethex.c` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | imported | Hexadecimal floating input conversion helper. |
| `libc/src/gdtoa/gmisc.c` | `libc/upstream-openbsd/lib/libc/gdtoa/gmisc.c` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | imported | Generic gdtoa bit-copy helpers. |
| `libc/src/gdtoa/hd_init.c` | `libc/upstream-openbsd/lib/libc/gdtoa/hd_init.c` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | imported | Hex digit table initialization. |
| `libc/src/gdtoa/hdtoa.c` | `libc/upstream-openbsd/lib/libc/gdtoa/hdtoa.c` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | imported/compiled | Hex output-conversion source used by printf `%a`/`%A`. |
| `libc/src/gdtoa/hexnan.c` | `libc/upstream-openbsd/lib/libc/gdtoa/hexnan.c` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | imported | NaN payload parsing helper. |
| `libc/src/gdtoa/ldtoa.c` | `libc/upstream-openbsd/lib/libc/gdtoa/ldtoa.c` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | imported/compiled | Long-double output-conversion source used by printf `L` modifier decimal conversions. |
| `libc/src/gdtoa/misc.c` | `libc/upstream-openbsd/lib/libc/gdtoa/misc.c` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | imported | Bigint allocator, freelists, and cached power helpers. |
| `libc/src/gdtoa/smisc.c` | `libc/upstream-openbsd/lib/libc/gdtoa/smisc.c` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | imported | Shared bigint/string helpers. |
| `libc/src/gdtoa/strtod.c` | `libc/upstream-openbsd/lib/libc/gdtoa/strtod.c` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | imported | Public `strtod`. |
| `libc/src/gdtoa/strtodg.c` | `libc/upstream-openbsd/lib/libc/gdtoa/strtodg.c` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | imported | General decimal-to-binary conversion engine. |
| `libc/src/gdtoa/strtof.c` | `libc/upstream-openbsd/lib/libc/gdtoa/strtof.c` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | imported | Public `strtof`. |
| `libc/src/gdtoa/strtord.c` | `libc/upstream-openbsd/lib/libc/gdtoa/strtord.c` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | imported | Internal double rounding helper. |
| `libc/src/gdtoa/strtorQ.c` | `libc/upstream-openbsd/lib/libc/gdtoa/strtorQ.c` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | adapted | IEEE quad parser compiled and used by `strtold` only on `LDBL_MANT_DIG == 113` targets; double-sized Windows/macOS long double targets skip this body. |
| `libc/src/gdtoa/sum.c` | `libc/upstream-openbsd/lib/libc/gdtoa/sum.c` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | imported | Bigint summation helper. |
| `libc/src/gdtoa/ulp.c` | `libc/upstream-openbsd/lib/libc/gdtoa/ulp.c` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | imported | ULP helper used during rounding. |
| `libc/src/gdtoa/thread_private.h` | none | project-owned | new | Adapter from OpenBSD gdtoa lock hooks to local pthread mutexes. |
| `libc/src/gdtoa/gdtoa_support.c` | `libc/upstream-openbsd/android/gdtoa_support.cpp` shape | project-owned | new | C99 slot-specific lock implementation for gdtoa `MULTIPLE_THREADS`. |
| `libc/src/gdtoa/machine/ieee.h` | OpenBSD/BSD `<machine/ieee.h>` layout contract | project-owned | new | Little-endian IEEE layout adapter for double, x87 80-bit long double, and IEEE quad long double. |
| `libc/src/atof.c` | `libc/bionic/atof.cpp` and `libc/bionic/strtold.cpp` policy shape | project-owned | adapted | Keeps `atof` wrapper and target-policy `strtold` dispatch while `strtod`/`strtof` come from gdtoa. |

### Stdio Scanf Snapshot

Bionic main's scanner sources are preserved under `third_party/bionic/stdio/`
as reference snapshots. They are not compiled directly yet because Bionic's
current stdio scanner is C++ and uses private `FILE` buffer/refill internals.
The active C99 implementation in `libc/src/scanf.c` is an adapter that ports
the Bionic/BSD state machine onto this project's `scan_source` abstraction.

| Local file | Upstream path | Upstream ref | Status | Notes |
| --- | --- | --- | --- | --- |
| `third_party/bionic/stdio/scanf_common.h` | `libc/stdio/scanf_common.h` | Bionic `main` | reference snapshot | Scanner flags, conversion classes, and Bionic/OpenBSD license/provenance reference. |
| `third_party/bionic/stdio/vfscanf.cpp` | `libc/stdio/vfscanf.cpp` | Bionic `main` | reference snapshot | Current Bionic byte scanner source used as the behavior model for `libc/src/scanf.c`. |
| `third_party/bionic/stdio/vfwscanf.cpp` | `libc/stdio/vfwscanf.cpp` | Bionic `main` | reference snapshot | Current Bionic wide scanner source used as the behavior model for wide scanf wrappers. |
| `third_party/bionic/stdio/local.h` | `libc/stdio/local.h` | Bionic `main` | reference snapshot | Private stdio dependency that explains why direct compilation is deferred until the local `FILE` internals converge further. |
| `libc/src/scanf.c` | `libc/stdio/vfscanf.cpp` behavior plus OpenBSD/BSD scanner algorithm | project-owned C99 adapter | adapted/project-owned | Ports Bionic scanset ranges, `%b`/`0b`, `%D`/`%O`/`%U`, `%q`, `%w`/`%wf`, `%p`, gdtoa-backed floats, wide destinations, and `%m` allocation to the CRT backend. |

### C99 Base Header Tranche

| Local file | Upstream path | Upstream ref | Status | Notes |
| --- | --- | --- | --- | --- |
| `include/stdint.h` | `libc/include/stdint.h` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | adapted | Uses compiler predefined type macros instead of Bionic's `__LP64__` branch to support Windows LLP64. |
| `include/float.h` | compiler-provided header layer | project-owned | new | Defines C floating-point limits from compiler predefined macros so imported fdlibm/msun sources do not depend on host libc headers. |
| `include/bits/crt_types.h` | none | project-owned | new | Centralizes shared public ABI scalar types for the first minimal `bits/` layer. |
| `include/stdbool.h` | `libc/include/stdbool.h` | project-owned | new | Minimal C99 boolean macro header. |
| `include/stddef.h` | compiler-provided header layer | project-owned | new | Defines `ptrdiff_t`, `size_t`, `wchar_t`, `NULL`, and `offsetof` from compiler builtins. |
| `include/stdarg.h` | compiler-provided header layer | project-owned | new | Defines `va_list` and `va_*` macros from compiler builtins. |

### Wide Character And Multibyte Tranche

| Local file | Upstream path | Upstream ref | Status | Notes |
| --- | --- | --- | --- | --- |
| `include/wchar.h` | `libc/include/wchar.h` | project-owned | new | Bionic main shaped wide-character, multibyte, wide stdio, and BSD wide string declarations using the project-forced signed 32-bit `wchar_t` ABI. |
| `include/wctype.h` | `libc/include/wctype.h` | project-owned | new | Minimal wide classification and transform declarations for the C/POSIX locale bootstrap. |
| `libc/src/wchar.c` | mixed Bionic/BSD/POSIX surface | project-owned | new | UTF-8 bootstrap conversion, per-stream stdio `mbstate_t`, wide stdio/memory stream helpers, BSD wide string helpers, numeric wrappers, and C-locale width/collation policy. |
| `libc/src/wctype.c` | mixed Bionic/POSIX surface | project-owned | new | ASCII/C-locale wide classification and case mapping over the existing ctype policy. |

### Libm Bootstrap Tranche

| Local file | Upstream path | Upstream ref | Status | Notes |
| --- | --- | --- | --- | --- |
| `include/math.h` | `libc/include/math.h` | project-owned | new | Minimal C99/POSIX math declarations and classification macros for the first `libm.a` boundary. |
| `include/fenv.h` | `libc/include/fenv.h` | project-owned | new | Minimal C99 floating-point environment declarations with project-owned `fenv_t` storage for architecture-backed state. |
| `libm/src/basic.c` | `libm/builtins.cpp` plus mixed Bionic/POSIX surface | adapted/project-owned | new | Current-Bionic-style builtin `fabs*`, `copysign*`, `fminf`, `fmaxf`, `sqrt`, and `sqrtf`, plus simple long-double min/max helpers; uses Clang elementwise sqrt to avoid recursive Debug/O0 libcalls. |
| `libm/src/fenv.c` | `libm/fenv-*.c` policy surface | project-owned | new | Architecture-backed C99 fenv over x86_64 MXCSR plus x87 control/status and AArch64 FPCR/FPSR, with a generic fallback for unsupported architectures. |
| `libm/src/long_double.c` | none | project-owned | new | Portable bootstrap long double implementation for rounding, decomposition, elementary, trigonometric, power, and remainder APIs; follows the compiler target's native long-double ABI. |

### Libm FreeBSD/msun Import Tranche 1

| Local file | Upstream path | Upstream ref | Status | Notes |
| --- | --- | --- | --- | --- |
| `libm/src/freebsd/math_private.h` | `libm/upstream-freebsd/lib/msun/src/math_private.h` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | adapted | Reduced to word extraction/insertion helpers and no-op weak reference macros for the freestanding import. |
| `libm/src/freebsd/fpmath.h` | `libm/fpmath.h` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | adapted | Keeps the double bit layout needed by `fmin`/`fmax` without depending on host endian headers. |
| `libm/src/freebsd/e_exp.c` | `libm/src/e_exp.c` | Bionic `884e4f8` | adapted | Older portable fdlibm source used because current Bionic `main` no longer lists direct double `e_exp.c`; local adaptation renames `__ieee754_exp` to public `exp` and uses local private word helpers. |
| `libm/src/freebsd/e_log.c` | `libm/src/e_log.c` | Bionic `884e4f8` | adapted | Older portable fdlibm source used because current Bionic `main` no longer lists direct double `e_log.c`; local adaptation renames `__ieee754_log` to public `log` and uses local private word helpers. |
| `libm/src/freebsd/e_pow.c` | `libm/src/e_pow.c` | Bionic `884e4f8` | adapted | Older portable fdlibm source used because current Bionic `main` no longer lists direct double `e_pow.c`; local adaptation renames `__ieee754_pow` to public `pow` and uses local private word helpers. |
| `libm/src/freebsd/e_expf.c` | `libm/src/e_expf.c` | Bionic `884e4f8` | adapted | Native float fdlibm source used because current Bionic `main` does not provide the same simple portable fdlibm source path. |
| `libm/src/freebsd/e_logf.c` | `libm/src/e_logf.c` | Bionic `884e4f8` | adapted | Native float fdlibm source used because current Bionic `main` does not provide the same simple portable fdlibm source path. |
| `libm/src/freebsd/e_powf.c` | `libm/src/e_powf.c` | Bionic `884e4f8` | adapted | Native float fdlibm source used because current Bionic `main` does not provide the same simple portable fdlibm source path. |
| `libm/src/freebsd/e_rem_pio2.c` | `libm/upstream-freebsd/lib/msun/src/e_rem_pio2.c` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | imported/adapted | Included inline by `s_sin.c`, `s_cos.c`, and `s_tan.c`; uses local private helpers and shared `k_rem_pio2.c`. |
| `libm/src/freebsd/k_cos.c` | `libm/upstream-freebsd/lib/msun/src/k_cos.c` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | imported | Upstream copyright preserved; shared cosine kernel for trigonometric wrappers. |
| `libm/src/freebsd/k_rem_pio2.c` | `libm/upstream-freebsd/lib/msun/src/k_rem_pio2.c` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | imported/adapted | Upstream copyright preserved; uses local `STRICT_ASSIGN`, `scalbn`, and `floor`. |
| `libm/src/freebsd/k_sin.c` | `libm/upstream-freebsd/lib/msun/src/k_sin.c` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | imported | Upstream copyright preserved; shared sine kernel for trigonometric wrappers. |
| `libm/src/freebsd/k_tan.c` | `libm/upstream-freebsd/lib/msun/src/k_tan.c` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | imported | Upstream copyright preserved; shared tangent kernel for trigonometric wrappers. |
| `libm/src/freebsd/s_scalbn.c` | `libm/src/s_scalbn.c` | Bionic `884e4f8` | adapted | Older fdlibm source chosen over current Bionic's musl-derived `s_scalbn.c`; provides local `ldexp` wrapper. |
| `libm/src/freebsd/s_scalbnf.c` | `libm/src/s_scalbnf.c` | Bionic `884e4f8` | adapted | Older fdlibm source chosen over current Bionic's musl-derived `s_scalbnf.c`; provides local `ldexpf` wrapper. |
| `libm/src/freebsd/s_cos.c` | `libm/upstream-freebsd/lib/msun/src/s_cos.c` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | imported/adapted | Public `cos`; includes `e_rem_pio2.c` with `INLINE_REM_PIO2`. |
| `libm/src/freebsd/s_ceil.c` | `libm/upstream-freebsd/lib/msun/src/s_ceil.c` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | adapted | Upstream copyright preserved; uses the local adapted private header. |
| `libm/src/freebsd/s_floor.c` | `libm/upstream-freebsd/lib/msun/src/s_floor.c` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | adapted | Upstream copyright preserved; uses the local adapted private header. |
| `libm/src/freebsd/s_fmax.c` | `libm/upstream-freebsd/lib/msun/src/s_fmax.c` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | adapted | Upstream copyright preserved; built with `USE_BUILTIN_FMAX`; weak long-double alias disabled for this tranche. |
| `libm/src/freebsd/s_fmin.c` | `libm/upstream-freebsd/lib/msun/src/s_fmin.c` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | adapted | Upstream copyright preserved; built with `USE_BUILTIN_FMIN`; weak long-double alias disabled for this tranche. |
| `libm/src/freebsd/s_round.c` | `libm/upstream-freebsd/lib/msun/src/s_round.c` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | adapted | Upstream copyright preserved; uses the local adapted private header. |
| `libm/src/freebsd/s_sin.c` | `libm/upstream-freebsd/lib/msun/src/s_sin.c` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | imported/adapted | Public `sin`; includes `e_rem_pio2.c` with `INLINE_REM_PIO2`. |
| `libm/src/freebsd/s_tan.c` | `libm/upstream-freebsd/lib/msun/src/s_tan.c` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | imported/adapted | Public `tan`; includes `e_rem_pio2.c` with `INLINE_REM_PIO2`. |
| `libm/src/freebsd/s_trunc.c` | `libm/upstream-freebsd/lib/msun/src/s_trunc.c` | `main` at `731631f300090436d7f5df80d50b6275c8c60a93` | adapted | Upstream copyright preserved; uses the local adapted private header. |
| `libm/src/freebsd/k_log.h` | `libm/upstream-freebsd/lib/msun/src/k_log.h` | `main` FreeBSD/msun tree at `7732717429078dd0c583559b2cdc741c7681daf7` | imported | Header helper used by `e_log10.c`. |
| `libm/src/freebsd/k_logf.h` | `libm/upstream-freebsd/lib/msun/src/k_logf.h` | `main` FreeBSD/msun tree at `7732717429078dd0c583559b2cdc741c7681daf7` | imported | Header helper used by `e_log10f.c`. |
| `libm/src/freebsd/e_log10.c` | `libm/upstream-freebsd/lib/msun/src/e_log10.c` | `main` FreeBSD/msun tree at `7732717429078dd0c583559b2cdc741c7681daf7` | imported/adapted | Replaces the project-owned bootstrap `log10` wrapper. |
| `libm/src/freebsd/e_log10f.c` | `libm/upstream-freebsd/lib/msun/src/e_log10f.c` | `main` FreeBSD/msun tree at `7732717429078dd0c583559b2cdc741c7681daf7` | imported/adapted | Native float `log10f`; uses `k_logf.h`. |
| `libm/src/freebsd/e_log2.c` | `lib/msun/src/e_log2.c` | FreeBSD upstream main | adapted | Explicit source-family exception because observed Bionic trees did not expose a direct `e_log2.c`. |
| `libm/src/freebsd/e_log2f.c` | `lib/msun/src/e_log2f.c` | FreeBSD upstream main | adapted | Explicit source-family exception because observed Bionic trees did not expose a direct `e_log2f.c`. |
| `libm/src/freebsd/s_expm1.c` | `libm/upstream-freebsd/lib/msun/src/s_expm1.c` | `main` FreeBSD/msun tree at `7732717429078dd0c583559b2cdc741c7681daf7` | imported/adapted | Replaces the project-owned bootstrap `expm1` wrapper. |
| `libm/src/freebsd/s_expm1f.c` | `libm/upstream-freebsd/lib/msun/src/s_expm1f.c` | `main` FreeBSD/msun tree at `7732717429078dd0c583559b2cdc741c7681daf7` | imported/adapted | Native float `expm1f`. |
| `libm/src/freebsd/s_log1p.c` | `libm/upstream-freebsd/lib/msun/src/s_log1p.c` | `main` FreeBSD/msun tree at `7732717429078dd0c583559b2cdc741c7681daf7` | imported/adapted | Replaces the project-owned bootstrap `log1p` wrapper. |
| `libm/src/freebsd/s_log1pf.c` | `libm/upstream-freebsd/lib/msun/src/s_log1pf.c` | `main` FreeBSD/msun tree at `7732717429078dd0c583559b2cdc741c7681daf7` | imported/adapted | Native float `log1pf`. |
| `libm/src/freebsd/e_fmod.c` | `libm/upstream-freebsd/lib/msun/src/e_fmod.c` | `main` FreeBSD/msun tree at `7732717429078dd0c583559b2cdc741c7681daf7` | imported/adapted | Replaces the project-owned bootstrap `fmod` implementation; uses local `nan_mix_op` compatibility helper. |
| `libm/src/freebsd/e_fmodf.c` | `libm/upstream-freebsd/lib/msun/src/e_fmodf.c` | `main` FreeBSD/msun tree at `7732717429078dd0c583559b2cdc741c7681daf7` | imported/adapted | Native float `fmodf`; uses local `nan_mix_op` compatibility helper. |
| `libm/src/freebsd/e_remainder.c` | `libm/upstream-freebsd/lib/msun/src/e_remainder.c` | `main` FreeBSD/msun tree at `7732717429078dd0c583559b2cdc741c7681daf7` | imported/adapted | Replaces the project-owned bootstrap `remainder` implementation. |
| `libm/src/freebsd/e_remainderf.c` | `libm/upstream-freebsd/lib/msun/src/e_remainderf.c` | `main` FreeBSD/msun tree at `7732717429078dd0c583559b2cdc741c7681daf7` | imported/adapted | Native float `remainderf`. |
| `libm/src/freebsd/s_remquo.c` | `libm/upstream-freebsd/lib/msun/src/s_remquo.c` | `main` FreeBSD/msun tree at `7732717429078dd0c583559b2cdc741c7681daf7` | imported/adapted | Replaces the project-owned bootstrap `remquo` implementation. |
| `libm/src/freebsd/s_remquof.c` | `libm/upstream-freebsd/lib/msun/src/s_remquof.c` | `main` FreeBSD/msun tree at `7732717429078dd0c583559b2cdc741c7681daf7` | imported/adapted | Native float `remquof`. |
| `libm/src/freebsd/s_frexp.c` | `libm/upstream-freebsd/lib/msun/src/s_frexp.c` | `main` FreeBSD/msun tree at `7732717429078dd0c583559b2cdc741c7681daf7` | imported/adapted | Replaces the local IEEE decomposition bootstrap `frexp`. |
| `libm/src/freebsd/s_frexpf.c` | `libm/upstream-freebsd/lib/msun/src/s_frexpf.c` | `main` FreeBSD/msun tree at `7732717429078dd0c583559b2cdc741c7681daf7` | imported/adapted | Native float `frexpf`. |
| `libm/src/freebsd/s_modf.c` | `libm/upstream-freebsd/lib/msun/src/s_modf.c` | `main` FreeBSD/msun tree at `7732717429078dd0c583559b2cdc741c7681daf7` | imported/adapted | Replaces the project-owned bootstrap `modf` implementation. |
| `libm/src/freebsd/s_modff.c` | `libm/upstream-freebsd/lib/msun/src/s_modff.c` | `main` FreeBSD/msun tree at `7732717429078dd0c583559b2cdc741c7681daf7` | imported/adapted | Native float `modff`. |
| `libm/src/freebsd/e_rem_pio2f.c` | `libm/upstream-freebsd/lib/msun/src/e_rem_pio2f.c` | `main` FreeBSD/msun tree at `7732717429078dd0c583559b2cdc741c7681daf7` | imported/adapted | Included inline by `s_tanf.c`; not compiled as a separate object. |
| `libm/src/freebsd/k_sinf.c` | `libm/upstream-freebsd/lib/msun/src/k_sinf.c` | `main` FreeBSD/msun tree at `7732717429078dd0c583559b2cdc741c7681daf7` | imported | Included inline by project-owned `s_sincosf.c`; not compiled as a separate object. |
| `libm/src/freebsd/k_cosf.c` | `libm/upstream-freebsd/lib/msun/src/k_cosf.c` | `main` FreeBSD/msun tree at `7732717429078dd0c583559b2cdc741c7681daf7` | imported | Included inline by project-owned `s_sincosf.c`; not compiled as a separate object. |
| `libm/src/freebsd/s_sincosf.c` | none | project-owned | new | Public `sinf`/`cosf` wrappers over Bionic main float argument reduction and kernels because observed Bionic main lacks standalone public float wrappers. |
| `libm/src/freebsd/k_tanf.c` | `libm/upstream-freebsd/lib/msun/src/k_tanf.c` | `main` FreeBSD/msun tree at `7732717429078dd0c583559b2cdc741c7681daf7` | imported | Included inline by `s_tanf.c`; not compiled as a separate object. |
| `libm/src/freebsd/s_tanf.c` | `libm/upstream-freebsd/lib/msun/src/s_tanf.c` | `main` FreeBSD/msun tree at `7732717429078dd0c583559b2cdc741c7681daf7` | imported/adapted | Native float tangent wrapper; includes float argument reduction and kernel helpers inline. |

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
| `libc/src/arch/linux/x86_64/syscall.S` | mixed Bionic/Linux syscall surface | project-owned | extended | Adds raw clone, futex, and thread exit syscall wrappers. |
| `libc/src/arch/linux/aarch64/syscall.S` | mixed Bionic/Linux syscall surface | project-owned | extended | Adds raw clone, futex, and thread exit syscall wrappers. |

### Libdl Bootstrap Tranche

| Local file | Upstream path | Upstream ref | Status | Notes |
| --- | --- | --- | --- | --- |
| `include/dlfcn.h` | `libc/include/dlfcn.h` API shape | project-owned, Bionic-shaped | new | Defines the first POSIX/Bionic-style `dlopen`/`dlsym`/`dlclose`/`dlerror` surface and common `RTLD_*` constants. |
| `libdl/src/dl.c` | mixed Bionic/POSIX libdl surface | project-owned | new | Host adapter over Windows Kernel32 and macOS dyld APIs; Linux real loading is deferred to the project linker or a documented host bridge. |

### C++ Runtime Bootstrap Tranche

| Local file | Upstream path | Upstream ref | Status | Notes |
| --- | --- | --- | --- | --- |
| `libstdc++/src/cxxabi.c` | mixed Bionic/Itanium C++ ABI surface | project-owned | new | Provides first-tranche `__cxa_guard_*`, `__cxa_atexit`, `__cxa_finalize`, pure/deleted virtual handlers, and `__dso_handle`; full libc++abi/libunwind/libc++ import is deferred. |
| `libstdc++/src/msvcabi.c` | MSVC/UCRT C++ ABI surface | project-owned | new | Provides first Windows bridge-lane `_Init_thread_*` and `_purecall` hooks for simple MSVC-ABI probes; richer C++ interop is deferred. |

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
| `libc/src/arch/linux/x86_64/syscall.S` | mixed Bionic/Linux syscall surface | project-owned | extended | Adds raw futex syscall wrapper. |
| `libc/src/arch/linux/aarch64/syscall.S` | mixed Bionic/Linux syscall surface | project-owned | extended | Adds raw futex syscall wrapper. |

### Regex Tranche

Replaces a hand-rolled backtracking matcher (which had no `|` alternation
support at all -- it silently matched `|` as a literal character, breaking
GNU Autoconf's `checking for a sed`/`egrep`/`fgrep` self-tests) with the real
Henry Spencer/NetBSD strip-VM regex engine Bionic imports under
`libc/upstream-netbsd/lib/libc/regex/`. Full POSIX BRE/ERE, backreferences,
bounded repetition, POSIX character classes, and (opt-in via `REG_GNU`) GNU
BRE extensions (`\+`, `\?`, `\|`).

| Local file | Upstream path | Upstream ref | Status | Notes |
| --- | --- | --- | --- | --- |
| `libc/src/regex/regcomp.c` | `libc/upstream-netbsd/lib/libc/regex/regcomp.c` | `main` | adapted | BRE/ERE compiler. Adaptation is limited to the portability shim in `netbsd-compat.h` (force-included, no hand-edits to this file). |
| `libc/src/regex/regexec.c` | `libc/upstream-netbsd/lib/libc/regex/regexec.c` | `main` | adapted | `regexec()` entry point; textually includes `engine.c` three times (small/large/multibyte state representations). |
| `libc/src/regex/engine.c` | `libc/upstream-netbsd/lib/libc/regex/engine.c` | `main` | adapted | Matching engine. Deliberately **not** compiled as its own translation unit (not listed in `libc/CMakeLists.txt`) -- only ever reached via `regexec.c`'s `#include`. |
| `libc/src/regex/regerror.c` | `libc/upstream-netbsd/lib/libc/regex/regerror.c` | `main` | adapted | `regerror()`. |
| `libc/src/regex/regfree.c` | `libc/upstream-netbsd/lib/libc/regex/regfree.c` | `main` | adapted | `regfree()`. |
| `libc/src/regex/regex2.h` | `libc/upstream-netbsd/lib/libc/regex/regex2.h` | `main` | adapted | Internal `struct re_guts`/strip-VM opcodes. Opaque outside this directory -- `include/regex.h` (already Bionic-matching byte-for-byte) keeps `re_g` as a forward-declared pointer, so this carries no public ABI surface. |
| `libc/src/regex/utils.h` | `libc/upstream-netbsd/lib/libc/regex/utils.h` | `main` | adapted | `DUPMAX`/`INFINITY`/`NC`, and the non-NLS fallback `wint_t`/`mbstate_t`/`wctype_t` typedefs -- **NLS is deliberately left undefined**: this project's real `wint_t` (`__WINT_TYPE__` on this target) is `unsigned short`, which breaks the engine's negative `OUT`/`BADCHAR` sentinel comparisons (a 16-bit *unsigned* type can't round-trip a negative literal the way the engine's sentinel design assumes); the signed-`short` fallback utils.h already provides does not have that problem. See `netbsd-compat.h`'s top comment for the full reasoning. |
| `libc/src/regex/cname.h` | `libc/upstream-netbsd/lib/libc/regex/cname.h` | `main` | imported | Symbolic bracket-expression name table (e.g. `[.hyphen.]`). Pristine. |
| `libc/src/regex/netbsd-compat.h` | none | project-owned | new | Force-included (`-include`, same pattern as `libc/src/gdtoa/openbsd-compat.h`) ahead of every file above: defines `LIBHACK`/`REGEX_GNU_EXTENSIONS`, no-op `_DIAGASSERT`/`__RCSID`/`__FBSDID`, `__UNCONST`/`__arraycount`, and pulls in `<unistd.h>` for `_POSIX2_RE_DUP_MAX`. |
| `libc/src/regex/namespace.h` | none | project-owned | new | Empty stub satisfying `regerror.c`/`regfree.c`'s unconditional `#include "namespace.h"` (NetBSD's public/private symbol renaming scheme; not applicable here). |
| `libc/src/reallocarray.c` | none (Bionic-compatible signature/semantics; exact current Bionic source file not located) | project-owned | new | Overflow-checked `realloc(ptr, nmemb*size)`, needed by the regex engine's dynamic strip/cset array growth. |
| `include/limits.h` | `libc/include/limits.h` | project-owned, Bionic-shaped | extended | Adds `MB_LEN_MAX` (needed by `regcomp.c`). |

## Rules

- Preserve original copyright and license headers.
- Record every imported file in this manifest.
- Keep pristine snapshots, if added later, separate from project-adapted runtime
  sources.
- Do not import broad Bionic internals until a specific tranche requires them.
