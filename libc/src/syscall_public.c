#include <errno.h>
#include <stdarg.h>
#include <unistd.h>

/* The public, glibc/Bionic-compatible variadic syscall(2): "make this raw
 * syscall number with these arguments, however this platform doesn't have
 * a named wrapper for it". Before this, this function was a pure stub on
 * every OS (`return __set_errno(ENOSYS)` unconditionally, `number` never
 * even inspected) -- harmless for this project's OWN code, which never
 * called the generic syscall() itself (every internal caller already had
 * its own fixed-syscall-number trampoline, e.g. __crt_sys_read/
 * __crt_sys_write in libc/src/arch/linux/{aarch64,x86_64}/syscall.S), but
 * a real gap for any *external* caller -- confirmed for real: LLVM
 * libunwind's own UnwindCursor.hpp calls plain
 * `syscall(SYS_rt_sigprocmask, ...)` directly (its isReadableAddr(),
 * gated on _LIBUNWIND_CHECK_LINUX_SIGRETURN), got ENOSYS back from this
 * stub instead of the real kernel result, and its own
 * `assert(errno == EFAULT || errno == EINVAL)` right after aborted every
 * real run of crt-libcxx-smoke's shared-linkage exception test (see
 * HISTORY.md's 2026-08-21 entry for the full trace).
 *
 * Scoped to Linux only, matching CRT_LINUX_AUXV_FILE/CRT_LINUX_TERMIOS_
 * FILE/CRT_LINUX_CXA_ATEXIT_FILE in libc/CMakeLists.txt: a raw numbered
 * syscall(2) is a Linux-specific concept (this project's own crt1/libc
 * primitives never needed one for macOS/Windows either -- see the guard
 * this file used to skip entirely before this change), so those two
 * platforms keep the original unconditional ENOSYS stub unchanged below.
 *
 * __crt_generic_syscall() (libc/src/arch/linux/{aarch64,x86_64}/
 * syscall.S) is the actual SVC/`syscall`-instruction trampoline; this
 * wrapper's only job is unpacking the C variadic argument list into the
 * fixed 6-long argument list that primitive expects, matching exactly
 * what every other real libc's syscall(2) does (glibc/musl/Bionic all
 * blindly read 6 va_arg(ap, long) slots regardless of how many the
 * caller actually passed -- extra reads are harmless stack/register
 * garbage the target syscall's own argument count simply never looks
 * at). */
#if defined(CRT_TARGET_OS_LINUX)
long __crt_generic_syscall(long number, long a1, long a2, long a3, long a4, long a5, long a6);

long syscall(long number, ...) {
  va_list ap;
  long a1, a2, a3, a4, a5, a6;
  long result;

  va_start(ap, number);
  a1 = va_arg(ap, long);
  a2 = va_arg(ap, long);
  a3 = va_arg(ap, long);
  a4 = va_arg(ap, long);
  a5 = va_arg(ap, long);
  a6 = va_arg(ap, long);
  va_end(ap);

  result = __crt_generic_syscall(number, a1, a2, a3, a4, a5, a6);
  if (result < 0 && result >= -4095) {
    errno = (int)-result;
    return -1;
  }
  return result;
}
#else
long syscall(long number, ...) {
  (void)number;
  return __set_errno(ENOSYS);
}
#endif
