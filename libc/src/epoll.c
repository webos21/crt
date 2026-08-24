#include <errno.h>
#include <sys/epoll.h>

#if defined(CRT_TARGET_OS_LINUX)
long __crt_sys_epoll_create1(int flags);
long __crt_sys_epoll_ctl(int epfd, int op, int fd, struct epoll_event* event);
long __crt_sys_epoll_pwait(
    int epfd, struct epoll_event* events, int maxevents, int timeout, const void* sigmask,
    unsigned long sigsetsize);

static int normalize_syscall_result(long result) {
  if (result < 0 && result >= -4095) {
    errno = (int)-result;
    return -1;
  }
  return (int)result;
}
#endif

int epoll_create1(int flags) {
#if defined(CRT_TARGET_OS_LINUX)
  return normalize_syscall_result(__crt_sys_epoll_create1(flags));
#else
  (void)flags;
  return __set_errno(ENOSYS);
#endif
}

int epoll_create(int size) {
  /* Real glibc/Bionic semantics: the kernel has ignored `size` as anything
   * but a "must be > 0" sanity check since Linux 2.6.29 -- epoll_create1(0)
   * is documented as exactly equivalent to epoll_create(size) for any
   * valid size, so this is implemented in terms of the already-present
   * epoll_create1() rather than adding a second, separate raw syscall
   * trampoline (__NR_epoll_create) purely for a legacy entry point real
   * callers only reach as an epoll_create1-unavailable fallback anyway.
   * Added 2026-08-24: src/wayland-os.c's own wl_os_epoll_create_cloexec()
   * (Wayland core external build) unconditionally compiles a call to this
   * as its own #ifdef EPOLL_CLOEXEC/epoll_create1 fallback path -- dead
   * code on any real modern kernel (epoll_create1 never actually fails
   * with EINVAL in practice), but still has to compile. */
  if (size <= 0) {
    return __set_errno(EINVAL);
  }
  return epoll_create1(0);
}

int epoll_ctl(int epfd, int op, int fd, struct epoll_event* event) {
#if defined(CRT_TARGET_OS_LINUX)
  return normalize_syscall_result(__crt_sys_epoll_ctl(epfd, op, fd, event));
#else
  (void)epfd;
  (void)op;
  (void)fd;
  (void)event;
  return __set_errno(ENOSYS);
#endif
}

int epoll_wait(int epfd, struct epoll_event* events, int maxevents, int timeout) {
#if defined(CRT_TARGET_OS_LINUX)
  /* Implemented via epoll_pwait with a NULL signal mask, matching how
   * glibc itself implements epoll_wait() -- see sys/epoll.h's own comment:
   * there is no separate epoll_wait syscall on aarch64, only epoll_pwait,
   * so this keeps a single codepath for both architectures instead of
   * needing one raw trampoline per architecture per function. */
  return normalize_syscall_result(
      __crt_sys_epoll_pwait(epfd, events, maxevents, timeout, 0, 8));
#else
  (void)epfd;
  (void)events;
  (void)maxevents;
  (void)timeout;
  return __set_errno(ENOSYS);
#endif
}
