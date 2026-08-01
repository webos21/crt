#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <unistd.h>

#if defined(CRT_TARGET_OS_MACOS)
struct crt_darwin_sockaddr_in {
  unsigned char sin_len;
  unsigned char sin_family;
  in_port_t sin_port;
  struct in_addr sin_addr;
  unsigned char sin_zero[8];
};

#define CRT_DARWIN_SOL_SOCKET 0xffff
#define CRT_DARWIN_SO_REUSEADDR 0x0004
#endif

long __crt_sys_socket(int domain, int type, int protocol);
long __crt_sys_bind(int sockfd, const void* addr, unsigned int addrlen);
long __crt_sys_listen(int sockfd, int backlog);
long __crt_sys_accept(int sockfd, void* addr, unsigned int* addrlen);
long __crt_sys_connect(int sockfd, const void* addr, unsigned int addrlen);
long __crt_sys_sendto(
    int sockfd,
    const void* buf,
    unsigned long len,
    int flags,
    const void* dest_addr,
    unsigned int addrlen);
long __crt_sys_recvfrom(
    int sockfd,
    void* buf,
    unsigned long len,
    int flags,
    void* src_addr,
    unsigned int* addrlen);
long __crt_sys_getsockname(int sockfd, void* addr, unsigned int* addrlen);
long __crt_sys_setsockopt(int sockfd, int level, int optname, const void* optval, unsigned int optlen);
long __crt_sys_shutdown(int sockfd, int how);

int h_errno;

static long normalize_socket_result(long result) {
  if (result < 0 && result >= -4095) {
    return __set_errno((int)-result);
  }
  return result;
}

static int host_is_little_endian(void) {
  const uint16_t value = 1;
  return *(const unsigned char*)&value == 1;
}

uint16_t htons(uint16_t hostshort) {
  if (!host_is_little_endian()) {
    return hostshort;
  }
  return (uint16_t)((hostshort << 8) | (hostshort >> 8));
}

uint16_t ntohs(uint16_t netshort) {
  return htons(netshort);
}

uint32_t htonl(uint32_t hostlong) {
  if (!host_is_little_endian()) {
    return hostlong;
  }
  return ((hostlong & 0x000000ffU) << 24) |
         ((hostlong & 0x0000ff00U) << 8) |
         ((hostlong & 0x00ff0000U) >> 8) |
         ((hostlong & 0xff000000U) >> 24);
}

uint32_t ntohl(uint32_t netlong) {
  return htonl(netlong);
}

static int parse_ipv4_octet(const char** cursor, unsigned int* octet) {
  unsigned int value = 0;
  int digits = 0;

  while (**cursor >= '0' && **cursor <= '9') {
    value = value * 10U + (unsigned int)(**cursor - '0');
    if (value > 255U) {
      return 0;
    }
    ++*cursor;
    ++digits;
  }
  if (digits == 0) {
    return 0;
  }
  *octet = value;
  return 1;
}

int inet_pton(int af, const char* src, void* dst) {
  const char* cursor = src;
  unsigned int octets[4];
  int i;

  if (af != AF_INET) {
    errno = EAFNOSUPPORT;
    return -1;
  }
  if (src == 0 || dst == 0) {
    errno = EINVAL;
    return -1;
  }
  for (i = 0; i < 4; ++i) {
    if (!parse_ipv4_octet(&cursor, &octets[i])) {
      return 0;
    }
    if (i != 3) {
      if (*cursor != '.') {
        return 0;
      }
      ++cursor;
    }
  }
  if (*cursor != 0) {
    return 0;
  }
  ((struct in_addr*)dst)->s_addr =
      htonl((octets[0] << 24) | (octets[1] << 16) | (octets[2] << 8) | octets[3]);
  return 1;
}

static char* append_decimal(char* out, unsigned int value) {
  char tmp[3];
  int n = 0;

  do {
    tmp[n++] = (char)('0' + value % 10U);
    value /= 10U;
  } while (value != 0);
  while (n > 0) {
    *out++ = tmp[--n];
  }
  return out;
}

const char* inet_ntop(int af, const void* src, char* dst, socklen_t size) {
  uint32_t addr;
  unsigned int octets[4];
  char tmp[INET_ADDRSTRLEN];
  char* out = tmp;
  int i;
  size_t len;

  if (af != AF_INET) {
    errno = EAFNOSUPPORT;
    return 0;
  }
  if (src == 0 || dst == 0) {
    errno = EINVAL;
    return 0;
  }
  addr = ntohl(((const struct in_addr*)src)->s_addr);
  octets[0] = (addr >> 24) & 0xffU;
  octets[1] = (addr >> 16) & 0xffU;
  octets[2] = (addr >> 8) & 0xffU;
  octets[3] = addr & 0xffU;
  for (i = 0; i < 4; ++i) {
    if (i != 0) {
      *out++ = '.';
    }
    out = append_decimal(out, octets[i]);
  }
  *out = 0;
  len = strlen(tmp);
  if (size <= len) {
    errno = ENOSPC;
    return 0;
  }
  memcpy(dst, tmp, len + 1);
  return dst;
}

