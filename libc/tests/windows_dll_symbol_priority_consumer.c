/* Regression test for tools/crt-cc's Windows shared-library link order
 * -- see windows_dll_symbol_priority_dll.c's and _middle.c's own
 * comments for the full DLL-to-DLL scenario this reproduces ("mbedtls"
 * and "libcurl" stand-ins, respectively), and tools/crt-cc's own
 * Windows/shared_mode comment for the fix.
 *
 * This executable is deliberately thin: it links only against the
 * "middle" (libcurl stand-in) DLL and calls its one exported check
 * function, exactly mirroring how curl's own real
 * porting/tests/curl_http_roundtrip.c links only libcurl.dll.a
 * directly, with libcurl-4.dll's own link against mbedtls already
 * baked into libcurl-4.dll's prior build. The actual scenario under
 * test happens entirely inside the middle DLL's own link, not here. */
#include <stdio.h>

extern int windows_dll_symbol_priority_check(void);

int main(void) {
  int result = windows_dll_symbol_priority_check();

  if (result != 0) {
    fprintf(stderr,
      "windows_dll_symbol_priority_test: check failed (code %d) -- the "
      "middle DLL's own real libc read() call was shadowed by the "
      "fixture DLL's re-exported symbol of the same name (tools/crt-cc's "
      "Windows link order regressed)\n",
      result);
    return 1;
  }

  printf("windows_dll_symbol_priority_test: ok\n");
  return 0;
}
