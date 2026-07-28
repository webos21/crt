#include <errno.h>
#include <stdio.h>
#include <string.h>
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
  clock_t cpu_ticks;
  time_t now;
  time_t stored = 0;
  time_t epoch = 0;
  time_t leap_day = 951782400;
  time_t roundtrip;
  struct timeval tv;
  struct timespec realtime;
  struct timespec monotonic;
  struct timespec monotonic_after;
  struct timespec utc_time;
  struct timespec tiny_sleep;
  struct timespec invalid_sleep;
  struct tm tm_epoch;
  struct tm tm_leap;
  struct tm tm_local;
  char text[64];
  char* ctime_text;

  now = time(&stored);
  if (now <= 0 || stored != now) {
    return fail("time");
  }

  cpu_ticks = clock();
  if (cpu_ticks == (clock_t)-1) {
    return fail("clock");
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

  if (timespec_get(&utc_time, TIME_UTC) != TIME_UTC || !valid_timespec(&utc_time) ||
      timespec_get(&utc_time, 0) != 0) {
    return fail("timespec_get");
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

  if (gmtime_r(&epoch, &tm_epoch) != &tm_epoch ||
      tm_epoch.tm_year != 70 || tm_epoch.tm_mon != 0 || tm_epoch.tm_mday != 1 ||
      tm_epoch.tm_hour != 0 || tm_epoch.tm_min != 0 || tm_epoch.tm_sec != 0 ||
      tm_epoch.tm_wday != 4 || tm_epoch.tm_yday != 0 || tm_epoch.tm_isdst != 0) {
    return fail("gmtime epoch");
  }
  if (gmtime(&leap_day) == 0 ||
      gmtime_r(&leap_day, &tm_leap) != &tm_leap ||
      tm_leap.tm_year != 100 || tm_leap.tm_mon != 1 || tm_leap.tm_mday != 29 ||
      tm_leap.tm_wday != 2 || tm_leap.tm_yday != 59) {
    return fail("gmtime leap");
  }
  if (localtime_r(&epoch, &tm_local) != &tm_local ||
      tm_local.tm_year != tm_epoch.tm_year || tm_local.tm_yday != tm_epoch.tm_yday) {
    return fail("localtime");
  }
  roundtrip = mktime(&tm_leap);
  if (roundtrip != leap_day || tm_leap.tm_year != 100 ||
      tm_leap.tm_mon != 1 || tm_leap.tm_mday != 29) {
    return fail("mktime");
  }
  if (asctime_r(&tm_epoch, text) != text ||
      strcmp(text, "Thu Jan  1 00:00:00 1970\n") != 0) {
    return fail("asctime_r");
  }
  ctime_text = ctime_r(&epoch, text);
  if (ctime_text != text || strcmp(text, "Thu Jan  1 00:00:00 1970\n") != 0 ||
      ctime(&epoch) == 0) {
    return fail("ctime_r");
  }
  memset(text, 0, sizeof(text));
  if (strftime(text, sizeof(text), "%F %T %a %b %%", &tm_leap) != 29 ||
      strcmp(text, "2000-02-29 00:00:00 Tue Feb %") != 0) {
    return fail("strftime");
  }
  if (strftime(text, 4, "%F", &tm_leap) != 0) {
    return fail("strftime small");
  }

  printf("time_test: ok\n");
  return 0;
}
