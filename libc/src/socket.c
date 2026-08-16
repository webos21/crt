#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <private/crt_tls.h>

#if defined(CRT_TARGET_OS_MACOS)
#include <private/crt_macho_symbol.h>

struct crt_darwin_sockaddr_in {
  unsigned char sin_len;
  unsigned char sin_family;
  in_port_t sin_port;
  struct in_addr sin_addr;
  unsigned char sin_zero[8];
};

struct crt_darwin_addrinfo {
  int ai_flags;
  int ai_family;
  int ai_socktype;
  int ai_protocol;
  socklen_t ai_addrlen;
  char* ai_canonname;
  struct sockaddr* ai_addr;
  struct crt_darwin_addrinfo* ai_next;
};

typedef int (*crt_darwin_getaddrinfo_fn)(
    const char*, const char*, const struct crt_darwin_addrinfo*, struct crt_darwin_addrinfo**);
typedef void (*crt_darwin_freeaddrinfo_fn)(struct crt_darwin_addrinfo*);

#define CRT_RTLD_NEXT ((void*)-1)

void* dlsym(void* handle, const char* symbol);

#define CRT_DARWIN_SOL_SOCKET 0xffff
#define CRT_DARWIN_SO_REUSEADDR 0x0004
#define CRT_DARWIN_SO_ERROR 0x1007
#define CRT_DARWIN_AI_NUMERICSERV 0x00001000
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
long __crt_sys_sendmsg(int sockfd, const struct msghdr* msg, int flags);
long __crt_sys_recvmsg(int sockfd, struct msghdr* msg, int flags);
long __crt_sys_getsockname(int sockfd, void* addr, unsigned int* addrlen);
long __crt_sys_setsockopt(int sockfd, int level, int optname, const void* optval, unsigned int optlen);
long __crt_sys_getsockopt(int sockfd, int level, int optname, void* optval, unsigned int* optlen);
long __crt_sys_shutdown(int sockfd, int how);

int* __get_h_errno(void) {
  return __crt_thread_h_errno();
}


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

ssize_t sendmsg(int sockfd, const struct msghdr* msg, int flags) {
  if (msg == 0) {
    errno = EFAULT;
    return -1;
  }
#if defined(CRT_TARGET_OS_MACOS)
  if (msg->msg_name != 0) {
    /* Only AF_INET is translatable today -- see to_darwin_sockaddr()'s own
     * comment/behavior above, already the exact same limitation bind()/
     * connect()/sendto() have on this host. The realistic near-term
     * sendmsg()+SCM_RIGHTS consumer (Wayland-style fd passing) always
     * uses an already-connected AF_UNIX socket with msg_name == 0 anyway,
     * so this isn't a new gap sendmsg() introduces. */
    struct crt_darwin_sockaddr_in darwin_addr;
    unsigned int darwin_len = 0;
    struct msghdr translated;

    if (to_darwin_sockaddr(
            (const struct sockaddr*)msg->msg_name, msg->msg_namelen, &darwin_addr, &darwin_len) != 0) {
      return -1;
    }
    translated = *msg;
    translated.msg_name = &darwin_addr;
    translated.msg_namelen = darwin_len;
    return (ssize_t)normalize_socket_result(__crt_sys_sendmsg(sockfd, &translated, flags));
  }
#endif
  return (ssize_t)normalize_socket_result(__crt_sys_sendmsg(sockfd, msg, flags));
}

