#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

static int fail(const char* message) {
  static const char prefix[] = "fd_errno_test: ";
  static const char suffix[] = "\n";
  write(2, prefix, sizeof(prefix) - 1);
  write(2, message, strlen(message));
  write(2, suffix, sizeof(suffix) - 1);
  return 1;
}

int main(void) {
  char buffer[4];
  int fd;
  ssize_t bytes_read;

  errno = 0;
  if (close(-1) != -1 || errno != EBADF) {
    return fail("close errno");
  }

  fd = open("README.md", O_RDONLY);
  if (fd < 0) {
    return fail("open README.md");
  }

  bytes_read = read(fd, buffer, sizeof(buffer));
  if (bytes_read != (ssize_t)sizeof(buffer)) {
    close(fd);
    return fail("read README.md");
  }
  if (buffer[0] != '#' || buffer[1] != ' ' || buffer[2] != 'C' || buffer[3] != 'R') {
    close(fd);
    return fail("read bytes");
  }

  if (lseek(fd, 0, SEEK_SET) != 0) {
    close(fd);
    return fail("lseek");
  }

  if (close(fd) != 0) {
    return fail("close README.md");
  }

  write(1, "fd_errno_test: ok\n", 18);
  return 0;
}
