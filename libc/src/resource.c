#include <errno.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/times.h>
#include <time.h>
#include <unistd.h>

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
