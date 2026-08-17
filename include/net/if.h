#ifndef CRT_NET_IF_H
#define CRT_NET_IF_H

#define IFNAMSIZ 16

/*
 * struct ifaddrs.ifa_flags values. The low common bits below (IFF_UP
 * through IFF_RUNNING) share the same numeric values across the BSD/
 * Darwin/Linux lineage, so ifaddrs.c's macOS backend (which copies
 * ifa_flags verbatim from the real Darwin getifaddrs()) reports them
 * correctly too -- this is not an exhaustive or guaranteed bit-for-bit
 * mapping for every possible flag on every host, just for this common
 * subset.
 */
#define IFF_UP 0x1
#define IFF_BROADCAST 0x2
#define IFF_DEBUG 0x4
#define IFF_LOOPBACK 0x8
#define IFF_POINTOPOINT 0x10
#define IFF_RUNNING 0x40
#define IFF_NOARP 0x80
#define IFF_PROMISC 0x100
#define IFF_MULTICAST 0x1000

struct if_nameindex {
  unsigned int if_index;
  char* if_name;
};

#ifdef __cplusplus
extern "C" {
#endif

unsigned int if_nametoindex(const char* ifname);
char* if_indextoname(unsigned int ifindex, char* ifname);
struct if_nameindex* if_nameindex(void);
void if_freenameindex(struct if_nameindex* ptr);

#ifdef __cplusplus
}
#endif

#endif
