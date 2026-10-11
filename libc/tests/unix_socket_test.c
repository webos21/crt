/* AF_UNIX through the CRT libc surface, on every host that has it.
 *
 * Gate A: SOCK_STREAM -- bind/listen/connect/accept on a path, getsockname/getpeername, half-close and EOF,
 * address reuse and cleanup, non-blocking reads, poll(), socketpair().
 * Gate B: SOCK_SEQPACKET -- record boundaries, truncation and MSG_TRUNC, zero-length records, SO_TYPE, and
 * descriptor passing with SCM_RIGHTS: a file, a memfd mapping, a connected TCP socket, several at once,
 * MSG_CMSG_CLOEXEC, a control buffer that is too small, and the same between two processes.
 *
 * On Windows the stream part is Winsock's native AF_UNIX and everything else is the CRT's own layer
 * (libc/src/arch/windows/common/syscall.c, "AF_UNIX on Windows"); the test is the same program everywhere. */
#define _GNU_SOURCE 1
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

static int failures;

#define CHECK(condition, message)                                                                    \
  do {                                                                                               \
    if (!(condition)) {                                                                              \
      fprintf(stderr, "unix_socket_test: %s (line %d, errno %d)\n", message, __LINE__, errno);       \
      ++failures;                                                                                    \
    }                                                                                                \
  } while (0)

static char socket_path[256];

static void make_path(const char* tag) {
  const char* base = getenv("TMPDIR");

  if (base == 0) {
    base = getenv("TEMP");
  }
  if (base == 0) {
    base = ".";
  }
  snprintf(socket_path, sizeof(socket_path), "%s/crt_unix_%s_%d.sock", base, tag, (int)getpid());
  unlink(socket_path);
}

static socklen_t fill_address(struct sockaddr_un* address) {
  memset(address, 0, sizeof(*address));
  address->sun_family = AF_UNIX;
  strncpy(address->sun_path, socket_path, sizeof(address->sun_path) - 1);
  return (socklen_t)sizeof(*address);
}

static int listen_on(int type) {
  struct sockaddr_un address;
  socklen_t length = fill_address(&address);
  int server = socket(AF_UNIX, type, 0);

  if (server < 0) {
    return -1;
  }
  if (bind(server, (const struct sockaddr*)&address, length) != 0 || listen(server, 4) != 0) {
    int saved = errno;

    close(server);
    errno = saved;
    return -1;
  }
  return server;
}

static int connect_to(int type) {
  struct sockaddr_un address;
  socklen_t length = fill_address(&address);
  int client = socket(AF_UNIX, type, 0);

  if (client < 0) {
    return -1;
  }
  if (connect(client, (const struct sockaddr*)&address, length) != 0) {
    int saved = errno;

    close(client);
    errno = saved;
    return -1;
  }
  return client;
}

/* ---- Gate A ------------------------------------------------------------------------------------ */

static void test_stream_path(void) {
  int server;
  int client;
  int accepted;
  char buffer[32];
  struct sockaddr_un name;
  socklen_t name_len = sizeof(name);
  struct sockaddr_un again;
  socklen_t again_len;

  make_path("stream");
  server = listen_on(SOCK_STREAM);
  CHECK(server >= 0, "bind/listen on a path");
  if (server < 0) {
    return;
  }
  client = connect_to(SOCK_STREAM);
  CHECK(client >= 0, "connect to the listener");
  accepted = accept(server, 0, 0);
  CHECK(accepted >= 0, "accept");
  if (client < 0 || accepted < 0) {
    close(server);
    unlink(socket_path);
    return;
  }

  CHECK(getsockname(server, (struct sockaddr*)&name, &name_len) == 0 && name.sun_family == AF_UNIX,
        "getsockname of the listener is AF_UNIX");
  again_len = sizeof(again);
  CHECK(getpeername(client, (struct sockaddr*)&again, &again_len) == 0 && again.sun_family == AF_UNIX,
        "getpeername of the client is AF_UNIX");

  CHECK(write(client, "ping", 4) == 4 && read(accepted, buffer, sizeof(buffer)) == 4 && memcmp(buffer, "ping", 4) == 0,
        "client to server");
  CHECK(write(accepted, "pong!", 5) == 5 && read(client, buffer, sizeof(buffer)) == 5 && memcmp(buffer, "pong!", 5) == 0,
        "server to client");

  CHECK(shutdown(client, SHUT_WR) == 0, "half-close");
  CHECK(read(accepted, buffer, sizeof(buffer)) == 0, "the peer sees end of stream after SHUT_WR");
  CHECK(write(accepted, "tail", 4) == 4 && read(client, buffer, sizeof(buffer)) == 4, "the other direction still works");
  close(client);
  close(accepted);

  {
    struct sockaddr_un address;
    socklen_t length = fill_address(&address);
    int again_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    CHECK(bind(again_fd, (const struct sockaddr*)&address, length) == -1 && errno == EADDRINUSE,
          "a second bind to the same path is EADDRINUSE");
    close(again_fd);
  }
  close(server);
  CHECK(unlink(socket_path) == 0, "the socket file can be unlinked");
  CHECK(connect_to(SOCK_STREAM) == -1 && (errno == ENOENT || errno == ECONNREFUSED),
        "connect to a path that is gone fails");
}