in_addr_t inet_addr(const char* cp) {
  struct in_addr addr;
  if (inet_pton(AF_INET, cp, &addr) != 1) {
    return INADDR_NONE;
  }
  return addr.s_addr;
}

int gethostname(char* name, size_t len) {
  struct utsname uts;
  size_t n;

  if (name == 0 || len == 0) {
    errno = EINVAL;
    return -1;
  }
  if (uname(&uts) != 0) {
    return -1;
  }
  n = strlen(uts.nodename);
  if (n >= len) {
    errno = ENAMETOOLONG;
    return -1;
  }
  memcpy(name, uts.nodename, n + 1);
  return 0;
}

int sethostname(const char* name, size_t len) {
  (void)name;
  (void)len;
  errno = ENOTSUP;
  return -1;
}

struct hostent* gethostbyname(const char* name) {
  static struct hostent host;
  static char canonical[256];
  static char* aliases[] = {0};
  static struct in_addr addr;
  static char* addr_list[] = {(char*)&addr, 0};

  if (name == 0 || name[0] == 0) {
    h_errno = HOST_NOT_FOUND;
    return 0;
  }
  if (strcmp(name, "localhost") == 0) {
    addr.s_addr = htonl(0x7f000001U);
  } else if (inet_pton(AF_INET, name, &addr) != 1) {
    h_errno = HOST_NOT_FOUND;
    return 0;
  }
  strncpy(canonical, name, sizeof(canonical) - 1);
  canonical[sizeof(canonical) - 1] = 0;
  host.h_name = canonical;
  host.h_aliases = aliases;
  host.h_addrtype = AF_INET;
  host.h_length = (int)sizeof(addr);
  host.h_addr_list = addr_list;
  h_errno = 0;
  return &host;
}

const char* hstrerror(int err) {
  switch (err) {
    case 0:
      return "Resolver Error 0";
    case HOST_NOT_FOUND:
      return "Host not found";
    case TRY_AGAIN:
      return "Try again";
    case NO_RECOVERY:
      return "Non recoverable error";
    case NO_DATA:
      return "No data";
    default:
      return "Unknown resolver error";
  }
}

#if defined(CRT_TARGET_OS_MACOS)
static int to_darwin_sockaddr(
    const struct sockaddr* addr,
    socklen_t addrlen,
    struct crt_darwin_sockaddr_in* out,
    unsigned int* outlen) {
  const struct sockaddr_in* in;

  if (addr == 0 || out == 0 || outlen == 0 || addrlen < sizeof(struct sockaddr_in)) {
    errno = EINVAL;
    return -1;
  }
  if (addr->sa_family != AF_INET) {
    errno = EAFNOSUPPORT;
    return -1;
  }
  in = (const struct sockaddr_in*)addr;
  memset(out, 0, sizeof(*out));
  out->sin_len = (unsigned char)sizeof(*out);
  out->sin_family = AF_INET;
  out->sin_port = in->sin_port;
  out->sin_addr = in->sin_addr;
  memcpy(out->sin_zero, in->sin_zero, sizeof(out->sin_zero));
  *outlen = (unsigned int)sizeof(*out);
  return 0;
}

static void from_darwin_sockaddr(
    const struct crt_darwin_sockaddr_in* in,
    struct sockaddr* addr,
    socklen_t* addrlen) {
  struct sockaddr_in* out;

  if (addr == 0 || addrlen == 0 || *addrlen < sizeof(struct sockaddr_in)) {
    return;
  }
  out = (struct sockaddr_in*)addr;
  memset(out, 0, sizeof(*out));
  out->sin_family = in->sin_family;
  out->sin_port = in->sin_port;
  out->sin_addr = in->sin_addr;
  memcpy(out->sin_zero, in->sin_zero, sizeof(out->sin_zero));
  *addrlen = (socklen_t)sizeof(*out);
}
#endif

int socket(int domain, int type, int protocol) {
  return (int)normalize_socket_result(__crt_sys_socket(domain, type, protocol));
}

int bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
#if defined(CRT_TARGET_OS_MACOS)
  struct crt_darwin_sockaddr_in darwin_addr;
  unsigned int darwin_len;
  if (to_darwin_sockaddr(addr, addrlen, &darwin_addr, &darwin_len) != 0) {
    return -1;
  }
  return (int)normalize_socket_result(__crt_sys_bind(sockfd, &darwin_addr, darwin_len));
