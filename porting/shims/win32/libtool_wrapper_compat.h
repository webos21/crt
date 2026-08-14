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
 * implementations instead of macro pairs.
 *
 * `setmode()` (no leading underscore) is a fourth, later-discovered
 * variant of the same problem, found porting curl: curl's own recipe
 * undefines __MINGW32__ too (needed so curl's *own* configure/source
 * treats this target as a portable POSIX host, not just to steer the
 * wrapper's #include choice the way libpng's recipe does) -- so the
 * `#elif defined __MINGW32__` rename block above never fires at all,
 * and the wrapper's own generated call site (`setmode(1,_O_BINARY);`,
 * emitted as a literal, unconditional line of C by ltmain.sh's own
 * `func_emit_wrapper`, not gated behind any further #ifdef of its own)
 * stays spelled exactly `setmode` -- undeclared, since neither the
 * _MSC_VER nor __MINGW32__ branch that would have declared *or* renamed
 * it ever ran. Aliased unconditionally to _setmode() below (`#define
 * setmode _setmode`) -- confirmed harmless: curl's own real source has
 * exactly one reference to the bare name (`lib/curl_setup.h`'s
 * `CURL_BINMODE` macro), itself inside a permanently-dead `#ifdef
 * MSDOS` branch on every host this project targets, so this doesn't
 * touch a second, unrelated `setmode` binding anywhere in either port's
 * own code.
 *
 * Separately, _spawnv() also fixes up a broken PATH env var before
 * actually launching the target program (see _crt_libtool_wrapper_
 * fix_path_env() below). The wrapper's own generated LIB_PATH_VALUE/
 * EXE_PATH_VALUE C-string constants (built by libtool's *shell script*
 * at generation time, from lt_cv_to_host_file_cmd/lt_cv_to_tool_file_cmd
 * -- preset to func_convert_file_noop in tools/crt-port-build.py's
 * make_env(), see that preset's own comment for why: this project's
 * $build is a genuinely native mksh/toybox PAL, not real MSYS, so the
 * only libtool-provided conversion that actually applies here is "no
 * conversion, paths are already host format") are ':'-joined, matching
 * this project's own POSIX-list convention -- but the real Windows OS
 * (used by the real CreateProcess-equivalent this _spawnv() is about to
 * call) parses its PATH environment variable on ';', not ':'. The
 * wrapper's own lt_update_lib_path()/lt_update_exe_path() (ltmain.sh's
 * generated code, not ours) do nothing more than blind string
 * concatenation (`new_value = LIB_PATH_VALUE + getenv("PATH")`), so by
 * the time this process's own `environ` reaches _spawnv(), PATH already
 * contains ':'-joined entries a real Windows path search cannot split.
 * No practical failure was ever observed from this (Windows' own
 * "search the target .exe's own directory first" default DLL
 * resolution already finds a co-located .dll regardless of PATH), but
 * it's a real latent bug for any case that isn't co-located. Fixed at
 * the one point in this whole chain that is genuinely this project's
 * own code (not generated output, not part of any port's source, and
 * the last point before the real OS-level spawn happens): rewrite
 * PATH's *list* separators from ':' to ';' right before spawning,
 * carefully leaving every drive-letter colon untouched -- Windows
 * filesystem rules forbid ':' from ever appearing in a path except as
 * the drive-letter separator, so any ':' that isn't in that exact
 * position is unambiguously a stray POSIX-style list separator, never
 * a genuine part of a path component. */
#ifndef CRT_PORT_SHIM_LIBTOOL_WRAPPER_COMPAT_H
#define CRT_PORT_SHIM_LIBTOOL_WRAPPER_COMPAT_H

/* Deliberately minimal includes -- see the long comment below for why.
 * <spawn.h>/<sys/wait.h> are kept because _spawnv()'s own body genuinely
 * needs their real declarations (posix_spawn()'s opaque attribute/
 * file-actions types aren't safe to hand-roll); everything else this
 * file needs is forward-declared by hand instead of pulled in via a
 * whole header. */
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>

