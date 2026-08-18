#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

long __crt_sys_clock_gettime(clockid_t clock_id, struct timespec* tp);

#if defined(CRT_TARGET_OS_MACOS)
long __crt_sys_utimes(const char* path, const struct timeval times[2]);
long __crt_sys_futimes(int fd, const struct timeval times[2]);
#else
long __crt_sys_gettimeofday(struct timeval* tv);
#endif

#if defined(CRT_TARGET_OS_LINUX)
long __crt_sys_utimensat(int dirfd, const char* path, const struct timespec times[2], int flags);
#elif defined(CRT_TARGET_OS_WINDOWS)
long __crt_sys_utimes(const char* path, const struct timeval times[2]);
long __crt_sys_futimes(int fd, const struct timeval times[2]);
#endif

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

/* Darwin's own CLOCK_REALTIME value (matches ours; only used to call
 * clock_gettime_nsec_np() below). */
#define CRT_DARWIN_CLOCK_REALTIME 0

extern uint64_t mach_absolute_time(void);
extern int mach_timebase_info(struct crt_mach_timebase_info* info);
extern uint64_t clock_gettime_nsec_np(int clock_id);
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

static int is_leap_year(int year) {
  return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

static int days_in_year(int year) {
  return is_leap_year(year) ? 366 : 365;
}

static int days_in_month(int year, int month) {
  static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

  if (month == 1 && is_leap_year(year)) {
    return 29;
  }
  return days[month];
}

static void time_to_tm(time_t seconds, struct tm* result) {
  int64_t days = seconds / 86400;
  int64_t rem = seconds % 86400;
  int year = 1970;
  int month = 0;
  int yday = 0;

  if (rem < 0) {
    rem += 86400;
    --days;
  }

  while (days < 0) {
    --year;
    days += days_in_year(year);
  }
  while (days >= days_in_year(year)) {
    days -= days_in_year(year);
    ++year;
  }

  yday = (int)days;
  while (days >= days_in_month(year, month)) {
    days -= days_in_month(year, month);
    ++month;
  }

  result->tm_sec = (int)(rem % 60);
  result->tm_min = (int)((rem / 60) % 60);
  result->tm_hour = (int)(rem / 3600);
  result->tm_mday = (int)days + 1;
  result->tm_mon = month;
  result->tm_year = year - 1900;
  result->tm_wday = (int)((seconds / 86400 + 4) % 7);
  if (result->tm_wday < 0) {
    result->tm_wday += 7;
  }
  result->tm_yday = yday;
  result->tm_isdst = 0;
}

static time_t tm_to_time(const struct tm* tm) {
  int year = tm->tm_year + 1900;
  int month = tm->tm_mon;
  int64_t days = 0;
  int i;

  while (month < 0) {
    month += 12;
    --year;
  }
  while (month >= 12) {
    month -= 12;
    ++year;
  }

  if (year >= 1970) {
    for (i = 1970; i < year; ++i) {
      days += days_in_year(i);
    }
  } else {
    for (i = year; i < 1970; ++i) {
      days -= days_in_year(i);
    }
  }
  for (i = 0; i < month; ++i) {
    days += days_in_month(year, i);
  }
  days += tm->tm_mday - 1;

  return (time_t)(days * 86400 + (int64_t)tm->tm_hour * 3600 +
                  (int64_t)tm->tm_min * 60 + tm->tm_sec);
}

static int append_chars(char** out, size_t* remaining, const char* src, size_t len) {
  if (*remaining <= len) {
    return 0;
  }
  memcpy(*out, src, len);
  *out += len;
  *remaining -= len;
  return 1;
}

static int append_string(char** out, size_t* remaining, const char* src) {
  return append_chars(out, remaining, src, strlen(src));
}

static int append_number(char** out, size_t* remaining, int value, int width) {
  char reversed[16];
  char buffer[16];
  int negative = value < 0;
  unsigned int n = negative ? (unsigned int)(-value) : (unsigned int)value;
  int len = 0;
  int pos = 0;

  do {
    reversed[len++] = (char)('0' + n % 10U);
    n /= 10U;
  } while (n != 0U && len < (int)sizeof(reversed));

  if (negative) {
    buffer[pos++] = '-';
  }
  while (len < width && pos < (int)sizeof(buffer) - 1) {
    buffer[pos++] = '0';
    --width;
  }
  while (len > 0 && pos < (int)sizeof(buffer) - 1) {
    buffer[pos++] = reversed[--len];
  }
  buffer[pos] = '\0';
  return append_string(out, remaining, buffer);
}

int gettimeofday(struct timeval* tv, void* tz) {
  (void)tz;

  if (tv == 0) {
    return __set_errno(EINVAL);
  }
#if defined(CRT_TARGET_OS_MACOS)
  {
    /* clock_gettime_nsec_np() reads the commpage directly, like
     * mach_absolute_time() below for CLOCK_MONOTONIC. Avoid the legacy
     * mach_host_self()/host_get_clock_service()/clock_get_time() path: it
     * does a real Mach IPC round trip through a per-thread cached reply
     * port (mig_get_reply_port()), and that cache does not survive our
     * raw-syscall fork() cleanly across a nested fork (our fork() does not
     * run libSystem's internal post-fork port-cache reset), which can send
     * mig_get_reply_port()/_kernelrpc_mach_port_construct() into unbounded
     * mutual recursion and crash with a stack overflow. */
    uint64_t nsec = clock_gettime_nsec_np(CRT_DARWIN_CLOCK_REALTIME);

    tv->tv_sec = (time_t)(nsec / UINT64_C(1000000000));
    tv->tv_usec = (long)((nsec % UINT64_C(1000000000)) / 1000U);
    return 0;
  }
#else
  return normalize_syscall_result(__crt_sys_gettimeofday(tv));
#endif
}

int settimeofday(const struct timeval* tv, const void* tz) {
  (void)tv;
  (void)tz;
  return __set_errno(ENOTSUP);
}

int utimes(const char* path, const struct timeval times[2]) {
  if (path == 0) {
    return __set_errno(EINVAL);
  }
#if defined(CRT_TARGET_OS_LINUX)
  {
    struct timespec ts[2];
    const struct timespec* tsp = 0;

    if (times != 0) {
      if (times[0].tv_usec < 0 || times[0].tv_usec >= 1000000L ||
          times[1].tv_usec < 0 || times[1].tv_usec >= 1000000L) {
        return __set_errno(EINVAL);
      }
      ts[0].tv_sec = times[0].tv_sec;
      ts[0].tv_nsec = times[0].tv_usec * 1000L;
      ts[1].tv_sec = times[1].tv_sec;
      ts[1].tv_nsec = times[1].tv_usec * 1000L;
      tsp = ts;
    }
    return normalize_syscall_result(__crt_sys_utimensat(-100, path, tsp, 0));
  }
#else
  return normalize_syscall_result(__crt_sys_utimes(path, times));
#endif
}

int futimes(int fd, const struct timeval times[2]) {
#if defined(CRT_TARGET_OS_LINUX)
  struct timespec ts[2];
  const struct timespec* tsp = 0;

  if (times != 0) {
    if (times[0].tv_usec < 0 || times[0].tv_usec >= 1000000L ||
        times[1].tv_usec < 0 || times[1].tv_usec >= 1000000L) {
      return __set_errno(EINVAL);
    }
    ts[0].tv_sec = times[0].tv_sec;
    ts[0].tv_nsec = times[0].tv_usec * 1000L;
    ts[1].tv_sec = times[1].tv_sec;
    ts[1].tv_nsec = times[1].tv_usec * 1000L;
    tsp = ts;
  }
  return normalize_syscall_result(__crt_sys_utimensat(fd, "", tsp, 0x1000));
#else
  return normalize_syscall_result(__crt_sys_futimes(fd, times));
#endif
}

int futimens(int fd, const struct timespec times[2]) {
#if defined(CRT_TARGET_OS_LINUX)
  return normalize_syscall_result(__crt_sys_utimensat(fd, "", times, 0x1000));
#else
  struct timeval tv[2];
  const struct timeval* tvp = 0;

  if (times != 0) {
    if (times[0].tv_nsec < 0 || times[0].tv_nsec >= 1000000000L ||
        times[1].tv_nsec < 0 || times[1].tv_nsec >= 1000000000L) {
      return __set_errno(EINVAL);
    }
    tv[0].tv_sec = times[0].tv_sec;
    tv[0].tv_usec = (long)(times[0].tv_nsec / 1000L);
    tv[1].tv_sec = times[1].tv_sec;
    tv[1].tv_usec = (long)(times[1].tv_nsec / 1000L);
    tvp = tv;
  }
  return normalize_syscall_result(__crt_sys_futimes(fd, tvp));
#endif
}

int utimensat(int dirfd, const char* path, const struct timespec times[2], int flags) {
  if (path == 0) {
    return __set_errno(EINVAL);
  }
  if (times != 0 &&
      ((times[0].tv_nsec < 0 || times[0].tv_nsec >= 1000000000L) &&
       times[0].tv_nsec != UTIME_NOW && times[0].tv_nsec != UTIME_OMIT)) {
    return __set_errno(EINVAL);
  }
  if (times != 0 &&
      ((times[1].tv_nsec < 0 || times[1].tv_nsec >= 1000000000L) &&
       times[1].tv_nsec != UTIME_NOW && times[1].tv_nsec != UTIME_OMIT)) {
    return __set_errno(EINVAL);
  }
#if defined(CRT_TARGET_OS_LINUX)
  return normalize_syscall_result(__crt_sys_utimensat(dirfd, path, times, flags));
#else
  if (dirfd != AT_FDCWD || flags != 0) {
    return __set_errno(ENOTSUP);
  }
  if (times == 0) {
    return utimes(path, 0);
  }
  if (times[0].tv_nsec == UTIME_NOW || times[0].tv_nsec == UTIME_OMIT ||
      times[1].tv_nsec == UTIME_NOW || times[1].tv_nsec == UTIME_OMIT) {
    return __set_errno(ENOTSUP);
  }
  {
    struct timeval tv[2];

    tv[0].tv_sec = times[0].tv_sec;
    tv[0].tv_usec = times[0].tv_nsec / 1000L;
    tv[1].tv_sec = times[1].tv_sec;
    tv[1].tv_usec = times[1].tv_nsec / 1000L;
    return utimes(path, tv);
  }
#endif
}

int clock_gettime(clockid_t clock_id, struct timespec* tp) {
  if (tp == 0) {
    return __set_errno(EINVAL);
  }
  return normalize_syscall_result(__crt_sys_clock_gettime(clock_id, tp));
}

int clock_getres(clockid_t clock_id, struct timespec* resolution) {
  (void)clock_id;
  if (resolution != 0) {
    resolution->tv_sec = 0;
    resolution->tv_nsec = 1000000L;
  }
  return 0;
}

int clock_nanosleep(clockid_t clock_id, int flags, const struct timespec* req, struct timespec* rem) {
  (void)clock_id;
  if (flags != 0) {
    return ENOTSUP;
  }
  return nanosleep(req, rem) == 0 ? 0 : errno;
}

int clock_settime(clockid_t clock_id, const struct timespec* tp) {
  (void)clock_id;
  (void)tp;
  return __set_errno(ENOTSUP);
}

int timer_create(clockid_t clock_id, struct sigevent* event, timer_t* timer_ptr) {
  (void)clock_id;
  (void)event;
  (void)timer_ptr;
  return __set_errno(ENOTSUP);
}

int timer_delete(timer_t timer) {
  (void)timer;
  return __set_errno(ENOTSUP);
}

int timer_settime(timer_t timer, int flags, const struct itimerspec* new_value, struct itimerspec* old_value) {
  (void)timer;
  (void)flags;
  (void)new_value;
  (void)old_value;
  return __set_errno(ENOTSUP);
}

int timer_gettime(timer_t timer, struct itimerspec* value) {
  (void)timer;
  (void)value;
  return __set_errno(ENOTSUP);
}

int timer_getoverrun(timer_t timer) {
  (void)timer;
  return __set_errno(ENOTSUP);
}

clock_t clock(void) {
  struct timespec ts;

  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    return (clock_t)-1;
  }
  return (clock_t)(ts.tv_sec * CLOCKS_PER_SEC + ts.tv_nsec / 1000L);
}

