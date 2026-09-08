#include <errno.h>
#include <sched.h>
#include <stdio.h>

static int fail(const char* message) {
  fprintf(stderr, "sched_test: %s\n", message);
  return 1;
}

int main(void) {
  errno = 0;
  if (sched_yield() != 0 || errno != 0) {
    return fail("sched_yield");
  }

  printf("sched_test: ok\n");
  return 0;
}
