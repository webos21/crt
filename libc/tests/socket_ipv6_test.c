/* IPv6 through the CRT libc surface: text conversion, address literals in getaddrinfo(),
 * and a TCP and UDP exchange over ::1 with the Bionic AF_INET6 / sockaddr_in6 / IPV6_V6ONLY
 * numbering (the macOS socket layer translates all three to Darwin's). */
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

static int failures;

#define CHECK(condition, message)                                                  \
  do {                                                                             \
    if (!(condition)) {                                                            \
      fprintf(stderr, "socket_ipv6_test: %s (line %d, errno %d)\n", message, __LINE__, errno); \
      ++failures;                                                                  \
    }                                                                              \
  } while (0)

static void check_text(const char* text, const unsigned char* expected, const char* canonical) {
  struct in6_addr addr;
  char out[INET6_ADDRSTRLEN];

  CHECK(inet_pton(AF_INET6, text, &addr) == 1, "inet_pton(AF_INET6) accepts the literal");
  CHECK(memcmp(&addr, expected, 16) == 0, "inet_pton(AF_INET6) bytes");
  CHECK(inet_ntop(AF_INET6, &addr, out, sizeof(out)) != 0 && strcmp(out, canonical) == 0,
        "inet_ntop(AF_INET6) canonical text");
}