static void test_stream_nonblocking_and_poll(void) {
  int sv[2];
  char byte = 'x';
  char buffer[4];
  struct pollfd pfd;

  CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0, "socketpair(AF_UNIX, SOCK_STREAM)");
  if (sv[0] < 0 || sv[1] < 0) {
    return;
  }
  CHECK(fcntl(sv[1], F_SETFL, O_NONBLOCK) == 0, "O_NONBLOCK");
  CHECK(read(sv[1], buffer, sizeof(buffer)) == -1 && (errno == EAGAIN || errno == EWOULDBLOCK),
        "a non-blocking read on an empty socket is EAGAIN");
  pfd.fd = sv[1];
  pfd.events = POLLIN;
  pfd.revents = 0;
  CHECK(poll(&pfd, 1, 0) == 0, "poll reports nothing on an empty socket");
  CHECK(write(sv[0], &byte, 1) == 1, "write");
  pfd.revents = 0;
  CHECK(poll(&pfd, 1, 2000) == 1 && (pfd.revents & POLLIN) != 0, "poll reports POLLIN after a write");
  CHECK(read(sv[1], buffer, sizeof(buffer)) == 1 && buffer[0] == 'x', "read after poll");
  close(sv[0]);
  pfd.revents = 0;
  CHECK(poll(&pfd, 1, 2000) == 1 && (pfd.revents & (POLLIN | POLLHUP)) != 0, "poll wakes when the peer closes");
  CHECK(read(sv[1], buffer, sizeof(buffer)) == 0, "end of stream");
  close(sv[1]);
}

static void test_native_peer_sees_raw_bytes(void) {
  /* A stream socket is never framed: a plain stream socket pair carries exactly the written bytes. */
  int sv[2];
  char buffer[16];

  CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0, "stream pair");
  if (sv[0] < 0) {
    return;
  }
  CHECK(send(sv[0], "ab", 2, 0) == 2 && send(sv[0], "cde", 3, 0) == 3, "two sends");
  usleep(20000);
  CHECK(recv(sv[1], buffer, sizeof(buffer), 0) == 5 && memcmp(buffer, "abcde", 5) == 0,
        "a stream socket merges writes (no record boundaries)");
  close(sv[0]);
  close(sv[1]);
}

/* ---- Gate B ------------------------------------------------------------------------------------ */

static int seqpacket_available(void) {
  int fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);

  if (fd < 0) {
    return 0;
  }
  close(fd);
  return 1;
}

