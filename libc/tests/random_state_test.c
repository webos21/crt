/* initstate()/setstate() sequences and the BSD timeval macros of <sys/time.h>. */
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

static int fail(const char* what) {
  fprintf(stderr, "random_state_test: %s\n", what);
  return 1;
}

int main(void) {
  char first[32];
  char second[32];
  long a[4];
  long b[4];
  char* initial;
  struct timeval x = {5, 900000};
  struct timeval y = {2, 300000};
  struct timeval r;
  int i;

  initial = initstate(7, first, sizeof(first));
  if (initial == NULL) return fail("initstate returned NULL");
  for (i = 0; i < 4; i++) a[i] = random();
  if (initstate(7, second, sizeof(second)) != first) return fail("initstate did not return the previous buffer");
  for (i = 0; i < 4; i++) b[i] = random();
  for (i = 0; i < 4; i++) {
    if (a[i] != b[i]) return fail("the same seed gave different sequences");
  }
  /* Switching back resumes the first sequence where it stopped (after four values). */
  if (setstate(first) != second) return fail("setstate did not return the previous buffer");
  {
    long next_first = random();
    initstate(7, second, sizeof(second));
    for (i = 0; i < 4; i++) (void)random();
    if (random() != next_first) return fail("setstate did not resume the sequence");
  }
  if (initstate(1, first, 4) != NULL) return fail("a 4-byte state was accepted");
  setstate(initial);

  timeradd(&x, &y, &r);
  if (r.tv_sec != 8 || r.tv_usec != 200000) return fail("timeradd carry");
  timersub(&y, &x, &r);
  if (r.tv_sec != -4 || r.tv_usec != 400000) return fail("timersub borrow");
  if (!timercmp(&y, &x, <) || timercmp(&y, &x, >) || !timerisset(&x)) return fail("timercmp/timerisset");
  timerclear(&r);
  if (timerisset(&r)) return fail("timerclear");
  printf("random_state_test: ok\n");
  return 0;
}