/* Deliberately NOT #include <unistd.h>/<stdlib.h>/<sys/stat.h>/
 * <string.h> here, and NOT relying on <spawn.h>'s own transitive pull
 * of <sched.h>/<signal.h> for anything beyond what posix_spawn() itself
 * needs -- all found, the hard way, breaking curl's own configure,
 * because this shim is force-included via -include into EVERY compile
 * in the whole build (needed so the actual libtool wrapper compile,
 * reached only through Automake's LINK rule, picks it up -- see this
 * file's own top comment), not just the wrapper's own. That includes
 * autoconf's own tiny, header-free conftest.c probes, which deliberately
 * avoid including anything themselves to test raw, undecorated symbol
 * visibility. Two distinct failure shapes were found this way:
 *
 * (1) A hard configure error: `AC_C_UNDECLARED_BUILTIN_OPTIONS` compiles
 *     `int main(void) { (void) strchr; return 0; }` expecting it to FAIL
 *     (that's how it detects the compiler truly treats undeclared
 *     identifiers as errors). Pulling in the real <string.h> here made
 *     `strchr` already declared, so that conftest compiled cleanly
 *     instead -- confirmed via config.log ($?=0 where autoconf expected
 *     nonzero) -- and configure hard-errored ("cannot make ... report
 *     undeclared builtins").
 * (2) A silent, much more dangerous misdetection: GNU Autoconf's classic
 *     `ac_fn_c_check_func` (the shell function backing every plain
 *     `AC_CHECK_FUNC`/`AC_CHECK_FUNCS` call, used by curl's configure
 *     for dozens of functions) compiles a K&R-style probe --
 *     `char FUNCNAME ();` followed by `return FUNCNAME ();` -- to test
 *     pure link-time symbol existence, deliberately with NO headers of
 *     its own either. If this shim had already declared that same
 *     symbol with its REAL, correctly-typed prototype (as <unistd.h>
 *     does for `pipe(int[2])`, <stdlib.h> does for `realpath(...)`, or
 *     <spawn.h>'s own transitive <sched.h> does for `sched_yield(void)`),
 *     the two conflicting declarations in one translation unit are a
 *     hard type-conflict compile error -- which `ac_fn_c_check_func`
 *     treats not as a hard failure but as an ordinary, silent "no, this
 *     function doesn't exist" result. Confirmed for real by diffing this
 *     port's own Windows configure output against a from-scratch rerun
 *     after this fix: `checking for pipe... no` (should be `yes` -- this
 *     PAL genuinely implements it) turned out to be exactly why curl's
 *     own `Curl_multi_handle()` failed at runtime with "Out of memory"
 *     on the very first `curl_easy_perform()` -- curl's own internal
 *     wakeup-pipe mechanism (`lib/socketpair.c`) needs a working `pipe()`
 *     it believed it didn't have, and picked a different, broken path
 *     instead. `checking for realpath...`/`checking for sched_yield...`
 *     also silently flipped from `yes` to `no` the same way, from
 *     <stdlib.h>'s real `realpath()` prototype and <spawn.h>'s
 *     transitive <sched.h> pull of `sched_yield()`, respectively --
 *     confirmed non-fatal to this port's own real, exercised code paths
 *     (a full HTTP+HTTPS round trip through this project's own real
 *     toolchain still passes end to end after this fix), but left here
 *     as a known, deliberately-accepted residual gap: `<spawn.h>` is the
 *     one header this shim cannot avoid (posix_spawn()'s own opaque
 *     attribute/file-actions types aren't safe to hand-roll), so any
 *     *other* function `<sched.h>`/`<signal.h>` happen to also declare
 *     stays at this same small, bounded risk of a false "no" if curl (or
 *     a future port reusing this shim) ever comes to depend on one. The
 *     general lesson, not just curl-specific: force-including anything
 *     into a whole autoconf-driven build's CFLAGS makes this shim's own
 *     header footprint part of that build's own probe surface -- keep it
 *     to the absolute minimum, and treat any header this file adds back
 *     as a real, audited decision, not a convenience.
 *
 * environ (only <unistd.h> declares it in this project) and
 * malloc()/free()/strlen()/memcpy()/strncmp() (only <stdlib.h>/
 * <string.h> do) are all needed directly by this file's own code below,
 * so they're forward-declared by hand here instead. */
extern char** environ;
void* malloc(size_t size);
void free(void* ptr);
size_t strlen(const char* s);
void* memcpy(void* dest, const void* src, size_t n);
int strncmp(const char* a, const char* b, size_t n);

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

/* Bare `setmode` (no leading underscore): the wrapper calls this
 * literal spelling directly, unconditionally, whenever neither the
 * _MSC_VER nor __MINGW32__ rename block above fired -- see the top
 * comment's own `setmode()` paragraph for why that's exactly curl's
 * case. Same real implementation as _setmode(), just under the other
 * name. */
