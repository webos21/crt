#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <private/crt_fd_table.h>
#include <private/crt_seqpacket.h>
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

/* Darwin's struct sockaddr_in6: a length byte then a 1-byte family where this project's
 * Bionic-shaped one has a 2-byte family; the rest is the same 28 bytes. */
struct crt_darwin_sockaddr_in6 {
  unsigned char sin6_len;
  unsigned char sin6_family;
  in_port_t sin6_port;
  uint32_t sin6_flowinfo;
  struct in6_addr sin6_addr;
  uint32_t sin6_scope_id;
};

/* Darwin's AF_INET6 (30); Bionic/Linux numbers it 10, which on Darwin is AF_CHAOS-era space. */
#define CRT_DARWIN_AF_INET6 30

/* Darwin's real struct sockaddr_un (<sys/un.h>): unlike this project's own
 * Bionic/Linux-shaped struct sockaddr_un (sys/un.h, no length prefix,
 * 108-byte sun_path), every Darwin sockaddr variant starts with a 1-byte
 * length then a 1-byte family (struct sockaddr's real layout), and
 * sun_path is only 104 bytes. */
struct crt_darwin_sockaddr_un {
  unsigned char sun_len;
  unsigned char sun_family;
  char sun_path[104];
};

/* Generic header shared by every Darwin sockaddr variant (sa_len then
 * sa_family, both 1 byte -- see struct crt_darwin_sockaddr_un's comment
 * above), used to read back which concrete type a syscall filled in
 * without caring which union member it actually is. */
struct crt_darwin_sockaddr_header {
  unsigned char sa_len;
  unsigned char sa_family;
};

/* Only AF_INET/AF_INET6/AF_UNIX are translatable today -- see to_darwin_sockaddr()'s
 * own behavior below. Sized to fit whichever Darwin variant is larger
 * (currently sockaddr_un), so it's always safe to hand syscalls that fill
 * in an address (accept()/recvfrom()/getsockname()/recvmsg()) a pointer to
 * one of these regardless of which family the peer turns out to be. */
