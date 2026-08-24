#ifndef CRT_SYS_EPOLL_H
#define CRT_SYS_EPOLL_H

/* Linux-only in real Bionic too -- see sys/eventfd.h's own comment for why
 * this is still declared on every host, and docs/bionic_libc_gaps.md/
 * HISTORY.md's 2026-08-17 entry.
 *
 * struct epoll_event's layout is architecture-conditional in the REAL
 * Linux kernel header (include/uapi/linux/eventpoll.h), not just a glibc
 * choice: on x86_64 the kernel expects it packed to 12 bytes (no padding
 * between the 4-byte `events` and the 8-byte `data` union) for historical
 * ABI-compat reasons; on aarch64 the kernel expects the naturally-aligned
 * 16-byte layout (4 bytes of padding before `data`). Getting this wrong
 * silently corrupts every epoll_ctl()/epoll_wait() call's event data on
 * whichever architecture it's wrong for -- the exact same class of bug as
 * struct cmsghdr's Linux-vs-macOS cmsg_len width mismatch (see sys/
 * socket.h's own comment and HISTORY.md's 2026-08-16 cmsghdr-fix entry),
 * just architecture-conditional here instead of OS-conditional. The
 * static_assert-style size check below exists specifically to catch this
 * class of mistake at compile time rather than as a silent runtime data
 * corruption. */
#include <fcntl.h> /* O_CLOEXEC, reused below. */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef union epoll_data {
  void* ptr;
  int fd;
  uint32_t u32;
  uint64_t u64;
} epoll_data_t;

#if defined(__x86_64__) || defined(_M_X64)
struct epoll_event {
  uint32_t events;
  epoll_data_t data;
} __attribute__((packed));
#else
struct epoll_event {
  uint32_t events;
  epoll_data_t data;
};
#endif

#if defined(__x86_64__) || defined(_M_X64)
typedef char __crt_epoll_event_size_check[sizeof(struct epoll_event) == 12 ? 1 : -1];
#else
typedef char __crt_epoll_event_size_check[sizeof(struct epoll_event) == 16 ? 1 : -1];
#endif

#define EPOLL_CLOEXEC O_CLOEXEC

#define EPOLL_CTL_ADD 1
#define EPOLL_CTL_DEL 2
#define EPOLL_CTL_MOD 3

#define EPOLLIN 0x00000001
#define EPOLLPRI 0x00000002
#define EPOLLOUT 0x00000004
#define EPOLLERR 0x00000008
#define EPOLLHUP 0x00000010
#define EPOLLRDNORM 0x00000040
#define EPOLLRDBAND 0x00000080
#define EPOLLWRNORM 0x00000100
#define EPOLLWRBAND 0x00000200
#define EPOLLMSG 0x00000400
#define EPOLLRDHUP 0x00002000
#define EPOLLEXCLUSIVE 0x10000000
#define EPOLLWAKEUP 0x20000000
#define EPOLLONESHOT 0x40000000
#define EPOLLET 0x80000000

/* Legacy entry point, implemented in terms of epoll_create1(0) -- see
 * libc/src/epoll.c's own comment. Prefer epoll_create1() in new code. */
int epoll_create(int size);
int epoll_create1(int flags);
int epoll_ctl(int epfd, int op, int fd, struct epoll_event* event);
int epoll_wait(int epfd, struct epoll_event* events, int maxevents, int timeout);

#ifdef __cplusplus
}
#endif

#endif
