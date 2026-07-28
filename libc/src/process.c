#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

long __crt_sys_getpid(void);
long __crt_sys_getppid(void);
long __crt_sys_kill(long pid, int sig);

static long normalize_syscall_result(long result) {
  if (result < 0 && result >= -4095) {
    errno = (int)-result;
    return -1;
  }
  return result;
}

pid_t getpid(void) {
  return (pid_t)normalize_syscall_result(__crt_sys_getpid());
}

pid_t getppid(void) {
  return (pid_t)normalize_syscall_result(__crt_sys_getppid());
}

int kill(pid_t pid, int sig) {
  long self;

  if (sig < 0) {
    errno = EINVAL;
    return -1;
  }
  self = __crt_sys_getpid();
  if (pid == (pid_t)self && sig != 0) {
    return raise(sig);
  }
  return (int)normalize_syscall_result(__crt_sys_kill((long)pid, sig));
}
