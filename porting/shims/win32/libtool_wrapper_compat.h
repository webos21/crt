/* Project-owned compatibility shim for GNU Libtool's own generated
 * "uninstalled execution" wrapper source (ltmain.sh's `func_emit_wrapper`
 * template, materialized fresh as `.libs/lt-*.c` for every executable a
 * port builds against an uninstalled shared library -- not part of the
 * port's own source at all).
 *
 * That template has two *independent*, mismatched conditionals:
 *
 *   #if defined _WIN32 && !defined __GNUC__
 *     #include <direct.h> / <process.h> / <io.h>   (MSVC-style headers)
 *   #else
 *     #include <unistd.h>                          (POSIX-style header)
 *   #endif
 *   ...
 *   #elif defined __MINGW32__
 *     #define getcwd  _getcwd
 *     #define stat    _stat
 *     #define chmod   _chmod
 *     #define putenv  _putenv
 *     #define setmode _setmode
 *   #endif
 *
 * This project's CFLAGS undefine _WIN32 (so libpng/etc. stay on their
 * generic POSIX code, matching what this sysroot actually provides --
 * see porting/recipes/libpng.json's own notes), which correctly steers
 * the *include* choice to <unistd.h>. But __MINGW32__ stays defined
 * (needed elsewhere, e.g. the `make` port's own dir.c fix), and the
 * *macro-renaming* block above keys off __MINGW32__ alone, independent
 * of the include choice -- so it *still* renames getcwd/stat/chmod/
 * putenv/setmode to their underscore-prefixed MSVCRT-only spellings,
 * which nothing declared, since the POSIX include branch was correctly
 * taken. Confirmed for real building libpng's own pngtest wrapper:
 * "call to undeclared function '_getcwd'" etc.
 *
 * Force-included (via this recipe's -include CFLAGS, not a #include
 * upstream ever writes -- there's no upstream file name to shadow the
 * way porting/shims/win32/windows.h shadows a real #include <windows.h>)
 * ahead of any of the wrapper's own text, so its *later*
 * `#define getcwd _getcwd`-style macros pair up with the reverse
 * `#define _getcwd getcwd` below into a standard, well-defined mutually-
 * recursive macro pair: the C preprocessor's own "blue paint" rule
 * (a macro name is never re-expanded within its own expansion chain)
 * means whichever spelling appears in the wrapper's body -- `stat` or
 * `_stat` -- expands exactly once through the pair and stops, always
 * landing back on this project's real, Bionic-style POSIX name. No
 * patch to the generated wrapper text itself, no public libc header
 * gains a non-Bionic MSVC-style alias -- scoped to this one force-
 * included compile only.
 *
 * _spawnv()/_P_WAIT have no "plain name" counterpart in the wrapper at
 * all (used with the underscore spelling directly, unconditionally --
 * even a real mingw-w64 <process.h> just declares them as-is) and
 * `_setmode()` has no real POSIX equivalent to pair back to (this
 * project's own I/O is already byte-transparent -- no CRLF translation
 * exists to switch on or off), so those three are real, independent
 * implementations instead of macro pairs. */
#ifndef CRT_PORT_SHIM_LIBTOOL_WRAPPER_COMPAT_H
#define CRT_PORT_SHIM_LIBTOOL_WRAPPER_COMPAT_H

#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <spawn.h>
#include <stdlib.h>

#define _getcwd getcwd
#define _stat stat
#define _chmod chmod
#define _putenv putenv

/* Byte-transparent I/O (no CRLF translation, matching this project's
 * Bionic-style Linux semantics throughout) has no text/binary mode
 * distinction to actually switch -- a real no-op success return is the
 * faithful mapping, not merely a stub. */
static __inline int _setmode(int fd, int mode) {
  (void)fd;
  (void)mode;
  return 0;
}

/* _spawnv()'s _P_WAIT mode: start `path` with `argv`, wait for it
 * synchronously, and return its exit status (or -1 if it couldn't even
 * be started). Deliberately posix_spawn()+waitpid(), NOT fork()+
 * execv()+waitpid() -- fork() on this project's Windows PAL is a real,
 * heavy memory-copy clone of the calling process (see
 * docs/windows_fork_emulation.md) that additionally requires the
 * calling *program itself* to have opted into the ASLR-mitigation
 * self-relaunch dance at startup (only crt_mksh and the ctest suite do
 * -- see fork_capable_relaunch.c); any other program calling a
 * fork()-based _spawnv() would silently fail every single call
 * ("stack commit failed", confirmed hitting this directly in a
 * from-scratch standalone test program during development). _spawnv()
 * only ever means "run a *different* program in a new process" -- it
 * was never going to keep this process's own memory around after the
 * exec() anyway -- so posix_spawn() (a direct CreateProcessA()-style
 * spawn, no memory copy, no ASLR-mitigation opt-in required) is both
 * the correct semantic match and strictly cheaper. The wrapper's own
 * comment explains why it avoids plain execv() in the first place
 * ("execv doesn't actually work on mingw as expected on unix") -- not
 * a concern here either way, since this isn't using execv(). */
#define _P_WAIT 0
static __inline int _spawnv(int mode, const char* path, const char* const* argv) {
  pid_t pid;
  int status;

  (void)mode;
  /* posix_spawn()'s own POSIX signature takes char *const argv[], not
   * const char *const argv[] -- a decades-old historical wart shared
   * by the whole exec/spawn family (none of them actually modify
   * argv), matching real MSVCRT's own _spawnv() also declaring argv
   * const while the underlying OS call it wraps does not. */
  if (posix_spawn(&pid, path, 0, 0, (char* const*)argv, environ) != 0) {
    return -1;
  }
  if (waitpid(pid, &status, 0) < 0) {
    return -1;
  }
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

#endif /* CRT_PORT_SHIM_LIBTOOL_WRAPPER_COMPAT_H */
