#include <errno.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/times.h>
#include <time.h>
#include <unistd.h>

#if defined(CRT_TARGET_OS_WINDOWS)
extern size_t __crt_initial_stack_size;
#endif
#if defined(CRT_TARGET_OS_MACOS)
long __crt_sys_getrusage(int who, struct rusage* usage);
#elif defined(CRT_TARGET_OS_WINDOWS)
long __crt_sys_getrusage_self(long long* user_us, long long* system_us, long* max_rss_kib, long* page_faults);
#endif

int getrlimit(int resource, struct rlimit* rlim) {
  if (rlim == 0) {
    errno = EFAULT;
    return -1;
  }
  switch (resource) {
    case RLIMIT_NOFILE:
      rlim->rlim_cur = 1024;
      rlim->rlim_max = 1024;
      return 0;
    case RLIMIT_STACK:
      rlim->rlim_cur = 8 * 1024 * 1024;
      rlim->rlim_max = RLIM_INFINITY;
#if defined(CRT_TARGET_OS_WINDOWS)
      /* The main thread's stack is what the executable reserved, not a POSIX 8 MiB. */
      if (__crt_initial_stack_size != 0) {
        rlim->rlim_cur = (rlim_t)__crt_initial_stack_size;
        rlim->rlim_max = (rlim_t)__crt_initial_stack_size;
      }
#endif
      return 0;
    case RLIMIT_CORE:
    case RLIMIT_CPU:
    case RLIMIT_DATA:
    case RLIMIT_FSIZE:
    case RLIMIT_RSS:
    case RLIMIT_AS:
      rlim->rlim_cur = RLIM_INFINITY;
      rlim->rlim_max = RLIM_INFINITY;
      return 0;
    case RLIMIT_NPROC:
      rlim->rlim_cur = 2048;
      rlim->rlim_max = 2048;
      return 0;
    case RLIMIT_MEMLOCK:
      rlim->rlim_cur = 64 * 1024;
      rlim->rlim_max = 64 * 1024;
      return 0;
    default:
      errno = EINVAL;
      return -1;
  }
}

int setrlimit(int resource, const struct rlimit* rlim) {
  struct rlimit current;

  if (rlim == 0) {
    errno = EFAULT;
    return -1;
  }
  if (getrlimit(resource, &current) != 0) {
    return -1;
  }
  if (rlim->rlim_cur <= current.rlim_max && rlim->rlim_max <= current.rlim_max) {
    return 0;
  }
  errno = ENOTSUP;
  return -1;
}

int getrusage(int who, struct rusage* usage) {
  if (usage == 0) {
    errno = EFAULT;
    return -1;
  }
  if (who != RUSAGE_SELF && who != RUSAGE_CHILDREN && who != RUSAGE_THREAD) {
    errno = EINVAL;
    return -1;
  }
  memset(usage, 0, sizeof(*usage));
#if defined(CRT_TARGET_OS_MACOS)
  /* Darwin's BSD getrusage(2): RUSAGE_SELF (0) and RUSAGE_CHILDREN (-1) are the same numbers as
   * here; Darwin has no per-thread variant, so RUSAGE_THREAD keeps reporting zeros. Its struct
   * is laid out like this one except that tv_usec is a 32-bit int followed by padding, so the
   * microsecond fields are normalized. ru_maxrss is in BYTES on Darwin (KiB on Linux/Bionic);
   * it is converted so callers see Bionic's unit. */
  if (who != RUSAGE_THREAD) {
    long result = __crt_sys_getrusage(who, usage);

    if (result < 0) {
      errno = (int)-result;
      return -1;
    }
    usage->ru_utime.tv_usec = (int)usage->ru_utime.tv_usec;
    usage->ru_stime.tv_usec = (int)usage->ru_stime.tv_usec;
    usage->ru_maxrss /= 1024;
  }
#elif defined(CRT_TARGET_OS_WINDOWS)
  /* Only the process's own usage is known (see the PAL); children and threads report zeros. */
  if (who == RUSAGE_SELF) {
    long long user_us;
    long long system_us;
    long max_rss_kib;
    long page_faults;

    __crt_sys_getrusage_self(&user_us, &system_us, &max_rss_kib, &page_faults);
    usage->ru_utime.tv_sec = (time_t)(user_us / 1000000);
    usage->ru_utime.tv_usec = (long)(user_us % 1000000);
    usage->ru_stime.tv_sec = (time_t)(system_us / 1000000);
    usage->ru_stime.tv_usec = (long)(system_us % 1000000);
    usage->ru_maxrss = max_rss_kib;
    usage->ru_minflt = page_faults;
  }
#endif
  return 0;
}

clock_t times(struct tms* buf) {
  clock_t now = clock();

  if (buf != 0) {
    memset(buf, 0, sizeof(*buf));
    buf->tms_utime = now;
  }
  return now;
}

pid_t getsid(pid_t pid) {
  (void)pid;
  return getpgrp();
}

int nice(int inc) {
  (void)inc;
  return 0;
}
