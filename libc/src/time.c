#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/time.h>
#include <time.h>

long __crt_sys_gettimeofday(struct timeval* tv);

#if !defined(CRT_TARGET_OS_MACOS)
long __crt_sys_nanosleep(const struct timespec* req, struct timespec* rem);
#endif

static int normalize_syscall_result(long result) {
  if (result < 0 && result >= -4095) {
    return __set_errno((int)-result);
  }
  return (int)result;
}

static int valid_timespec(const struct timespec* ts) {
  return ts != 0 && ts->tv_sec >= 0 && ts->tv_nsec >= 0 && ts->tv_nsec < 1000000000L;
}

int gettimeofday(struct timeval* tv, void* tz) {
  (void)tz;

  if (tv == 0) {
    return __set_errno(EINVAL);
  }
  return normalize_syscall_result(__crt_sys_gettimeofday(tv));
}

int clock_gettime(clockid_t clock_id, struct timespec* tp) {
  struct timeval tv;

  if (tp == 0) {
    return __set_errno(EINVAL);
  }
  if (clock_id != CLOCK_REALTIME && clock_id != CLOCK_MONOTONIC) {
    return __set_errno(EINVAL);
  }

  if (gettimeofday(&tv, 0) != 0) {
    return -1;
  }
  tp->tv_sec = tv.tv_sec;
  tp->tv_nsec = tv.tv_usec * 1000L;
  return 0;
}

time_t time(time_t* tloc) {
  struct timespec ts;

  if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
    return (time_t)-1;
  }
  if (tloc != 0) {
    *tloc = ts.tv_sec;
  }
  return ts.tv_sec;
}

int nanosleep(const struct timespec* req, struct timespec* rem) {
  if (!valid_timespec(req)) {
    return __set_errno(EINVAL);
  }

#if defined(CRT_TARGET_OS_MACOS)
  {
    struct timespec start;
    int64_t requested_ns;
    int64_t elapsed_ns = 0;

    (void)rem;
    if (clock_gettime(CLOCK_REALTIME, &start) != 0) {
      return -1;
    }
    requested_ns = req->tv_sec * INT64_C(1000000000) + req->tv_nsec;
    while (elapsed_ns < requested_ns) {
      struct timespec now;
      if (clock_gettime(CLOCK_REALTIME, &now) != 0) {
        return -1;
      }
      elapsed_ns = (now.tv_sec - start.tv_sec) * INT64_C(1000000000) + now.tv_nsec - start.tv_nsec;
    }
    return 0;
  }
#else
  return normalize_syscall_result(__crt_sys_nanosleep(req, rem));
#endif
}
