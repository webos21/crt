#include <errno.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#if defined(CRT_TARGET_OS_WINDOWS)
long __crt_sys_tcgetattr(int fd, struct termios* termios_p);
long __crt_sys_tcsetattr(int fd, const struct termios* termios_p);
/* tcdrain()/tcflow()/tcflush()/tcsendbreak(): a Windows console isn't a
 * real BSD/Linux tty line discipline (no transmit queue to drain, no
 * software/hardware flow control, no break condition), so these don't
 * translate 1:1 the way tcgetattr()/tcsetattr() do -- see
 * libc/src/arch/windows/common/syscall.c's own comment on the four
 * __crt_sys_tc*() functions below for exactly what each one really does
 * (FlushFileBuffers()/FlushConsoleInputBuffer() where a real Win32
 * equivalent exists, an honest no-op where it genuinely doesn't). */
long __crt_sys_tcdrain(int fd);
long __crt_sys_tcflow(int fd, int action);
long __crt_sys_tcflush(int fd, int queue_selector);
long __crt_sys_tcsendbreak(int fd, int duration);
#elif defined(CRT_TARGET_OS_LINUX) || defined(CRT_TARGET_OS_MACOS)
/* Real ioctl-backed implementation: TCGETS/TCSETS* on Linux
 * (libc/src/arch/linux/common/termios.c), TIOCGETA/TIOCSETA{,W,F} on
 * macOS (libc/src/arch/macos/common/termios.c) -- see either file's own
 * comment for why the previous hardcoded-stub behavior here was a real,
 * general round-trip bug (the same class already found and fixed on
 * Windows, tests/termios_echo_roundtrip_test.c). Both arch backends
 * expose the same __crt_sys_tc*() signatures despite having entirely
 * different wire ioctls/struct layouts underneath, so this file's own
 * dispatch logic below doesn't need to know which host it's on. */
long __crt_sys_tcgetattr(int fd, struct termios* termios_p);
long __crt_sys_tcsetattr(int fd, int optional_actions, const struct termios* termios_p);
long __crt_sys_tcdrain(int fd);
long __crt_sys_tcflow(int fd, int action);
long __crt_sys_tcflush(int fd, int queue_selector);
long __crt_sys_tcsendbreak(int fd, int duration);
#endif

#if defined(CRT_TARGET_OS_WINDOWS) || defined(CRT_TARGET_OS_LINUX) || defined(CRT_TARGET_OS_MACOS)
static int normalize_syscall_result(long result) {
  if (result < 0 && result >= -4095) {
    errno = (int)-result;
    return -1;
  }
  return (int)result;
}
#endif

int tcgetattr(int fd, struct termios* termios_p) {
  if (termios_p == 0) {
    errno = EFAULT;
    return -1;
  }
#if defined(CRT_TARGET_OS_WINDOWS) || defined(CRT_TARGET_OS_LINUX) || defined(CRT_TARGET_OS_MACOS)
  return normalize_syscall_result(__crt_sys_tcgetattr(fd, termios_p));
#else
  if (!isatty(fd)) {
    errno = ENOTTY;
    return -1;
  }
  memset(termios_p, 0, sizeof(*termios_p));
  termios_p->c_iflag = ICRNL | IXON;
  termios_p->c_oflag = OPOST | ONLCR;
  termios_p->c_cflag = CREAD | CS8;
  termios_p->c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHOK | IEXTEN;
  termios_p->c_ispeed = B38400;
  termios_p->c_ospeed = B38400;
  return 0;
#endif
}

int tcsetattr(int fd, int optional_actions, const struct termios* termios_p) {
  if (optional_actions != TCSANOW && optional_actions != TCSADRAIN && optional_actions != TCSAFLUSH) {
    errno = EINVAL;
    return -1;
  }
  if (termios_p == 0) {
    errno = EFAULT;
    return -1;
  }
#if defined(CRT_TARGET_OS_WINDOWS)
  return normalize_syscall_result(__crt_sys_tcsetattr(fd, termios_p));
#elif defined(CRT_TARGET_OS_LINUX) || defined(CRT_TARGET_OS_MACOS)
  return normalize_syscall_result(__crt_sys_tcsetattr(fd, optional_actions, termios_p));
#else
  if (!isatty(fd)) {
    errno = ENOTTY;
    return -1;
  }
  return 0;
#endif
}

