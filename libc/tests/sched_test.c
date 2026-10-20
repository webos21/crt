#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <sys/resource.h>

static int fail(const char* message) {
  fprintf(stderr, "sched_test: %s\n", message);
  return 1;
}

int main(void) {
  errno = 0;
  if (sched_yield() != 0 || errno != 0) {
    return fail("sched_yield");
  }

  /* getpriority/setpriority: the calling process always accepts its current nice value. */
  errno = 0;
  int nice_value = getpriority(PRIO_PROCESS, 0);
  if (nice_value == -1 && errno != 0) {
    return fail("getpriority");
  }
  if (setpriority(PRIO_PROCESS, 0, nice_value) != 0) {
    return fail("setpriority(current)");
  }
  if (getpriority(99, 0) != -1 || errno != EINVAL) {
    return fail("getpriority(invalid which)");
  }
  if (setpriority(99, 0, 0) != -1 || errno != EINVAL) {
    return fail("setpriority(invalid which)");
  }

  printf("sched_test: ok\n");
  return 0;
}
