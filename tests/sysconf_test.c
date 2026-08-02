#include <errno.h>
#include <stdio.h>
#include <unistd.h>

static int fail(const char* message) {
  fprintf(stderr, "sysconf_test: %s\n", message);
  return 1;
}

static int expect_positive(int name, const char* message) {
  errno = 0;
  if (sysconf(name) <= 0 || errno != 0) {
    return fail(message);
  }
  return 0;
}

static int expect_value(int name, long value, const char* message) {
  errno = 0;
  if (sysconf(name) != value || errno != 0) {
    return fail(message);
  }
  return 0;
}

static int expect_known_unsupported(int name, const char* message) {
  errno = 0;
  if (sysconf(name) != -1 || errno != 0) {
    return fail(message);
  }
  return 0;
}

static int expect_known_runtime(int name, const char* message) {
  long value;

  errno = 0;
  value = sysconf(name);
  if ((value <= 0 && value != -1) || errno != 0) {
    return fail(message);
  }
  return 0;
}

int main(void) {
  char path[8];

  if (_SC_OPEN_MAX != 0x000b ||
      _SC_CLK_TCK != 0x0006 ||
      _SC_MAPPED_FILES != 0x003b ||
      _SC_NPROCESSORS_ONLN != 0x0061 ||
      _SC_MONOTONIC_CLOCK != 0x0064) {
    return fail("Bionic sysconf numbers");
  }
  if (expect_positive(_SC_PAGESIZE, "pagesize") ||
      expect_positive(_SC_PAGE_SIZE, "page size") ||
      expect_positive(_SC_OPEN_MAX, "open max") ||
      expect_value(_SC_CLK_TCK, 100, "clk tck") ||
      expect_value(_SC_MAPPED_FILES, _POSIX_MAPPED_FILES, "mapped files") ||
      expect_value(_SC_MONOTONIC_CLOCK, _POSIX_MONOTONIC_CLOCK, "monotonic clock") ||
      expect_positive(_SC_NPROCESSORS_CONF, "nprocessors conf") ||
      expect_positive(_SC_NPROCESSORS_ONLN, "nprocessors onln") ||
      expect_value(_SC_THREADS, _POSIX_THREADS, "threads") ||
      expect_value(_SC_READER_WRITER_LOCKS, _POSIX_READER_WRITER_LOCKS, "rwlocks") ||
      expect_positive(_SC_THREAD_KEYS_MAX, "thread keys") ||
      expect_positive(_SC_THREAD_STACK_MIN, "thread stack min") ||
      expect_known_runtime(_SC_PHYS_PAGES, "phys pages") ||
      expect_known_runtime(_SC_AVPHYS_PAGES, "avphys pages") ||
      expect_known_unsupported(_SC_2_C_DEV, "2 c dev")) {
    return 1;
  }
  errno = 0;
  if (sysconf(0x7fffffff) != -1 || errno != ENOSYS) {
    return fail("unknown errno");
  }
  errno = 0;
  if (confstr(_CS_PATH, 0, 0) != sizeof("/system/bin:/bin:/usr/bin") || errno != 0) {
    return fail("confstr size");
  }
  if (confstr(_CS_PATH, path, sizeof(path)) != sizeof("/system/bin:/bin:/usr/bin") ||
      path[sizeof(path) - 1] != '\0') {
    return fail("confstr copy");
  }
  if (confstr(_CS_V7_ENV, 0, 0) != sizeof("POSIXLY_CORRECT=1")) {
    return fail("confstr v7 env");
  }
  errno = 0;
  if (confstr(0x7fffffff, path, sizeof(path)) != 0 || errno != EINVAL) {
    return fail("confstr unknown");
  }

  printf("sysconf_test: ok\n");
  return 0;
}
