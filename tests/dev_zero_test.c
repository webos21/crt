#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int fail(const char* message) {
  fprintf(stderr, "dev_zero_test: %s\n", message);
  return 1;
}

int main(void) {
  int fd;
  char buf[256];
  size_t i;
  struct stat st;
  ssize_t n;

  memset(buf, 0x5a, sizeof(buf));

  fd = open("/dev/zero", O_RDONLY);
  if (fd < 0) {
    return fail("open O_RDONLY failed");
  }
  n = read(fd, buf, sizeof(buf));
  if (n != (ssize_t)sizeof(buf)) {
    close(fd);
    return fail("short read");
  }
  for (i = 0; i < sizeof(buf); ++i) {
    if (buf[i] != 0) {
      close(fd);
      return fail("read returned nonzero byte");
    }
  }
  /* A second read must keep returning zeros -- this is not a one-shot
   * device, unlike /dev/urandom's own "exhausted after one read" reasoning
   * would never apply to (there is no entropy pool to exhaust here). */
  memset(buf, 0x5a, sizeof(buf));
  n = read(fd, buf, sizeof(buf));
  if (n != (ssize_t)sizeof(buf) || buf[0] != 0 || buf[sizeof(buf) - 1] != 0) {
    close(fd);
    return fail("second read not all zero");
  }
  if (fstat(fd, &st) != 0 || !S_ISCHR(st.st_mode)) {
    close(fd);
    return fail("fstat not a char device");
  }
  if (close(fd) != 0) {
    return fail("close failed");
  }

  /* Real /dev/zero also accepts writes and just discards them. */
  fd = open("/dev/zero", O_WRONLY);
  if (fd < 0) {
    return fail("open O_WRONLY failed");
  }
  n = write(fd, "discard me", 10);
  if (n != 10) {
    close(fd);
    return fail("write did not report full count");
  }
  close(fd);

  if (access("/dev/zero", F_OK | R_OK | W_OK) != 0) {
    return fail("access failed");
  }
  if (stat("/dev/zero", &st) != 0 || !S_ISCHR(st.st_mode)) {
    return fail("stat not a char device");
  }

  printf("dev_zero_test: ok\n");
  return 0;
}
