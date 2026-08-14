#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char* message) {
  fprintf(stderr, "dns_resolve_test: %s\n", message);
  return 1;
}

static int dns_unavailable_ok(int rc) {
  const char* required = getenv("CRT_DNS_REQUIRED");

  if (required != 0 && required[0] != '\0' && strcmp(required, "0") != 0) {
    fprintf(stderr, "dns_resolve_test: required dns failed rc=%d %s\n", rc, gai_strerror(rc));
    return 1;
  }
  printf("dns_resolve_test: ok (dns unavailable rc=%d %s)\n", rc, gai_strerror(rc));
  return 0;
}

int main(void) {
  struct addrinfo hints;
  struct addrinfo* result = 0;
  const struct addrinfo* it;
  int rc;
  int saw_ipv4 = 0;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_ADDRCONFIG | AI_NUMERICSERV;

  rc = getaddrinfo("example.com", "80", &hints, &result);
  if (rc != 0) {
    if (rc == EAI_AGAIN || rc == EAI_FAIL || rc == EAI_NONAME || rc == EAI_SYSTEM) {
      return dns_unavailable_ok(rc);
    }
    return fail("unexpected getaddrinfo error");
  }

  for (it = result; it != 0; it = it->ai_next) {
    char text[INET_ADDRSTRLEN];
    const struct sockaddr_in* addr;

    if (it->ai_family != AF_INET || it->ai_addr == 0 ||
        it->ai_addrlen < (socklen_t)sizeof(struct sockaddr_in)) {
      continue;
    }
    addr = (const struct sockaddr_in*)it->ai_addr;
    if (addr->sin_port != htons(80)) {
      freeaddrinfo(result);
      return fail("resolved port");
    }
    if (inet_ntop(AF_INET, &addr->sin_addr, text, sizeof(text)) == 0) {
      freeaddrinfo(result);
      return fail("inet_ntop resolved address");
    }
    saw_ipv4 = 1;
  }

  freeaddrinfo(result);
  if (!saw_ipv4) {
    return fail("no ipv4 result");
  }

  printf("dns_resolve_test: ok\n");
  return 0;
}