static void test_seqpacket_records(void) {
  int sv[2];
  char buffer[64];
  char small[6];
  struct msghdr msg;
  struct iovec iov;
  int type = 0;
  socklen_t type_len = sizeof(type);
  ssize_t got;

  CHECK(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sv) == 0, "socketpair(AF_UNIX, SOCK_SEQPACKET)");
  if (sv[0] < 0) {
    return;
  }
  CHECK(getsockopt(sv[0], SOL_SOCKET, SO_TYPE, &type, &type_len) == 0 && type == SOCK_SEQPACKET,
        "SO_TYPE is SOCK_SEQPACKET");

  CHECK(send(sv[0], "one", 3, 0) == 3 && send(sv[0], "second record", 13, 0) == 13 && send(sv[0], "", 0, 0) == 0,
        "three records, one of them empty");
  CHECK(recv(sv[1], buffer, sizeof(buffer), 0) == 3 && memcmp(buffer, "one", 3) == 0, "first record alone");
  CHECK(recv(sv[1], buffer, sizeof(buffer), 0) == 13 && memcmp(buffer, "second record", 13) == 0, "second record");
  CHECK(recv(sv[1], buffer, sizeof(buffer), 0) == 0, "the empty record is a zero-length read, not end of stream");

  CHECK(send(sv[0], "0123456789abcdef", 16, 0) == 16, "a record longer than the receive buffer");
  memset(&msg, 0, sizeof(msg));
  iov.iov_base = small;
  iov.iov_len = sizeof(small);
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  got = recvmsg(sv[1], &msg, 0);
  CHECK(got == (ssize_t)sizeof(small) && (msg.msg_flags & MSG_TRUNC) != 0 && memcmp(small, "012345", 6) == 0,
        "a short buffer truncates the record and sets MSG_TRUNC");
  CHECK(recv(sv[1], buffer, sizeof(buffer), MSG_DONTWAIT) == -1 && errno == EAGAIN,
        "the rest of a truncated record is discarded");
  CHECK(send(sv[0], "0123456789abcdef", 16, 0) == 16 && recv(sv[1], small, sizeof(small), MSG_TRUNC) == 16,
        "MSG_TRUNC in flags returns the real length");

  {
    struct iovec parts[2];
    char a[3];
    char b[8];

    CHECK(send(sv[0], "scatter-gather", 14, 0) == 14, "one record for two buffers");
    parts[0].iov_base = a;
    parts[0].iov_len = sizeof(a);
    parts[1].iov_base = b;
    parts[1].iov_len = sizeof(b);
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = parts;
    msg.msg_iovlen = 2;
    got = recvmsg(sv[1], &msg, 0);
    CHECK(got == 11 && (msg.msg_flags & MSG_TRUNC) != 0 && memcmp(a, "sca", 3) == 0 && memcmp(b, "tter-gat", 8) == 0,
          "recvmsg scatters one record over two buffers");
  }

  CHECK(fcntl(sv[1], F_SETFL, O_NONBLOCK) == 0, "O_NONBLOCK");
  CHECK(recv(sv[1], buffer, sizeof(buffer), 0) == -1 && errno == EAGAIN, "empty non-blocking receive is EAGAIN");

  close(sv[0]);
  CHECK(recv(sv[1], buffer, sizeof(buffer), 0) == 0, "end of stream after the peer closes");
  close(sv[1]);
}

static void test_seqpacket_path(void) {
  int server;
  int client;
  int accepted;
  char buffer[16];
  int type = 0;
  socklen_t type_len = sizeof(type);

  make_path("seq");
  server = listen_on(SOCK_SEQPACKET);
  CHECK(server >= 0, "bind/listen on a SOCK_SEQPACKET path");
  if (server < 0) {
    return;
  }
  client = connect_to(SOCK_SEQPACKET);
  CHECK(client >= 0, "connect");
  accepted = accept(server, 0, 0);
  CHECK(accepted >= 0, "accept");
  if (client >= 0 && accepted >= 0) {
    CHECK(getsockopt(accepted, SOL_SOCKET, SO_TYPE, &type, &type_len) == 0 && type == SOCK_SEQPACKET,
          "an accepted socket is SOCK_SEQPACKET too");
    CHECK(send(client, "hello", 5, 0) == 5 && send(client, "world", 5, 0) == 5, "two records");
    CHECK(recv(accepted, buffer, sizeof(buffer), 0) == 5 && memcmp(buffer, "hello", 5) == 0, "first");
    CHECK(recv(accepted, buffer, sizeof(buffer), 0) == 5 && memcmp(buffer, "world", 5) == 0, "second");
    CHECK(send(accepted, "back", 4, 0) == 4 && recv(client, buffer, sizeof(buffer), 0) == 4, "reverse direction");
  }
  if (client >= 0) close(client);
  if (accepted >= 0) close(accepted);
  close(server);
  unlink(socket_path);
}

