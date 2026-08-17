#ifndef CRT_IFADDRS_H
#define CRT_IFADDRS_H

#include <sys/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ifaddrs {
  struct ifaddrs* ifa_next;
  char* ifa_name;
  unsigned int ifa_flags;
  struct sockaddr* ifa_addr;
  struct sockaddr* ifa_netmask;
  union {
    struct sockaddr* ifu_broadaddr;
    struct sockaddr* ifu_dstaddr;
  } ifa_ifu;
  void* ifa_data;
};

#define ifa_broadaddr ifa_ifu.ifu_broadaddr
#define ifa_dstaddr ifa_ifu.ifu_dstaddr

/*
 * Real per-host implementations (ifaddrs.c), IPv4 (AF_INET) only --
 * matching this project's existing scope limit for translating host-
 * native sockaddrs on macOS (see socket.c: "Only AF_INET/AF_UNIX are
 * translatable today"). AF_INET6/AF_LINK/AF_PACKET entries are skipped
 * rather than fabricated.
 *   - Linux: enumerates interface names via /sys/class/net (avoids any
 *     dependency on the exact real kernel struct ifreq/ifconf byte
 *     layout), then fills in address/netmask/broadcast/flags per
 *     interface via SIOCGIFADDR/SIOCGIFNETMASK/SIOCGIFBRDADDR/
 *     SIOCGIFFLAGS ioctls -- real, reasoned carefully from well-known
 *     stable UAPI ioctl numbers, flagged unverified pending real Linux
 *     hardware/CI, matching this session's established discipline.
 *   - macOS: calls the real Darwin getifaddrs()/freeifaddrs() (resolved
 *     at runtime, same technique as this project's getaddrinfo() macOS
 *     backend) and translates each Darwin-native (sa_len-prefixed)
 *     sockaddr into this project's own Bionic/Linux-shaped sockaddr.
 *   - Windows: real via the IP Helper API's GetAdaptersInfo() (iphlpapi
 *     .dll, loaded dynamically like this project's other optional
 *     Windows APIs), which is IPv4-only itself, so this project's
 *     Windows backend is IPv4-only for a real reason on that host, not
 *     just this project's own AF_INET-only translation scope.
 */
int getifaddrs(struct ifaddrs** ifap);
void freeifaddrs(struct ifaddrs* ifa);

#ifdef __cplusplus
}
#endif

#endif
