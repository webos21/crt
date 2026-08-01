#include <errno.h>
#include <stdarg.h>
#include <unistd.h>

long syscall(long number, ...) {
  va_list ap;

  (void)number;
  va_start(ap, number);
  va_end(ap);
  return __set_errno(ENOSYS);
}