static ssize_t send_fds(int sock, const void* data, size_t length, const int* fds, int count) {
  char control[CMSG_SPACE(sizeof(int) * 8)];
  struct msghdr msg;
  struct iovec iov;
  struct cmsghdr* cmsg;

  memset(&msg, 0, sizeof(msg));
  memset(control, 0, sizeof(control));
  iov.iov_base = (void*)data;
  iov.iov_len = length;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = control;
  msg.msg_controllen = CMSG_SPACE(sizeof(int) * (size_t)count);
  cmsg = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS;
  cmsg->cmsg_len = CMSG_LEN(sizeof(int) * (size_t)count);
  memcpy(CMSG_DATA(cmsg), fds, sizeof(int) * (size_t)count);
  return sendmsg(sock, &msg, 0);
}

static ssize_t recv_fds(int sock, void* data, size_t length, int* fds, int capacity, int* received, int flags,
                        int* msg_flags) {
  char control[CMSG_SPACE(sizeof(int) * 8)];
  struct msghdr msg;
  struct iovec iov;
  struct cmsghdr* cmsg;
  ssize_t got;

  memset(&msg, 0, sizeof(msg));
  memset(control, 0, sizeof(control));
  iov.iov_base = data;
  iov.iov_len = length;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = control;
  msg.msg_controllen = CMSG_SPACE(sizeof(int) * (size_t)capacity);
  *received = 0;
  got = recvmsg(sock, &msg, flags);
  if (got < 0) {
    return got;
  }
  if (msg_flags != 0) {
    *msg_flags = msg.msg_flags;
  }
  for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != 0; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
    if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
      int count = (int)((cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int));

      memcpy(fds + *received, CMSG_DATA(cmsg), sizeof(int) * (size_t)count);
      *received += count;
    }
  }
  return got;
}

static int make_tcp_pair(int* a, int* b) {
  struct sockaddr_in address;
  struct sockaddr_in bound;
  socklen_t bound_len = sizeof(bound);
  int listener = socket(AF_INET, SOCK_STREAM, 0);

  if (listener < 0) {
    return -1;
  }
  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (bind(listener, (const struct sockaddr*)&address, sizeof(address)) != 0 || listen(listener, 1) != 0 ||
      getsockname(listener, (struct sockaddr*)&bound, &bound_len) != 0) {
    close(listener);
    return -1;
  }
  *a = socket(AF_INET, SOCK_STREAM, 0);
  if (*a < 0 || connect(*a, (const struct sockaddr*)&bound, sizeof(bound)) != 0) {
    close(listener);
    return -1;
  }
  *b = accept(listener, 0, 0);
  close(listener);
  return *b >= 0 ? 0 : -1;
}

