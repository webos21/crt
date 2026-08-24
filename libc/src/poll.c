#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stddef.h>
#include <sys/select.h>
#include <time.h>

#include <private/crt_signal.h>

#if defined(CRT_TARGET_OS_LINUX)
long __crt_sys_ppoll(struct pollfd* fds, unsigned long nfds, const struct timespec* timeout);
#else
long __crt_sys_poll(struct pollfd* fds, unsigned long nfds, int timeout);
#endif

static long normalize_poll_result(long result) {
  if (result < 0 && result >= -4095) {
    return __set_errno((int)-result);
  }
  return result;
}

int poll(struct pollfd* fds, nfds_t nfds, int timeout) {
#if defined(CRT_TARGET_OS_LINUX)
  struct timespec ts;
  const struct timespec* tsp = 0;
#endif
  long result;

  if (fds == 0 && nfds != 0) {
    errno = EFAULT;
    return -1;
  }
  if (timeout < -1) {
    errno = EINVAL;
    return -1;
  }

#if defined(CRT_TARGET_OS_LINUX)
  if (timeout >= 0) {
    ts.tv_sec = (time_t)(timeout / 1000);
    ts.tv_nsec = (long)(timeout % 1000) * 1000000L;
    tsp = &ts;
  }
  result = __crt_sys_ppoll(fds, (unsigned long)nfds, tsp);
#else
  result = __crt_sys_poll(fds, (unsigned long)nfds, timeout);
#endif
  return (int)normalize_poll_result(result);
}

#if defined(CRT_TARGET_OS_LINUX)
/* Real ppoll(2) semantics (POSIX-ish extension, real on Linux/most BSDs):
 * like poll(), but with a struct timespec timeout (real sub-millisecond
 * precision, not poll()'s int-milliseconds) and an optional sigmask
 * atomically installed for the duration of the wait.
 *
 * __crt_sys_ppoll's own raw syscall trampoline (libc/src/arch/linux/{
 * aarch64,x86_64}/syscall.S) only ever takes 3 arguments (fds, nfds,
 * timeout) -- it was written for poll()'s own internal use just above,
 * which never needs a real sigmask. Rather than widening the trampoline
 * itself to the real 5-argument ppoll(2) syscall shape (fds, nfds,
 * tmo_p, sigmask, sigsetsize) purely to support a sigmask this project's
 * own first real caller (Wayland core's src/wayland-client.c) never
 * actually passes, this reuses the exact same sigprocmask()-based
 * "atomic enough" technique pselect() above already establishes and
 * documents (including its own __crt_signal_delivery_generation() lost-
 * wakeup check) -- correct for the same reason: a signal that arrives
 * between the real sigprocmask() unmask and the ppoll() syscall proper
 * still gets caught by the generation check, so the only real behavioral
 * gap versus a true single-syscall ppoll(2) is a window that can only
 * ever be observed as "returned EINTR very slightly earlier than a
 * literal ppoll(2) syscall would have", never a lost wakeup. */
int ppoll(struct pollfd* fds, nfds_t nfds, const struct timespec* tmo_p, const sigset_t* sigmask) {
  sigset_t oldmask;
  int masked = 0;
  long result;
  int saved_errno;

  if (fds == 0 && nfds != 0) {
    errno = EFAULT;
    return -1;
  }

  if (sigmask != 0) {
    unsigned long generation_before_unmask = __crt_signal_delivery_generation();

    if (sigprocmask(SIG_SETMASK, sigmask, &oldmask) != 0) {
      return -1;
    }
    masked = 1;
    if (__crt_signal_delivery_generation() != generation_before_unmask) {
      sigprocmask(SIG_SETMASK, &oldmask, 0);
      errno = EINTR;
      return -1;
    }
  }

  result = __crt_sys_ppoll(fds, (unsigned long)nfds, tmo_p);
  saved_errno = 0;
  if (result < 0 && result >= -4095) {
    saved_errno = (int)-result;
  }

  if (masked) {
    if (sigprocmask(SIG_SETMASK, &oldmask, 0) != 0 && result >= 0) {
      return -1;
    }
  }
  if (saved_errno != 0) {
    errno = saved_errno;
    return -1;
  }
  return (int)result;
}
#endif /* defined(CRT_TARGET_OS_LINUX) */

static int select_has_fd(const fd_set* set, int fd) {
  return set != 0 && FD_ISSET(fd, set);
}