#if defined(CRT_TARGET_OS_MACOS)
long __crt_sys_clock_gettime(clockid_t clock_id, struct timespec* tp) {
  if (clock_id == CLOCK_REALTIME) {
    struct timeval tv;
    if (gettimeofday(&tv, 0) != 0) {
      return -errno;
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

int timespec_get(struct timespec* ts, int base) {
  if (base != TIME_UTC || ts == 0) {
    return 0;
  }
  if (clock_gettime(CLOCK_REALTIME, ts) != 0) {
    return 0;
  }
  return base;
}

struct tm* gmtime_r(const time_t* timep, struct tm* result) {
  if (timep == 0 || result == 0) {
    errno = EINVAL;
    return 0;
  }
  time_to_tm(*timep, result);
  return result;
}

struct tm* gmtime(const time_t* timep) {
  static struct tm result;

  return gmtime_r(timep, &result);
}

struct tm* localtime_r(const time_t* timep, struct tm* result) {
  return gmtime_r(timep, result);
}

struct tm* localtime(const time_t* timep) {
  static struct tm result;

  return localtime_r(timep, &result);
}

char* asctime_r(const struct tm* tm, char* buf) {
  static const char* const weekdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  static const char* const months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                       "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

  if (tm == 0 || buf == 0 || tm->tm_wday < 0 || tm->tm_wday > 6 ||
      tm->tm_mon < 0 || tm->tm_mon > 11) {
    errno = EINVAL;
    return 0;
  }
  snprintf(buf, 26, "%.3s %.3s %2d %02d:%02d:%02d %d\n",
           weekdays[tm->tm_wday], months[tm->tm_mon], tm->tm_mday,
           tm->tm_hour, tm->tm_min, tm->tm_sec, tm->tm_year + 1900);
  return buf;
}

char* asctime(const struct tm* tm) {
  static char buffer[26];

  return asctime_r(tm, buffer);
}

char* ctime_r(const time_t* timep, char* buf) {
  struct tm tm;

  if (gmtime_r(timep, &tm) == 0) {
    return 0;
  }
  return asctime_r(&tm, buf);
}

char* ctime(const time_t* timep) {
  static char buffer[26];

  return ctime_r(timep, buffer);
}

time_t mktime(struct tm* tm) {
  time_t result;

  if (tm == 0) {
    errno = EINVAL;
    return (time_t)-1;
  }
  result = tm_to_time(tm);
  time_to_tm(result, tm);
  return result;
}

double difftime(time_t time1, time_t time0) {
  return (double)time1 - (double)time0;
}

size_t strftime(char* s, size_t max, const char* format, const struct tm* tm) {
  static const char* const weekdays_full[] = {"Sunday", "Monday", "Tuesday", "Wednesday",
                                              "Thursday", "Friday", "Saturday"};
  static const char* const weekdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  static const char* const months_full[] = {"January", "February", "March", "April", "May", "June",
                                            "July", "August", "September", "October", "November", "December"};
  static const char* const months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                       "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  char* out = s;
  size_t remaining = max;
  const char* p;

  if (s == 0 || format == 0 || tm == 0 || max == 0 ||
      tm->tm_wday < 0 || tm->tm_wday > 6 || tm->tm_mon < 0 || tm->tm_mon > 11) {
    return 0;
  }

  for (p = format; *p != '\0'; ++p) {
    if (*p != '%') {
      if (!append_chars(&out, &remaining, p, 1)) {
        return 0;
      }
      continue;
    }
    ++p;
    switch (*p) {
      case '\0':
        if (!append_chars(&out, &remaining, "%", 1)) {
          return 0;
        }
        --p;
        break;
      case '%':
        if (!append_chars(&out, &remaining, "%", 1)) {
          return 0;
        }
        break;
      case 'Y':
        if (!append_number(&out, &remaining, tm->tm_year + 1900, 4)) {
          return 0;
        }
        break;
      case 'm':
        if (!append_number(&out, &remaining, tm->tm_mon + 1, 2)) {
          return 0;
        }
        break;
      case 'd':
        if (!append_number(&out, &remaining, tm->tm_mday, 2)) {
          return 0;
        }
        break;
      case 'H':
        if (!append_number(&out, &remaining, tm->tm_hour, 2)) {
          return 0;
        }
        break;
      case 'M':
        if (!append_number(&out, &remaining, tm->tm_min, 2)) {
          return 0;
        }
        break;
      case 'S':
        if (!append_number(&out, &remaining, tm->tm_sec, 2)) {
          return 0;
        }
        break;
      case 'a':
        if (!append_string(&out, &remaining, weekdays[tm->tm_wday])) {
          return 0;
        }
        break;
      case 'A':
        if (!append_string(&out, &remaining, weekdays_full[tm->tm_wday])) {
          return 0;
        }
        break;
      case 'b':
      case 'h':
        if (!append_string(&out, &remaining, months[tm->tm_mon])) {
          return 0;
        }
        break;
      case 'B':
        if (!append_string(&out, &remaining, months_full[tm->tm_mon])) {
          return 0;
        }
        break;
      case 'F':
        if (!append_number(&out, &remaining, tm->tm_year + 1900, 4) ||
            !append_chars(&out, &remaining, "-", 1) ||
            !append_number(&out, &remaining, tm->tm_mon + 1, 2) ||
            !append_chars(&out, &remaining, "-", 1) ||
            !append_number(&out, &remaining, tm->tm_mday, 2)) {
          return 0;
        }
        break;
      case 'T':
        if (!append_number(&out, &remaining, tm->tm_hour, 2) ||
            !append_chars(&out, &remaining, ":", 1) ||
            !append_number(&out, &remaining, tm->tm_min, 2) ||
            !append_chars(&out, &remaining, ":", 1) ||
            !append_number(&out, &remaining, tm->tm_sec, 2)) {
          return 0;
        }
        break;
      default:
        if (!append_chars(&out, &remaining, "%", 1) ||
            !append_chars(&out, &remaining, p, 1)) {
          return 0;
        }
        break;
    }
  }
  *out = '\0';
  return (size_t)(out - s);
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

unsigned int sleep(unsigned int seconds) {
  struct timespec req;
  struct timespec rem;

  req.tv_sec = (time_t)seconds;
  req.tv_nsec = 0;
  if (nanosleep(&req, &rem) != 0) {
    return (unsigned int)rem.tv_sec;
  }
  return 0;
}

unsigned int alarm(unsigned int seconds) {
  (void)seconds;
  return 0;
}

void tzset(void) {
}

char* strptime(const char* buf, const char* format, struct tm* tm) {
  (void)tm;
  if (buf == 0 || format == 0) {
    return 0;
  }
  if (format[0] == 0) {
    return (char*)buf;
  }
  return 0;
}
