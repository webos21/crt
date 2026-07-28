#ifndef CRT_NETINET_IN_H
#define CRT_NETINET_IN_H

#include <stdint.h>
#include <sys/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint16_t in_port_t;
typedef uint32_t in_addr_t;

struct in_addr {
  in_addr_t s_addr;
};

struct sockaddr_in {
  sa_family_t sin_family;
  in_port_t sin_port;
  struct in_addr sin_addr;
  unsigned char sin_zero[8];
};

#define INADDR_ANY ((in_addr_t)0x00000000U)
#define INADDR_LOOPBACK ((in_addr_t)0x7f000001U)
#define INADDR_BROADCAST ((in_addr_t)0xffffffffU)
#define INADDR_NONE ((in_addr_t)0xffffffffU)

#define IPPROTO_IP 0
#define IPPROTO_ICMP 1
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17

uint16_t htons(uint16_t hostshort);
uint16_t ntohs(uint16_t netshort);
uint32_t htonl(uint32_t hostlong);
uint32_t ntohl(uint32_t netlong);

#ifdef __cplusplus
}
#endif

#endif