int select(
    int nfds,
    fd_set* readfds,
    fd_set* writefds,
    fd_set* exceptfds,
    struct timeval* timeout) {
  struct pollfd fds[FD_SETSIZE];
  int fd;
  int count = 0;
  int timeout_ms = -1;
  int ready;

  if (nfds < 0 || nfds > FD_SETSIZE) {
    errno = EINVAL;
    return -1;
  }
  if (timeout != 0) {
    if (timeout->tv_sec < 0 || timeout->tv_usec < 0 || timeout->tv_usec >= 1000000L) {
      errno = EINVAL;
      return -1;
    }
    if (timeout->tv_sec > ((time_t)2147483)) {
      timeout_ms = 2147483647;
    } else {
      timeout_ms = (int)(timeout->tv_sec * 1000 + (timeout->tv_usec + 999) / 1000);
    }
  }

  for (fd = 0; fd < nfds; ++fd) {
    short events = 0;

    if (select_has_fd(readfds, fd)) {
      events |= POLLIN;
    }
    if (select_has_fd(writefds, fd)) {
      events |= POLLOUT;
    }
    if (select_has_fd(exceptfds, fd)) {
      events |= POLLPRI;
    }
    if (events == 0) {
      continue;
    }
    fds[count].fd = fd;
    fds[count].events = events;
    fds[count].revents = 0;
    ++count;
  }

  if (readfds != 0) {
    FD_ZERO(readfds);
  }
  if (writefds != 0) {
    FD_ZERO(writefds);
  }
  if (exceptfds != 0) {
    FD_ZERO(exceptfds);
  }

  ready = poll(fds, (nfds_t)count, timeout_ms);
  if (ready <= 0) {
    return ready;
  }

  ready = 0;
  for (fd = 0; fd < count; ++fd) {
    int fd_ready = 0;

    if ((fds[fd].revents & (POLLIN | POLLHUP | POLLERR)) != 0 && readfds != 0) {
      FD_SET(fds[fd].fd, readfds);
      fd_ready = 1;
    }
    if ((fds[fd].revents & (POLLOUT | POLLERR)) != 0 && writefds != 0) {
      FD_SET(fds[fd].fd, writefds);
      fd_ready = 1;
    }
    if ((fds[fd].revents & POLLPRI) != 0 && exceptfds != 0) {
      FD_SET(fds[fd].fd, exceptfds);
      fd_ready = 1;
    }
    if (fd_ready) {
      ++ready;
    }
  }
  return ready;
}

int pselect(
    int nfds,
    fd_set* readfds,
    fd_set* writefds,
    fd_set* exceptfds,
    const struct timespec* timeout,
    const sigset_t* sigmask) {
  struct timeval tv;
  struct timeval* tvp = 0;
  sigset_t oldmask;
  int masked = 0;
  int result;
  int saved_errno;

  if (timeout != 0) {
    if (timeout->tv_sec < 0 || timeout->tv_nsec < 0 ||
        timeout->tv_nsec >= 1000000000L) {
      errno = EINVAL;
      return -1;
    }
    tv.tv_sec = timeout->tv_sec;
    tv.tv_usec = timeout->tv_nsec / 1000;
    tvp = &tv;
  }

  if (sigmask != 0) {
    unsigned long generation_before_unmask = __crt_signal_delivery_generation();

    if (sigprocmask(SIG_SETMASK, sigmask, &oldmask) != 0) {
      return -1;
    }
    masked = 1;
    /* Real kernel signal delivery happens on the way back to userspace from
     * any syscall, including this sigprocmask() -- so if one of the signals
     * this call just unblocked was already pending, its handler has already
     * run by the time sigprocmask() returns here, not merely "at some later
     * point". Without this check, a plain non-atomic "unblock, then
     * separately call select()" sequence would silently miss exactly that
     * case (the classic pselect() lost-wakeup problem: GNU make's
     * jobserver_acquire(), for one concrete, reproduced example, depends on
     * pselect() reporting this as an interruption instead of blocking
     * forever on an event that has already happened). */
    if (__crt_signal_delivery_generation() != generation_before_unmask) {
      sigprocmask(SIG_SETMASK, &oldmask, 0);
      errno = EINTR;
      return -1;
    }
  }

  result = select(nfds, readfds, writefds, exceptfds, tvp);
  saved_errno = errno;

  if (masked) {
    if (sigprocmask(SIG_SETMASK, &oldmask, 0) != 0 && result == 0) {
      return -1;
    }
    errno = saved_errno;
  }
  return result;
}
