#include <errno.h>
#include <sched.h>
#include <sys/syscall.h>
#include <unistd.h>

long __crt_sys_sched_yield(void);

int sched_yield(void) {
  long result = __crt_sys_sched_yield();
  if (result < 0 && result >= -4095) {
    return __set_errno((int)-result);
  }
  return (int)result;
}

#if defined(CRT_TARGET_OS_LINUX)
static int sched_syscall_result(long result) {
  if (result < 0) {
    return -1;
  }
  return (int)result;
}

int sched_get_priority_min(int policy) {
  return sched_syscall_result(syscall(SYS_sched_get_priority_min, (long)policy));
}

int sched_get_priority_max(int policy) {
  return sched_syscall_result(syscall(SYS_sched_get_priority_max, (long)policy));
}

int sched_getscheduler(pid_t pid) {
  return sched_syscall_result(syscall(SYS_sched_getscheduler, (long)pid));
}

int sched_setscheduler(pid_t pid, int policy, const struct sched_param* param) {
  return sched_syscall_result(syscall(SYS_sched_setscheduler, (long)pid, (long)policy, (long)param));
}

int sched_getparam(pid_t pid, struct sched_param* param) {
  return sched_syscall_result(syscall(SYS_sched_getparam, (long)pid, (long)param));
}

int sched_setparam(pid_t pid, const struct sched_param* param) {
  return sched_syscall_result(syscall(SYS_sched_setparam, (long)pid, (long)param));
}
#else
/* macOS and Windows: only SCHED_OTHER, with the single priority 0. */
int sched_get_priority_min(int policy) {
  if (policy != SCHED_OTHER) {
    return __set_errno(EINVAL);
  }
  return 0;
}

int sched_get_priority_max(int policy) {
  return sched_get_priority_min(policy);
}

int sched_getscheduler(pid_t pid) {
  (void)pid;
  return SCHED_OTHER;
}

int sched_setscheduler(pid_t pid, int policy, const struct sched_param* param) {
  (void)pid;
  (void)param;
  if (policy != SCHED_OTHER) {
    return __set_errno(ENOTSUP);
  }
  return 0;
}

int sched_getparam(pid_t pid, struct sched_param* param) {
  (void)pid;
  if (param == 0) {
    return __set_errno(EINVAL);
  }
  param->sched_priority = 0;
  return 0;
}

int sched_setparam(pid_t pid, const struct sched_param* param) {
  (void)pid;
  if (param == 0) {
    return __set_errno(EINVAL);
  }
  return param->sched_priority == 0 ? 0 : __set_errno(EINVAL);
}
#endif
