#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct dns_thread_result {
  int rc;
  int saw_ipv4;
};

static void* dns_worker(void* arg) {
  struct dns_thread_result* result = (struct dns_thread_result*)arg;
  struct addrinfo hints;
  struct addrinfo* ai = 0;
  const struct addrinfo* it;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_flags = AI_ADDRCONFIG | AI_NUMERICSERV;

  result->rc = getaddrinfo("example.com", "80", &hints, &ai);
  if (result->rc != 0) {
    return 0;
  }
  for (it = ai; it != 0; it = it->ai_next) {
    if (it->ai_family == AF_INET && it->ai_addr != 0 &&
        it->ai_addrlen >= (socklen_t)sizeof(struct sockaddr_in)) {
      result->saw_ipv4 = 1;
      break;
    }
  }
  freeaddrinfo(ai);
  return 0;
}

static int dns_unavailable_ok(int rc) {
  const char* required = getenv("CRT_DNS_REQUIRED");

  if (required != 0 && required[0] != '\0' && strcmp(required, "0") != 0) {
    fprintf(stderr, "dns_thread_test: required dns failed rc=%d %s\n", rc, gai_strerror(rc));
    return 1;
  }
  printf("dns_thread_test: ok (dns unavailable rc=%d %s)\n", rc, gai_strerror(rc));
  return 0;
}

int main(void) {
  struct dns_thread_result result;
  pthread_t thread;

  result.rc = 0;
  result.saw_ipv4 = 0;

  if (pthread_create(&thread, 0, dns_worker, &result) != 0) {
    fprintf(stderr, "dns_thread_test: pthread_create\n");
    return 1;
  }
  if (pthread_join(thread, 0) != 0) {
    fprintf(stderr, "dns_thread_test: pthread_join\n");
    return 1;
  }

  if (result.rc != 0) {
    if (result.rc == EAI_AGAIN || result.rc == EAI_FAIL ||
        result.rc == EAI_NONAME || result.rc == EAI_SYSTEM) {
      return dns_unavailable_ok(result.rc);
    }
    fprintf(stderr, "dns_thread_test: unexpected getaddrinfo rc=%d %s\n",
            result.rc, gai_strerror(result.rc));
    return 1;
  }
  if (!result.saw_ipv4) {
    fprintf(stderr, "dns_thread_test: no ipv4 result\n");
    return 1;
  }

  printf("dns_thread_test: ok\n");
  return 0;
}
