#ifndef CRT_SYS_SOCKET_H
#define CRT_SYS_SOCKET_H

#include <fcntl.h> /* O_CLOEXEC, reused by SOCK_CLOEXEC below -- same
                     * convention as sys/epoll.h's own EPOLL_CLOEXEC. */
#include <stddef.h>
#include <sys/select.h> /* fd_set/FD_ZERO/FD_SET/select() -- real-world POSIX
                          * systems commonly expose these from <sys/socket.h>
                          * too, not just <sys/select.h>; plenty of portable
                          * software assumes this (mbedTLS's net_sockets.c,
                          * curl's own public curl/multi.h). */
#include <sys/types.h>
#include <sys/uio.h> /* struct iovec, used by struct msghdr below. */

#ifdef __cplusplus
extern "C" {
#endif

typedef __crt_sa_family_t sa_family_t;

struct sockaddr {
  sa_family_t sa_family;
  char sa_data[14];
};

struct sockaddr_storage {
  sa_family_t ss_family;
  char __ss_padding[126];
} __attribute__((aligned(__alignof__(void*))));

#define AF_UNSPEC 0
#define AF_UNIX 1
#define AF_LOCAL AF_UNIX
#define AF_INET 2
#define AF_INET6 10

#define PF_UNSPEC AF_UNSPEC
#define PF_UNIX AF_UNIX
#define PF_LOCAL AF_LOCAL
#define PF_INET AF_INET
#define PF_INET6 AF_INET6

#define SOCK_STREAM 1
#define SOCK_DGRAM 2
#define SOCK_RAW 3
/* Real Linux kernel ABI fact (not just a glibc choice, matching sys/
 * epoll.h's own EPOLL_CLOEXEC precedent): the kernel deliberately reuses
 * O_CLOEXEC's own bit value for this socket()/accept4() type flag, ORed
 * into the `type` argument to atomically set close-on-exec on the new fd.
 * Added 2026-08-24 for the Wayland core external build (src/wayland-os.c's
 * own wl_os_socket_cloexec(): `wl_socket(domain, type | SOCK_CLOEXEC,
 * protocol)`). */
#define SOCK_CLOEXEC O_CLOEXEC

#define SOL_SOCKET 1
#define SO_REUSEADDR 2
#define SO_KEEPALIVE 9
#define SO_BROADCAST 6
#define SO_LINGER 13
#define SO_RCVBUF 8
#define SO_SNDBUF 7
#define SO_ERROR 4

/* SO_PEERCRED/struct ucred: real Linux UAPI values (asm-generic/socket.h),
 * added 2026-08-24 for the Wayland core external build (src/wayland-os.c's
 * own wl_os_socket_peercred(), used by a real compositor to identify which
 * process/uid/gid connected to its listening socket -- upstream's own
 * #elif defined(SO_PEERCRED)/#else #error "Don't know how to read ucred on
 * this platform" leaves no portable fallback, so this constant and struct
 * genuinely have to exist for that file to compile at all, matching this
 * project's own porting-loop policy of filling a real, confirmed CRT/PAL
 * gap rather than routing around it).
 *
 * Declared cross-platform, not Linux-only (an earlier version of this
 * guarded it to Linux only, reasoning wl_os_socket_peercred() -- a real
 * compositor's own credential check -- would never actually run on
 * Windows, since no real Windows AF_UNIX Wayland compositor exists): the
 * function still has to *compile* on every target this project's own
 * Wayland client-side source list builds for, confirmed for real
 * (2026-08-24) that src/wayland-os.c is part of that shared source list on
 * Windows too, not just Linux. The macro/struct existing does not imply
 * this project's own Windows AF_UNIX implementation actually supports
 * SO_PEERCRED at the getsockopt() level -- it does not, and nothing in
 * this project's current Windows build ever calls wl_os_socket_peercred()
 * at runtime, since it is unreachable without a real compositor listener
 * to call it against. macOS is a real, separate future gap if this ever
 * needs to compile there too: its own equivalent mechanism is
 * LOCAL_PEERCRED/struct xucred, a different SOL_LOCAL-level option, not
 * this SOL_SOCKET/SO_PEERCRED one -- not added speculatively here. */
#define SO_PEERCRED 17

/* SO_PASSCRED (real Linux UAPI value, asm-generic/socket.h): asks the
 * kernel to attach an SCM_CREDENTIALS ancillary message (see below) to
 * every AF_UNIX message this socket receives -- added 2026-09-02 for
 * libcrtmedia's own PulseAudio native-protocol audio sink backend
 * (src/arch/linux/audio_sink_linux.c), which sets it the same way the
 * real libpulse client library does (confirmed via a real strace capture
 * against a live PulseAudio server, not guessed) before sending its own
 * first control message. Linux-only in real, meaningful effect (no
 * Windows/macOS equivalent -- macOS's own peer-credential mechanism is
 * the separate SO_PEERCRED/LOCAL_PEERCRED getsockopt() path just above,
 * not an ancillary message), but declared unconditionally here, matching
 * SO_PEERCRED's own precedent immediately above: the constant existing
 * does not imply this project's own non-Linux AF_UNIX implementations
 * actually honor it at the setsockopt() level. */
#define SO_PASSCRED 16

struct ucred {
  pid_t pid;
  uid_t uid;
  gid_t gid;
};

#define SHUT_RD 0
#define SHUT_WR 1
#define SHUT_RDWR 2

#define MSG_OOB 0x01
#define MSG_PEEK 0x02
#define MSG_DONTROUTE 0x04
#define MSG_WAITALL 0x100
/* MSG_DONTWAIT/MSG_NOSIGNAL/MSG_CMSG_CLOEXEC: real Linux values (asm-
 * generic/socket.h, bits/socket.h), added 2026-08-24 for the Wayland core
 * external build's wire-protocol read/write path (src/connection.c's own
 * wl_connection_flush()/wl_connection_read(), src/wayland-os.c's own
 * wl_os_recvmsg_cloexec()). MSG_NOSIGNAL suppresses SIGPIPE on a write to
 * a peer that already closed its end; MSG_DONTWAIT makes one send()/
 * recv() call non-blocking without a separate fcntl(O_NONBLOCK) round
 * trip; MSG_CMSG_CLOEXEC atomically sets FD_CLOEXEC on SCM_RIGHTS-
 * received file descriptors (wl_os_recvmsg_cloexec() already has a
 * portable manual-fcntl fallback for hosts where the flag itself doesn't
 * apply, but still needs the macro to exist for `flags | MSG_CMSG_CLOEXEC`
 * to compile).
 *
 * Declared cross-platform, not Linux-only (an earlier version guarded all
 * three to Linux only): confirmed for real (2026-08-24) that src/
 * connection.c/src/wayland-os.c are part of the shared Wayland client
 * source list on Windows too, so these three need to at least *compile*
 * there as well. This project's own Windows socket syscalls (libc/src/
 * arch/windows/common/syscall.c) already pass the `flags` argument
 * straight through to the real winsock.send()/recv() calls with no
 * validation or translation of individual bits (the same is already true
 * of MSG_WAITALL's own value above, which does not match real Winsock's
 * own MSG_WAITALL=0x8 either -- a pre-existing, unrelated gap, not
 * something newly introduced here) -- real Winsock has no MSG_DONTWAIT/
 * MSG_NOSIGNAL/MSG_CMSG_CLOEXEC equivalent at all (Windows sockets have
 * no SIGPIPE to suppress, no per-call non-blocking mode, and no fork()
 * to protect an inherited fd from), so these three values are inert
 * placeholders on Windows -- correct to *compile* against, but not
 * claimed to change real socket behavior there. Nothing in this
 * project's current Windows build calls this code path at runtime
 * (unreachable without a real compositor to connect to). */
#define MSG_DONTWAIT 0x40
#define MSG_NOSIGNAL 0x4000
#define MSG_CMSG_CLOEXEC 0x40000000

int socket(int domain, int type, int protocol);
int bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen);
int listen(int sockfd, int backlog);
int accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen);
int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen);
ssize_t send(int sockfd, const void* buf, size_t len, int flags);
ssize_t recv(int sockfd, void* buf, size_t len, int flags);
ssize_t sendto(
    int sockfd,
    const void* buf,
    size_t len,
    int flags,
    const struct sockaddr* dest_addr,
    socklen_t addrlen);
