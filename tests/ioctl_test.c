#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

static int fail(const char* message) {
  fprintf(stderr, "ioctl_test: %s\n", message);
  return 1;
}

int main(void) {
  int pipefd[2];
  int available = -1;
  int fd;
  char byte = 'I';
  struct winsize ws;

  if (pipe(pipefd) != 0) {
    return fail("pipe");
  }
  if (write(pipefd[1], &byte, 1) != 1) {
    close(pipefd[0]);
    close(pipefd[1]);
    return fail("write pipe");
  }
  if (ioctl(pipefd[0], FIONREAD, &available) != 0 || available < 1) {
    close(pipefd[0]);
    close(pipefd[1]);
    return fail("FIONREAD pipe");
  }
  close(pipefd[0]);
  close(pipefd[1]);

  fd = open("ioctl_test.tmp", O_CREAT | O_RDWR | O_TRUNC, 0600);
  if (fd < 0) {
    return fail("open");
  }
  errno = 0;
  if (ioctl(fd, TIOCGWINSZ, &ws) == 0 || errno != ENOTTY) {
    close(fd);
    unlink("ioctl_test.tmp");
    return fail("TIOCGWINSZ regular errno");
  }
  errno = 0;
  if (ioctl(fd, 0x12345678, &available) == 0 || errno == 0) {
    close(fd);
    unlink("ioctl_test.tmp");
    return fail("unknown request errno");
  }
  close(fd);
  unlink("ioctl_test.tmp");

  printf("ioctl_test: ok\n");
  return 0;
}