#else
  return (int)normalize_socket_result(__crt_sys_bind(sockfd, addr, (unsigned int)addrlen));
#endif
}

int listen(int sockfd, int backlog) {
  return (int)normalize_socket_result(__crt_sys_listen(sockfd, backlog));
}

int accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen) {
#if defined(CRT_TARGET_OS_MACOS)
  struct crt_darwin_sockaddr_in darwin_addr;
  unsigned int darwin_len = sizeof(darwin_addr);
  long result;

  result = __crt_sys_accept(sockfd, addr != 0 ? &darwin_addr : 0, addrlen != 0 ? &darwin_len : 0);
  if (result < 0 && result >= -4095) {
    return (int)__set_errno((int)-result);
  }
  if (addr != 0 && addrlen != 0) {
    from_darwin_sockaddr(&darwin_addr, addr, addrlen);
  }
  return (int)result;
#else
  unsigned int len = addrlen != 0 ? (unsigned int)*addrlen : 0;
  long result = __crt_sys_accept(sockfd, addr, addrlen != 0 ? &len : 0);
  if (result < 0 && result >= -4095) {
    return (int)__set_errno((int)-result);
  }
  if (addrlen != 0) {
    *addrlen = (socklen_t)len;
  }
  return (int)result;
#endif
}

int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
#if defined(CRT_TARGET_OS_MACOS)
  struct crt_darwin_sockaddr_in darwin_addr;
  unsigned int darwin_len;
  if (to_darwin_sockaddr(addr, addrlen, &darwin_addr, &darwin_len) != 0) {
    return -1;
  }
  return (int)normalize_socket_result(__crt_sys_connect(sockfd, &darwin_addr, darwin_len));
#else
  return (int)normalize_socket_result(__crt_sys_connect(sockfd, addr, (unsigned int)addrlen));
#endif
}

ssize_t sendto(
    int sockfd,
    const void* buf,
    size_t len,
    int flags,
    const struct sockaddr* dest_addr,
    socklen_t addrlen) {
#if defined(CRT_TARGET_OS_MACOS)
  struct crt_darwin_sockaddr_in darwin_addr;
  unsigned int darwin_len = 0;
  const void* out_addr = 0;

  if (dest_addr != 0) {
    if (to_darwin_sockaddr(dest_addr, addrlen, &darwin_addr, &darwin_len) != 0) {
      return -1;
    }
    out_addr = &darwin_addr;
  }
  return (ssize_t)normalize_socket_result(
      __crt_sys_sendto(sockfd, buf, (unsigned long)len, flags, out_addr, darwin_len));
#else
  return (ssize_t)normalize_socket_result(
      __crt_sys_sendto(sockfd, buf, (unsigned long)len, flags, dest_addr, (unsigned int)addrlen));
#endif
}

ssize_t send(int sockfd, const void* buf, size_t len, int flags) {
  return sendto(sockfd, buf, len, flags, 0, 0);
}

ssize_t recvfrom(
    int sockfd,
    void* buf,
    size_t len,
    int flags,
    struct sockaddr* src_addr,
    socklen_t* addrlen) {
#if defined(CRT_TARGET_OS_MACOS)
  struct crt_darwin_sockaddr_in darwin_addr;
  unsigned int darwin_len = sizeof(darwin_addr);
  long result = __crt_sys_recvfrom(
      sockfd,
      buf,
      (unsigned long)len,
      flags,
      src_addr != 0 ? &darwin_addr : 0,
      addrlen != 0 ? &darwin_len : 0);

  if (result < 0 && result >= -4095) {
    return (ssize_t)__set_errno((int)-result);
  }
  if (src_addr != 0 && addrlen != 0) {
    from_darwin_sockaddr(&darwin_addr, src_addr, addrlen);
  }
  return (ssize_t)result;
#else
  unsigned int len_inout = addrlen != 0 ? (unsigned int)*addrlen : 0;
  long result = __crt_sys_recvfrom(
      sockfd,
      buf,
      (unsigned long)len,
      flags,
      src_addr,
      addrlen != 0 ? &len_inout : 0);
  if (result < 0 && result >= -4095) {
    return (ssize_t)__set_errno((int)-result);
  }
  if (addrlen != 0) {
    *addrlen = (socklen_t)len_inout;
  }
  return (ssize_t)result;
#endif
}

ssize_t recv(int sockfd, void* buf, size_t len, int flags) {
  return recvfrom(sockfd, buf, len, flags, 0, 0);
}

