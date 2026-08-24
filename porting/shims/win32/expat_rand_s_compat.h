/* Windows-only compatibility shim for porting/recipes/expat.json.
 *
 * expat's own configure.ac classifies this project's --build=@CRT_MINGW_
 * TRIPLE@ host as `mingw*` (a real, load-bearing classification -- it is
 * also what steers GNU Libtool onto its Windows DLL/import-library code
 * path, the same "shared-pass" precedent zlib.json/libpng.json/pcre2.json
 * already rely on), which unconditionally adds lib/random_rand_s.c to the
 * Automake `MINGW` source-list conditional (lib/Makefile.am), regardless
 * of this recipe's own `-U_WIN32 ...` CFLAGS override. Unlike expat's
 * other Windows-only code (gated `#ifdef _WIN32` inside translation units
 * that are always compiled, where undefining _WIN32 is enough to keep
 * expat on its generic POSIX path -- see expat.json's own notes),
 * random_rand_s.c is a *separate file*, added to the build at configure
 * time based on host-triple classification alone; undefining _WIN32 at
 * compile time cannot remove it from the Makefile's source list.
 *
 * random_rand_s.c calls a bare, undeclared `rand_s()` -- a real Microsoft
 * CRT extension function this project's own Bionic-compatible libc does
 * not implement (rand_s() is not POSIX and Android/Bionic does not have
 * it either), so the file fails to compile outright ("call to undeclared
 * function 'rand_s'"). Confirmed for real (2026-08-24) this is genuinely
 * dead weight, not a real gap: expat's own lib/xmlparse.c entropy-source
 * priority chain (generate_hash_secret_salt()) checks HAVE_ARC4RANDOM_BUF,
 * HAVE_ARC4RANDOM, HAVE_GETENTROPY, then HAVE_GETRANDOM/HAVE_SYSCALL_
 * GETRANDOM, *before* it would ever fall through to a Windows-only rand_s
 * path -- and this project's own libc already provides a real, working
 * getrandom() on Windows (confirmed: expat's own configure printed
 * "getrandom: true" and lib/random_getrandom.c, which actually calls it,
 * compiled and linked successfully) -- so random_rand_s.c's own output is
 * never reached at runtime regardless of whether it compiles.
 *
 * Rather than either (a) implementing a real rand_s() generally in this
 * project's own libc purely to satisfy one dead code path in one port, or
 * (b) patching expat's own configure.ac/Makefile.am (this project's
 * standing no-upstream-source-patch policy -- see libcrtgfx/third_party/
 * skia/README.md's "Do not patch the fetched upstream source" for the
 * general statement of this policy), this shim provides a real, working
 * rand_s() implementation in terms of this project's own already-linked,
 * already-verified getrandom() -- so random_rand_s.c compiles AND, in the
 * (currently unreachable) event something ever did call it, would return
 * real random data from the same underlying source expat already trusts
 * elsewhere, not a fake/insecure stand-in. Wired in via the same
 * `force_include` recipe mechanism (folded into CFLAGS, not CPPFLAGS --
 * see tools/crt-port-build.py's apply_recipe_env()) already established
 * for porting/recipes/libpng.json's own libtool_wrapper_compat.h.
 */
#ifndef CRT_PORTING_SHIMS_WIN32_EXPAT_RAND_S_COMPAT_H
#define CRT_PORTING_SHIMS_WIN32_EXPAT_RAND_S_COMPAT_H

#include <sys/random.h>

static inline int rand_s(unsigned int *random_value) {
  return getrandom(random_value, sizeof(*random_value), 0) == (ssize_t)sizeof(*random_value) ? 0 : -1;
}

#endif /* CRT_PORTING_SHIMS_WIN32_EXPAT_RAND_S_COMPAT_H */