static void test_fd_passing_in_process(void) {
  int sv[2];
  int file_fd;
  int mem_fd;
  int tcp_a = -1;
  int tcp_b = -1;
  int sent[3];
  int got[8];
  int count = 0;
  char data[16];
  char file_path[256];
  char* map;
  char readback[16];
  const char* base = getenv("TMPDIR") != 0 ? getenv("TMPDIR") : (getenv("TEMP") != 0 ? getenv("TEMP") : ".");

  CHECK(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sv) == 0, "pair for descriptor passing");
  if (sv[0] < 0) {
    return;
  }
  snprintf(file_path, sizeof(file_path), "%s/crt_unix_file_%d.txt", base, (int)getpid());
  file_fd = open(file_path, O_RDWR | O_CREAT | O_TRUNC, 0600);
  CHECK(file_fd >= 0, "open a file");
  mem_fd = memfd_create("crt-unix-test", 0);
  CHECK(mem_fd >= 0, "memfd_create");
  CHECK(make_tcp_pair(&tcp_a, &tcp_b) == 0, "a connected TCP pair");
  if (file_fd < 0 || mem_fd < 0 || tcp_a < 0) {
    close(sv[0]);
    close(sv[1]);
    return;
  }

  /* One file: the received descriptor is a new fd for the same open object. */
  CHECK(write(file_fd, "from the sender", 15) == 15, "write to the file");
  sent[0] = file_fd;
  CHECK(send_fds(sv[0], "F", 1, sent, 1) == 1, "send a file descriptor");
  CHECK(recv_fds(sv[1], data, sizeof(data), got, 8, &count, 0, 0) == 1 && data[0] == 'F' && count == 1 && got[0] >= 0 &&
            got[0] != file_fd,
        "the receiver gets a new descriptor");
  if (count == 1) {
    CHECK(lseek(got[0], 0, SEEK_SET) == 0 && read(got[0], readback, sizeof(readback)) == 15 &&
              memcmp(readback, "from the sender", 15) == 0,
          "the received file shows the sender's data");
    CHECK(write(got[0], "!", 1) == 1 && lseek(file_fd, 0, SEEK_SET) == 0 && read(file_fd, readback, 16) == 16 &&
              readback[15] == '!',
          "a write through the received fd is visible through the original");
    close(got[0]);
  }

  /* A memfd mapping shared between the two descriptors. */
  CHECK(ftruncate(mem_fd, 4096) == 0, "size the memfd");
  map = (char*)mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, 0);
  CHECK(map != MAP_FAILED, "map the memfd");
  if (map != MAP_FAILED) {
    strcpy(map, "shared page");
    sent[0] = mem_fd;
    CHECK(send_fds(sv[0], "M", 1, sent, 1) == 1, "send the memfd");
    CHECK(recv_fds(sv[1], data, sizeof(data), got, 8, &count, 0, 0) == 1 && count == 1, "receive the memfd");
    if (count == 1) {
      char* other = (char*)mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, got[0], 0);

      CHECK(other != MAP_FAILED && strcmp(other, "shared page") == 0, "the receiver maps the same memory");
      if (other != MAP_FAILED) {
        strcpy(other, "changed");
        CHECK(strcmp(map, "changed") == 0, "a write through one mapping shows in the other");
        munmap(other, 4096);
      }
      close(got[0]);
    }
    munmap(map, 4096);
  }

  /* A connected socket: both descriptors drive the same connection. */
  sent[0] = tcp_a;
  CHECK(send_fds(sv[0], "S", 1, sent, 1) == 1, "send a TCP socket");
  CHECK(recv_fds(sv[1], data, sizeof(data), got, 8, &count, 0, 0) == 1 && count == 1, "receive the socket");
  if (count == 1) {
    CHECK(send(got[0], "via received", 12, 0) == 12 && recv(tcp_b, readback, sizeof(readback), 0) == 12 &&
              memcmp(readback, "via received", 12) == 0,
          "the received socket writes to the same peer");
    close(got[0]);
  }

  /* Three at once, close-on-exec requested at receive time. */
  sent[0] = file_fd;
  sent[1] = mem_fd;
  sent[2] = tcp_a;
  CHECK(send_fds(sv[0], "3", 1, sent, 3) == 1, "send three descriptors in one message");
  CHECK(recv_fds(sv[1], data, sizeof(data), got, 8, &count, MSG_CMSG_CLOEXEC, 0) == 1 && count == 3,
        "receive three descriptors");
  if (count == 3) {
    int i;

    for (i = 0; i < 3; ++i) {
      int flags = fcntl(got[i], F_GETFD);

      CHECK(flags >= 0 && (flags & FD_CLOEXEC) != 0, "MSG_CMSG_CLOEXEC sets close-on-exec");
      close(got[i]);
    }
  }

  /* A control buffer that is too small: the message arrives, the descriptors that do not fit are dropped and
   * closed, MSG_CTRUNC says so. (CMSG_SPACE rounds to 8 bytes, so a "one descriptor" buffer holds two.) */
  sent[0] = file_fd;
  sent[1] = mem_fd;
  sent[2] = tcp_a;
  CHECK(send_fds(sv[0], "T", 1, sent, 3) == 1, "send three");
  {
    int flags = 0;
    int i;

    CHECK(recv_fds(sv[1], data, sizeof(data), got, 1, &count, 0, &flags) == 1 && count >= 1 && count < 3 &&
              (flags & MSG_CTRUNC) != 0,
          "a small control buffer gets fewer descriptors and MSG_CTRUNC");
    for (i = 0; i < count; ++i) {
      close(got[i]);
    }
  }

  close(file_fd);
  close(mem_fd);
  close(tcp_a);
  close(tcp_b);
  close(sv[0]);
  close(sv[1]);
  unlink(file_path);
}