int getsockname(int sockfd, struct sockaddr* addr, socklen_t* addrlen) {
#if defined(CRT_TARGET_OS_MACOS)
  struct crt_darwin_sockaddr_in darwin_addr;
  unsigned int darwin_len = sizeof(darwin_addr);
  long result;

  if (addr == 0 || addrlen == 0) {
    errno = EINVAL;
    return -1;
  }
  result = __crt_sys_getsockname(sockfd, &darwin_addr, &darwin_len);
  if (result < 0 && result >= -4095) {
    return (int)__set_errno((int)-result);
  }
  from_darwin_sockaddr(&darwin_addr, addr, addrlen);
  return (int)result;
#else
  unsigned int len = addrlen != 0 ? (unsigned int)*addrlen : 0;
  long result;

  if (addr == 0 || addrlen == 0) {
    errno = EINVAL;
    return -1;
  }
  result = __crt_sys_getsockname(sockfd, addr, &len);
  if (result < 0 && result >= -4095) {
    return (int)__set_errno((int)-result);
  }
  *addrlen = (socklen_t)len;
  return (int)result;
#endif
}

int setsockopt(int sockfd, int level, int optname, const void* optval, socklen_t optlen) {
#if defined(CRT_TARGET_OS_MACOS)
  if (level == SOL_SOCKET) {
    level = CRT_DARWIN_SOL_SOCKET;
    if (optname == SO_REUSEADDR) {
      optname = CRT_DARWIN_SO_REUSEADDR;
    }
  }
#endif
  return (int)normalize_socket_result(
      __crt_sys_setsockopt(sockfd, level, optname, optval, (unsigned int)optlen));
}

int shutdown(int sockfd, int how) {
  return (int)normalize_socket_result(__crt_sys_shutdown(sockfd, how));
}

static int parse_service(const char* service, in_port_t* port) {
  unsigned long value = 0;

  if (service == 0 || *service == 0) {
    *port = 0;
    return 0;
  }
  while (*service != 0) {
    if (*service < '0' || *service > '9') {
      return EAI_SERVICE;
    }
    value = value * 10UL + (unsigned long)(*service - '0');
    if (value > 65535UL) {
      return EAI_SERVICE;
    }
    ++service;
  }
  *port = htons((uint16_t)value);
  return 0;
}

int getaddrinfo(
    const char* node,
    const char* service,
    const struct addrinfo* hints,
    struct addrinfo** res) {
  struct addrinfo* ai;
  struct sockaddr_in* addr;
  in_port_t port;
  int family = hints != 0 ? hints->ai_family : AF_UNSPEC;
  int socktype = hints != 0 ? hints->ai_socktype : 0;
  int protocol = hints != 0 ? hints->ai_protocol : 0;
  int service_result;

  if (res == 0) {
    return EAI_FAIL;
  }
  *res = 0;
  if (family != AF_UNSPEC && family != AF_INET) {
    return EAI_FAMILY;
  }
  service_result = parse_service(service, &port);
  if (service_result != 0) {
    return service_result;
  }

  ai = (struct addrinfo*)calloc(1, sizeof(*ai));
  addr = (struct sockaddr_in*)calloc(1, sizeof(*addr));
  if (ai == 0 || addr == 0) {
    free(ai);
    free(addr);
    return EAI_MEMORY;
  }

  addr->sin_family = AF_INET;
  addr->sin_port = port;
  if (node == 0 || node[0] == 0) {
    addr->sin_addr.s_addr =
        (hints != 0 && (hints->ai_flags & AI_PASSIVE) != 0) ? htonl(INADDR_ANY) : htonl(INADDR_LOOPBACK);
  } else if (inet_pton(AF_INET, node, &addr->sin_addr) != 1) {
    free(ai);
    free(addr);
    return EAI_NONAME;
  }

  ai->ai_family = AF_INET;
  ai->ai_socktype = socktype;
  ai->ai_protocol = protocol;
  ai->ai_addrlen = (socklen_t)sizeof(*addr);
  ai->ai_addr = (struct sockaddr*)addr;
  *res = ai;
  return 0;
}

void freeaddrinfo(struct addrinfo* res) {
  while (res != 0) {
    struct addrinfo* next = res->ai_next;
    free(res->ai_addr);
    free(res->ai_canonname);
    free(res);
    res = next;
  }
}

const char* gai_strerror(int errcode) {
  switch (errcode) {
    case 0:
      return "Success";
    case EAI_BADFLAGS:
      return "Bad flags";
    case EAI_NONAME:
      return "Name or service not known";
    case EAI_AGAIN:
      return "Temporary failure in name resolution";
    case EAI_FAIL:
      return "Non-recoverable failure in name resolution";
    case EAI_FAMILY:
      return "Address family not supported";
    case EAI_SOCKTYPE:
      return "Socket type not supported";
    case EAI_SERVICE:
      return "Service not supported";
    case EAI_MEMORY:
      return "Memory allocation failure";
    case EAI_SYSTEM:
      return "System error";
    default:
      return "Unknown getaddrinfo error";
  }
}
