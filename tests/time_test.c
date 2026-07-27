#include <errno.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

static int fail(const char* message) {
  fprintf(stderr, "time_test: %s\n", message);
  return 1;
}

static int valid_timespec(const struct timespec* ts) {
  return ts->tv_sec > 0 && ts->tv_nsec >= 0 && ts->tv_nsec < 1000000000L;
}

static int timespec_less(const struct timespec* lhs, const struct timespec* rhs) {
  return lhs->tv_sec < rhs->tv_sec || (lhs->tv_sec == rhs->tv_sec && lhs->tv_nsec < rhs->tv_nsec);
}

int main(void) {
  time_t now;
  time_t stored = 0;
  struct timeval tv;
  struct timespec realtime;
  struct timespec monotonic;
  struct timespec monotonic_after;
  struct timespec tiny_sleep;
  struct timespec invalid_sleep;

  now = time(&stored);
  if (now <= 0 || stored != now) {
    return fail("time");
  }

  if (gettimeofday(&tv, 0) != 0 || tv.tv_sec <= 0 || tv.tv_usec < 0 || tv.tv_usec >= 1000000L) {
    return fail("gettimeofday");
  }

  if (clock_gettime(CLOCK_REALTIME, &realtime) != 0 || !valid_timespec(&realtime)) {
    return fail("clock_gettime realtime");
  }

  if (clock_gettime(CLOCK_MONOTONIC, &monotonic) != 0 || !valid_timespec(&monotonic)) {
    return fail("clock_gettime monotonic");
  }

  errno = 0;
  if (clock_gettime(-1, &realtime) != -1 || errno != EINVAL) {
    return fail("clock_gettime invalid id");
  }

  tiny_sleep.tv_sec = 0;
  tiny_sleep.tv_nsec = 1000000L;
  if (nanosleep(&tiny_sleep, 0) != 0) {
    return fail("nanosleep tiny");
  }
  if (clock_gettime(CLOCK_MONOTONIC, &monotonic_after) != 0 ||
      timespec_less(&monotonic_after, &monotonic)) {
    return fail("clock_gettime monotonic progression");
  }

  invalid_sleep.tv_sec = 0;
  invalid_sleep.tv_nsec = 1000000000L;
  errno = 0;
  if (nanosleep(&invalid_sleep, 0) != -1 || errno != EINVAL) {
    return fail("nanosleep invalid");
  }

  printf("time_test: ok\n");
  return 0;
}
