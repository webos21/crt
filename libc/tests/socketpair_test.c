/* socketpair(): AF_UNIX stream and (Linux) sequenced-packet pairs, the SOCK_CLOEXEC type flag,
 * record boundaries and MSG_TRUNC. The web surface protocol (libcrtweb/platform/
 * crtweb_surface_wire.h) needs SOCK_SEQPACKET with SCM_RIGHTS; Windows has no socketpair. */
#define _GNU_SOURCE 1
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int failures;
#define CHECK(condition, message)                                                  \
  do {                                                                             \
    if (!(condition)) {                                                            \
      fprintf(stderr, "socketpair_test: %s (line %d, errno %d)\n", message, __LINE__, errno); \
      ++failures;                                                                  \
    }                                                                              \
  } while (0)

int main(void) {
#if defined(_WIN32) || defined(__MINGW32__)
  int sv[2];
  CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == -1 && errno == ENOSYS, "Windows reports ENOSYS");
#else
  int sv[2] = {-1, -1};
  char buffer[32];

  CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0, "stream pair");
  if (sv[0] >= 0) {
    CHECK(write(sv[0], "ping", 4) == 4, "write");
    CHECK(read(sv[1], buffer, sizeof buffer) == 4 && memcmp(buffer, "ping", 4) == 0, "read");
    CHECK(write(sv[1], "pong", 4) == 4 && read(sv[0], buffer, sizeof buffer) == 4, "reverse direction");
    close(sv[0]);
    CHECK(read(sv[1], buffer, sizeof buffer) == 0, "end of stream after the peer closes");
    close(sv[1]);
  }

  CHECK(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv) == 0, "stream pair with SOCK_CLOEXEC");
  if (sv[0] >= 0) {
    CHECK((fcntl(sv[0], F_GETFD) & FD_CLOEXEC) != 0 && (fcntl(sv[1], F_GETFD) & FD_CLOEXEC) != 0,
          "SOCK_CLOEXEC reached both descriptors");
    close(sv[0]);
    close(sv[1]);
  }
  CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, NULL) == -1 && errno == EFAULT, "NULL vector is EFAULT");

#if defined(__linux__)
  CHECK(socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sv) == 0, "seqpacket pair");
  if (sv[0] >= 0) {
    char big[16];
    ssize_t got;
    struct msghdr msg;
    struct iovec iov;

    CHECK(send(sv[0], "one", 3, 0) == 3 && send(sv[0], "second record", 13, 0) == 13, "two records");
    CHECK(recv(sv[1], buffer, sizeof buffer, 0) == 3 && memcmp(buffer, "one", 3) == 0, "record boundaries: first");
    memset(&msg, 0, sizeof msg);
    iov.iov_base = big;
    iov.iov_len = 6;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    got = recvmsg(sv[1], &msg, 0);
    CHECK(got == 6 && (msg.msg_flags & MSG_TRUNC) != 0, "a short buffer truncates the record and flags MSG_TRUNC");
    CHECK(recv(sv[1], buffer, sizeof buffer, MSG_DONTWAIT) == -1 && errno == EAGAIN,
          "the rest of the truncated record is gone");
    close(sv[0]);
    close(sv[1]);
  }
#endif
#endif
  if (failures != 0) {
    return 1;
  }
  printf("socketpair_test: ok\n");
  return 0;
}
