#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stddef.h>
#include <sys/select.h>
#include <time.h>

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
    if (sigprocmask(SIG_SETMASK, sigmask, &oldmask) != 0) {
      return -1;
    }
    masked = 1;
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
