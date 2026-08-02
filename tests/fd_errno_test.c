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
  int* errno_slot;
  ssize_t bytes_read;

  errno = 123;
  errno_slot = __errno();
  if (errno_slot == 0 || *errno_slot != 123 || __errno() != errno_slot) {
    return fail("errno slot");
  }

  errno = 0;
  if (close(-1) != -1 || errno != EBADF) {
    return fail("close errno");
  }
  errno = 0;
  if (open("missing-file-for-errno-test", O_RDONLY) != -1 || errno != ENOENT) {
    return fail("open missing errno");
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
  errno = 0;
  if (pread(fd, buffer, sizeof(buffer), 1024 * 1024) != 0 || errno != 0) {
    close(fd);
    return fail("pread eof errno");
  }

  if (close(fd) != 0) {
    return fail("close README.md");
  }

  write(1, "fd_errno_test: ok\n", 18);
  return 0;
}