#define setmode _setmode

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

/* Rewrite VALUE's list separators (':') to the real Windows separator
 * (';'), leaving every drive-letter colon untouched. A colon is a
 * drive-letter colon iff it is the very next character after exactly
 * one alphabetic character that itself starts a path component (the
 * very start of the string, or immediately after a previous
 * separator) -- every other colon is unambiguously a stray list
 * separator, per Windows' own filesystem naming rules (':' is illegal
 * anywhere in a real Windows path except that one position). Returns
 * a freshly malloc()'d copy; caller frees. Returns NULL on allocation
 * failure (caller treats that as "leave PATH alone"). */
static __inline char* _crt_libtool_wrapper_fix_path_seps(const char* value) {
  size_t len = strlen(value);
  char* fixed = (char*)malloc(len + 1);
  size_t i;
  size_t component_start = 0;
  if (!fixed) {
    return 0;
  }
  memcpy(fixed, value, len + 1);
  for (i = 0; i < len; i++) {
    if (fixed[i] == ':') {
      int is_drive_letter = (i == component_start + 1) &&
        ((fixed[component_start] >= 'A' && fixed[component_start] <= 'Z') ||
         (fixed[component_start] >= 'a' && fixed[component_start] <= 'z'));
      if (is_drive_letter) {
        continue;
      }
      fixed[i] = ';';
      component_start = i + 1;
    } else if (fixed[i] == ';') {
      component_start = i + 1;
    }
  }
  return fixed;
}

/* Copy ENVP, replacing its "PATH=..." entry (if any) with one whose
 * value has been through _crt_libtool_wrapper_fix_path_seps(). All
 * other entries are aliased, not copied -- only the replaced PATH
 * entry and the new outer array are ever freed by the caller. Returns
 * ENVP itself, unmodified, if no PATH entry is found or an allocation
 * fails (a safe, inert fallback -- the same broken-but-usually-benign
 * behavior this project already shipped before this fix). *OUT_OWNED
 * is set to the replacement PATH string when one was allocated (for
 * the caller to free), or NULL otherwise. */
static __inline char** _crt_libtool_wrapper_fix_path_env(char* const* envp, char** out_owned) {
  size_t count = 0;
  size_t i;
  char** fixed_envp;

  *out_owned = 0;
  while (envp[count]) {
    count++;
  }
  fixed_envp = (char**)malloc((count + 1) * sizeof(char*));
  if (!fixed_envp) {
    return (char**)envp;
  }
  for (i = 0; i < count; i++) {
    fixed_envp[i] = envp[i];
    if (!*out_owned && strncmp(envp[i], "PATH=", 5) == 0) {
      char* fixed_value = _crt_libtool_wrapper_fix_path_seps(envp[i] + 5);
      if (fixed_value) {
        size_t entry_len = 5 + strlen(fixed_value) + 1;
        char* entry = (char*)malloc(entry_len);
        if (entry) {
          memcpy(entry, "PATH=", 5);
          memcpy(entry + 5, fixed_value, strlen(fixed_value) + 1);
          fixed_envp[i] = entry;
          *out_owned = entry;
        }
        free(fixed_value);
      }
    }
  }
  fixed_envp[count] = 0;
  return fixed_envp;
}

static __inline int _spawnv(int mode, const char* path, const char* const* argv) {
  pid_t pid;
  int status;
  int rc;
  char* owned_path_entry;
  char** fixed_envp = _crt_libtool_wrapper_fix_path_env(environ, &owned_path_entry);

  (void)mode;
  /* posix_spawn()'s own POSIX signature takes char *const argv[], not
   * const char *const argv[] -- a decades-old historical wart shared
   * by the whole exec/spawn family (none of them actually modify
   * argv), matching real MSVCRT's own _spawnv() also declaring argv
   * const while the underlying OS call it wraps does not. */
  if (posix_spawn(&pid, path, 0, 0, (char* const*)argv, fixed_envp) != 0) {
    rc = -1;
  } else if (waitpid(pid, &status, 0) < 0) {
    rc = -1;
  } else {
    rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  }
  if (fixed_envp != (char**)environ) {
    free(fixed_envp);
  }
  free(owned_path_entry);
  return rc;
}

#endif /* CRT_PORT_SHIM_LIBTOOL_WRAPPER_COMPAT_H */
