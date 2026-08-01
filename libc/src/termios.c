#include <errno.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#if defined(CRT_TARGET_OS_WINDOWS)
long __crt_sys_tcgetattr(int fd, struct termios* termios_p);
long __crt_sys_tcsetattr(int fd, const struct termios* termios_p);

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
#if defined(CRT_TARGET_OS_WINDOWS)
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
#else
  if (!isatty(fd)) {
    errno = ENOTTY;
    return -1;
  }
  return 0;
#endif
}

int tcsendbreak(int fd, int duration) {
  (void)duration;
  if (!isatty(fd)) {
    errno = ENOTTY;
    return -1;
  }
  return 0;
}

int tcdrain(int fd) {
  if (!isatty(fd)) {
    errno = ENOTTY;
    return -1;
  }
  return 0;
}

int tcflow(int fd, int action) {
  if (action != TCOOFF && action != TCOON && action != TCIOFF && action != TCION) {
    errno = EINVAL;
    return -1;
  }
  if (!isatty(fd)) {
    errno = ENOTTY;
    return -1;
  }
  return 0;
}

int tcflush(int fd, int queue_selector) {
  if (queue_selector != TCIFLUSH && queue_selector != TCOFLUSH && queue_selector != TCIOFLUSH) {
    errno = EINVAL;
    return -1;
  }
  if (!isatty(fd)) {
    errno = ENOTTY;
    return -1;
  }
  return 0;
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