ssize_t recvfrom(
    int sockfd,
    void* buf,
    size_t len,
    int flags,
    struct sockaddr* src_addr,
    socklen_t* addrlen);
int getsockname(int sockfd, struct sockaddr* addr, socklen_t* addrlen);
int setsockopt(int sockfd, int level, int optname, const void* optval, socklen_t optlen);
int getsockopt(int sockfd, int level, int optname, void* optval, socklen_t* optlen);
int shutdown(int sockfd, int how);

/* sendmsg()/recvmsg() and SCM_RIGHTS ancillary-data fd passing --
 * Wayland's core wire-protocol mechanism (every wl_shm buffer and the
 * initial socket handshake pass fds this way over an AF_UNIX socket), see
 * docs/bionic_libc_gaps.md and HISTORY.md's 2026-08-16 entry. Linux and
 * macOS both have full native kernel/BSD support for this; Windows has no
 * SCM_RIGHTS-equivalent mechanism for AF_UNIX sockets at all (Windows'
 * cross-process handle sharing is DuplicateHandle()-based, a completely
 * different, PID-targeted model) -- sendmsg()/recvmsg() still work for
 * plain data on Windows (gathered/scattered over the existing send()/
 * recv()), but a control message containing SCM_RIGHTS specifically fails
 * with ENOTSUP there. See __crt_sys_sendmsg()/__crt_sys_recvmsg() in
 * libc/src/arch/windows/common/syscall.c for the exact behavior. */
