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
#if defined(CRT_TARGET_OS_LINUX)
/* Real Linux-only syscall-backed API (matches Bionic's own real ppoll();
 * glibc gates this behind _GNU_SOURCE, but this project's headers have no
 * such feature-test-macro gating anywhere else). Declaration guarded to
 * match libc/src/poll.c's own implementation, which is genuinely Linux-
 * only there too (built on the real ppoll(2) syscall's own sub-
 * millisecond timeout precision, not emulated on macOS/Windows the way
 * poll() itself is) -- an unguarded declaration with no definition on
 * those platforms would silently create a link-time-only gap instead of
 * a clear compile-time one. Added 2026-08-24 for the Wayland core
 * external build (src/wayland-client.c's own dispatch loop calls it
 * directly). */
int ppoll(struct pollfd* fds, nfds_t nfds, const struct timespec* tmo_p, const sigset_t* sigmask);
#endif /* defined(CRT_TARGET_OS_LINUX) */

#ifdef __cplusplus
}
#endif

#endif
