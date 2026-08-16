/* sendmsg()/recvmsg() + SCM_RIGHTS fd passing -- see docs/bionic_libc_gaps.md
 * and HISTORY.md's 2026-08-16 entry.
 *
 * The plain-data gather/scatter path (part 1) runs identically on every
 * host and is fully verified by this session (a real AF_INET loopback
 * round trip through the new Windows __crt_sys_sendmsg()/__crt_sys_recvmsg()
 * implementation).
 *
 * The real SCM_RIGHTS fd-passing path (part 2) can only be meaningfully
 * exercised over AF_UNIX -- real Linux/BSD kernels reject SCM_RIGHTS
 * ancillary data on AF_INET sockets outright, it isn't just a convention.
 * Windows has no SCM_RIGHTS-equivalent mechanism for AF_UNIX sockets at
 * all, so on Windows this test instead verifies the documented ENOTSUP
 * failure. On Linux/macOS this exercises the new raw sendmsg/recvmsg
 * syscall trampolines (libc/src/arch/{linux,macos}/{x86_64,aarch64}/
 * syscall.S) for real -- those were written carefully but were NOT
 * independently verified on real hardware from the Windows-only dev
 * session that wrote them (matching the exact same gap linkat()'s own
 * trampolines had until real hardware testing closed it, see HISTORY.md).
 * This test is what closes that gap the next time it runs on real Linux/
 * macOS CI or hardware. */
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <unistd.h>

#if !defined(CRT_TARGET_OS_WINDOWS)
#include <sys/un.h>
#endif

static int fail(const char* message) {
  fprintf(stderr, "sendmsg_scm_rights_test: %s\n", message);
  return 1;
}

static int fail_errno(const char* message) {
  fprintf(stderr, "sendmsg_scm_rights_test: %s errno=%d\n", message, errno);
  return 1;
}

static int network_unavailable_ok(int fd) {
  close(fd);
  printf("sendmsg_scm_rights_test: ok (network unavailable)\n");
  return 0;
}

/* Part 1: plain multi-iovec sendmsg()/recvmsg() over AF_INET loopback --
 * exercises the gather-into-one-buffer / scatter-back-out logic, not just
 * a single-segment message. */
