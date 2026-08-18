#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define CRT_ATEXIT_MAX 32

void __crt_sys_exit(int status) __attribute__((noreturn));
void __cxa_finalize(void* dso) __attribute__((weak));
/* Linux (libc/src/arch/linux/common/init_fini_array.c) and Windows
 * (libc/src/arch/windows/common/init_fini_array.c) both define
 * __crt_run_fini_array(), but ONLY as part of the executable's own crt1
 * startup objects (CRT_STARTUP_OBJECTS in libc/CMakeLists.txt), never as
 * part of libc itself -- this exact translation unit is compiled into
 * BOTH the static "c" library and the "c_shared" shared library/DLL, and
 * the walker object is deliberately not linked into c_shared (it has no
 * business running more than once, and only the final executable's own
 * crt1 knows the true boundaries of ITS OWN .init_array/.ctors sections
 * -- a shared libc could not use its own copy for that). So this must
 * stay a weak reference, resolved by the final executable's link (which
 * always includes the walker) and left unresolved-but-harmless inside
 * c_shared's own link.
 *
 * Guarded by CRT_TARGET_OS_LINUX/CRT_TARGET_OS_WINDOWS (not just a bare
 * __attribute__((weak)) declaration left unguarded on every platform,
 * the way __cxa_finalize above is) because leaving an unresolved weak
 * symbol reference in the link only works portably when that platform's
 * linker actually supports resolving-to-NULL for a truly unsatisfied
 * weak reference. ELF (Linux) and COFF (Windows, via lld-link's weak
 * external support) both do; Mach-O (macOS) does not -- confirmed for
 * real: macOS CI failed libc.dylib with "Undefined symbols ...
 * ___crt_run_fini_array" the first time this shipped without the guard.
 * __cxa_finalize survives unguarded because Apple's own libSystem.dylib
 * genuinely provides it, so it's weak-but-resolved there, unlike
 * __crt_run_fini_array which macOS has no equivalent of at all -- and,
 * unlike Linux/Windows, genuinely doesn't need one: Mach-O constructors
 * (__DATA,__mod_init_func) are run by dyld itself, automatically, for
 * every loaded image (including the main executable) before dyld ever
 * transfers control to this project's own entry point (libc/src/arch/
 * macos/{x86_64,aarch64}/crt1.S's _start) -- true regardless of what
 * that entry symbol is named, since dyld's own image-initialization
 * sequence runs first for ALL dynamically-linked Mach-O binaries, which
 * is the only kind macOS supports. Mach-O destructors
 * (__DATA,__mod_term_func) are, in turn, registered by dyld via the same
 * Itanium C++ ABI atexit mechanism __cxa_finalize(0) above already
 * drains unconditionally -- so both already run correctly on macOS
 * through pre-existing machinery, with no custom walker required. */
#if defined(CRT_TARGET_OS_LINUX) || defined(CRT_TARGET_OS_WINDOWS)
void __crt_run_fini_array(void) __attribute__((weak));
#endif

static void (*atexit_handlers[CRT_ATEXIT_MAX])(void);
static int atexit_count;

int atexit(void (*function)(void)) {
  if (function == 0 || atexit_count >= CRT_ATEXIT_MAX) {
    return -1;
  }
  atexit_handlers[atexit_count++] = function;
  return 0;
}

void exit(int status) {
  if (__cxa_finalize != 0) {
    __cxa_finalize(0);
  }
  while (atexit_count > 0) {
    void (*handler)(void) = atexit_handlers[--atexit_count];
    handler();
  }
#if defined(CRT_TARGET_OS_LINUX) || defined(CRT_TARGET_OS_WINDOWS)
  if (__crt_run_fini_array != 0) {
    __crt_run_fini_array();
  }
#endif
  (void)fflush(0);
  __crt_sys_exit(status);
}

void _exit(int status) {
  __crt_sys_exit(status);
}

void _Exit(int status) {
  _exit(status);
}
