#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

static int fail(const char* message) {
  fprintf(stderr, "socket_network_test: %s\n", message);
  return 1;
}

static int fail_errno(const char* message) {
  fprintf(stderr, "socket_network_test: %s errno=%d\n", message, errno);
  return 1;
}

static int network_unavailable_ok(int fd) {
  close(fd);
  printf("socket_network_test: ok (network unavailable)\n");
  return 0;
}

int main(void) {
  int server = -1;
  int client = -1;
  int accepted = -1;
  int yes = 1;
  struct sockaddr_in addr;
  struct sockaddr_in bound;
  socklen_t bound_len = sizeof(bound);
  char text[INET_ADDRSTRLEN];
  char byte = 'N';
  char readback = 0;
  int available = 0;
  struct pollfd pfd;
  struct addrinfo hints;
  struct addrinfo* res = 0;

  if (htons(0x1234) != 0x3412 || ntohs(0x3412) != 0x1234 ||
      htonl(0x01020304U) != 0x04030201U || ntohl(0x04030201U) != 0x01020304U) {
    return fail("byte order");
  }
  if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1 ||
      inet_ntop(AF_INET, &addr.sin_addr, text, sizeof(text)) == 0 ||
      strcmp(text, "127.0.0.1") != 0) {
    return fail("inet conversion");
  }

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  if (getaddrinfo("127.0.0.1", "12345", &hints, &res) != 0 ||
      res == 0 ||
      res->ai_family != AF_INET ||
      ((struct sockaddr_in*)res->ai_addr)->sin_port != htons(12345)) {
    freeaddrinfo(res);
    return fail("getaddrinfo numeric");
  }
  freeaddrinfo(res);

  server = socket(AF_INET, SOCK_STREAM, 0);
  if (server < 0) {
    return fail("server socket");
  }
  if (setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) != 0) {
    close(server);
    return fail("setsockopt");
  }
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
  if (getsockname(server, (struct sockaddr*)&bound, &bound_len) != 0 ||
      bound.sin_family != AF_INET ||
      bound.sin_port == 0) {
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
  if (send(client, &byte, 1, 0) != 1) {
    close(accepted);
    close(client);
    close(server);
    return fail("send");
  }
  pfd.fd = accepted;
  pfd.events = POLLIN;
  pfd.revents = 0;
  if (poll(&pfd, 1, 1000) < 1 ||
      ioctl(accepted, FIONREAD, &available) != 0 ||
      available < 1) {
    close(accepted);
    close(client);
    close(server);
    return fail("socket FIONREAD");
  }
  if (recv(accepted, &readback, 1, 0) != 1 || readback != 'N') {
    close(accepted);
    close(client);
    close(server);
    return fail("recv");
  }

  shutdown(client, SHUT_RDWR);
  close(accepted);
  close(client);
  close(server);

  printf("socket_network_test: ok\n");
  return 0;
}