struct msghdr {
  void* msg_name;
  socklen_t msg_namelen;
  struct iovec* msg_iov;
  /* Real Darwin/XNU's own struct msghdr uses a 4-byte int/socklen_t for
   * these two fields, not 8-byte size_t like Linux's real ABI -- the same
   * class of Linux-vs-BSD divergence struct cmsghdr's cmsg_len already
   * documents below, and for the same reason: sendmsg()/recvmsg() (see
   * socket.c) pass this struct's bytes straight through to the raw host
   * kernel syscall field-by-field, without translating msg_iovlen/
   * msg_controllen individually the way msg_name/msg_namelen are (via
   * to_darwin_sockaddr()), so the width has to match each host's real ABI
   * exactly or the fields after msg_iovlen (msg_control/msg_controllen/
   * msg_flags) all land at the wrong byte offsets -- found via a real
   * macOS EINVAL failure on sendmsg() with an SCM_RIGHTS control message,
   * see HISTORY.md. All existing callers only ever assign/compare small
   * counts here, never anything sensitive to the field's exact width. */
#if defined(CRT_TARGET_OS_MACOS)
  int msg_iovlen;
  void* msg_control;
  socklen_t msg_controllen;
#else
  size_t msg_iovlen;
  void* msg_control;
  size_t msg_controllen;
#endif
  int msg_flags;
};

struct cmsghdr {
  /* Real Darwin/XNU's own struct cmsghdr uses a 4-byte socklen_t here
   * (X/Open XSI compliance), not an 8-byte size_t like Linux's real ABI --
   * a genuine, documented Linux-vs-BSD divergence point, not a project
   * choice. This struct's bytes get parsed directly by the real host
   * kernel through the raw sendmsg()/recvmsg() syscalls in the per-arch
   * syscall.S files under libc/src/arch/linux and libc/src/arch/macos, so
   * cmsg_len's width has to match each host's real ABI exactly or the
   * whole message layout shifts (cmsg_level/cmsg_type get misread from
   * the wrong bytes) -- found via a real macOS CI failure on the first
   * attempt, which used size_t unconditionally (correct for Linux, wrong
   * for Darwin); see HISTORY.md's 2026-08-16 entry. All the CMSG_* macros
   * below are defined in terms of sizeof(struct cmsghdr), so they adapt
   * automatically -- no other code needs to know about this. */
#if defined(CRT_TARGET_OS_MACOS)
  socklen_t cmsg_len;
#else
  size_t cmsg_len;
#endif
  int cmsg_level;
  int cmsg_type;
};

