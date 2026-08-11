/* Runs ELF .init_array/.fini_array constructors/destructors for the
 * executable's own static link.
 *
 * Found and fixed while porting xz/liblzma (porting/recipes/xz.json):
 * liblzma's CRC32 dispatcher picks its implementation once, via a
 * `static void crc32_set_func(void) __attribute__((__constructor__))`
 * that the linker places into .init_array -- and this project's crt1
 * startup never ran .init_array at all, on any of the three OSes, for
 * the executable entry point (a *shared* library's own .init_array is a
 * completely different, unaffected mechanism: the OS's own dynamic
 * linker runs that automatically when the .so/.dylib/.dll loads -- which
 * is exactly why this had never surfaced before now: every prior port
 * that got this far linked shared, not static). Left uncalled, liblzma's
 * `crc32_func` stayed at its zero-initialized (NULL) value, and the
 * first real call into `lzma_crc32()` jumped through a null function
 * pointer. See HISTORY.md's dated entry for the full investigation.
 *
 * __*_array_start/__*_array_end are boundary symbols the default GNU ld/
 * lld linker script already provides automatically (PROVIDE_HIDDEN)
 * whenever the corresponding output section exists -- true for every
 * link this project produces, so these always resolve (to an empty
 * range when nothing actually needs them, which is the common case for
 * this project's own code -- none of it currently relies on
 * __attribute__((constructor))).
 *
 * Scoped to Linux only for now: __crt_run_init_array() is called
 * directly from crt1.S, which is always statically embedded into every
 * executable regardless of whether libc itself is linked static or
 * shared, so the *construction* side is correctly fixed either way.
 * __crt_run_fini_array() is reached via a weak reference from the
 * OS-agnostic libc/src/exit.c, which does NOT hold for an executable
 * dynamically linked against libc.so: exit() then lives in a different
 * DSO than this file (only ever compiled into the `crt1` object library,
 * not into c_shared), and this project's toolchain doesn't pass
 * -rdynamic/--export-dynamic, so libc.so's weak reference to this
 * executable-local symbol won't resolve across that DSO boundary.
 * Statically-linked executables (this fix's actual, verified case) are
 * unaffected by that gap. macOS (__mod_init_func/__mod_term_func,
 * Mach-O's own section-name convention) and Windows (.CRT$XCU-family PE
 * sections) need their own, separately-implemented equivalents -- not
 * done here; see TODO.md.
 */

typedef void (*crt_init_func)(void);

extern crt_init_func __init_array_start[] __attribute__((weak));
extern crt_init_func __init_array_end[] __attribute__((weak));
extern crt_init_func __fini_array_start[] __attribute__((weak));
extern crt_init_func __fini_array_end[] __attribute__((weak));

void __crt_run_init_array(void) {
  crt_init_func* f;

  if (__init_array_start == 0) {
    return;
  }
  for (f = __init_array_start; f != __init_array_end; ++f) {
    (*f)();
  }
}

void __crt_run_fini_array(void) {
  crt_init_func* f;

  if (__fini_array_start == 0) {
    return;
  }
  /* Destructors run in the reverse of construction order, matching
   * every other libc's convention (glibc, musl, Bionic). */
  for (f = __fini_array_end; f != __fini_array_start;) {
    --f;
    (*f)();
  }
}
