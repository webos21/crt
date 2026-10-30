#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(CRT_TARGET_OS_LINUX) && defined(__aarch64__)
_Static_assert(O_DIRECTORY == 040000 && O_NOFOLLOW == 0100000, "arm64 kernel open flags");
#endif

static int fail(const char* message) {
  fprintf(stderr, "open_nofollow_test: %s (errno %d)\n", message, errno);
  return 1;
}

int main(void) {
  char dir[] = "open_nofollow_test_XXXXXX";
  char file[64];
  char link[64];
  char dangling[64];
  int dirfd;
  int fd;

  if (mkdtemp(dir) == NULL) {
    return fail("mkdtemp");
  }
  snprintf(file, sizeof(file), "%s/file", dir);
  snprintf(link, sizeof(link), "%s/link", dir);
  snprintf(dangling, sizeof(dangling), "%s/dangling", dir);
  fd = open(file, O_CREAT | O_WRONLY, 0600);
  if (fd < 0 || close(fd) != 0) {
    return fail("create file");
  }
  if (symlink("file", link) != 0 || symlink("missing-target", dangling) != 0) {
    return fail("symlink");
  }

  /* A followed link opens; O_NOFOLLOW must refuse it, including a dangling one that would otherwise be ENOENT. */
  fd = open(link, O_RDONLY);
  if (fd < 0 || close(fd) != 0) {
    return fail("open through link");
  }
  errno = 0;
  if (open(link, O_RDONLY | O_NOFOLLOW) != -1 || errno != ELOOP) {
    return fail("O_NOFOLLOW on link must fail with ELOOP");
  }
  errno = 0;
  if (open(dangling, O_RDONLY | O_NOFOLLOW) != -1 || errno != ELOOP) {
    return fail("O_NOFOLLOW on dangling link must fail with ELOOP");
  }

  dirfd = open(dir, O_RDONLY | O_DIRECTORY);
  if (dirfd < 0) {
    return fail("open directory with O_DIRECTORY");
  }
  errno = 0;
  if (openat(dirfd, "file", O_RDONLY | O_DIRECTORY) != -1 || errno != ENOTDIR) {
    return fail("O_DIRECTORY on a file must fail with ENOTDIR");
  }
  errno = 0;
  if (openat(dirfd, "dangling", O_RDONLY | O_NOFOLLOW | O_DIRECTORY) != -1 || (errno != ELOOP && errno != ENOTDIR)) {
    return fail("O_NOFOLLOW|O_DIRECTORY on a link must fail");
  }
  close(dirfd);

  if (unlink(file) != 0 || unlink(link) != 0 || unlink(dangling) != 0 || rmdir(dir) != 0) {
    return fail("cleanup");
  }
  puts("open_nofollow_test: ok");
  return 0;
}
