#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>

/* Real Linux termios support, via the same TCGETS/TCSETS*[/TCSBRK/TCXONC/
 * TCFLSH] ioctls fd.c's isatty() already proved out for real (TCGETS as a
 * pure "is this fd a tty" probe). libc/src/termios.c's portable
 * tcgetattr()/tcsetattr()/tcdrain()/tcflow()/tcflush()/tcsendbreak() used
 * to be pure software stubs on every non-Windows host -- tcsetattr()
 * silently discarded everything it was asked to set, and tcgetattr()
 * always returned one fixed, hardcoded struct regardless -- so a real
 * set-then-verify round trip (tests/termios_echo_roundtrip_test.c, a
 * regression already written for the equivalent Windows bug found
 * earlier -- see that file's own comment) failed the exact same way on
 * Linux the Windows bug did, just never caught until a real terminal was
 * available to run it against. Found while auditing CRT/PAL termios
 * behavior ahead of the libcrtgfx tranche.
 *
 * The kernel's own struct termios (asm-generic/termbits.h, shared by
 * aarch64 and x86_64 -- both use the "generic" Linux termios/ioctl ABI,
 * unlike mips/sparc/powerpc) has no separate speed fields at all: baud
 * rate lives packed into c_cflag's CBAUD/CBAUDEX bits, and NCCS is 19,
 * not this project's public NCCS(32). This project's public <termios.h>
 * already defines B0/B9600/B38400/B115200 etc as the literal raw
 * CBAUD-encoded values (not a separately-mapped bps number), so
 * extracting/injecting speed is just masking c_cflag with CBAUD directly
 * -- no lookup table needed. */

#define CRT_LINUX_TERMIOS_NCCS 19

struct crt_linux_kernel_termios {
  tcflag_t c_iflag;
  tcflag_t c_oflag;
  tcflag_t c_cflag;
  tcflag_t c_lflag;
  cc_t c_line;
  cc_t c_cc[CRT_LINUX_TERMIOS_NCCS];
};

long __crt_sys_ioctl(int fd, unsigned long request, void* arg);

static void kernel_to_termios(const struct crt_linux_kernel_termios* k, struct termios* t) {
  memset(t, 0, sizeof(*t));
  t->c_iflag = k->c_iflag;
  t->c_oflag = k->c_oflag;
  t->c_cflag = k->c_cflag;
  t->c_lflag = k->c_lflag;
  t->c_line = k->c_line;
  memcpy(t->c_cc, k->c_cc, sizeof(k->c_cc));
  t->c_ispeed = t->c_ospeed = (speed_t)(k->c_cflag & CBAUD);
}

static void termios_to_kernel(const struct termios* t, struct crt_linux_kernel_termios* k) {
  memset(k, 0, sizeof(*k));
  k->c_iflag = t->c_iflag;
  k->c_oflag = t->c_oflag;
  /* c_ospeed is authoritative for both directions, matching real glibc:
   * this project's tcsetattr() (and the wider POSIX API) has no way to
   * ask the classic TCGETS/TCSETS* ioctls for split input/output speeds
   * (that needs the separate, Linux-specific termios2/BOTHER/TCGETS2
   * mechanism, out of scope here -- nothing in this project's own shell/
   * toybox/rootfs work needs it). */
  k->c_cflag = (t->c_cflag & ~(tcflag_t)CBAUD) | (t->c_ospeed & CBAUD);
  k->c_lflag = t->c_lflag;
  k->c_line = t->c_line;
  memcpy(k->c_cc, t->c_cc, sizeof(k->c_cc));
}

long __crt_sys_tcgetattr(int fd, struct termios* termios_p) {
  struct crt_linux_kernel_termios kernel_termios;
  long result = __crt_sys_ioctl(fd, TCGETS, &kernel_termios);

  if (result < 0) {
    return result;
  }
  kernel_to_termios(&kernel_termios, termios_p);
  return 0;
}

long __crt_sys_tcsetattr(int fd, int optional_actions, const struct termios* termios_p) {
  struct crt_linux_kernel_termios kernel_termios;
  unsigned long request;

  switch (optional_actions) {
    case TCSANOW:
      request = TCSETS;
      break;
    case TCSADRAIN:
      request = TCSETSW;
      break;
    case TCSAFLUSH:
      request = TCSETSF;
      break;
    default:
      return -EINVAL;
  }
  termios_to_kernel(termios_p, &kernel_termios);
  return __crt_sys_ioctl(fd, request, &kernel_termios);
}

long __crt_sys_tcdrain(int fd) {
  /* glibc's own tcdrain(): ioctl(fd, TCSBRK, 1) -- a nonzero arg means
   * "wait for output to drain", zero would mean "send a break". */
  return __crt_sys_ioctl(fd, TCSBRK, (void*)(long)1);
}

long __crt_sys_tcflow(int fd, int action) {
  return __crt_sys_ioctl(fd, TCXONC, (void*)(long)action);
}

long __crt_sys_tcflush(int fd, int queue_selector) {
  return __crt_sys_ioctl(fd, TCFLSH, (void*)(long)queue_selector);
}

long __crt_sys_tcsendbreak(int fd, int duration) {
  /* glibc's own tcsendbreak(): ioctl(fd, TCSBRK, 0) -- duration is
   * platform-defined by POSIX and Linux's own TCSBRK ignores the exact
   * nonzero value (any nonzero arg drains instead of breaking), so 0 is
   * the only value that actually sends a real break here. */
  (void)duration;
  return __crt_sys_ioctl(fd, TCSBRK, (void*)(long)0);
}