union crt_darwin_sockaddr_storage {
  struct crt_darwin_sockaddr_in in;
  struct crt_darwin_sockaddr_in6 in6;
  struct crt_darwin_sockaddr_un un;
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
#define CRT_DARWIN_SO_TYPE 0x1008
#define CRT_DARWIN_AI_NUMERICSERV 0x00001000
#endif

long __crt_sys_socket(int domain, int type, int protocol);
long __crt_sys_socketpair(int domain, int type, int protocol, int* sv);
#if defined(CRT_TARGET_OS_MACOS)
long __crt_sys_close(int fd);
long __crt_sys_read(int fd, void* buf, unsigned long count);
#endif
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
long __crt_sys_getpeername(int sockfd, void* addr, unsigned int* addrlen);
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

/* Dotted-quad text to four network-order bytes; the whole string must be consumed. */
static int inet_pton4_bytes(const char* src, unsigned char out[4]) {
  const char* cursor = src;
  unsigned int octets[4];
  int i;

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
  for (i = 0; i < 4; ++i) {
    out[i] = (unsigned char)octets[i];
  }
  return 1;
}

static int hex_digit_value(int ch) {
  if (ch >= '0' && ch <= '9') {
    return ch - '0';
  }
  if (ch >= 'a' && ch <= 'f') {
    return ch - 'a' + 10;
  }
  if (ch >= 'A' && ch <= 'F') {
    return ch - 'A' + 10;
  }
  return -1;
}

/* RFC 4291 text form: eight 16-bit groups, one "::" run, optional trailing dotted quad. */
static int inet_pton6_bytes(const char* src, unsigned char dst[16]) {
  unsigned char tmp[16];
  unsigned char* tp = tmp;
  unsigned char* const endp = tmp + 16;
  unsigned char* colonp = 0;
  const char* curtok;
  unsigned int val = 0;
  int seen_xdigits = 0;
  int ch;

  memset(tmp, 0, sizeof(tmp));
  if (*src == ':') {
    if (*++src != ':') {
      return 0;
    }
  }
  curtok = src;
  while ((ch = (unsigned char)*src++) != 0) {
    int digit = hex_digit_value(ch);

    if (digit >= 0) {
      val = (val << 4) | (unsigned int)digit;
      if (++seen_xdigits > 4) {
        return 0;
      }
      continue;
    }
    if (ch == ':') {
      curtok = src;
      if (!seen_xdigits) {
        if (colonp != 0) {
          return 0;
        }
        colonp = tp;
        continue;
      }
      if (*src == 0 || tp + 2 > endp) {
        return 0;
      }
      *tp++ = (unsigned char)(val >> 8);
      *tp++ = (unsigned char)(val & 0xffU);
      seen_xdigits = 0;
      val = 0;
      continue;
    }
    if (ch == '.' && tp + 4 <= endp && inet_pton4_bytes(curtok, tp)) {
      tp += 4;
      seen_xdigits = 0;
      break;
    }
    return 0;
  }
  if (seen_xdigits) {
    if (tp + 2 > endp) {
      return 0;
    }
    *tp++ = (unsigned char)(val >> 8);
    *tp++ = (unsigned char)(val & 0xffU);
  }
  if (colonp != 0) {
    const int n = (int)(tp - colonp);
    int i;

    if (tp == endp) {
      return 0;
    }
    for (i = 1; i <= n; ++i) {
      endp[-i] = colonp[n - i];
      colonp[n - i] = 0;
    }
    tp = endp;
  }
  if (tp != endp) {
    return 0;
  }
  memcpy(dst, tmp, sizeof(tmp));
  return 1;
}

const struct in6_addr in6addr_any = IN6ADDR_ANY_INIT;
const struct in6_addr in6addr_loopback = IN6ADDR_LOOPBACK_INIT;

int inet_pton(int af, const char* src, void* dst) {
  unsigned char bytes[4];

  if (af != AF_INET && af != AF_INET6) {
    errno = EAFNOSUPPORT;
    return -1;
  }
  if (src == 0 || dst == 0) {
    errno = EINVAL;
    return -1;
  }
  if (af == AF_INET6) {
    return inet_pton6_bytes(src, ((struct in6_addr*)dst)->s6_addr);
  }
  if (!inet_pton4_bytes(src, bytes)) {
    return 0;
  }
  memcpy(&((struct in_addr*)dst)->s_addr, bytes, 4);
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

static char* append_hex16(char* out, unsigned int value) {
  static const char digits[] = "0123456789abcdef";
  int shift;
  int started = 0;

  for (shift = 12; shift >= 0; shift -= 4) {
    const unsigned int nibble = (value >> shift) & 0xfU;

    if (nibble != 0 || started || shift == 0) {
      *out++ = digits[nibble];
      started = 1;
    }
  }
  return out;
}

/* RFC 5952 text form: lowercase, the longest run (>= 2) of zero groups compressed to "::", an
 * IPv4-mapped address as "::ffff:a.b.c.d". */
static void inet_ntop6_text(const unsigned char* a, char* out) {
  unsigned int words[8];
  int best_base = -1;
  int best_len = 0;
  int cur_base = -1;
  int cur_len = 0;
  int i;

  for (i = 0; i < 8; ++i) {
    words[i] = ((unsigned int)a[i * 2] << 8) | a[i * 2 + 1];
  }
  for (i = 0; i < 8; ++i) {
    if (words[i] == 0) {
      if (cur_base < 0) {
        cur_base = i;
        cur_len = 1;
      } else {
        ++cur_len;
      }
    } else if (cur_base >= 0) {
      if (cur_len > best_len) {
        best_base = cur_base;
        best_len = cur_len;
      }
      cur_base = -1;
    }
  }
  if (cur_base >= 0 && cur_len > best_len) {
    best_base = cur_base;
    best_len = cur_len;
  }
  if (best_len < 2) {
    best_base = -1;
  }
  for (i = 0; i < 8; ++i) {
    if (best_base >= 0 && i >= best_base && i < best_base + best_len) {
      if (i == best_base) {
        *out++ = ':';
      }
      continue;
    }
    if (i != 0) {
      *out++ = ':';
    }
    if (i == 6 && best_base == 0 && (best_len == 6 || (best_len == 5 && words[5] == 0xffffU))) {
      out = append_decimal(out, a[12]);
      *out++ = '.';
      out = append_decimal(out, a[13]);
      *out++ = '.';
      out = append_decimal(out, a[14]);
      *out++ = '.';
      out = append_decimal(out, a[15]);
      break;
    }
    out = append_hex16(out, words[i]);
  }
  if (best_base >= 0 && best_base + best_len == 8) {
    *out++ = ':';
  }
  *out = 0;
}

const char* inet_ntop(int af, const void* src, char* dst, socklen_t size) {
  uint32_t addr;
  unsigned int octets[4];
  char tmp[INET6_ADDRSTRLEN];
  char* out = tmp;
  int i;
  size_t len;

  if (af != AF_INET && af != AF_INET6) {
    errno = EAFNOSUPPORT;
    return 0;
  }
  if (src == 0 || dst == 0) {
    errno = EINVAL;
    return 0;
  }
  if (af == AF_INET6) {
    inet_ntop6_text(((const struct in6_addr*)src)->s6_addr, tmp);
    len = strlen(tmp);
    if (size <= len) {
      errno = ENOSPC;
      return 0;
    }
    memcpy(dst, tmp, len + 1);
    return dst;
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
    union crt_darwin_sockaddr_storage* out,
    unsigned int* outlen) {
  if (addr == 0 || out == 0 || outlen == 0) {
    errno = EINVAL;
    return -1;
  }
  if (addr->sa_family == AF_INET) {
    const struct sockaddr_in* in;

    if (addrlen < sizeof(struct sockaddr_in)) {
      errno = EINVAL;
      return -1;
    }
    in = (const struct sockaddr_in*)addr;
    memset(&out->in, 0, sizeof(out->in));
    out->in.sin_len = (unsigned char)sizeof(out->in);
    out->in.sin_family = AF_INET;
    out->in.sin_port = in->sin_port;
    out->in.sin_addr = in->sin_addr;
    memcpy(out->in.sin_zero, in->sin_zero, sizeof(out->in.sin_zero));
    *outlen = (unsigned int)sizeof(out->in);
    return 0;
  }
  if (addr->sa_family == AF_INET6) {
    const struct sockaddr_in6* in6;

    if (addrlen < sizeof(struct sockaddr_in6)) {
      errno = EINVAL;
      return -1;
    }
    in6 = (const struct sockaddr_in6*)addr;
    memset(&out->in6, 0, sizeof(out->in6));
    out->in6.sin6_len = (unsigned char)sizeof(out->in6);
    out->in6.sin6_family = CRT_DARWIN_AF_INET6;
    out->in6.sin6_port = in6->sin6_port;
    out->in6.sin6_flowinfo = in6->sin6_flowinfo;
    out->in6.sin6_addr = in6->sin6_addr;
    out->in6.sin6_scope_id = in6->sin6_scope_id;
    *outlen = (unsigned int)sizeof(out->in6);
    return 0;
  }
  if (addr->sa_family == AF_UNIX) {
    const struct sockaddr_un* in;
    size_t path_len;

    if (addrlen < offsetof(struct sockaddr_un, sun_path)) {
      errno = EINVAL;
      return -1;
    }
    in = (const struct sockaddr_un*)addr;
    /* addrlen may cover less than the full sun_path array (the common
     * convention: SUN_LEN()-style callers only pass strlen(sun_path)+1
     * worth of it), so bound the copy by whichever is smaller -- what the
     * caller actually says is valid, or a NUL terminator found first. */
    path_len = strnlen(
        in->sun_path,
        (size_t)addrlen - offsetof(struct sockaddr_un, sun_path) < sizeof(in->sun_path)
            ? (size_t)addrlen - offsetof(struct sockaddr_un, sun_path)
            : sizeof(in->sun_path));
    if (path_len >= sizeof(out->un.sun_path)) {
      /* Darwin's sun_path (104 bytes) is 4 bytes shorter than this
       * project's own Bionic-shaped one (108 bytes); a path that only
       * fits in the larger one is a real, if unusual, error here. */
      errno = ENAMETOOLONG;
      return -1;
    }
    memset(&out->un, 0, sizeof(out->un));
    out->un.sun_len = (unsigned char)(offsetof(struct crt_darwin_sockaddr_un, sun_path) + path_len + 1);
    out->un.sun_family = AF_UNIX;
    memcpy(out->un.sun_path, in->sun_path, path_len);
    *outlen = (unsigned int)out->un.sun_len;
    return 0;
  }
  errno = EAFNOSUPPORT;
  return -1;
}

/* sendmsg()/recvmsg() pass msg_control's ancillary-data bytes straight
 * through to the raw host kernel syscall (unlike msg_name, there is no
 * dedicated struct to translate into -- a cmsghdr chain is caller-defined,
 * variable-length data). setsockopt()/getsockopt() already translate
 * SOL_SOCKET (this project's own Bionic/Linux value, 1) to/from Darwin's
 * real value (CRT_DARWIN_SOL_SOCKET, 0xffff) for the socket-option path,
 * but that translation never extended to a cmsghdr's own cmsg_level field
 * -- the one and only realistic ancillary-data use case here (SCM_RIGHTS
 * fd passing) always sets cmsg_level to SOL_SOCKET, and passing our
 * un-translated value straight to the real kernel makes it reject the
 * whole message with EINVAL (found via a real macOS sendmsg() failure
 * with an SCM_RIGHTS control message; the earlier struct msghdr field
 * width and CMSG_ALIGN() unit fixes above got the ancillary-data *layout*
 * byte-for-byte identical to a real host-native reference program, but
 * this cmsg_level mismatch remained until diffed against that reference
 * byte-by-byte). CMSG_FIRSTHDR()/CMSG_NXTHDR() (sys/socket.h) already walk
 * a cmsghdr chain generically, so this just runs that walk over a
 * translation copy instead of the caller's own buffer. */
static void translate_cmsg_levels(void* control, unsigned int controllen, int to_darwin) {
  struct msghdr tmp;
  struct cmsghdr* cmsg;

  memset(&tmp, 0, sizeof(tmp));
  tmp.msg_control = control;
  tmp.msg_controllen = controllen;
  for (cmsg = CMSG_FIRSTHDR(&tmp); cmsg != 0; cmsg = CMSG_NXTHDR(&tmp, cmsg)) {
    if (to_darwin && cmsg->cmsg_level == SOL_SOCKET) {
      cmsg->cmsg_level = CRT_DARWIN_SOL_SOCKET;
    } else if (!to_darwin && cmsg->cmsg_level == CRT_DARWIN_SOL_SOCKET) {
      cmsg->cmsg_level = SOL_SOCKET;
    }
  }
}

static void from_darwin_sockaddr(
    const union crt_darwin_sockaddr_storage* in,
    struct sockaddr* addr,
    socklen_t* addrlen) {
  const struct crt_darwin_sockaddr_header* header = (const struct crt_darwin_sockaddr_header*)in;

  if (addr == 0 || addrlen == 0) {
    return;
  }
  if (header->sa_family == AF_INET) {
    struct sockaddr_in* out;

    if (*addrlen < sizeof(struct sockaddr_in)) {
      return;
    }
    out = (struct sockaddr_in*)addr;
    memset(out, 0, sizeof(*out));
    out->sin_family = in->in.sin_family;
    out->sin_port = in->in.sin_port;
    out->sin_addr = in->in.sin_addr;
    memcpy(out->sin_zero, in->in.sin_zero, sizeof(out->sin_zero));
    *addrlen = (socklen_t)sizeof(*out);
    return;
  }
  if (header->sa_family == CRT_DARWIN_AF_INET6) {
    struct sockaddr_in6* out;

    if (*addrlen < sizeof(struct sockaddr_in6)) {
      return;
    }
    out = (struct sockaddr_in6*)addr;
    memset(out, 0, sizeof(*out));
    out->sin6_family = AF_INET6;
    out->sin6_port = in->in6.sin6_port;
    out->sin6_flowinfo = in->in6.sin6_flowinfo;
    out->sin6_addr = in->in6.sin6_addr;
    out->sin6_scope_id = in->in6.sin6_scope_id;
    *addrlen = (socklen_t)sizeof(*out);
    return;
  }
  if (header->sa_family == AF_UNIX) {
    struct sockaddr_un* out;
    size_t path_len = (size_t)in->un.sun_len - offsetof(struct crt_darwin_sockaddr_un, sun_path);

    if (*addrlen < offsetof(struct sockaddr_un, sun_path) || path_len > sizeof(in->un.sun_path)) {
      return;
    }
    out = (struct sockaddr_un*)addr;
    memset(out, 0, sizeof(*out));
    out->sun_family = AF_UNIX;
    memcpy(out->sun_path, in->un.sun_path, path_len);
    *addrlen = (socklen_t)(offsetof(struct sockaddr_un, sun_path) + path_len);
  }
}
#endif

#if defined(CRT_TARGET_OS_MACOS)
/* ---- Darwin message flags ---------------------------------------------------------------------
 * recv/send/recvmsg/sendmsg flags use Bionic's (Linux) numbers in this libc's headers; Darwin
 * numbers MSG_EOR, MSG_TRUNC, MSG_CTRUNC, MSG_WAITALL and MSG_DONTWAIT differently, so passing them
 * through meant MSG_DONTWAIT (0x40) reached the kernel as MSG_WAITALL. MSG_CMSG_CLOEXEC has no
 * Darwin equivalent and is applied to the received descriptors afterwards. */
#define CRT_DARWIN_MSG_OOB 0x1
#define CRT_DARWIN_MSG_PEEK 0x2
#define CRT_DARWIN_MSG_DONTROUTE 0x4
#define CRT_DARWIN_MSG_EOR 0x8
#define CRT_DARWIN_MSG_TRUNC 0x10
#define CRT_DARWIN_MSG_CTRUNC 0x20
#define CRT_DARWIN_MSG_WAITALL 0x40
#define CRT_DARWIN_MSG_DONTWAIT 0x80
#define CRT_DARWIN_MSG_NOSIGNAL 0x80000

static int to_darwin_msg_flags(int flags) {
  int out = 0;

  if (flags & MSG_OOB) out |= CRT_DARWIN_MSG_OOB;
  if (flags & MSG_PEEK) out |= CRT_DARWIN_MSG_PEEK;
  if (flags & MSG_DONTROUTE) out |= CRT_DARWIN_MSG_DONTROUTE;
  if (flags & MSG_EOR) out |= CRT_DARWIN_MSG_EOR;
  if (flags & MSG_TRUNC) out |= CRT_DARWIN_MSG_TRUNC;
  if (flags & MSG_WAITALL) out |= CRT_DARWIN_MSG_WAITALL;
  if (flags & MSG_DONTWAIT) out |= CRT_DARWIN_MSG_DONTWAIT;
  if (flags & MSG_NOSIGNAL) out |= CRT_DARWIN_MSG_NOSIGNAL;
  return out;
}

static int from_darwin_msg_flags(int flags) {
  int out = 0;

  if (flags & CRT_DARWIN_MSG_OOB) out |= MSG_OOB;
  if (flags & CRT_DARWIN_MSG_EOR) out |= MSG_EOR;
  if (flags & CRT_DARWIN_MSG_TRUNC) out |= MSG_TRUNC;
  if (flags & CRT_DARWIN_MSG_CTRUNC) out |= MSG_CTRUNC;
  return out;
}

/* Darwin sets MSG_CTRUNC when the control buffer was too small but leaves the cmsg_len of the
 * (single, truncated) SCM_RIGHTS message at the size it *would* have had; Linux reports what was
 * delivered. Clamp it so a receiver that counts descriptors from cmsg_len never reads past the
 * buffer it gave, and keep msg_controllen within that buffer. */
static void clamp_truncated_control(struct msghdr* msg, size_t capacity) {
  struct cmsghdr* first;

  if ((msg->msg_flags & MSG_CTRUNC) == 0 || msg->msg_control == 0) {
    return;
  }
  if (capacity < CMSG_LEN(0)) {
    msg->msg_controllen = 0;
    return;
  }
  if (msg->msg_controllen > capacity) {
    msg->msg_controllen = (socklen_t)capacity;
  }
  first = (struct cmsghdr*)msg->msg_control;
  if (first->cmsg_len > capacity) {
    if (first->cmsg_level == CRT_DARWIN_SOL_SOCKET && first->cmsg_type == SCM_RIGHTS) {
      first->cmsg_len = (socklen_t)CMSG_LEN(((capacity - CMSG_LEN(0)) / sizeof(int)) * sizeof(int));
    } else {
      first->cmsg_len = (socklen_t)capacity;
    }
  }
}

/* Marks every descriptor in an SCM_RIGHTS message close-on-exec (MSG_CMSG_CLOEXEC emulation). */
static void cloexec_received_descriptors(struct msghdr* msg) {
  struct cmsghdr* cmsg;

  for (cmsg = CMSG_FIRSTHDR(msg); cmsg != 0; cmsg = CMSG_NXTHDR(msg, cmsg)) {
    if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS && cmsg->cmsg_len >= CMSG_LEN(0)) {
      const size_t count = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);
      const int* fds = (const int*)CMSG_DATA(cmsg);
      size_t i;

      for (i = 0; i < count; ++i) {
        (void)__crt_fd_set_cloexec(fds[i], 1);
      }
    }
  }
}

/* ---- AF_UNIX SOCK_SEQPACKET -------------------------------------------------------------------
 * Darwin's AF_UNIX has no sequenced-packet type (socket(2) says EPROTONOSUPPORT), and the CRT web
 * surface protocol (libcrtweb/platform/crtweb_surface_wire.h) needs record boundaries with
 * SCM_RIGHTS. So, as on Windows, the CRT provides it: a SOCK_STREAM socket whose records are a
 * 4-byte length followed by the payload; SCM_RIGHTS travels in the same sendmsg() as the length, so
 * Darwin delivers the descriptors with the first bytes of the record. Both ends must be CRT: the
 * framing is this libc's, not the kernel's.
 *
 * Which descriptors are framed is process state (a flag per fd, copied by dup, forgotten by close,
 * inherited by fork). It is not carried across exec: a program that inherits such a socket must
 * receive it by connect/accept or SCM_RIGHTS-free means and create its own, which is how the wire
 * protocol uses it. One thread should read a socket at a time (a record is not atomic against
 * another reader) and a non-blocking socket waits for the rest of a record once it started it.
 * MSG_PEEK is not provided. */
#define CRT_SEQPACKET_FD_MAX 4096
#define CRT_SEQPACKET_MAX_RECORD (64u * 1024u * 1024u)
#define CRT_SEQPACKET_MAX_IOV 64

static unsigned char seqpacket_flags[CRT_SEQPACKET_FD_MAX];

int __crt_seqpacket_fd(int fd) {
  return fd >= 0 && fd < CRT_SEQPACKET_FD_MAX && __atomic_load_n(&seqpacket_flags[fd], __ATOMIC_RELAXED) != 0;
}

static void seqpacket_set(int fd, int on) {
  if (fd >= 0 && fd < CRT_SEQPACKET_FD_MAX) {
    __atomic_store_n(&seqpacket_flags[fd], (unsigned char)(on != 0), __ATOMIC_RELAXED);
  }
}

void __crt_seqpacket_close(int fd) {
  seqpacket_set(fd, 0);
}

void __crt_seqpacket_dup(int oldfd, int newfd) {
  seqpacket_set(newfd, __crt_seqpacket_fd(oldfd));
}

static void seqpacket_wait(int fd, short events) {
  struct pollfd entry;

  entry.fd = fd;
  entry.events = events;
  entry.revents = 0;
  (void)poll(&entry, 1, -1);
}

/* Sends one record: `iov` is the caller's payload, `control` (may be 0) rides with the first byte.
 * Returns the payload length or -1 with errno. */
static ssize_t seqpacket_send(int fd, const struct iovec* iov, int iovcnt, void* control,
                              unsigned int controllen, int bionic_flags) {
  struct iovec vec[CRT_SEQPACKET_MAX_IOV + 1];
  uint32_t length = 0;
  uint32_t header;
  size_t total;
  size_t sent = 0;
  struct msghdr msg;
  int i;
  int dflags = to_darwin_msg_flags(bionic_flags) | CRT_DARWIN_MSG_NOSIGNAL;

  if (iovcnt < 0 || iovcnt > CRT_SEQPACKET_MAX_IOV) {
    errno = EMSGSIZE;
    return -1;
  }
  for (i = 0; i < iovcnt; ++i) {
    if (iov[i].iov_len > CRT_SEQPACKET_MAX_RECORD - length) {
      errno = EMSGSIZE;
      return -1;
    }
    length += (uint32_t)iov[i].iov_len;
  }
  header = length;
  vec[0].iov_base = &header;
  vec[0].iov_len = sizeof(header);
  for (i = 0; i < iovcnt; ++i) {
    vec[i + 1] = iov[i];
  }
  total = sizeof(header) + length;

  memset(&msg, 0, sizeof(msg));
  msg.msg_iov = vec;
  msg.msg_iovlen = iovcnt + 1;
  msg.msg_control = control;
  msg.msg_controllen = controllen;
  for (;;) {
    long r = __crt_sys_sendmsg(fd, &msg, dflags);
    size_t advance;

    if (r < 0) {
      if (r == -EAGAIN && sent != 0) {
        /* Half a record is on the wire: it has to be completed, whatever the socket's mode. */
        seqpacket_wait(fd, POLLOUT);
        continue;
      }
      if (r == -EINTR && sent != 0) {
        continue;
      }
      return (ssize_t)__set_errno((int)-r);
    }
    sent += (size_t)r;
    if (sent >= total) {
      return (ssize_t)length;
    }
    /* Partial: drop what went out and carry on without the control data (it left with the first
     * byte). */
    advance = (size_t)r;
    {
      int index = 0;

      while (index < msg.msg_iovlen && advance >= msg.msg_iov[index].iov_len) {
        advance -= msg.msg_iov[index].iov_len;
        ++index;
      }
      msg.msg_iov = &msg.msg_iov[index];
      msg.msg_iovlen -= index;
      if (advance != 0 && msg.msg_iovlen != 0) {
        msg.msg_iov[0].iov_base = (char*)msg.msg_iov[0].iov_base + advance;
        msg.msg_iov[0].iov_len -= advance;
      }
    }
    msg.msg_control = 0;
    msg.msg_controllen = 0;
  }
}

/* Reads exactly `count` bytes into `buffer` (or discards them when buffer is 0). Returns 1 done,
 * 0 end of stream before any byte, -1 error (EOF mid-record is ECONNRESET). */
static int seqpacket_read_exact(int fd, void* buffer, size_t count, int first_call_nonblocking,
                                int* got_any) {
  size_t done = 0;
  char scratch[1024];

  while (done < count) {
    size_t want = count - done;
    void* target = buffer != 0 ? (char*)buffer + done : scratch;
    long r;

    if (buffer == 0 && want > sizeof(scratch)) {
      want = sizeof(scratch);
    }
    r = __crt_sys_read(fd, target, (unsigned long)want);
    if (r == 0) {
      if (done == 0 && !*got_any) {
        return 0;
      }
      errno = ECONNRESET;
      return -1;
    }
    if (r < 0) {
      if (r == -EINTR) {
        continue;
      }
      if (r == -EAGAIN) {
        if (first_call_nonblocking && done == 0 && !*got_any) {
          errno = EAGAIN;
          return -1;
        }
        seqpacket_wait(fd, POLLIN);
        continue;
      }
      errno = (int)-r;
      return -1;
    }
    done += (size_t)r;
    *got_any = 1;
  }
  return 1;
}

static ssize_t seqpacket_recvmsg(int fd, struct msghdr* msg, int bionic_flags) {
  uint32_t length = 0;
  size_t have = 0;
  size_t capacity = 0;
  size_t copied = 0;
  int i;
  int got_any = 0;
  int truncated = 0;
  int dflags = to_darwin_msg_flags(bionic_flags & ~MSG_TRUNC);
  const size_t control_capacity = msg->msg_controllen;
  struct msghdr header_msg;
  struct iovec header_iov;

  if ((bionic_flags & MSG_PEEK) != 0) {
    errno = EOPNOTSUPP;
    return -1;
  }
  for (i = 0; i < msg->msg_iovlen; ++i) {
    capacity += msg->msg_iov[i].iov_len;
  }

  /* The length, with the ancillary data that came with its first byte. */
  while (have < sizeof(length)) {
    long r;

    memset(&header_msg, 0, sizeof(header_msg));
    header_iov.iov_base = (char*)&length + have;
    header_iov.iov_len = sizeof(length) - have;
    header_msg.msg_iov = &header_iov;
    header_msg.msg_iovlen = 1;
    if (have == 0) {
      header_msg.msg_control = msg->msg_control;
      header_msg.msg_controllen = msg->msg_controllen;
    }
    r = __crt_sys_recvmsg(fd, &header_msg, have == 0 ? dflags : 0);
    if (r == -EINTR) {
      continue;
    }
    if (r == -EAGAIN && have != 0) {
      seqpacket_wait(fd, POLLIN);
      continue;
    }
    if (r < 0) {
      return (ssize_t)__set_errno((int)-r);
    }
    if (r == 0) {
      if (have == 0) {
        msg->msg_controllen = 0;
        msg->msg_flags = 0;
        return 0;
      }
      errno = ECONNRESET;
      return -1;
    }
    if (have == 0) {
      msg->msg_controllen = header_msg.msg_controllen;
      msg->msg_flags = from_darwin_msg_flags(header_msg.msg_flags);
      clamp_truncated_control(msg, control_capacity);
      if (msg->msg_control != 0 && msg->msg_controllen > 0) {
        translate_cmsg_levels(msg->msg_control, msg->msg_controllen, 0);
        if ((bionic_flags & MSG_CMSG_CLOEXEC) != 0) {
          struct msghdr view;

          memset(&view, 0, sizeof(view));
          view.msg_control = msg->msg_control;
          view.msg_controllen = msg->msg_controllen;
          cloexec_received_descriptors(&view);
        }
      }
    }
    have += (size_t)r;
    got_any = 1;
  }
  if (length > CRT_SEQPACKET_MAX_RECORD) {
    errno = EPROTO;
    return -1;
  }

  for (i = 0; i < msg->msg_iovlen && copied < length; ++i) {
    size_t want = msg->msg_iov[i].iov_len;
    int status;

    if (want > length - copied) {
      want = length - copied;
    }
    status = seqpacket_read_exact(fd, msg->msg_iov[i].iov_base, want, 0, &got_any);
    if (status != 1) {
      if (status == 0) {
        errno = ECONNRESET;
      }
      return -1;
    }
    copied += want;
  }
  if (copied < length) {
    int status = seqpacket_read_exact(fd, 0, length - copied, 0, &got_any);

    truncated = 1;
    if (status != 1) {
      errno = ECONNRESET;
      return -1;
    }
  }
  if (truncated || length > capacity) {
    msg->msg_flags |= MSG_TRUNC;
  }
  /* Linux: with MSG_TRUNC among the call's flags the return value is the record's real length. */
  return (bionic_flags & MSG_TRUNC) != 0 ? (ssize_t)length : (ssize_t)copied;
}

ssize_t __crt_seqpacket_read(int fd, void* buf, size_t count) {
  struct iovec iov;
  struct msghdr msg;

  iov.iov_base = buf;
  iov.iov_len = count;
  memset(&msg, 0, sizeof(msg));
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  return seqpacket_recvmsg(fd, &msg, 0);
}

ssize_t __crt_seqpacket_write(int fd, const void* buf, size_t count) {
  struct iovec iov;

  iov.iov_base = (void*)buf;
  iov.iov_len = count;
  return seqpacket_send(fd, &iov, 1, 0, 0, 0);
}
#endif

int socket(int domain, int type, int protocol) {
#if defined(CRT_TARGET_OS_MACOS)
  /* Darwin's real socket(2) has no SOCK_CLOEXEC/SOCK_NONBLOCK bits in
   * `type` at all (that is a Linux/Bionic-only extension this project's
   * own <sys/socket.h> still defines, as SOCK_CLOEXEC == O_CLOEXEC, so
   * portable Linux-style callers -- e.g. curl's cf-socket.c doing
   * `socktype |= SOCK_CLOEXEC` whenever its own configure probe finds
   * the macro defined -- compile and link on macOS too). Passing that
   * bit straight through to the raw Darwin syscall corrupts `type` into
   * a value Darwin's kernel does not recognize at all, so the syscall
   * itself fails (observed: EPROTONOSUPPORT, sometimes surfacing as
   * other errno values depending on which unrecognized bits happen to
   * be set) -- it is never silently ignored. So: strip the bit before
   * the syscall and apply the same effect afterward via a real
   * fcntl(F_SETFD, FD_CLOEXEC), the same emulate-after-the-fact pattern
   * already used for pipe()'s own missing pipe2() on this OS (see
   * __crt_sys_eventfd2() above, and pipe() below). */
  int cloexec = (type & SOCK_CLOEXEC) != 0;
  int framed = domain == AF_UNIX && (type & ~SOCK_CLOEXEC) == SOCK_SEQPACKET;
  long result = __crt_sys_socket(domain == AF_INET6 ? CRT_DARWIN_AF_INET6 : domain,
                                 framed ? SOCK_STREAM : (type & ~SOCK_CLOEXEC), protocol);
  int fd = (int)normalize_socket_result(result);
  if (fd >= CRT_SEQPACKET_FD_MAX && framed) {
    __crt_sys_close(fd);
    return (int)__set_errno(EMFILE);
  }
  if (fd >= 0 && cloexec && __crt_fd_set_cloexec(fd, 1) != 0) {
    int saved_errno = errno;
    __crt_sys_close(fd);
    return (int)__set_errno(saved_errno);
  }
  if (fd >= 0) {
    seqpacket_set(fd, framed);
  }
  return fd;
#else
  return (int)normalize_socket_result(__crt_sys_socket(domain, type, protocol));
#endif
}

/* socketpair(2): Linux passes straight through (SOCK_CLOEXEC is a kernel type flag there). Darwin
 * has no such flag, so it is stripped and applied with fcntl() to both ends, as socket() does.
 * Which socket types a domain supports is the host kernel's call (AF_UNIX SOCK_SEQPACKET: Linux
 * only); an unsupported combination is an ordinary errno. Windows: ENOSYS. */
int socketpair(int domain, int type, int protocol, int sv[2]) {
  if (sv == 0) {
    return (int)__set_errno(EFAULT);
  }
#if defined(CRT_TARGET_OS_MACOS)
  {
    int cloexec = (type & SOCK_CLOEXEC) != 0;
    int framed = domain == AF_UNIX && (type & ~SOCK_CLOEXEC) == SOCK_SEQPACKET;
    int pair[2] = {-1, -1};
    long result = __crt_sys_socketpair(domain, framed ? SOCK_STREAM : (type & ~SOCK_CLOEXEC), protocol, pair);
    int status = (int)normalize_socket_result(result);
    if (status != 0) {
      return status;
    }
    if (framed && (pair[0] >= CRT_SEQPACKET_FD_MAX || pair[1] >= CRT_SEQPACKET_FD_MAX)) {
      __crt_sys_close(pair[0]);
      __crt_sys_close(pair[1]);
      return (int)__set_errno(EMFILE);
    }
    if (cloexec && (__crt_fd_set_cloexec(pair[0], 1) != 0 || __crt_fd_set_cloexec(pair[1], 1) != 0)) {
      int saved_errno = errno;
      __crt_sys_close(pair[0]);
      __crt_sys_close(pair[1]);
      return (int)__set_errno(saved_errno);
    }
    seqpacket_set(pair[0], framed);
    seqpacket_set(pair[1], framed);
    sv[0] = pair[0];
    sv[1] = pair[1];
    return 0;
  }
#else
  return (int)normalize_socket_result(__crt_sys_socketpair(domain, type, protocol, sv));
#endif
}

int bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
#if defined(CRT_TARGET_OS_MACOS)
  union crt_darwin_sockaddr_storage darwin_addr;
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
  union crt_darwin_sockaddr_storage darwin_addr;
  unsigned int darwin_len = sizeof(darwin_addr);
  long result;

