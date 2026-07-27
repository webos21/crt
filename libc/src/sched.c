#include <errno.h>
#include <sched.h>

long __crt_sys_sched_yield(void);

int sched_yield(void) {
  long result = __crt_sys_sched_yield();
  if (result < 0 && result >= -4095) {
    return __set_errno((int)-result);
  }
  return (int)result;
}