static int test_plain_data(void) {
  int server, client, accepted;
  int yes = 1;
  struct sockaddr_in addr;
  struct sockaddr_in bound;
  socklen_t bound_len = sizeof(bound);
  const char part_a[] = "hello, ";
  const char part_b[] = "world!";
  struct iovec send_iov[2];
  struct msghdr send_msg;
  char recv_a[8];
  char recv_b[8];
  struct iovec recv_iov[2];
  struct msghdr recv_msg;
  ssize_t n;

  server = socket(AF_INET, SOCK_STREAM, 0);
  if (server < 0) {
    return fail("server socket");
  }
  setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(0);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (bind(server, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
    if (errno == EPERM || errno == EACCES) {
      return network_unavailable_ok(server);
    }
    close(server);
    return fail_errno("bind");
  }
  if (listen(server, 1) != 0) {
    close(server);
    return fail("listen");
  }
  memset(&bound, 0, sizeof(bound));
  if (getsockname(server, (struct sockaddr*)&bound, &bound_len) != 0) {
    close(server);
    return fail("getsockname");
  }

  client = socket(AF_INET, SOCK_STREAM, 0);
  if (client < 0) {
    close(server);
    return fail("client socket");
  }
  if (connect(client, (struct sockaddr*)&bound, sizeof(bound)) != 0) {
    close(client);
    close(server);
    return fail("connect");
  }
  accepted = accept(server, (struct sockaddr*)&addr, &bound_len);
  if (accepted < 0) {
    close(client);
    close(server);
    return fail("accept");
  }

  send_iov[0].iov_base = (void*)part_a;
  send_iov[0].iov_len = sizeof(part_a) - 1;
  send_iov[1].iov_base = (void*)part_b;
  send_iov[1].iov_len = sizeof(part_b) - 1;
  memset(&send_msg, 0, sizeof(send_msg));
  send_msg.msg_iov = send_iov;
  send_msg.msg_iovlen = 2;

  n = sendmsg(client, &send_msg, 0);
  if (n != (ssize_t)(sizeof(part_a) - 1 + sizeof(part_b) - 1)) {
    close(accepted);
    close(client);
    close(server);
    return fail_errno("sendmsg");
  }

  memset(recv_a, 0, sizeof(recv_a));
  memset(recv_b, 0, sizeof(recv_b));
  recv_iov[0].iov_base = recv_a;
  recv_iov[0].iov_len = 7; /* "hello, " */
  recv_iov[1].iov_base = recv_b;
  recv_iov[1].iov_len = 6; /* "world!" */
  memset(&recv_msg, 0, sizeof(recv_msg));
  recv_msg.msg_iov = recv_iov;
  recv_msg.msg_iovlen = 2;

  n = recvmsg(accepted, &recv_msg, 0);
  close(client);
  close(accepted);
  close(server);

  if (n != 13 || memcmp(recv_a, "hello, ", 7) != 0 || memcmp(recv_b, "world!", 6) != 0) {
    return fail("recvmsg did not scatter the gathered data correctly");
  }
  return -1; /* sentinel: caller distinguishes "ran and passed" from "skipped" */
}

#if defined(CRT_TARGET_OS_WINDOWS)
/* Windows: verify the documented ENOTSUP failure -- constructing an
 * SCM_RIGHTS control message doesn't need a working AF_UNIX socket at all,
 * since __crt_sys_sendmsg()'s ancillary-data check runs before any actual
 * I/O is attempted, regardless of address family. */
static int test_scm_rights_unsupported(void) {
  int s;
  char cmsgbuf[CMSG_SPACE(sizeof(int))];
  struct cmsghdr* cmsg;
  int fd_to_send = 0; /* stdin -- never actually used, just needs to be a
                        * real fd for the ancillary-data payload. */
  struct iovec iov;
  char byte = 'x';
  struct msghdr msg;

  s = socket(AF_INET, SOCK_DGRAM, 0);
  if (s < 0) {
    return fail("socket");
  }

  memset(cmsgbuf, 0, sizeof(cmsgbuf));
  iov.iov_base = &byte;
  iov.iov_len = 1;
  memset(&msg, 0, sizeof(msg));
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = cmsgbuf;
  msg.msg_controllen = sizeof(cmsgbuf);

  cmsg = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_len = CMSG_LEN(sizeof(int));
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS;
  memcpy(CMSG_DATA(cmsg), &fd_to_send, sizeof(int));

  if (sendmsg(s, &msg, 0) >= 0 || errno != ENOTSUP) {
    close(s);
    return fail_errno("sendmsg with SCM_RIGHTS should fail with ENOTSUP on Windows");
  }
  if (recvmsg(s, &msg, 0) >= 0 || errno != ENOTSUP) {
    close(s);
    return fail_errno("recvmsg with SCM_RIGHTS control buffer should fail with ENOTSUP on Windows");
  }

  close(s);
  return 0;
}
#else
/* Linux/macOS: a real fd-passing round trip over a connected AF_UNIX
 * SOCK_STREAM pair -- the actual, intended use case (matches how Wayland/
 * D-Bus/systemd pass fds in practice). NOT independently verified from the
 * Windows-only session that wrote the underlying raw syscall trampolines;
 * this is exactly what verifies them for real. */
static int test_scm_rights_real(void) {
  int server, client, accepted;
  struct sockaddr_un addr;
  int payload_fd;
  char cmsgbuf[CMSG_SPACE(sizeof(int))];
  struct cmsghdr* cmsg;
  struct iovec send_iov;
  char send_byte = 'x';
  struct msghdr send_msg;
  struct iovec recv_iov;
  char recv_byte = 0;
  char recv_cmsgbuf[CMSG_SPACE(sizeof(int))];
  struct msghdr recv_msg;
  int received_fd = -1;
  char readback[16];
  ssize_t n;

  payload_fd = memfd_create("crt-scm-rights-test", 0);
  if (payload_fd < 0) {
    return fail_errno("memfd_create for payload");
  }
  if (write(payload_fd, "passed-over-fd", 14) != 14 || lseek(payload_fd, 0, SEEK_SET) != 0) {
    close(payload_fd);
    return fail("prepare payload fd");
  }

  server = socket(AF_UNIX, SOCK_STREAM, 0);
  if (server < 0) {
    close(payload_fd);
    return fail("AF_UNIX socket");
  }
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  snprintf(addr.sun_path, sizeof(addr.sun_path), "/tmp/crt-scm-rights-test-%d.sock", (int)getpid());
  unlink(addr.sun_path);
  if (bind(server, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
    close(server);
    close(payload_fd);
    return fail_errno("AF_UNIX bind");
  }
  if (listen(server, 1) != 0) {
    close(server);
    close(payload_fd);
    unlink(addr.sun_path);
    return fail("AF_UNIX listen");
  }

  client = socket(AF_UNIX, SOCK_STREAM, 0);
  if (client < 0 || connect(client, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
    close(server);
    close(payload_fd);
    if (client >= 0) close(client);
    unlink(addr.sun_path);
    return fail_errno("AF_UNIX connect");
  }
  accepted = accept(server, 0, 0);
  if (accepted < 0) {
    close(server);
    close(client);
    close(payload_fd);
    unlink(addr.sun_path);
    return fail("AF_UNIX accept");
  }
  unlink(addr.sun_path);

  memset(cmsgbuf, 0, sizeof(cmsgbuf));
  send_iov.iov_base = &send_byte;
  send_iov.iov_len = 1;
  memset(&send_msg, 0, sizeof(send_msg));
  send_msg.msg_iov = &send_iov;
  send_msg.msg_iovlen = 1;
  send_msg.msg_control = cmsgbuf;
  send_msg.msg_controllen = sizeof(cmsgbuf);
  cmsg = CMSG_FIRSTHDR(&send_msg);
  cmsg->cmsg_len = CMSG_LEN(sizeof(int));
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS;
  memcpy(CMSG_DATA(cmsg), &payload_fd, sizeof(int));

  n = sendmsg(client, &send_msg, 0);
  close(payload_fd);
  if (n != 1) {
    close(accepted);
    close(client);
    close(server);
    return fail_errno("sendmsg with SCM_RIGHTS");
  }

  recv_iov.iov_base = &recv_byte;
  recv_iov.iov_len = 1;
  memset(&recv_msg, 0, sizeof(recv_msg));
  recv_msg.msg_iov = &recv_iov;
  recv_msg.msg_iovlen = 1;
  recv_msg.msg_control = recv_cmsgbuf;
  recv_msg.msg_controllen = sizeof(recv_cmsgbuf);

  n = recvmsg(accepted, &recv_msg, 0);
  close(accepted);
  close(client);
  close(server);

  if (n != 1 || recv_byte != 'x') {
    return fail_errno("recvmsg data");
  }
  cmsg = CMSG_FIRSTHDR(&recv_msg);
  if (cmsg == 0 || cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS) {
    return fail("recvmsg did not deliver an SCM_RIGHTS control message");
  }
  memcpy(&received_fd, CMSG_DATA(cmsg), sizeof(int));
  if (received_fd < 0) {
    return fail("received fd is invalid");
  }

  memset(readback, 0, sizeof(readback));
  if (read(received_fd, readback, 14) != 14 || memcmp(readback, "passed-over-fd", 14) != 0) {
    close(received_fd);
    return fail("content read through the passed fd did not match");
  }
  close(received_fd);
  return 0;
}
#endif

int main(void) {
  int plain_result = test_plain_data();

  if (plain_result != -1) {
    return plain_result; /* either a real failure, or the network-
                           * unavailable early-ok already printed. */
  }

#if defined(CRT_TARGET_OS_WINDOWS)
  if (test_scm_rights_unsupported() != 0) {
    return 1;
  }
#else
  if (test_scm_rights_real() != 0) {
    return 1;
  }
#endif

  printf("sendmsg_scm_rights_test: ok\n");
  return 0;
}