/* Child side of the two-process test: connect, receive a file and a memfd, check them, answer. */
static int run_child(const char* path) {
  struct sockaddr_un address;
  int sock = socket(AF_UNIX, SOCK_SEQPACKET, 0);
  int fds[4];
  int count = 0;
  char data[32];
  char buffer[64];
  int ok = 1;

  memset(&address, 0, sizeof(address));
  address.sun_family = AF_UNIX;
  strncpy(address.sun_path, path, sizeof(address.sun_path) - 1);
  if (sock < 0 || connect(sock, (const struct sockaddr*)&address, sizeof(address)) != 0) {
    return 10;
  }
  if (recv_fds(sock, data, sizeof(data), fds, 4, &count, 0, 0) != 2 || count != 2) {
    return 11;
  }
  if (lseek(fds[0], 0, SEEK_SET) != 0 || read(fds[0], buffer, sizeof(buffer)) != 11 ||
      memcmp(buffer, "parent file", 11) != 0) {
    ok = 0;
  }
  {
    char* map = (char*)mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fds[1], 0);

    if (map == MAP_FAILED || strcmp(map, "parent memory") != 0) {
      ok = 0;
    } else {
      strcpy(map, "child was here");
      munmap(map, 4096);
    }
  }
  if (send(sock, ok ? "ok" : "no", 2, 0) != 2) {
    return 12;
  }
  /* wait for the parent to be done reading the answer, then leave */
  (void)recv(sock, buffer, sizeof(buffer), 0);
  return ok ? 0 : 13;
}

static void test_fd_passing_between_processes(const char* self) {
  char file_path[256];
  int server;
  int accepted;
  int file_fd;
  int mem_fd;
  int sent[2];
  char* map;
  pid_t pid;
  int status = 0;
  char answer[8];
  char* const argv[] = {(char*)self, (char*)"child", socket_path, 0};
  const char* base = getenv("TMPDIR") != 0 ? getenv("TMPDIR") : (getenv("TEMP") != 0 ? getenv("TEMP") : ".");

  make_path("proc");
  server = listen_on(SOCK_SEQPACKET);
  CHECK(server >= 0, "listen for the child");
  if (server < 0) {
    return;
  }
  snprintf(file_path, sizeof(file_path), "%s/crt_unix_pfile_%d.txt", base, (int)getpid());
  file_fd = open(file_path, O_RDWR | O_CREAT | O_TRUNC, 0600);
  mem_fd = memfd_create("crt-unix-proc", 0);
  CHECK(file_fd >= 0 && mem_fd >= 0, "file and memfd");
  if (file_fd < 0 || mem_fd < 0) {
    close(server);
    return;
  }
  CHECK(write(file_fd, "parent file", 11) == 11, "write the file");
  CHECK(ftruncate(mem_fd, 4096) == 0, "size the memfd");
  map = (char*)mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, 0);
  if (map == MAP_FAILED) {
    CHECK(0, "map the memfd");
    return;
  }
  strcpy(map, "parent memory");

  CHECK(posix_spawn(&pid, self, 0, 0, argv, environ) == 0, "start the child");
  accepted = accept(server, 0, 0);
  CHECK(accepted >= 0, "accept the child");
  if (accepted >= 0) {
    sent[0] = file_fd;
    sent[1] = mem_fd;
    CHECK(send_fds(accepted, "go", 2, sent, 2) == 2, "send a file and a memfd to another process");
    memset(answer, 0, sizeof(answer));
    CHECK(recv(accepted, answer, sizeof(answer), 0) == 2 && memcmp(answer, "ok", 2) == 0,
          "the child saw the file and the memory");
    CHECK(strcmp(map, "child was here") == 0, "the child's write shows in the parent's mapping");
    close(accepted);
  }
  waitpid(pid, &status, 0);
  CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0, "the child exited cleanly");
  munmap(map, 4096);
  close(file_fd);
  close(mem_fd);
  close(server);
  unlink(socket_path);
  unlink(file_path);
}

int main(int argc, char** argv) {
  if (argc > 2 && strcmp(argv[1], "child") == 0) {
    return run_child(argv[2]);
  }
  test_stream_path();
  test_stream_nonblocking_and_poll();
  test_native_peer_sees_raw_bytes();
  if (seqpacket_available()) {
    test_seqpacket_records();
    test_seqpacket_path();
    test_fd_passing_in_process();
    test_fd_passing_between_processes(argv[0]);
  } else {
    printf("unix_socket_test: SOCK_SEQPACKET is not available on this host (skipped)\n");
  }
  if (failures != 0) {
    fprintf(stderr, "unix_socket_test: %d check(s) failed\n", failures);
    return 1;
  }
  printf("unix_socket_test: ok\n");
  return 0;
}
