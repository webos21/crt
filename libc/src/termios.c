#include <errno.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

int tcgetattr(int fd, struct termios* termios_p) {
  if (termios_p == 0) {
    errno = EFAULT;
    return -1;
  }
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
}

int tcsetattr(int fd, int optional_actions, const struct termios* termios_p) {
  (void)termios_p;
  if (optional_actions != TCSANOW && optional_actions != TCSADRAIN && optional_actions != TCSAFLUSH) {
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