int tcsendbreak(int fd, int duration) {
#if defined(CRT_TARGET_OS_WINDOWS) || defined(CRT_TARGET_OS_LINUX) || defined(CRT_TARGET_OS_MACOS)
  return normalize_syscall_result(__crt_sys_tcsendbreak(fd, duration));
#else
  (void)duration;
  if (!isatty(fd)) {
    errno = ENOTTY;
    return -1;
  }
  return 0;
#endif
}

int tcdrain(int fd) {
#if defined(CRT_TARGET_OS_WINDOWS) || defined(CRT_TARGET_OS_LINUX) || defined(CRT_TARGET_OS_MACOS)
  return normalize_syscall_result(__crt_sys_tcdrain(fd));
#else
  if (!isatty(fd)) {
    errno = ENOTTY;
    return -1;
  }
  return 0;
#endif
}

int tcflow(int fd, int action) {
  if (action != TCOOFF && action != TCOON && action != TCIOFF && action != TCION) {
    errno = EINVAL;
    return -1;
  }
#if defined(CRT_TARGET_OS_WINDOWS) || defined(CRT_TARGET_OS_LINUX) || defined(CRT_TARGET_OS_MACOS)
  return normalize_syscall_result(__crt_sys_tcflow(fd, action));
#else
  if (!isatty(fd)) {
    errno = ENOTTY;
    return -1;
  }
  return 0;
#endif
}

int tcflush(int fd, int queue_selector) {
  if (queue_selector != TCIFLUSH && queue_selector != TCOFLUSH && queue_selector != TCIOFLUSH) {
    errno = EINVAL;
    return -1;
  }
#if defined(CRT_TARGET_OS_WINDOWS) || defined(CRT_TARGET_OS_LINUX) || defined(CRT_TARGET_OS_MACOS)
  return normalize_syscall_result(__crt_sys_tcflush(fd, queue_selector));
#else
  if (!isatty(fd)) {
    errno = ENOTTY;
    return -1;
  }
  return 0;
#endif
}

speed_t cfgetispeed(const struct termios* termios_p) {
  return termios_p == 0 ? 0 : termios_p->c_ispeed;
}

speed_t cfgetospeed(const struct termios* termios_p) {
  return termios_p == 0 ? 0 : termios_p->c_ospeed;
}

int cfsetispeed(struct termios* termios_p, speed_t speed) {
  if (termios_p == 0) {
    errno = EFAULT;
    return -1;
  }
  termios_p->c_ispeed = speed;
  return 0;
}

int cfsetospeed(struct termios* termios_p, speed_t speed) {
  if (termios_p == 0) {
    errno = EFAULT;
    return -1;
  }
  termios_p->c_ospeed = speed;
  return 0;
}

int cfsetspeed(struct termios* termios_p, speed_t speed) {
  if (cfsetispeed(termios_p, speed) != 0) {
    return -1;
  }
  return cfsetospeed(termios_p, speed);
}

void cfmakeraw(struct termios* termios_p) {
  if (termios_p == 0) {
    return;
  }
  termios_p->c_iflag &= ~(IGNBRK | BRKINT | IGNPAR | PARMRK | INPCK | ISTRIP |
                          INLCR | IGNCR | ICRNL | IXON | IXOFF);
  termios_p->c_oflag &= ~OPOST;
  termios_p->c_cflag &= ~CSIZE;
  termios_p->c_cflag |= CS8;
  termios_p->c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL | ICANON | ISIG | IEXTEN);
  termios_p->c_cc[VMIN] = 1;
  termios_p->c_cc[VTIME] = 0;
}