#define SCM_RIGHTS 0x01
/* Real Linux UAPI value (bits/socket.h) -- the ancillary-message type
 * SO_PASSCRED-enabled sockets attach, carrying a struct ucred (pid/uid/
 * gid) identifying the actual sender; also what a client proactively
 * sendmsg()s on its own first message to a PulseAudio server (see
 * SO_PASSCRED's own comment above for the real, confirmed source). */
#define SCM_CREDENTIALS 0x02

/* Real Darwin/XNU aligns ancillary-data records to 4 bytes
 * (__DARWIN_ALIGNBYTES32, i.e. sizeof(uint32_t) - 1), not to sizeof(size_t)
 * (8 bytes) like Linux's real ABI -- yet another instance of the same
 * Linux-vs-BSD divergence struct cmsghdr's cmsg_len and struct msghdr's
 * msg_iovlen/msg_controllen already document above (see their comments,
 * and HISTORY.md). Getting this wrong doesn't just waste 4 bytes: since
 * CMSG_DATA()/CMSG_SPACE()/CMSG_NXTHDR() are all defined in terms of this
 * alignment, an 8-byte-aligned CMSG_DATA() offset disagrees with where the
 * real kernel actually put (or expects to find) the ancillary payload,
 * which surfaced as a real macOS sendmsg() EINVAL with a SCM_RIGHTS
 * control message -- not a wasted-space cosmetic issue. */
#if defined(CRT_TARGET_OS_MACOS)
#define __CRT_CMSG_ALIGN_UNIT sizeof(unsigned int)
#else
#define __CRT_CMSG_ALIGN_UNIT sizeof(size_t)
#endif
#define CMSG_ALIGN(len) \
  (((len) + __CRT_CMSG_ALIGN_UNIT - 1) & ~(__CRT_CMSG_ALIGN_UNIT - 1))
#define CMSG_DATA(cmsg) \
  ((unsigned char*)(cmsg) + CMSG_ALIGN(sizeof(struct cmsghdr)))
#define CMSG_SPACE(len) \
  (CMSG_ALIGN(sizeof(struct cmsghdr)) + CMSG_ALIGN(len))
#define CMSG_LEN(len) (CMSG_ALIGN(sizeof(struct cmsghdr)) + (len))
#define CMSG_FIRSTHDR(msgp) \
  ((size_t)(msgp)->msg_controllen >= sizeof(struct cmsghdr) \
       ? (struct cmsghdr*)(msgp)->msg_control \
       : (struct cmsghdr*)0)

static inline struct cmsghdr* __crt_cmsg_nxthdr(
    struct msghdr* msgp, struct cmsghdr* cmsg) {
  unsigned char* next = (unsigned char*)cmsg + CMSG_ALIGN(cmsg->cmsg_len);
  unsigned char* end = (unsigned char*)msgp->msg_control + msgp->msg_controllen;

  if (cmsg->cmsg_len < sizeof(struct cmsghdr)) {
    return (struct cmsghdr*)0;
  }
  if (next + sizeof(struct cmsghdr) > end) {
    return (struct cmsghdr*)0;
  }
  if (next + CMSG_ALIGN(((struct cmsghdr*)next)->cmsg_len) > end) {
    return (struct cmsghdr*)0;
  }
  return (struct cmsghdr*)next;
}
#define CMSG_NXTHDR(msgp, cmsg) __crt_cmsg_nxthdr(msgp, cmsg)

ssize_t sendmsg(int sockfd, const struct msghdr* msg, int flags);
ssize_t recvmsg(int sockfd, struct msghdr* msg, int flags);

#ifdef __cplusplus
}
#endif

#endif
