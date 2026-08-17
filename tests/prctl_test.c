/* prctl() -- Linux-only in real Bionic too, see docs/bionic_libc_gaps.md.
 * The PR_* constants below are a fixed Linux UAPI, checked on every host;
 * the real prctl() syscall behavior (under CRT_TARGET_OS_LINUX) carries
 * the same unverified-pending-real-Linux-hardware caveat as this
 * session's other raw Linux syscall trampolines (eventfd/timerfd/epoll,
 * dl_iterate_phdr/dladdr). */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/prctl.h>

static int fail(const char* message) {
  fprintf(stderr, "prctl_test: %s\n", message);
  return 1;
}

int main(void) {
  if (PR_SET_PDEATHSIG != 1 || PR_GET_PDEATHSIG != 2 || PR_GET_DUMPABLE != 3 ||
      PR_SET_DUMPABLE != 4 || PR_SET_NAME != 15 || PR_GET_NAME != 16 ||
      PR_SET_TIMERSLACK != 29 || PR_GET_TIMERSLACK != 30 ||
      PR_SET_NO_NEW_PRIVS != 38 || PR_GET_NO_NEW_PRIVS != 39) {
    return fail("PR_* constants");
  }
  if (CRT_PR_NAME_MAX != 16) {
    return fail("CRT_PR_NAME_MAX");
  }

#if defined(CRT_TARGET_OS_LINUX)
  {
    char name[CRT_PR_NAME_MAX];
    long dumpable;

    memset(name, 0, sizeof(name));
    if (prctl(PR_SET_NAME, "prctltest", 0, 0, 0) != 0) {
      return fail("PR_SET_NAME");
    }
    if (prctl(PR_GET_NAME, name, 0, 0, 0) != 0 || strcmp(name, "prctltest") != 0) {
      return fail("PR_GET_NAME");
    }

    dumpable = prctl(PR_GET_DUMPABLE, 0, 0, 0, 0);
    if (dumpable < 0) {
      return fail("PR_GET_DUMPABLE");
    }

    errno = 0;
    if (prctl(-1, 0, 0, 0, 0) != -1 || errno != EINVAL) {
      return fail("prctl invalid option");
    }
  }
#else
  errno = 0;
  if (prctl(PR_SET_NAME, "x", 0, 0, 0) != -1 || errno != ENOSYS) {
    return fail("prctl ENOSYS");
  }
#endif

  printf("prctl_test: ok\n");
  return 0;
}