static void test_text(void) {
  static const unsigned char loopback[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
  static const unsigned char any[16] = {0};
  static const unsigned char doc[16] = {0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
  static const unsigned char mapped[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff, 192, 0, 2, 33};
  static const unsigned char full[16] = {0x20, 0x01, 0x0d, 0xb8, 0x85, 0xa3, 0x00, 0x00,
                                         0x00, 0x00, 0x8a, 0x2e, 0x03, 0x70, 0x73, 0x34};
  static const unsigned char tail[16] = {0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  struct in6_addr addr;
  char small[8];

  check_text("::1", loopback, "::1");
  check_text("::", any, "::");
  check_text("2001:db8::1", doc, "2001:db8::1");
  check_text("2001:0DB8:0:0:0:0:0:1", doc, "2001:db8::1");
  check_text("::ffff:192.0.2.33", mapped, "::ffff:192.0.2.33");
  check_text("2001:db8:85a3::8a2e:370:7334", full, "2001:db8:85a3::8a2e:370:7334");
  check_text("fe80::", tail, "fe80::");

  CHECK(inet_pton(AF_INET6, "1::2::3", &addr) == 0, "two :: runs rejected");
  CHECK(inet_pton(AF_INET6, "12345::1", &addr) == 0, "five-digit group rejected");
  CHECK(inet_pton(AF_INET6, "1:2:3:4:5:6:7", &addr) == 0, "seven groups rejected");
  CHECK(inet_pton(AF_INET6, ":1", &addr) == 0, "leading single colon rejected");
  CHECK(inet_pton(AF_INET6, "1:2:3:4:5:6:7:8:9", &addr) == 0, "nine groups rejected");
  CHECK(inet_pton(AF_INET6, "::g", &addr) == 0, "non-hex rejected");
  CHECK(inet_ntop(AF_INET6, &in6addr_loopback, small, 2) == 0 && errno == ENOSPC, "short buffer: ENOSPC");
  CHECK(memcmp(&in6addr_loopback, loopback, 16) == 0 && memcmp(&in6addr_any, any, 16) == 0,
        "in6addr_loopback/in6addr_any");
}

static void test_getaddrinfo(void) {
  struct addrinfo hints;
  struct addrinfo* res = 0;
  const struct sockaddr_in6* in6;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_NUMERICHOST;
  CHECK(getaddrinfo("::1", "8443", &hints, &res) == 0 && res != 0, "getaddrinfo literal ::1");
  if (res != 0) {
    CHECK(res->ai_family == AF_INET6 && res->ai_addrlen == sizeof(struct sockaddr_in6),
          "literal result is AF_INET6");
    in6 = (const struct sockaddr_in6*)res->ai_addr;
    CHECK(in6->sin6_family == AF_INET6 && in6->sin6_port == htons(8443) &&
              memcmp(&in6->sin6_addr, &in6addr_loopback, 16) == 0,
          "literal result carries port and address");
    CHECK(res->ai_socktype == SOCK_STREAM, "literal result keeps the socket type");
    freeaddrinfo(res);
    res = 0;
  }

  CHECK(getaddrinfo("fe80::1%3", "80", &hints, &res) == 0 && res != 0 &&
            ((const struct sockaddr_in6*)res->ai_addr)->sin6_scope_id == 3,
        "literal with a numeric zone");
  freeaddrinfo(res);
  res = 0;

  hints.ai_family = AF_INET;
  CHECK(getaddrinfo("::1", "80", &hints, &res) == EAI_NONAME, "AF_INET refuses an IPv6 literal");
  hints.ai_family = AF_INET6;
  CHECK(getaddrinfo("127.0.0.1", "80", &hints, &res) == EAI_NONAME, "AF_INET6 refuses an IPv4 literal");

  hints.ai_flags = AI_PASSIVE;
  CHECK(getaddrinfo(0, "9000", &hints, &res) == 0 && res != 0 && res->ai_family == AF_INET6 &&
            memcmp(&((const struct sockaddr_in6*)res->ai_addr)->sin6_addr, &in6addr_any, 16) == 0,
        "AI_PASSIVE AF_INET6 is ::");
  freeaddrinfo(res);
  res = 0;
  hints.ai_flags = 0;
  CHECK(getaddrinfo(0, "9000", &hints, &res) == 0 && res != 0 &&
            memcmp(&((const struct sockaddr_in6*)res->ai_addr)->sin6_addr, &in6addr_loopback, 16) == 0,
        "AF_INET6 without AI_PASSIVE is ::1");
  freeaddrinfo(res);
  res = 0;

#if defined(__APPLE__) || defined(_WIN32) || defined(__MINGW32__)
  /* A name goes to the host resolver (macOS) or is answered as RFC 6761 loopback (Windows); the answer
   * comes back in this libc's layouts. */
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET6;
  hints.ai_socktype = SOCK_STREAM;
  if (getaddrinfo("localhost", "443", &hints, &res) == 0 && res != 0) {
    CHECK(res->ai_family == AF_INET6 && res->ai_addrlen == sizeof(struct sockaddr_in6),
          "localhost AF_INET6 result layout");
    in6 = (const struct sockaddr_in6*)res->ai_addr;
    CHECK(in6->sin6_family == AF_INET6 && in6->sin6_port == htons(443) &&
              memcmp(&in6->sin6_addr, &in6addr_loopback, 16) == 0,
          "localhost resolves to ::1 with the port applied");
    freeaddrinfo(res);
  } else {
    CHECK(0, "getaddrinfo(localhost, AF_INET6) through the host resolver");
  }
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  res = 0;
  if (getaddrinfo("localhost", "443", &hints, &res) == 0 && res != 0) {
    int saw4 = 0;
    int saw6 = 0;
    const struct addrinfo* it;

    for (it = res; it != 0; it = it->ai_next) {
      saw4 += it->ai_family == AF_INET;
      saw6 += it->ai_family == AF_INET6;
    }
    CHECK(saw4 > 0 && saw6 > 0, "localhost AF_UNSPEC lists both families");
    freeaddrinfo(res);
  } else {
    CHECK(0, "getaddrinfo(localhost, AF_UNSPEC)");
  }
#endif
}

static int test_sockets(void) {
  int server = socket(AF_INET6, SOCK_STREAM, 0);
  int client = -1;
  int accepted = -1;
  int on = 1;
  int value = -1;
  socklen_t value_len = sizeof(value);
  struct sockaddr_in6 addr;
  struct sockaddr_in6 bound;
  struct sockaddr_in6 peer;
  socklen_t bound_len = sizeof(bound);
  socklen_t peer_len = sizeof(peer);
  char byte = 'q';
  char readback = 0;
  char text[INET6_ADDRSTRLEN];

  if (server < 0) {
#if defined(__APPLE__) || defined(_WIN32) || defined(__MINGW32__)
    /* macOS and Windows always have ::1; a failure here is the AF_INET6 translation, not the host. */
    CHECK(0, "socket(AF_INET6)");
    return 1;
#else
    printf("socket_ipv6_test: ok (no IPv6 sockets here, errno %d)\n", errno);
    return 0;
#endif
  }
  memset(&addr, 0, sizeof(addr));
  addr.sin6_family = AF_INET6;
  addr.sin6_addr = in6addr_loopback;
  addr.sin6_port = 0;
  CHECK(setsockopt(server, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on)) == 0, "setsockopt IPV6_V6ONLY");
  CHECK(getsockopt(server, IPPROTO_IPV6, IPV6_V6ONLY, &value, &value_len) == 0 && value != 0,
        "getsockopt IPV6_V6ONLY reads back");
  CHECK(setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == 0, "setsockopt SO_REUSEADDR");
  if (bind(server, (const struct sockaddr*)&addr, sizeof(addr)) != 0) {
    int saved = errno;

    close(server);
    if (saved == EADDRNOTAVAIL || saved == EAFNOSUPPORT) {
#if defined(__APPLE__) || defined(_WIN32) || defined(__MINGW32__)
      errno = saved;
      CHECK(0, "bind ::1");
      return 1;
#endif
      printf("socket_ipv6_test: ok (no ::1 here, errno %d)\n", saved);
      return 0;
    }
    errno = saved;
    CHECK(0, "bind ::1");
    return 1;
  }
  CHECK(listen(server, 1) == 0, "listen");
  CHECK(getsockname(server, (struct sockaddr*)&bound, &bound_len) == 0 &&
            bound_len == sizeof(bound) && bound.sin6_family == AF_INET6 && bound.sin6_port != 0 &&
            memcmp(&bound.sin6_addr, &in6addr_loopback, 16) == 0,
        "getsockname returns the Bionic sockaddr_in6");

  client = socket(AF_INET6, SOCK_STREAM, 0);
  CHECK(client >= 0, "client socket(AF_INET6)");
  CHECK(connect(client, (const struct sockaddr*)&bound, sizeof(bound)) == 0, "connect ::1");
  accepted = accept(server, (struct sockaddr*)&peer, &peer_len);
  CHECK(accepted >= 0, "accept");
  CHECK(peer_len == sizeof(peer) && peer.sin6_family == AF_INET6 &&
            memcmp(&peer.sin6_addr, &in6addr_loopback, 16) == 0,
        "accept reports the IPv6 peer in Bionic layout");
  CHECK(inet_ntop(AF_INET6, &peer.sin6_addr, text, sizeof(text)) != 0 && strcmp(text, "::1") == 0,
        "peer text is ::1");
  CHECK(send(client, &byte, 1, 0) == 1, "send");
  CHECK(accepted >= 0 && recv(accepted, &readback, 1, 0) == 1 && readback == byte, "recv");

  peer_len = sizeof(peer);
  CHECK(accepted >= 0 && getpeername(accepted, (struct sockaddr*)&peer, &peer_len) == 0 &&
            peer.sin6_family == AF_INET6 && peer.sin6_port != 0,
        "getpeername on the accepted socket");

  if (accepted >= 0) {
    close(accepted);
  }
  close(client);
  close(server);

  {
    int a = socket(AF_INET6, SOCK_DGRAM, 0);
    int b = socket(AF_INET6, SOCK_DGRAM, 0);
    struct sockaddr_in6 from;
    socklen_t from_len = sizeof(from);

    CHECK(a >= 0 && b >= 0, "UDP sockets");
    if (a >= 0 && b >= 0) {
      memset(&addr, 0, sizeof(addr));
      addr.sin6_family = AF_INET6;
      addr.sin6_addr = in6addr_loopback;
      CHECK(bind(a, (const struct sockaddr*)&addr, sizeof(addr)) == 0, "UDP bind");
      bound_len = sizeof(bound);
      CHECK(getsockname(a, (struct sockaddr*)&bound, &bound_len) == 0, "UDP getsockname");
      CHECK(sendto(b, &byte, 1, 0, (const struct sockaddr*)&bound, sizeof(bound)) == 1, "sendto ::1");
      readback = 0;
      CHECK(recvfrom(a, &readback, 1, 0, (struct sockaddr*)&from, &from_len) == 1 && readback == byte &&
                from.sin6_family == AF_INET6 && from_len == sizeof(from),
            "recvfrom reports an IPv6 sender");
    }
    if (a >= 0) {
      close(a);
    }
    if (b >= 0) {
      close(b);
    }
  }
  return 0;
}

/* Option numbering and message-form I/O: the socket options a portable program sets, an IPv6 option, and
 * sendmsg()/recvmsg() carrying a sockaddr_in6 name (Windows rebuilds both on top of Winsock). */
static void test_options_and_messages(void) {
  int tcp = socket(AF_INET, SOCK_STREAM, 0);
  int on = 1;
  int value = 0;
  socklen_t value_len = sizeof(value);
  struct linger linger_in = {1, 5};
  struct linger linger_out = {0, 0};
  socklen_t linger_len = sizeof(linger_out);
  int a;
  int b;

  CHECK(tcp >= 0, "socket(AF_INET, SOCK_STREAM)");
  if (tcp >= 0) {
    CHECK(setsockopt(tcp, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on)) == 0, "setsockopt SO_KEEPALIVE");
    CHECK(getsockopt(tcp, SOL_SOCKET, SO_KEEPALIVE, &value, &value_len) == 0 && value != 0,
          "getsockopt SO_KEEPALIVE reads back");
    CHECK(setsockopt(tcp, SOL_SOCKET, SO_LINGER, &linger_in, sizeof(linger_in)) == 0, "setsockopt SO_LINGER");
    CHECK(getsockopt(tcp, SOL_SOCKET, SO_LINGER, &linger_out, &linger_len) == 0 && linger_out.l_onoff != 0 &&
              linger_out.l_linger == 5,
          "getsockopt SO_LINGER reads back {on, 5}");
    value = 65536;
    CHECK(setsockopt(tcp, SOL_SOCKET, SO_SNDBUF, &value, sizeof(value)) == 0, "setsockopt SO_SNDBUF");
    value = 0;
    value_len = sizeof(value);
    CHECK(getsockopt(tcp, SOL_SOCKET, SO_RCVBUF, &value, &value_len) == 0 && value > 0, "getsockopt SO_RCVBUF");
    value = 0;
    value_len = sizeof(value);
    CHECK(getsockopt(tcp, SOL_SOCKET, SO_TYPE, &value, &value_len) == 0 && value == SOCK_STREAM,
          "getsockopt SO_TYPE is SOCK_STREAM in Bionic numbering");
    close(tcp);
  }

  a = socket(AF_INET6, SOCK_DGRAM, 0);
  b = socket(AF_INET6, SOCK_DGRAM, 0);
  if (a >= 0 && b >= 0) {
    struct sockaddr_in6 addr;
    struct sockaddr_in6 bound;
    struct sockaddr_in6 from;
    socklen_t bound_len = sizeof(bound);
    int hops = 7;
    char byte = 'm';
    char readback = 0;
    struct iovec out_iov;
    struct iovec in_iov;
    struct msghdr out;
    struct msghdr in;

    value = 0;
    value_len = sizeof(value);
    CHECK(setsockopt(a, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &hops, sizeof(hops)) == 0, "setsockopt IPV6_UNICAST_HOPS");
    CHECK(getsockopt(a, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &value, &value_len) == 0 && value == 7,
          "getsockopt IPV6_UNICAST_HOPS reads back 7");

    memset(&addr, 0, sizeof(addr));
    addr.sin6_family = AF_INET6;
    addr.sin6_addr = in6addr_loopback;
    if (bind(a, (const struct sockaddr*)&addr, sizeof(addr)) == 0 &&
        getsockname(a, (struct sockaddr*)&bound, &bound_len) == 0) {
      memset(&out, 0, sizeof(out));
      out_iov.iov_base = &byte;
      out_iov.iov_len = 1;
      out.msg_name = &bound;
      out.msg_namelen = sizeof(bound);
      out.msg_iov = &out_iov;
      out.msg_iovlen = 1;
      CHECK(sendmsg(b, &out, 0) == 1, "sendmsg with a sockaddr_in6 name");

      memset(&in, 0, sizeof(in));
      memset(&from, 0, sizeof(from));
      in_iov.iov_base = &readback;
      in_iov.iov_len = 1;
      in.msg_name = &from;
      in.msg_namelen = sizeof(from);
      in.msg_iov = &in_iov;
      in.msg_iovlen = 1;
      CHECK(recvmsg(a, &in, 0) == 1 && readback == byte && from.sin6_family == AF_INET6 &&
                in.msg_namelen == sizeof(from) && memcmp(&from.sin6_addr, &in6addr_loopback, 16) == 0,
            "recvmsg reports the IPv6 sender in Bionic layout");
    }
  }
  if (a >= 0) {
    close(a);
  }
  if (b >= 0) {
    close(b);
  }
}

/* A dual-stack listener (IPV6_V6ONLY off) takes an IPv4 client as an IPv4-mapped IPv6 peer. Skipped when
 * the host cannot make one. */
static void test_dual_stack(void) {
  int server = socket(AF_INET6, SOCK_STREAM, 0);
  int off = 0;
  struct sockaddr_in6 addr;
  struct sockaddr_in6 bound;
  socklen_t bound_len = sizeof(bound);
  int client;
  int accepted;
  struct sockaddr_in6 peer;
  socklen_t peer_len = sizeof(peer);
  struct sockaddr_in target;
  static const unsigned char mapped_prefix[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff};

  if (server < 0) {
    return;
  }
  memset(&addr, 0, sizeof(addr));
  addr.sin6_family = AF_INET6;
  addr.sin6_addr = in6addr_any;
  if (setsockopt(server, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off)) != 0 ||
      bind(server, (const struct sockaddr*)&addr, sizeof(addr)) != 0 || listen(server, 1) != 0 ||
      getsockname(server, (struct sockaddr*)&bound, &bound_len) != 0) {
    close(server);
    return;
  }
  client = socket(AF_INET, SOCK_STREAM, 0);
  CHECK(client >= 0, "IPv4 client socket");
  memset(&target, 0, sizeof(target));
  target.sin_family = AF_INET;
  target.sin_port = bound.sin6_port;
  target.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  CHECK(client >= 0 && connect(client, (const struct sockaddr*)&target, sizeof(target)) == 0,
        "IPv4 client connects to the dual-stack listener");
  accepted = accept(server, (struct sockaddr*)&peer, &peer_len);
  CHECK(accepted >= 0, "dual-stack accept");
  CHECK(accepted >= 0 && peer_len == sizeof(peer) && peer.sin6_family == AF_INET6 &&
            memcmp(&peer.sin6_addr, mapped_prefix, 12) == 0 && peer.sin6_addr.s6_addr[12] == 127 &&
            peer.sin6_addr.s6_addr[15] == 1,
        "the IPv4 peer arrives as ::ffff:127.0.0.1");
  if (accepted >= 0) {
    close(accepted);
  }
  if (client >= 0) {
    close(client);
  }
  close(server);
}

int main(void) {
  test_text();
  test_getaddrinfo();
  if (test_sockets() != 0) {
    ++failures;
  }
  test_options_and_messages();
  test_dual_stack();
  if (failures != 0) {
    fprintf(stderr, "socket_ipv6_test: %d check(s) failed\n", failures);
    return 1;
  }
  printf("socket_ipv6_test: ok\n");
  return 0;
}
