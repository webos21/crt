/* Regression for the real macOS termios port (2026-08-17, see HISTORY.md):
 * libc/src/termios.c's tcgetattr()/tcsetattr()/tcdrain()/tcflow()/
 * tcflush()/tcsendbreak() used to fall through to pure hardcoded-value/
 * no-op stubs on macOS (confirmed failing directly on real macOS hardware
 * before libc/src/arch/macos/common/termios.c existed). This test covers
 * the "line control functions" (tcdrain/tcflow/tcflush/tcsendbreak, the
 * exact grouping this host's own man page uses) plus a handful of
 * tcgetattr()/tcsetattr() round trips tests/termios_echo_roundtrip_test.c
 * doesn't already cover: c_cflag's CSIZE/PARENB/CSTOPB bits (exercises
 * the CSIZE 2-bit index translation, not just single-bit flags), c_cc's
 * VMIN/VTIME (exercises the c_cc index-translation table at indices other
 * than VINTR/VEOF/ECHOE/ECHOK), and c_ispeed/c_ospeed (exercises the
 * Bionic-B-code<->literal-baud-number translation). */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static int fail(const char* message) {
  fprintf(stderr, "termios_line_control_test: %s: %s\n", message, strerror(errno));
  return 1;
}

int main(void) {
  int fd;
  struct termios before, want, got;

  /* Same environment caveat as termios_echo_roundtrip_test.c: /dev/tty
   * needs a real attached console, which some CI/sandbox environments
   * genuinely don't have -- skip rather than fail there. */
  fd = open("/dev/tty", O_RDWR | O_NOCTTY);
  if (fd < 0) {
    printf("termios_line_control_test: skip (no /dev/tty: %s)\n", strerror(errno));
    return 0;
  }
  if (tcgetattr(fd, &before) != 0) {
    close(fd);
    printf("termios_line_control_test: skip (tcgetattr: %s)\n", strerror(errno));
    return 0;
  }

  if (tcdrain(fd) != 0) {
    close(fd);
    return fail("tcdrain failed");
  }

  if (tcflush(fd, TCIFLUSH) != 0 || tcflush(fd, TCOFLUSH) != 0 ||
      tcflush(fd, TCIOFLUSH) != 0) {
    close(fd);
    return fail("tcflush failed");
  }

  /* Suspend/resume output and input flow control -- restore each side
   * immediately so a real terminal isn't left stuck if this fails. */
  if (tcflow(fd, TCOOFF) != 0 || tcflow(fd, TCOON) != 0 ||
      tcflow(fd, TCIOFF) != 0 || tcflow(fd, TCION) != 0) {
    close(fd);
    return fail("tcflow failed");
  }

  if (tcsendbreak(fd, 0) != 0) {
    close(fd);
    return fail("tcsendbreak failed");
  }

  want = before;
  want.c_cflag &= (tcflag_t)~CSIZE;
  want.c_cflag |= CS7;
  want.c_cflag |= PARENB | CSTOPB;
  want.c_cflag &= (tcflag_t)~PARODD;
  if (tcsetattr(fd, TCSANOW, &want) != 0) {
    close(fd);
    return fail("tcsetattr(CS7|PARENB|CSTOPB) failed");
  }
  if (tcgetattr(fd, &got) != 0) {
    close(fd);
    return fail("tcgetattr after cflag set failed");
  }
  if ((got.c_cflag & CSIZE) != CS7 || !(got.c_cflag & PARENB) ||
      !(got.c_cflag & CSTOPB) || (got.c_cflag & PARODD) != 0) {
    close(fd);
    fprintf(stderr, "termios_line_control_test: c_cflag round trip mismatch "
                     "(CS7/PARENB/CSTOPB/PARODD)\n");
    tcsetattr(fd, TCSANOW, &before);
    close(fd);
    return 1;
  }

  want = before;
  want.c_cc[VMIN] = 7;
  want.c_cc[VTIME] = 3;
  if (tcsetattr(fd, TCSANOW, &want) != 0) {
    tcsetattr(fd, TCSANOW, &before);
    close(fd);
    return fail("tcsetattr(VMIN/VTIME) failed");
  }
  if (tcgetattr(fd, &got) != 0) {
    tcsetattr(fd, TCSANOW, &before);
    close(fd);
    return fail("tcgetattr after VMIN/VTIME set failed");
  }
  if (got.c_cc[VMIN] != 7 || got.c_cc[VTIME] != 3) {
    fprintf(stderr, "termios_line_control_test: VMIN/VTIME did not round-trip\n");
    tcsetattr(fd, TCSANOW, &before);
    close(fd);
    return 1;
  }

  want = before;
  cfsetspeed(&want, B9600);
  if (tcsetattr(fd, TCSANOW, &want) != 0) {
    tcsetattr(fd, TCSANOW, &before);
    close(fd);
    return fail("tcsetattr(B9600) failed");
  }
  if (tcgetattr(fd, &got) != 0) {
    tcsetattr(fd, TCSANOW, &before);
    close(fd);
    return fail("tcgetattr after speed set failed");
  }
  if (cfgetispeed(&got) != B9600 || cfgetospeed(&got) != B9600) {
    fprintf(stderr, "termios_line_control_test: B9600 speed did not round-trip\n");
    tcsetattr(fd, TCSANOW, &before);
    close(fd);
    return 1;
  }

  tcsetattr(fd, TCSANOW, &before);
  close(fd);
  printf("termios_line_control_test: ok\n");
  return 0;
}
