#ifndef CRT_NETINET_IN_H
#define CRT_NETINET_IN_H

#include <stdint.h>
#include <sys/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef __crt_in_port_t in_port_t;
typedef __crt_in_addr_t in_addr_t;

struct in_addr {
  in_addr_t s_addr;
};

struct in6_addr {
  unsigned char s6_addr[16];
};

struct sockaddr_in {
  sa_family_t sin_family;
  in_port_t sin_port;
  struct in_addr sin_addr;
  unsigned char sin_zero[8];
};

struct sockaddr_in6 {
  sa_family_t sin6_family;
  in_port_t sin6_port;
  uint32_t sin6_flowinfo;
  struct in6_addr sin6_addr;
  uint32_t sin6_scope_id;
};

#define INADDR_ANY ((in_addr_t)0x00000000U)
#define INADDR_LOOPBACK ((in_addr_t)0x7f000001U)
#define INADDR_BROADCAST ((in_addr_t)0xffffffffU)
#define INADDR_NONE ((in_addr_t)0xffffffffU)

#define IPPROTO_IP 0
#define IPPROTO_ICMP 1
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17
#define IPPROTO_IPV6 41

/* IN6_IS_ADDR_* classification macros (RFC 3493/POSIX): pure bit tests on
 * struct in6_addr's bytes, no syscall involved. Real-world <netinet/in.h>
 * always ships these; curl's own lib/cf-socket.c needs LINKLOCAL for real
 * (a hard compile error otherwise, not gracefully feature-gated), and the
 * rest are the same trivial, standard set virtually every networking
 * consumer eventually reaches for. */
#define IN6_IS_ADDR_UNSPECIFIED(a)                                          \
  (((const unsigned char*)(a))[0] == 0 && ((const unsigned char*)(a))[1] == 0 && \
   ((const unsigned char*)(a))[2] == 0 && ((const unsigned char*)(a))[3] == 0 && \
   ((const unsigned char*)(a))[4] == 0 && ((const unsigned char*)(a))[5] == 0 && \
   ((const unsigned char*)(a))[6] == 0 && ((const unsigned char*)(a))[7] == 0 && \
   ((const unsigned char*)(a))[8] == 0 && ((const unsigned char*)(a))[9] == 0 && \
   ((const unsigned char*)(a))[10] == 0 && ((const unsigned char*)(a))[11] == 0 && \
   ((const unsigned char*)(a))[12] == 0 && ((const unsigned char*)(a))[13] == 0 && \
   ((const unsigned char*)(a))[14] == 0 && ((const unsigned char*)(a))[15] == 0)
#define IN6_IS_ADDR_LOOPBACK(a)                                             \
  (((const unsigned char*)(a))[0] == 0 && ((const unsigned char*)(a))[1] == 0 && \
   ((const unsigned char*)(a))[2] == 0 && ((const unsigned char*)(a))[3] == 0 && \
   ((const unsigned char*)(a))[4] == 0 && ((const unsigned char*)(a))[5] == 0 && \
   ((const unsigned char*)(a))[6] == 0 && ((const unsigned char*)(a))[7] == 0 && \
   ((const unsigned char*)(a))[8] == 0 && ((const unsigned char*)(a))[9] == 0 && \
   ((const unsigned char*)(a))[10] == 0 && ((const unsigned char*)(a))[11] == 0 && \
   ((const unsigned char*)(a))[12] == 0 && ((const unsigned char*)(a))[13] == 0 && \
   ((const unsigned char*)(a))[14] == 0 && ((const unsigned char*)(a))[15] == 1)
#define IN6_IS_ADDR_MULTICAST(a) (((const unsigned char*)(a))[0] == 0xff)
#define IN6_IS_ADDR_LINKLOCAL(a)                                            \
  (((const unsigned char*)(a))[0] == 0xfe && (((const unsigned char*)(a))[1] & 0xc0) == 0x80)
#define IN6_IS_ADDR_V4MAPPED(a)                                             \
  (((const unsigned char*)(a))[0] == 0 && ((const unsigned char*)(a))[1] == 0 && \
   ((const unsigned char*)(a))[2] == 0 && ((const unsigned char*)(a))[3] == 0 && \
   ((const unsigned char*)(a))[4] == 0 && ((const unsigned char*)(a))[5] == 0 && \
   ((const unsigned char*)(a))[6] == 0 && ((const unsigned char*)(a))[7] == 0 && \
   ((const unsigned char*)(a))[8] == 0 && ((const unsigned char*)(a))[9] == 0 && \
   ((const unsigned char*)(a))[10] == 0xff && ((const unsigned char*)(a))[11] == 0xff)

uint16_t htons(uint16_t hostshort);
uint16_t ntohs(uint16_t netshort);
uint32_t htonl(uint32_t hostlong);
uint32_t ntohl(uint32_t netlong);

#ifdef __cplusplus
}
#endif

#endif
