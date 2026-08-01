#ifndef CRT_NET_IF_H
#define CRT_NET_IF_H

#define IFNAMSIZ 16

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
