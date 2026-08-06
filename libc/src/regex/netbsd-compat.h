/* Portability shim for the NetBSD/Bionic regex engine imported into this
 * directory (regcomp.c, regexec.c, engine.c, regerror.c, regfree.c,
 * regex2.h, cname.h, utils.h -- see third_party/bionic/README.md for
 * upstream provenance). Force-included via `-include` ahead of every
 * translation unit in this directory (libc/CMakeLists.txt), matching the
 * gdtoa import's openbsd-compat.h pattern, so none of the upstream
 * sources need hand-editing for these particular differences.
 *
 * - LIBHACK: upstream's own escape hatch (already present in regcomp.c/
 *   regexec.c) for building outside a full NetBSD libc tree. Skips
 *   `#include "namespace.h"` (NetBSD's own public/private symbol
 *   renaming scheme -- irrelevant here) and the `__weak_alias(...)`
 *   calls that follow it. regerror.c/regfree.c include "namespace.h"
 *   unconditionally regardless of LIBHACK; a project-owned empty
 *   namespace.h in this same directory satisfies that #include with no
 *   effect (see namespace.h).
 * - REGEX_GNU_EXTENSIONS: upstream only turns this on as a side effect
 *   of the `#ifndef LIBHACK` block above (i.e. normally never, once
 *   LIBHACK is defined) -- defined independently here to keep the GNU
 *   BRE extensions (`\+`, `\?`, `\|`, etc.) regardless.
 * - NLS is deliberately left UNDEFINED, even though this project's
 *   MB_CUR_MAX is a fixed 4 (see include/stdlib.h) and so
 *   `MB_CUR_MAX > 1` is always true in regexec()'s own dispatch,
 *   meaning the multibyte-aware matcher (mmatcher, engine.c's MNAMES
 *   instantiation) is always what actually runs. utils.h's `#else`
 *   (non-NLS) branch typedefs its own SIGNED `wint_t`/`mbstate_t`/
 *   `wctype_t` (renamed to regex_wint_t etc. so they can't collide with
 *   this project's real, unsigned `wint_t` from <wchar.h>) specifically
 *   because the engine's OUT/BADCHAR out-of-band sentinel values
 *   (engine.c, e.g. `if (wc == BADCHAR)`) are negative int constants:
 *   comparing them against this project's real `wint_t`
 *   (`unsigned short`, per __WINT_TYPE__ on this target) is not just a
 *   spurious `-Wtautological-constant-out-of-range-compare` -- it is
 *   really always false, since assigning a negative sentinel into a
 *   16-bit *unsigned* short truncates/wraps it, and the zero-extending
 *   unsigned-short-to-int promotion on the read side can never produce
 *   a negative value to match the literal again. utils.h's own signed
 *   `short` fallback does not have this problem (a negative literal
 *   assigned into a signed short and read back still promotes to the
 *   same negative int). Leaving NLS undefined still gets the same
 *   mmatcher code path exercised either way (MB_CUR_MAX forces it
 *   regardless); the only behavioral difference is that
 *   regexec.c's `xmbrtowc()` takes its non-NLS branch (`*wi = *s;
 *   return 1;`, one raw byte at a time) instead of calling this
 *   project's real mbrtowc() for true UTF-8 decoding -- equivalent to
 *   what the single-byte matchers (smatcher/lmatcher) would have done
 *   anyway. Multibyte-aware character-class matching is accordingly
 *   not supported by this import; plain ASCII/Latin-1 pattern and
 *   subject text (essentially all real shell/configure-script grep/
 *   sed/expr usage) is unaffected.
 * - _DIAGASSERT/__RCSID/__FBSDID: NetBSD-internal debug-assert and
 *   RCS/FreeBSD version-string macros with no equivalent here and no
 *   functional purpose in this build; defined as no-ops. (sccsid/
 *   __FBSDID call sites upstream are already dead code inside `#if 0`
 *   or `#ifdef __FBSDID` guards in every file except the always-live
 *   `__RCSID(...)` line each .c file carries.)
 * - __UNCONST: NetBSD's const-stripping cast helper, used once in
 *   engine.c to free() a `const char **` field. Not provided by this
 *   project's sys/cdefs.h.
 * - __arraycount: NetBSD's array-length helper, used once in regcomp.c
 *   to iterate the built-in wctype name table. Not provided by this
 *   project's sys/cdefs.h.
 * - <unistd.h>: utils.h's DUPMAX needs _POSIX2_RE_DUP_MAX, which lives
 *   there (include/unistd.h) in this project, not in <limits.h> where
 *   upstream's NetBSD expects to already have it in scope. */
#ifndef CRT_REGEX_NETBSD_COMPAT_H
#define CRT_REGEX_NETBSD_COMPAT_H

#include <stdint.h>
#include <unistd.h>

#define LIBHACK 1
#define REGEX_GNU_EXTENSIONS 1

#define _DIAGASSERT(x) ((void)0)
#define __RCSID(s)
#define __FBSDID(s)
#define __UNCONST(a) ((void*)(uintptr_t)(const void*)(a))
#define __arraycount(a) (sizeof(a) / sizeof((a)[0]))

#endif
