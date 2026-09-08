/* Regression fixture for tools/crt-cc's Windows shared-library link
 * order (see that file's own Windows/shared_mode comment for the full
 * story): reproduces, deliberately and minimally, the exact shape of
 * bug mbedtls's own hand-rolled Windows .dll build hit for real while
 * porting curl -- a DLL that links this project's own libc with no
 * symbol-visibility control, so GNU ld's PE default (auto-export every
 * global symbol unless something is explicitly dllexport-annotated,
 * which nothing in this project's libc or in most upstream C libraries
 * is) re-exports names that collide with real libc functions right
 * alongside the DLL's own intended API.
 *
 * This file plays the "mbedtls" role: it defines its own read(), which
 * deliberately does something no real read() ever would (returns a
 * fixed, out-of-range sentinel instead of touching the fd at all), so
 * a consumer that accidentally resolves its own read() calls to THIS
 * DLL's copy instead of the real libc gets an unambiguous, immediately
 * observable wrong answer -- not a timing-dependent hang or a
 * plausible-looking wrong value that could be mistaken for a real
 * system difference.
 *
 * windows_dll_symbol_priority_probe() is a real, correctly-behaving
 * exported function -- included so this DLL is unambiguously a normal,
 * working shared library (with a real, intentional export) and not
 * just a container for the shadow symbol, matching what a real
 * upstream DLL like mbedtls's actually looks like: mostly legitimate
 * exports, with the shadowed libc symbols riding along unnoticed. */
#include <sys/types.h>

ssize_t read(int fd, void* buf, size_t count) {
  (void)fd;
  (void)buf;
  (void)count;
  return -12345;
}

int windows_dll_symbol_priority_probe(void) {
  return 42;
}
