/* Regression for a real bug found while implementing toybox's `stty` applet
 * (2026-08-16, see HISTORY.md): __crt_sys_tcgetattr() re-derived every
 * termios field from hardcoded constants and real console-mode bits on
 * every call, ignoring almost everything __crt_sys_tcsetattr() had actually
 * been asked to set. Windows' console mode only exposes ECHO, ECHOE, and
 * ECHOK bundled behind one bit (ENABLE_ECHO_INPUT) -- there is no separate
 * Win32 control for "visually erase on backspace" vs. "visually erase whole
 * line on kill char". Because of that bundling, asking to clear only ECHO
 * (leaving ECHOE/ECHOK set, exactly what `stty -echo` does) came back from
 * tcgetattr() with ECHOE/ECHOK cleared too -- a real, general round-trip
 * mismatch any termios consumer doing a set-then-verify would hit, not an
 * stty-specific one. Fixed via a per-fd shadow (fd_termios_shadow[] in
 * libc/src/arch/windows/common/syscall.c) that tcgetattr() returns
 * verbatim once tcsetattr() has been called at least once on that fd. */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static int fail(const char* message) {
  fprintf(stderr, "termios_echo_roundtrip_test: %s\n", message);
  return 1;
}

int main(void) {
  int fd;
  struct termios before, want, got;

  /* /dev/tty needs a real attached console; some CI/sandbox environments
   * genuinely don't have one (confirmed directly on this machine's own dev
   * environment: GetConsoleMode fails outright with no console attached at
   * all). That's an environment limitation, not something this test can
   * exercise -- skip rather than fail when it isn't available, matching
   * how a real device-dependent POSIX test would behave. */
  fd = open("/dev/tty", O_RDWR | O_NOCTTY);
  if (fd < 0) {
    printf("termios_echo_roundtrip_test: skip (no /dev/tty: %s)\n",
           strerror(errno));
    return 0;
  }

  if (tcgetattr(fd, &before) != 0) {
    close(fd);
    printf("termios_echo_roundtrip_test: skip (tcgetattr: %s)\n",
           strerror(errno));
    return 0;
  }

  /* Clear only ECHO, deliberately leaving ECHOE/ECHOK exactly as they were
   * -- this is what toybox's own stty.c does for a plain `stty -echo`, and
   * is the exact shape that exposed the bug. */
  want = before;
  want.c_lflag &= (tcflag_t)~ECHO;
  want.c_lflag |= (before.c_lflag & (ECHOE | ECHOK));

  if (tcsetattr(fd, TCSADRAIN, &want) != 0) {
    close(fd);
    return fail("tcsetattr(-ECHO) failed");
  }

  if (tcgetattr(fd, &got) != 0) {
    close(fd);
    return fail("tcgetattr() after tcsetattr() failed");
  }

  if (memcmp(&got, &want, sizeof(got)) != 0) {
    close(fd);
    return fail("tcgetattr() did not return exactly what tcsetattr() was "
                "asked to set (round-trip mismatch)");
  }

  if ((got.c_lflag & ECHO) != 0) {
    close(fd);
    return fail("ECHO bit was not actually cleared");
  }

  /* Restore the original settings before exiting. */
  tcsetattr(fd, TCSADRAIN, &before);
  close(fd);

  printf("termios_echo_roundtrip_test: ok\n");
  return 0;
}
