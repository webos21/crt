#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fenv.h>
#include <float.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

#define CRT_STATIC_ASSERT(name, expr) typedef char crt_static_assert_##name[(expr) ? 1 : -1]

CRT_STATIC_ASSERT(ssize_pointer_sized, sizeof(ssize_t) == sizeof(void*));
CRT_STATIC_ASSERT(off_64, sizeof(off_t) == 8);
CRT_STATIC_ASSERT(time_64, sizeof(time_t) == 8);
CRT_STATIC_ASSERT(dev_64, sizeof(dev_t) == 8);
CRT_STATIC_ASSERT(ino_64, sizeof(ino_t) == 8);
CRT_STATIC_ASSERT(nlink_64, sizeof(nlink_t) == 8);
CRT_STATIC_ASSERT(pid_32, sizeof(pid_t) == 4);
CRT_STATIC_ASSERT(socklen_32, sizeof(socklen_t) == 4);
CRT_STATIC_ASSERT(sa_family_16, sizeof(sa_family_t) == 2);
CRT_STATIC_ASSERT(in_port_16, sizeof(in_port_t) == 2);
CRT_STATIC_ASSERT(in_addr_32, sizeof(in_addr_t) == 4);
CRT_STATIC_ASSERT(fd_mask_ulong, sizeof(__fd_mask) == sizeof(unsigned long));
CRT_STATIC_ASSERT(pthread_t_pointer_sized, sizeof(pthread_t) == sizeof(void*));
CRT_STATIC_ASSERT(wchar_32, sizeof(wchar_t) == 4);
CRT_STATIC_ASSERT(stat_has_64_size, sizeof(((struct stat*)0)->st_size) == 8);
CRT_STATIC_ASSERT(timespec_has_64_sec, sizeof(((struct timespec*)0)->tv_sec) == 8);
CRT_STATIC_ASSERT(timeval_has_64_sec, sizeof(((struct timeval*)0)->tv_sec) == 8);
CRT_STATIC_ASSERT(sockaddr_size, sizeof(struct sockaddr) == 16);
CRT_STATIC_ASSERT(sockaddr_storage_large, sizeof(struct sockaddr_storage) >= 128);
CRT_STATIC_ASSERT(fd_set_1024, FD_SETSIZE == 1024);
CRT_STATIC_ASSERT(pollfd_layout, offsetof(struct pollfd, revents) > offsetof(struct pollfd, events));
CRT_STATIC_ASSERT(addrinfo_addrlen_socklen, sizeof(((struct addrinfo*)0)->ai_addrlen) == sizeof(socklen_t));

static int fail(const char* message) {
  fprintf(stderr, "header_abi_test: %s\n", message);
  return 1;
}

int main(void) {
  fd_set fds;
  struct sockaddr_in addr;
  struct pollfd pfd;
  struct stat st;
  struct timespec ts;
  struct timeval tv;

  memset(&fds, 0, sizeof(fds));
  memset(&addr, 0, sizeof(addr));
  memset(&pfd, 0, sizeof(pfd));
  memset(&st, 0, sizeof(st));
  memset(&ts, 0, sizeof(ts));
  memset(&tv, 0, sizeof(tv));

  FD_ZERO(&fds);
  FD_SET(3, &fds);
  if (!FD_ISSET(3, &fds)) {
    return fail("fd_set macros");
  }
  FD_CLR(3, &fds);
  if (FD_ISSET(3, &fds)) {
    return fail("fd_set clear");
  }

  addr.sin_family = AF_INET;
  addr.sin_port = htons(1234);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (addr.sin_family != AF_INET || ntohs(addr.sin_port) != 1234 ||
      ntohl(addr.sin_addr.s_addr) != INADDR_LOOPBACK) {
    return fail("inet ABI");
  }

  pfd.fd = -1;
  pfd.events = POLLIN | POLLOUT;
  st.st_size = (off_t)123;
  ts.tv_sec = (time_t)1;
  tv.tv_sec = (time_t)1;
  if (pfd.fd != -1 || st.st_size != 123 || ts.tv_sec != 1 || tv.tv_sec != 1) {
    return fail("basic ABI assignment");
  }

  printf("header_abi_test: ok\n");
  return 0;
}
