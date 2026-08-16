#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static int fail(const char* message) {
  fprintf(stderr, "memfd_create_test: %s\n", message);
  return 1;
}

static int fail_errno(const char* message) {
  fprintf(stderr, "memfd_create_test: %s errno=%d\n", message, errno);
  return 1;
}

int main(void) {
  int fd;
  int fd2;
  char* mapped;
  char readback[16];

  /* Unknown flag bits are rejected. */
  if (memfd_create("crt-test", 0x80000000U) >= 0 || errno != EINVAL) {
    return fail("unknown flag bits should be rejected");
  }

  fd = memfd_create("crt-test-1", MFD_CLOEXEC);
  if (fd < 0) {
    return fail_errno("memfd_create");
  }

  /* Two independent calls must never collide on the same underlying
   * storage. */
  fd2 = memfd_create("crt-test-2", 0);
  if (fd2 < 0) {
    close(fd);
    return fail_errno("second memfd_create");
  }
  if (write(fd2, "unrelated", 9) != 9) {
    close(fd);
    close(fd2);
    return fail("write to second memfd");
  }

  if (write(fd, "hello world", 11) != 11) {
    close(fd);
    close(fd2);
    return fail("write");
  }
  if (lseek(fd, 0, SEEK_SET) != 0) {
    close(fd);
    close(fd2);
    return fail("lseek");
  }
  memset(readback, 0, sizeof(readback));
  if (read(fd, readback, 11) != 11 || memcmp(readback, "hello world", 11) != 0) {
    close(fd);
    close(fd2);
    return fail("read back");
  }

  /* The second memfd's own content must be independent (not overwritten by
   * the first fd's writes above). */
  if (lseek(fd2, 0, SEEK_SET) != 0) {
    close(fd);
    close(fd2);
    return fail("lseek fd2");
  }
  memset(readback, 0, sizeof(readback));
  if (read(fd2, readback, 9) != 9 || memcmp(readback, "unrelated", 9) != 0) {
    close(fd);
    close(fd2);
    return fail("second memfd content was not independent");
  }
  close(fd2);

  /* Real MAP_SHARED mmap round trip -- this is the actual reason
   * memfd_create matters: an anonymous, shareable, mmap-able fd. */
  mapped = (char*)mmap(0, 11, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (mapped == MAP_FAILED) {
    close(fd);
    return fail_errno("mmap");
  }
  if (memcmp(mapped, "hello world", 11) != 0) {
    munmap(mapped, 11);
    close(fd);
    return fail("mmap content mismatch");
  }
  mapped[0] = 'H';
  if (munmap(mapped, 11) != 0) {
    close(fd);
    return fail("munmap");
  }
  if (lseek(fd, 0, SEEK_SET) != 0) {
    close(fd);
    return fail("lseek after mmap write");
  }
  memset(readback, 0, sizeof(readback));
  if (read(fd, readback, 11) != 11 || memcmp(readback, "Hello world", 11) != 0) {
    close(fd);
    return fail("mmap write did not reach the underlying fd");
  }

  close(fd);

  printf("memfd_create_test: ok\n");
  return 0;
}
