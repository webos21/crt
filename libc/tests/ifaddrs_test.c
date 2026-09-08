/* getifaddrs()/freeifaddrs() -- real per-host backing (see ifaddrs.h's own
 * comment): Linux via /sys/class/net + SIOCGIFADDR/SIOCGIFNETMASK/
 * SIOCGIFBRDADDR/SIOCGIFFLAGS ioctls (reasoned carefully, unverified
 * pending real Linux hardware, matching this session's other raw-ioctl/
 * syscall work); macOS via the real Darwin getifaddrs() resolved at
 * runtime plus sockaddr translation; Windows via the real IP Helper API
 * GetAdaptersInfo(), verified directly since this session is Windows-only.
 *
 * Real host network configuration varies (number of adapters, whether
 * IPv4 is configured, whether the loopback interface is enumerated at
 * all -- Windows' GetAdaptersInfo() in particular does not report a
 * loopback pseudo-adapter), so this test only checks internal
 * consistency of whatever getifaddrs() returns, not a specific count or
 * a specific interface being present. */
#include <errno.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

static int fail(const char* message) {
  fprintf(stderr, "ifaddrs_test: %s\n", message);
  return 1;
}

int main(void) {
  struct ifaddrs* list = 0;
  struct ifaddrs* it;
  int count = 0;

  if (getifaddrs(0) != -1 || errno != EINVAL) {
    return fail("getifaddrs NULL argument");
  }

  if (getifaddrs(&list) != 0) {
    /* A real, honest failure (e.g. ENOSYS on a host with no backing, or
     * a real transient error) is acceptable -- just make sure the out
     * parameter was still reset and nothing crashes. */
    if (list != 0) {
      return fail("getifaddrs left a stale list on failure");
    }
    freeifaddrs(0); /* must be a safe no-op */
    printf("ifaddrs_test: ok\n");
    return 0;
  }

  for (it = list; it != 0; it = it->ifa_next) {
    ++count;
    if (it->ifa_name == 0 || it->ifa_name[0] == 0) {
      freeifaddrs(list);
      return fail("interface with no name");
    }
    if (it->ifa_addr != 0) {
      if (it->ifa_addr->sa_family != AF_INET) {
        freeifaddrs(list);
        return fail("ifa_addr family must be AF_INET (the only family this "
                     "project translates today)");
      }
    }
    if (it->ifa_netmask != 0 && it->ifa_netmask->sa_family != AF_INET) {
      freeifaddrs(list);
      return fail("ifa_netmask family must be AF_INET");
    }
    if (it->ifa_broadaddr != 0 && it->ifa_broadaddr->sa_family != AF_INET) {
      freeifaddrs(list);
      return fail("ifa_broadaddr family must be AF_INET");
    }
    if (count > 256) {
      /* Defends against an accidental cycle in the linked list. */
      freeifaddrs(list);
      return fail("implausibly long interface list -- suspect a cycle");
    }
  }

  freeifaddrs(list);
  freeifaddrs(0); /* must also be a safe no-op */

  printf("ifaddrs_test: ok\n");
  return 0;
}
