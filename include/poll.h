#ifndef CRT_POLL_H
#define CRT_POLL_H

#include <signal.h> /* sigset_t, used by ppoll() below. */
#include <sys/types.h>
#include <time.h> /* struct timespec, used by ppoll() below. */

#ifdef __cplusplus
extern "C" {
#endif

typedef __crt_nfds_t nfds_t;

struct pollfd {
  int fd;
  short events;
  short revents;
};

#define POLLIN 0x0001
#define POLLPRI 0x0002
#define POLLOUT 0x0004
#define POLLERR 0x0008
#define POLLHUP 0x0010
#define POLLNVAL 0x0020

int poll(struct pollfd* fds, nfds_t nfds, int timeout);
/* A real Linux/BSD extension (matches Bionic's own real ppoll(); glibc
 * gates this behind _GNU_SOURCE, but this project's headers have no such
 * feature-test-macro gating anywhere else) -- declared and implemented
 * cross-platform, not Linux-only, because Wayland core's own src/
 * wayland-client.c calls it unconditionally with no __linux__ guard of
 * its own, and this project's own build targets it on both Linux and
 * Windows. libc/src/poll.c's own implementation is a portable millisecond-
 * timeout wrapper around poll() (matching how sys/select.h's own
 * pselect() is already a portable wrapper around select()), not a raw
 * syscall -- see that file's own comment. */
int ppoll(struct pollfd* fds, nfds_t nfds, const struct timespec* tmo_p, const sigset_t* sigmask);

#ifdef __cplusplus
}
#endif

#endif
