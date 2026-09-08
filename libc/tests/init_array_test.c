#include <stdio.h>

/* Permanent regression test for ELF .init_array/.fini_array (and their
 * Windows/macOS equivalents) support at the executable entry point --
 * see HISTORY.md's dated entry and TODO.md's ".init_array"/".fini_array"
 * item for the full story of the real, general CRT startup gap found
 * while porting xz/liblzma (porting/recipes/xz.json): liblzma's CRC32
 * dispatcher picks its implementation once via a real
 * `__attribute__((constructor))` function, and this project's crt1
 * startup had never run those (or their `__attribute__((destructor))`
 * counterpart) for an executable's own static link, on any of the three
 * OSes -- invisible until then because every port that got that far had
 * always linked shared, where the OS's own dynamic loader runs a
 * `.so`/`.dylib`/`.dll`'s own constructors automatically (a completely
 * different, unaffected mechanism from what this test exercises).
 *
 * Deliberately checks both halves together with one required output
 * line, printed *from the destructor*: if the constructor never ran,
 * `ctor_ran` stays 0 and the destructor (if it runs at all) prints the
 * failure line instead; if the destructor never runs at all, neither
 * line appears and the test fails on ctest's default "did the process
 * print what PASS_REGULAR_EXPRESSION expects" check. A destructor can
 * still safely printf() here: __crt_run_fini_array() runs before
 * fflush(0)/the real _exit() syscall (see libc/src/exit.c), so stdout is
 * still live.
 */

static int ctor_ran = 0;

__attribute__((constructor)) static void init_array_test_ctor(void) {
  ctor_ran = 1;
}

__attribute__((destructor)) static void init_array_test_dtor(void) {
  if (ctor_ran) {
    printf("init_array_test: ok\n");
  } else {
    printf("init_array_test: constructor never ran\n");
  }
  fflush(stdout);
}

int main(void) {
  return 0;
}