  result = __crt_sys_accept(sockfd, addr != 0 ? &darwin_addr : 0, addrlen != 0 ? &darwin_len : 0);
  if (result < 0 && result >= -4095) {
    return (int)__set_errno((int)-result);
  }
  if (__crt_seqpacket_fd(sockfd)) {
    if (result >= CRT_SEQPACKET_FD_MAX) {
      __crt_sys_close((int)result);
      return (int)__set_errno(EMFILE);
    }
    seqpacket_set((int)result, 1);
  } else {
    seqpacket_set((int)result, 0);
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
  union crt_darwin_sockaddr_storage darwin_addr;
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
  union crt_darwin_sockaddr_storage darwin_addr;
  unsigned int darwin_len = 0;
  const void* out_addr = 0;

  if (__crt_seqpacket_fd(sockfd)) {
    struct iovec iov;

    if (dest_addr != 0) {
      errno = EISCONN;
      return -1;
    }
    iov.iov_base = (void*)buf;
    iov.iov_len = len;
    return seqpacket_send(sockfd, &iov, 1, 0, 0, flags);
  }
  if (dest_addr != 0) {
    if (to_darwin_sockaddr(dest_addr, addrlen, &darwin_addr, &darwin_len) != 0) {
      return -1;
    }
    out_addr = &darwin_addr;
  }
  return (ssize_t)normalize_socket_result(__crt_sys_sendto(
      sockfd, buf, (unsigned long)len, to_darwin_msg_flags(flags), out_addr, darwin_len));
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
  union crt_darwin_sockaddr_storage darwin_addr;
  unsigned int darwin_len = sizeof(darwin_addr);
  long result;

  if (__crt_seqpacket_fd(sockfd)) {
    struct iovec iov;
    struct msghdr msg;

    iov.iov_base = buf;
    iov.iov_len = len;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    if (addrlen != 0) {
      *addrlen = 0; /* a connected, nameless AF_UNIX peer */
    }
    return seqpacket_recvmsg(sockfd, &msg, flags);
  }
  result = __crt_sys_recvfrom(
      sockfd,
      buf,
      (unsigned long)len,
      to_darwin_msg_flags(flags),
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
  {
    /* AF_INET/AF_UNIX msg_name (see to_darwin_sockaddr()) and SOL_SOCKET-
     * level ancillary data (see translate_cmsg_levels() above) both need
     * Bionic->Darwin translation before this reaches the raw kernel
     * syscall -- independently of each other. The realistic near-term
     * sendmsg()+SCM_RIGHTS consumer (Wayland-style fd passing) always uses
     * an already-connected AF_UNIX socket, so msg_name == 0 there while
     * msg_control is set; a translation gated on msg_name alone (the
     * original version of this function) silently skipped cmsg_level
     * translation for exactly that case, producing a real EINVAL from the
     * kernel. Any family to_darwin_sockaddr() doesn't recognize fails with
     * EAFNOSUPPORT, same as bind()/connect()/sendto() on this host. */
    union crt_darwin_sockaddr_storage darwin_addr;
    unsigned char control_buf[512];
    struct msghdr translated = *msg;

    if (__crt_seqpacket_fd(sockfd)) {
      unsigned int controllen = 0;

      if (msg->msg_name != 0) {
        errno = EISCONN;
        return -1;
      }
      if (msg->msg_control != 0 && msg->msg_controllen > 0) {
        if (msg->msg_controllen > sizeof(control_buf)) {
          errno = ENOMEM;
          return -1;
        }
        memcpy(control_buf, msg->msg_control, msg->msg_controllen);
        translate_cmsg_levels(control_buf, (unsigned int)msg->msg_controllen, 1);
        controllen = (unsigned int)msg->msg_controllen;
      }
      return seqpacket_send(sockfd, msg->msg_iov, (int)msg->msg_iovlen,
                            controllen != 0 ? control_buf : 0, controllen, flags);
    }
    if (msg->msg_name != 0) {
      unsigned int darwin_len = 0;

      if (to_darwin_sockaddr(
              (const struct sockaddr*)msg->msg_name, msg->msg_namelen, &darwin_addr, &darwin_len) != 0) {
        return -1;
      }
      translated.msg_name = &darwin_addr;
      translated.msg_namelen = darwin_len;
    }
    if (msg->msg_control != 0 && msg->msg_controllen > 0) {
      if (msg->msg_controllen > sizeof(control_buf)) {
        errno = ENOMEM;
        return -1;
      }
      memcpy(control_buf, msg->msg_control, msg->msg_controllen);
      translate_cmsg_levels(control_buf, msg->msg_controllen, 1);
      translated.msg_control = control_buf;
      translated.msg_controllen = msg->msg_controllen;
    }
    return (ssize_t)normalize_socket_result(
        __crt_sys_sendmsg(sockfd, &translated, to_darwin_msg_flags(flags)));
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
  {
    union crt_darwin_sockaddr_storage darwin_addr;
    struct msghdr translated = *msg;
    long result;

    if (__crt_seqpacket_fd(sockfd)) {
      if (msg->msg_name != 0) {
        msg->msg_namelen = 0; /* a connected, nameless AF_UNIX peer */
      }
      return seqpacket_recvmsg(sockfd, msg, flags);
    }
    if (msg->msg_name != 0) {
      translated.msg_name = &darwin_addr;
      translated.msg_namelen = sizeof(darwin_addr);
    }
    /* translated.msg_control still points at the caller's own buffer (the
     * shallow *msg copy above didn't redirect it, unlike msg_name), so the
     * kernel fills it in place -- no separate copy-back needed here, just
     * an in-place Darwin->Bionic cmsg_level translation below. */
    result = __crt_sys_recvmsg(sockfd, &translated, to_darwin_msg_flags(flags));
    if (result < 0 && result >= -4095) {
      return (ssize_t)__set_errno((int)-result);
    }
    if (msg->msg_name != 0) {
      from_darwin_sockaddr(&darwin_addr, (struct sockaddr*)msg->msg_name, &msg->msg_namelen);
    }
    {
      const size_t control_capacity = msg->msg_controllen;

      msg->msg_controllen = translated.msg_controllen;
      msg->msg_flags = from_darwin_msg_flags(translated.msg_flags);
      clamp_truncated_control(msg, control_capacity);
    }
    if (msg->msg_control != 0 && msg->msg_controllen > 0) {
      translate_cmsg_levels(msg->msg_control, msg->msg_controllen, 0);
      if ((flags & MSG_CMSG_CLOEXEC) != 0) {
        cloexec_received_descriptors(msg);
      }
    }
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

/* getsockname()/getpeername(): the same shape (an address of the socket's own side or of its
 * peer), so one body over the raw call. Darwin returns its own sockaddr layout. */
static int socket_name_query(long (*query)(int, void*, unsigned int*), int sockfd, struct sockaddr* addr,
                             socklen_t* addrlen) {
#if defined(CRT_TARGET_OS_MACOS)
  union crt_darwin_sockaddr_storage darwin_addr;
  unsigned int darwin_len = sizeof(darwin_addr);
  long result;

  if (addr == 0 || addrlen == 0) {
    errno = EINVAL;
    return -1;
  }
  result = query(sockfd, &darwin_addr, &darwin_len);
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
  result = query(sockfd, addr, &len);
  if (result < 0 && result >= -4095) {
    return (int)__set_errno((int)-result);
  }
  *addrlen = (socklen_t)len;
  return (int)result;
#endif
}

int getsockname(int sockfd, struct sockaddr* addr, socklen_t* addrlen) {
  return socket_name_query(__crt_sys_getsockname, sockfd, addr, addrlen);
}

int getpeername(int sockfd, struct sockaddr* addr, socklen_t* addrlen) {
  return socket_name_query(__crt_sys_getpeername, sockfd, addr, addrlen);
}

#if defined(CRT_TARGET_OS_MACOS)
/* Bionic/Linux option number to Darwin's, for the options this libc declares. Returns -1 for an
 * option that has no translation, which the caller reports as ENOPROTOOPT rather than handing a
 * Linux number to a kernel that numbers it differently. */
static int darwin_sol_socket_option(int optname) {
  switch (optname) {
    case SO_REUSEADDR:
      return CRT_DARWIN_SO_REUSEADDR;
    case SO_KEEPALIVE:
      return 0x0008;
    case SO_BROADCAST:
      return 0x0020;
    case SO_LINGER:
      return 0x0080;
    case SO_SNDBUF:
      return 0x1001;
    case SO_RCVBUF:
      return 0x1002;
    case SO_ERROR:
      return CRT_DARWIN_SO_ERROR;
    case SO_TYPE:
      return CRT_DARWIN_SO_TYPE;
    default:
      return -1;
  }
}

/* IPPROTO_IPV6 options: the level is 41 on both, the option numbers are not. */
static int darwin_ipv6_option(int optname) {
  switch (optname) {
    case IPV6_UNICAST_HOPS:
      return 4;
    case IPV6_MULTICAST_IF:
      return 9;
    case IPV6_MULTICAST_HOPS:
      return 10;
    case IPV6_MULTICAST_LOOP:
      return 11;
    case IPV6_V6ONLY:
      return 27;
    default:
      return -1;
  }
}

/* Translates (level, optname) in place; 0 on success, -1 with errno set when untranslatable. */
static int darwin_socket_option(int* level, int* optname) {
  int translated;

  if (*level == SOL_SOCKET) {
    translated = darwin_sol_socket_option(*optname);
    if (translated < 0) {
      /* Keep the previous behaviour for anything not in the table: Darwin's own number if the
       * caller already used one is passed on, as before this table existed. */
      *level = CRT_DARWIN_SOL_SOCKET;
      return 0;
    }
    *level = CRT_DARWIN_SOL_SOCKET;
    *optname = translated;
    return 0;
  }
  if (*level == IPPROTO_IPV6) {
    translated = darwin_ipv6_option(*optname);
    if (translated < 0) {
      errno = ENOPROTOOPT;
      return -1;
    }
    *optname = translated;
  }
  return 0;
}
#endif

int setsockopt(int sockfd, int level, int optname, const void* optval, socklen_t optlen) {
#if defined(CRT_TARGET_OS_MACOS)
  if (darwin_socket_option(&level, &optname) != 0) {
    return -1;
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
  if (darwin_socket_option(&level, &optname) != 0) {
    return -1;
  }
#endif
#if defined(CRT_TARGET_OS_MACOS)
  if (level == CRT_DARWIN_SOL_SOCKET && optname == CRT_DARWIN_SO_TYPE && __crt_seqpacket_fd(sockfd) &&
      optval != 0 && len >= sizeof(int)) {
    *(int*)optval = SOCK_SEQPACKET;
    *optlen = (socklen_t)sizeof(int);
    return 0;
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

/* One result: a heap addrinfo owning a heap sockaddr, as freeaddrinfo() releases it. */
static struct addrinfo* new_addrinfo(int family, const void* sockaddr, socklen_t sockaddr_len,
                                     int socktype, int protocol) {
  struct addrinfo* ai = (struct addrinfo*)calloc(1, sizeof(*ai));
  void* copy = calloc(1, sockaddr_len);

  if (ai == 0 || copy == 0) {
    free(ai);
    free(copy);
    return 0;
  }
  memcpy(copy, sockaddr, sockaddr_len);
  ai->ai_family = family;
  ai->ai_socktype = socktype;
  ai->ai_protocol = protocol;
  ai->ai_addrlen = sockaddr_len;
  ai->ai_addr = (struct sockaddr*)copy;
  return ai;
}

/* IPv6 results are produced on every host: Linux natively, macOS and Windows through the
 * AF_INET6/sockaddr_in6/option translation at their socket boundary. */
#define CRT_GETADDRINFO_IPV6 1

#if defined(CRT_TARGET_OS_WINDOWS)
/* RFC 6761: "localhost" and "*.localhost" name the loopback interface and are answered without a
 * resolver, so the answer does not depend on the A-record client below (which carries no AAAA query,
 * and on Windows has no hosts file behind it). */
static int is_localhost_name(const char* node) {
  size_t length = strlen(node);
  static const char suffix[] = ".localhost";

  if (length > 0 && node[length - 1] == '.') {
    --length;
  }
  if (length == 9) {
    return strncasecmp(node, "localhost", 9) == 0;
  }
  return length > sizeof(suffix) - 1 && strncasecmp(node + length - (sizeof(suffix) - 1), suffix, sizeof(suffix) - 1) == 0;
}
#endif

#if defined(CRT_TARGET_OS_MACOS)
static void addrinfo_append(struct addrinfo** head, struct addrinfo** tail, struct addrinfo* item) {
  if (*head == 0) {
    *head = item;
  } else {
    (*tail)->ai_next = item;
  }
  *tail = item;
}

/* Resolves a host name with the macOS system resolver (libsystem_info) and returns every result
 * of the requested family as Bionic-shaped addrinfo entries. The host names are the only thing
 * taken from the host: families, sockaddr layouts, flags and the port are this libc's. Returns 0
 * with *out_head set, or -1 (no resolver symbol, lookup failure, or no result of that family). */
static int macos_host_resolve_hostname(const char* node, int family, int flags, int socktype,
                                       int protocol, in_port_t port, struct addrinfo** out_head) {
  static crt_darwin_getaddrinfo_fn real_getaddrinfo;
  static crt_darwin_freeaddrinfo_fn real_freeaddrinfo;
  static int real_symbols_resolved;
  static int real_symbols_available;
  struct crt_darwin_addrinfo hints;
  struct crt_darwin_addrinfo* results = 0;
  struct crt_darwin_addrinfo* it;
  struct addrinfo* head = 0;
  struct addrinfo* tail = 0;
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
  hints.ai_family = family == AF_INET6 ? CRT_DARWIN_AF_INET6 : family == AF_INET ? AF_INET : AF_UNSPEC;
  /* One entry per address: ask for a stream socket type even when the caller left it open, and
   * report the caller's own (possibly zero) type in the result as this resolver always has. */
  hints.ai_socktype = socktype != 0 ? socktype : SOCK_STREAM;
  hints.ai_protocol = protocol;
  /* Only the flags whose numbers are the same on both sides (AI_PASSIVE, AI_ADDRCONFIG, ...);
   * the service is numeric and applied here, so the host never parses it. */
  hints.ai_flags = (flags & (AI_ADDRCONFIG | AI_ALL | AI_V4MAPPED)) | CRT_DARWIN_AI_NUMERICSERV;
  rc = real_getaddrinfo(node, 0, &hints, &results);
  if (rc != 0) {
    return -1;
  }

  for (it = results; it != 0; it = it->ai_next) {
    struct addrinfo* entry = 0;

    if (it->ai_addr == 0) {
      continue;
    }
    if (it->ai_family == AF_INET && it->ai_addrlen >= (socklen_t)sizeof(struct crt_darwin_sockaddr_in) &&
        family != AF_INET6) {
      const struct crt_darwin_sockaddr_in* darwin_addr = (const struct crt_darwin_sockaddr_in*)it->ai_addr;
      struct sockaddr_in out;

      memset(&out, 0, sizeof(out));
      out.sin_family = AF_INET;
      out.sin_port = port;
      out.sin_addr = darwin_addr->sin_addr;
      entry = new_addrinfo(AF_INET, &out, (socklen_t)sizeof(out), socktype, protocol);
    } else if (it->ai_family == CRT_DARWIN_AF_INET6 &&
               it->ai_addrlen >= (socklen_t)sizeof(struct crt_darwin_sockaddr_in6) && family != AF_INET) {
      const struct crt_darwin_sockaddr_in6* darwin_addr =
          (const struct crt_darwin_sockaddr_in6*)it->ai_addr;
      struct sockaddr_in6 out;

      memset(&out, 0, sizeof(out));
      out.sin6_family = AF_INET6;
      out.sin6_port = port;
      out.sin6_flowinfo = darwin_addr->sin6_flowinfo;
      out.sin6_addr = darwin_addr->sin6_addr;
      out.sin6_scope_id = darwin_addr->sin6_scope_id;
      entry = new_addrinfo(AF_INET6, &out, (socklen_t)sizeof(out), socktype, protocol);
    } else {
      continue;
    }
    if (entry == 0) {
      real_freeaddrinfo(results);
      freeaddrinfo(head);
      return -1;
    }
    addrinfo_append(&head, &tail, entry);
  }
  real_freeaddrinfo(results);
  if (head == 0) {
    return -1;
  }
  *out_head = head;
  return 0;
}
#endif

/* A numeric IPv6 literal with an optional "%scope" (a numeric zone index). */
static int parse_ipv6_literal(const char* node, struct in6_addr* addr, uint32_t* scope_id) {
  char text[INET6_ADDRSTRLEN + 16];
  const char* percent = strchr(node, '%');
  size_t length = percent != 0 ? (size_t)(percent - node) : strlen(node);

  *scope_id = 0;
  if (length == 0 || length >= sizeof(text) || strchr(node, ':') == 0) {
    return 0;
  }
  memcpy(text, node, length);
  text[length] = 0;
  if (inet_pton(AF_INET6, text, addr) != 1) {
    return 0;
  }
  if (percent != 0) {
    unsigned long value = 0;
    const char* zone = percent + 1;

    if (*zone == 0) {
      return 0;
    }
    for (; *zone != 0; ++zone) {
      if (*zone < '0' || *zone > '9') {
        return 0; /* interface names need if_nametoindex(), which this libc does not carry */
      }
      value = value * 10UL + (unsigned long)(*zone - '0');
      if (value > 0xffffffffUL) {
        return 0;
      }
    }
    *scope_id = (uint32_t)value;
  }
  return 1;
}

int getaddrinfo(
    const char* node,
    const char* service,
    const struct addrinfo* hints,
    struct addrinfo** res) {
  struct sockaddr_in addr4;
  struct sockaddr_in6 addr6;
  struct in6_addr literal6;
  uint32_t scope_id = 0;
  in_port_t port;
  int family = hints != 0 ? hints->ai_family : AF_UNSPEC;
  int flags = hints != 0 ? hints->ai_flags : 0;
  int socktype = hints != 0 ? hints->ai_socktype : 0;
  int protocol = hints != 0 ? hints->ai_protocol : 0;
  int service_result;
  struct addrinfo* item;

  if (res == 0) {
    return EAI_FAIL;
  }
  *res = 0;
  if (hints != 0 && (hints->ai_flags & ~AI_MASK) != 0) {
    return EAI_BADFLAGS;
  }
  if (family != AF_UNSPEC && family != AF_INET && !(CRT_GETADDRINFO_IPV6 && family == AF_INET6)) {
    return EAI_FAMILY;
  }
  service_result = parse_service(service, &port);
  if (service_result != 0) {
    return service_result;
  }

  memset(&addr4, 0, sizeof(addr4));
  addr4.sin_family = AF_INET;
  addr4.sin_port = port;
  memset(&addr6, 0, sizeof(addr6));
  addr6.sin6_family = AF_INET6;
  addr6.sin6_port = port;

  if (node == 0 || node[0] == 0) {
    const int passive = (flags & AI_PASSIVE) != 0;

    if (CRT_GETADDRINFO_IPV6 && family == AF_INET6) {
      addr6.sin6_addr = passive ? in6addr_any : in6addr_loopback;
      item = new_addrinfo(AF_INET6, &addr6, (socklen_t)sizeof(addr6), socktype, protocol);
    } else {
      addr4.sin_addr.s_addr = passive ? htonl(INADDR_ANY) : htonl(INADDR_LOOPBACK);
      item = new_addrinfo(AF_INET, &addr4, (socklen_t)sizeof(addr4), socktype, protocol);
    }
    if (item == 0) {
      return EAI_MEMORY;
    }
    *res = item;
    return 0;
  }

  if (inet_pton(AF_INET, node, &addr4.sin_addr) == 1) {
    if (family == AF_INET6) {
      return EAI_NONAME;
    }
    item = new_addrinfo(AF_INET, &addr4, (socklen_t)sizeof(addr4), socktype, protocol);
    if (item == 0) {
      return EAI_MEMORY;
    }
    *res = item;
    return 0;
  }
  if (CRT_GETADDRINFO_IPV6 && parse_ipv6_literal(node, &literal6, &scope_id)) {
    if (family == AF_INET) {
      return EAI_NONAME;
    }
    addr6.sin6_addr = literal6;
    addr6.sin6_scope_id = scope_id;
    item = new_addrinfo(AF_INET6, &addr6, (socklen_t)sizeof(addr6), socktype, protocol);
    if (item == 0) {
      return EAI_MEMORY;
    }
    *res = item;
    return 0;
  }

  if ((flags & AI_NUMERICHOST) != 0) {
    return EAI_NONAME;
  }
#if defined(CRT_TARGET_OS_WINDOWS)
  if (is_localhost_name(node)) {
    struct addrinfo* first = 0;
    struct addrinfo* second = 0;

    if (family != AF_INET) {
      addr6.sin6_addr = in6addr_loopback;
      first = new_addrinfo(AF_INET6, &addr6, (socklen_t)sizeof(addr6), socktype, protocol);
      if (first == 0) {
        return EAI_MEMORY;
      }
    }
    if (family != AF_INET6) {
      addr4.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
      second = new_addrinfo(AF_INET, &addr4, (socklen_t)sizeof(addr4), socktype, protocol);
      if (second == 0) {
        freeaddrinfo(first);
        return EAI_MEMORY;
      }
    }
    if (first != 0) {
      first->ai_next = second;
    }
    *res = first != 0 ? first : second;
    return 0;
  }
#endif
#if defined(CRT_TARGET_OS_MACOS)
  if (macos_host_resolve_hostname(node, CRT_GETADDRINFO_IPV6 ? family : AF_INET, flags, socktype, protocol,
                                  port, res) == 0) {
    return 0;
  }
#endif
  /* Not a literal -- a real DNS lookup (the A-record client above) before giving up. It carries no
   * AAAA query, so an IPv6-only request for a name has nothing to answer with. */
  if (family == AF_INET6) {
    return EAI_NONAME;
  }
  if (dns_resolve_hostname(node, &addr4.sin_addr) != 0) {
    return EAI_NONAME;
  }
  item = new_addrinfo(AF_INET, &addr4, (socklen_t)sizeof(addr4), socktype, protocol);
  if (item == 0) {
    return EAI_MEMORY;
  }
  *res = item;
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