ssize_t recvmsg(int sockfd, struct msghdr* msg, int flags) {
  if (msg == 0) {
    errno = EFAULT;
    return -1;
  }
#if defined(CRT_TARGET_OS_MACOS)
  if (msg->msg_name != 0) {
    struct crt_darwin_sockaddr_in darwin_addr;
    struct msghdr translated = *msg;
    long result;

    translated.msg_name = &darwin_addr;
    translated.msg_namelen = sizeof(darwin_addr);
    result = __crt_sys_recvmsg(sockfd, &translated, flags);
    if (result < 0 && result >= -4095) {
      return (ssize_t)__set_errno((int)-result);
    }
    from_darwin_sockaddr(&darwin_addr, (struct sockaddr*)msg->msg_name, &msg->msg_namelen);
    msg->msg_controllen = translated.msg_controllen;
    msg->msg_flags = translated.msg_flags;
    return (ssize_t)result;
  }
#endif
  {
    long result = __crt_sys_recvmsg(sockfd, msg, flags);

    if (result < 0 && result >= -4095) {
      return (ssize_t)__set_errno((int)-result);
    }
    return (ssize_t)result;
  }
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

int getsockopt(int sockfd, int level, int optname, void* optval, socklen_t* optlen) {
  unsigned int len;
  long result;

  if (optlen == 0) {
    errno = EFAULT;
    return -1;
  }
  len = (unsigned int)*optlen;
#if defined(CRT_TARGET_OS_MACOS)
  if (level == SOL_SOCKET) {
    level = CRT_DARWIN_SOL_SOCKET;
    if (optname == SO_REUSEADDR) {
      optname = CRT_DARWIN_SO_REUSEADDR;
    } else if (optname == SO_ERROR) {
      optname = CRT_DARWIN_SO_ERROR;
    }
  }
#endif
  result = __crt_sys_getsockopt(sockfd, level, optname, optval, &len);
  if (result < 0 && result >= -4095) {
    return (int)__set_errno((int)-result);
  }
  *optlen = (socklen_t)len;
  return (int)result;
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

/* --- Minimal DNS client -----------------------------------------------
 * getaddrinfo() previously only handled literal numeric IP addresses
 * (via inet_pton()) -- there was no real hostname resolution at all, a
 * gap invisible until curl (the first port in this project's own queue
 * to actually reach out to a real internet hostname rather than a
 * self-contained local round trip) needed it for real: a direct
 * getaddrinfo("example.com", ...) call returned EAI_NONAME immediately,
 * root-caused by reading this function's own source, not guessed.
 *
 * Scoped deliberately: a single synchronous UDP query for an A (IPv4)
 * record, with a couple of retries and a short timeout -- not the full
 * DNS client a real system resolver is (no AAAA/IPv6, no TCP fallback
 * for truncated responses, no search-domain suffix handling, no
 * caching). Sufficient for curl's own basic HTTP/HTTPS needs, the
 * immediate reason this was needed; a fuller resolver can grow from
 * here if a future port needs more.
 */
#define CRT_DNS_PORT 53
#define CRT_DNS_TIMEOUT_SEC 5
#define CRT_DNS_MAX_RETRIES 2
#define CRT_DNS_MSG_SIZE 512

/* Finds the first "nameserver X.X.X.X" line in /etc/resolv.conf. Falls
 * back to a public resolver (Google's 8.8.8.8) if the file is missing,
 * unreadable, or has no usable nameserver line -- the real, correct
 * source on Linux/macOS (which both ship a real /etc/resolv.conf this
 * PAL's own open()/read() can read directly, being real POSIX file
 * I/O against the real host filesystem), and a documented, deliberate
 * simplification on Windows (which has no such file at all -- a fuller
 * fix would query the OS's own configured adapter DNS servers via
 * IPHLPAPI's GetNetworkParams(), not attempted this pass since a
 * public resolver already works correctly from any host with a normal
 * internet connection, which is what this PAL's own port-build/fetch
 * machinery already depends on anyway). */
static void dns_get_nameserver(struct in_addr* out) {
  int fd;
  char buf[4096];
  ssize_t n;
  size_t total = 0;
  char* line;
  char* saveptr = 0;

  out->s_addr = 0;
  fd = open("/etc/resolv.conf", O_RDONLY);
  if (fd >= 0) {
    while (total < sizeof(buf) - 1) {
      n = read(fd, buf + total, sizeof(buf) - 1 - total);
      if (n <= 0) {
        break;
      }
      total += (size_t)n;
    }
    close(fd);
    buf[total] = 0;

    line = strtok_r(buf, "\n", &saveptr);
    while (line != 0) {
      while (*line == ' ' || *line == '\t') {
        ++line;
      }
      if (strncmp(line, "nameserver", 10) == 0 && (line[10] == ' ' || line[10] == '\t')) {
        char* p = line + 10;
        char ipbuf[64];
        size_t i = 0;
        while (*p == ' ' || *p == '\t') {
          ++p;
        }
        while (*p != 0 && *p != ' ' && *p != '\t' && *p != '\r' && i < sizeof(ipbuf) - 1) {
          ipbuf[i++] = *p++;
        }
        ipbuf[i] = 0;
        if (inet_pton(AF_INET, ipbuf, out) == 1) {
          return;
        }
      }
      line = strtok_r(0, "\n", &saveptr);
    }
  }
  inet_pton(AF_INET, "8.8.8.8", out);
}

/* Encodes `hostname` as DNS QNAME (length-prefixed labels, root-
 * terminated) plus a QTYPE=A/QCLASS=IN question, appended after a
 * standard 12-byte header with RD (recursion desired) set. */
static int dns_build_query(unsigned char* buf, size_t buf_size, const char* hostname, uint16_t query_id, size_t* out_len) {
  size_t pos;
  const char* p;

  if (buf_size < 12) {
    return -1;
  }
  buf[0] = (unsigned char)(query_id >> 8);
  buf[1] = (unsigned char)(query_id & 0xffU);
  buf[2] = 0x01; /* RD */
  buf[3] = 0x00;
  buf[4] = 0x00;
  buf[5] = 0x01; /* QDCOUNT=1 */
  buf[6] = 0x00;
  buf[7] = 0x00; /* ANCOUNT=0 */
  buf[8] = 0x00;
  buf[9] = 0x00; /* NSCOUNT=0 */
  buf[10] = 0x00;
  buf[11] = 0x00; /* ARCOUNT=0 */
  pos = 12;

  p = hostname;
  while (*p != 0) {
    const char* label_start = p;
    size_t label_len;
    while (*p != 0 && *p != '.') {
      ++p;
    }
    label_len = (size_t)(p - label_start);
    if (label_len == 0 || label_len > 63 || pos + 1 + label_len > buf_size) {
      return -1;
    }
    buf[pos++] = (unsigned char)label_len;
    memcpy(buf + pos, label_start, label_len);
    pos += label_len;
    if (*p == '.') {
      ++p;
    }
  }
  if (pos + 1 > buf_size) {
    return -1;
  }
  buf[pos++] = 0; /* root label */
  if (pos + 4 > buf_size) {
    return -1;
  }
  buf[pos++] = 0x00;
  buf[pos++] = 0x01; /* QTYPE=A */
  buf[pos++] = 0x00;
  buf[pos++] = 0x01; /* QCLASS=IN */
  *out_len = pos;
  return 0;
}

/* Advances past one DNS NAME field (either a length-prefixed label
 * sequence or a 2-byte compression pointer -- 0xC0 high bits), without
 * following/decoding compression pointers: this resolver only ever
 * needs to skip past NAMEs (the question echoed back, and each answer
 * record's own NAME) to reach the fixed-layout fields after them, never
 * to reconstruct the actual name string. */
static int dns_skip_name(const unsigned char* buf, size_t buf_len, size_t pos, size_t* out_pos) {
  while (pos < buf_len) {
    unsigned char len = buf[pos];
    if (len == 0) {
      *out_pos = pos + 1;
      return 0;
    }
    if ((len & 0xc0U) == 0xc0U) {
      if (pos + 2 > buf_len) {
        return -1;
      }
      *out_pos = pos + 2;
      return 0;
    }
    pos += 1 + (size_t)len;
  }
  return -1;
}

/* Parses a DNS response for the first A-record answer, verifying the
 * query ID echoes back, the response flag is set, and RCODE is 0
 * (success). Only IPv4 (TYPE=A/CLASS=IN, 4-byte RDATA) answers are
 * recognized -- see this DNS client's own top-of-section comment for
 * the deliberate AAAA/IPv6 scoping decision. */
static int dns_parse_response(const unsigned char* buf, size_t buf_len, uint16_t expected_id, struct in_addr* out) {
  uint16_t id, flags, qdcount, ancount;
  size_t pos;
  int i;

  if (buf_len < 12) {
    return -1;
  }
  id = (uint16_t)((buf[0] << 8) | buf[1]);
  if (id != expected_id) {
    return -1;
  }
  flags = (uint16_t)((buf[2] << 8) | buf[3]);
  if ((flags & 0x8000U) == 0 || (flags & 0x000fU) != 0) {
    return -1; /* not a response, or a nonzero RCODE (real failure) */
  }
  qdcount = (uint16_t)((buf[4] << 8) | buf[5]);
  ancount = (uint16_t)((buf[6] << 8) | buf[7]);

  pos = 12;
  for (i = 0; i < qdcount; ++i) {
    if (dns_skip_name(buf, buf_len, pos, &pos) != 0 || pos + 4 > buf_len) {
      return -1;
    }
    pos += 4; /* QTYPE + QCLASS */
  }

  for (i = 0; i < ancount; ++i) {
    uint16_t type, cls, rdlength;
    if (dns_skip_name(buf, buf_len, pos, &pos) != 0 || pos + 10 > buf_len) {
      return -1;
    }
    type = (uint16_t)((buf[pos] << 8) | buf[pos + 1]);
    cls = (uint16_t)((buf[pos + 2] << 8) | buf[pos + 3]);
    rdlength = (uint16_t)((buf[pos + 8] << 8) | buf[pos + 9]);
    pos += 10;
    if (pos + rdlength > buf_len) {
      return -1;
    }
    if (type == 1 && cls == 1 && rdlength == 4) {
      memcpy(&out->s_addr, buf + pos, 4);
      return 0;
    }
    pos += rdlength;
  }
  return -1;
}

/* Top-level synchronous resolve: builds a query, sends it over UDP to
 * the configured nameserver, and waits (via select(), so a nameserver
 * that never responds can't hang this function forever the way the
 * very first version of this fix -- discovered live, not planned --
 * would have) up to CRT_DNS_TIMEOUT_SEC per attempt, retrying up to
 * CRT_DNS_MAX_RETRIES times before giving up. */
static int dns_resolve_hostname(const char* hostname, struct in_addr* out) {
  struct in_addr nameserver;
  int sock;
  struct sockaddr_in server_addr;
  unsigned char query[CRT_DNS_MSG_SIZE];
  unsigned char response[CRT_DNS_MSG_SIZE];
  size_t query_len;
  uint16_t query_id;
  int attempt;
  int result = -1;

  dns_get_nameserver(&nameserver);

  query_id = (uint16_t)(random() & 0xffffL);
  if (dns_build_query(query, sizeof(query), hostname, query_id, &query_len) != 0) {
    return -1;
  }

  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    return -1;
  }

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(CRT_DNS_PORT);
  server_addr.sin_addr = nameserver;

  for (attempt = 0; attempt < CRT_DNS_MAX_RETRIES && result != 0; ++attempt) {
    fd_set readfds;
    struct timeval tv;
    ssize_t received;
    int sel;

    if (sendto(sock, query, query_len, 0, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
      continue;
    }

    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);
    tv.tv_sec = CRT_DNS_TIMEOUT_SEC;
    tv.tv_usec = 0;
    sel = select(sock + 1, &readfds, 0, 0, &tv);
    if (sel <= 0) {
      continue; /* timed out or interrupted -- retry */
    }

    received = recvfrom(sock, response, sizeof(response), 0, 0, 0);
    if (received < 12) {
      continue;
    }
    result = dns_parse_response(response, (size_t)received, query_id, out);
  }

  close(sock);
  return result;
}

#if defined(CRT_TARGET_OS_MACOS)
static int macos_host_resolve_hostname(
    const char* node,
    const char* service,
    int socktype,
    int protocol,
    struct sockaddr_in* out_addr) {
  static crt_darwin_getaddrinfo_fn real_getaddrinfo;
  static crt_darwin_freeaddrinfo_fn real_freeaddrinfo;
  static int real_symbols_resolved;
  static int real_symbols_available;
  struct crt_darwin_addrinfo hints;
  struct crt_darwin_addrinfo* results = 0;
  struct crt_darwin_addrinfo* it;
  int rc;

  if (!real_symbols_resolved) {
    real_symbols_resolved = 1;
    real_getaddrinfo = (crt_darwin_getaddrinfo_fn)__crt_macho_find_symbol_in_loaded_image(
        "/usr/lib/system/libsystem_info.dylib", "getaddrinfo");
    real_freeaddrinfo = (crt_darwin_freeaddrinfo_fn)__crt_macho_find_symbol_in_loaded_image(
        "/usr/lib/system/libsystem_info.dylib", "freeaddrinfo");
    if (real_getaddrinfo == 0) {
      real_getaddrinfo = (crt_darwin_getaddrinfo_fn)dlsym(CRT_RTLD_NEXT, "getaddrinfo");
    }
    if (real_freeaddrinfo == 0) {
      real_freeaddrinfo = (crt_darwin_freeaddrinfo_fn)dlsym(CRT_RTLD_NEXT, "freeaddrinfo");
    }
    if (real_getaddrinfo == (crt_darwin_getaddrinfo_fn)getaddrinfo) {
      real_getaddrinfo = 0;
    }
    if (real_freeaddrinfo == (crt_darwin_freeaddrinfo_fn)freeaddrinfo) {
      real_freeaddrinfo = 0;
    }
    real_symbols_available = real_getaddrinfo != 0 && real_freeaddrinfo != 0;
  }
  if (!real_symbols_available) {
    return -1;
  }

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = socktype;
  hints.ai_protocol = protocol;
  hints.ai_flags = CRT_DARWIN_AI_NUMERICSERV;
  rc = real_getaddrinfo(node, service, &hints, &results);
  if (rc != 0) {
    return -1;
  }

  for (it = results; it != 0; it = it->ai_next) {
    const struct crt_darwin_sockaddr_in* darwin_addr;
    if (it->ai_family != AF_INET || it->ai_addr == 0 ||
        it->ai_addrlen < (socklen_t)sizeof(struct crt_darwin_sockaddr_in)) {
      continue;
    }
    darwin_addr = (const struct crt_darwin_sockaddr_in*)it->ai_addr;
    memset(out_addr, 0, sizeof(*out_addr));
    out_addr->sin_family = AF_INET;
    out_addr->sin_port = darwin_addr->sin_port;
    out_addr->sin_addr = darwin_addr->sin_addr;
    real_freeaddrinfo(results);
    return 0;
  }

  real_freeaddrinfo(results);
  return -1;
}
#endif

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
  if (hints != 0 && (hints->ai_flags & ~AI_MASK) != 0) {
    return EAI_BADFLAGS;
  }
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
    if (hints != 0 && (hints->ai_flags & AI_NUMERICHOST) != 0) {
      free(ai);
      free(addr);
      return EAI_NONAME;
    }
#if defined(CRT_TARGET_OS_MACOS)
    if (macos_host_resolve_hostname(node, service, socktype, protocol, addr) == 0) {
      port = addr->sin_port;
    } else
#endif
    /* Not a literal IP -- try a real DNS lookup (see the DNS client
     * section above) before giving up. */
    if (dns_resolve_hostname(node, &addr->sin_addr) != 0) {
      free(ai);
      free(addr);
      return EAI_NONAME;
    }
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
