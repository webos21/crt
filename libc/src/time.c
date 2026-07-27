#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/time.h>
#include <time.h>

long __crt_sys_gettimeofday(struct timeval* tv);
long __crt_sys_clock_gettime(clockid_t clock_id, struct timespec* tp);

#if !defined(CRT_TARGET_OS_MACOS)
long __crt_sys_nanosleep(const struct timespec* req, struct timespec* rem);
#else
long __crt_sys_sleep_ms(unsigned long milliseconds);
#endif

#if defined(CRT_TARGET_OS_MACOS)
struct crt_mach_timebase_info {
  uint32_t numer;
  uint32_t denom;
};

extern uint64_t mach_absolute_time(void);
extern int mach_timebase_info(struct crt_mach_timebase_info* info);
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
  if (tp == 0) {
    return __set_errno(EINVAL);
  }
  return normalize_syscall_result(__crt_sys_clock_gettime(clock_id, tp));
}

#if defined(CRT_TARGET_OS_MACOS)
long __crt_sys_clock_gettime(clockid_t clock_id, struct timespec* tp) {
  if (clock_id == CLOCK_REALTIME) {
    struct timeval tv;
    long result = __crt_sys_gettimeofday(&tv);
    if (result != 0) {
      return result;
    }
    tp->tv_sec = tv.tv_sec;
    tp->tv_nsec = tv.tv_usec * 1000L;
    return 0;
  }

  if (clock_id == CLOCK_MONOTONIC) {
    static struct crt_mach_timebase_info timebase;
    uint64_t ticks;
    uint64_t quotient;
    uint64_t remainder;
    uint64_t total_nsec;

    if (timebase.denom == 0 && mach_timebase_info(&timebase) != 0) {
      return -EIO;
    }

    ticks = mach_absolute_time();
    quotient = ticks / timebase.denom;
    remainder = ticks % timebase.denom;
    total_nsec = quotient * timebase.numer + (remainder * timebase.numer) / timebase.denom;

    tp->tv_sec = (time_t)(total_nsec / UINT64_C(1000000000));
    tp->tv_nsec = (long)(total_nsec % UINT64_C(1000000000));
    return 0;
  }

  return -EINVAL;
}
#endif

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
    uint64_t milliseconds;

    (void)rem;
    if (req->tv_sec > (time_t)(UINT64_MAX / 1000U)) {
      return __set_errno(EINVAL);
    }
    milliseconds = (uint64_t)req->tv_sec * 1000U + ((uint64_t)req->tv_nsec + 999999U) / 1000000U;
    if (milliseconds > 0x7fffffffU) {
      return __set_errno(EINVAL);
    }
    return normalize_syscall_result(__crt_sys_sleep_ms((unsigned long)milliseconds));
  }
#else
  return normalize_syscall_result(__crt_sys_nanosleep(req, rem));
#endif
}
