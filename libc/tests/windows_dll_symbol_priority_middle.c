/* Regression fixture for tools/crt-cc's Windows shared-library link
 * order -- see windows_dll_symbol_priority_dll.c's own comment for the
 * "mbedtls" role this build plays alongside, and tools/crt-cc's own
 * Windows/shared_mode comment for the fix.
 *
 * This file plays the "libcurl" role: a SHARED library (like
 * libcurl-4.dll itself) that links against the fixture DLL's own
 * import library (playing "mbedtls"'s own libmbedcrypto.dll.a" role)
 * and, via tools/crt-cc's own default Windows link libraries, this
 * project's real c.lib -- exactly the two-import shape libcurl-4.dll's
 * own real build has. Being a DLL-to-DLL relationship, not an
 * EXE-to-DLL one, turned out to matter: an earlier version of this
 * fixture used a plain executable as the direct consumer of the fake
 * DLL, and lld-link resolved that case correctly regardless of link
 * order (a plain executable apparently prefers a real, directly-linked
 * static definition over any import-library stub, regardless of
 * command-line order) -- the real bug, and this fixture, are both
 * specifically about one DLL's own code calling into a symbol another
 * DLL it links against happens to also (accidentally) export.
 *
 * windows_dll_symbol_priority_check() does a real, ordinary pipe
 * write/read round trip using this file's own plain libc calls (never
 * calling anything from the fixture DLL) and returns 0 only if the
 * real bytes came back -- exactly mirroring libcurl-4.dll's own
 * Curl_wakeup_consume() calling read() on its own wakeup pipe. If
 * tools/crt-cc's link order is wrong, this DLL's own read() call
 * resolves to the fixture DLL's shadow stub instead of the real libc
 * implementation, and the round trip observably fails. */
#include <string.h>
#include <unistd.h>

int windows_dll_symbol_priority_check(void) {
  int fds[2];
  char buf[16];
  ssize_t written;
  ssize_t got;

  if (pipe(fds) != 0) {
    return 1;
  }
  written = write(fds[1], "hello", 5);
  if (written != 5) {
    return 2;
  }
  memset(buf, 0, sizeof(buf));
  got = read(fds[0], buf, sizeof(buf) - 1);
  if (got != 5 || memcmp(buf, "hello", 5) != 0) {
    return 3;
  }
  return 0;
}
