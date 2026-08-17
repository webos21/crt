#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

static struct ifaddrs* crt_ifaddrs_new_node(const char* name) {
  struct ifaddrs* node = (struct ifaddrs*)calloc(1, sizeof(*node));

  if (node == 0) {
    return 0;
  }
  node->ifa_name = strdup(name);
  if (node->ifa_name == 0) {
    free(node);
    return 0;
  }
  return node;
}

static void crt_ifaddrs_append(struct ifaddrs** head, struct ifaddrs** tail, struct ifaddrs* node) {
  if (*tail == 0) {
    *head = node;
  } else {
    (*tail)->ifa_next = node;
  }
  *tail = node;
}

void freeifaddrs(struct ifaddrs* ifa) {
  while (ifa != 0) {
    struct ifaddrs* next = ifa->ifa_next;

    free(ifa->ifa_name);
    free(ifa->ifa_addr);
    free(ifa->ifa_netmask);
    free(ifa->ifa_broadaddr);
    free(ifa);
    ifa = next;
  }
}

#if defined(CRT_TARGET_OS_LINUX)

/*
 * A deliberately over-sized private ifreq shape: only ifr_name (offset 0,
 * IFNAMSIZ bytes, matching the real kernel layout exactly) and the first
 * bytes of the union (where a struct sockaddr or a short flags word
 * lands) are ever read back by this file. The trailing padding just
 * guarantees the buffer handed to ioctl() is at least as large as
 * whatever the real kernel's ifr_ifru union turns out to be, without
 * this file needing to know that size exactly -- unlike SIOCGIFCONF's
 * packed-array-of-ifreq return, a single-ifreq ioctl call doesn't depend
 * on this struct's sizeof() matching the kernel's, only on it being big
 * enough. Interface enumeration itself goes through /sys/class/net
 * (real, layout-independent) instead of SIOCGIFCONF for the same reason.
 */
struct crt_linux_ifreq {
  char ifr_name[IFNAMSIZ];
  union {
    struct sockaddr addr;
    short flags;
    char reserved[32];
  } ifr_ifru;
};

#define CRT_SIOCGIFFLAGS 0x8913
#define CRT_SIOCGIFADDR 0x8915
#define CRT_SIOCGIFNETMASK 0x891b
#define CRT_SIOCGIFBRDADDR 0x8919

static struct sockaddr* crt_linux_ioctl_addr(int fd, const char* name, unsigned long request) {
  struct crt_linux_ifreq req;
  struct sockaddr* result;

  memset(&req, 0, sizeof(req));
  strncpy(req.ifr_name, name, IFNAMSIZ - 1);
  if (ioctl(fd, (int)request, &req) != 0) {
    return 0;
  }
  result = (struct sockaddr*)malloc(sizeof(struct sockaddr_in));
  if (result == 0) {
    return 0;
  }
  memcpy(result, &req.ifr_ifru.addr, sizeof(struct sockaddr_in));
  return result;
}

int getifaddrs(struct ifaddrs** ifap) {
  int fd;
  DIR* dir;
  struct dirent* entry;
  struct ifaddrs* head = 0;
  struct ifaddrs* tail = 0;

  if (ifap == 0) {
    errno = EINVAL;
    return -1;
  }
  *ifap = 0;

  dir = opendir("/sys/class/net");
  if (dir == 0) {
    return -1;
  }
  fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    closedir(dir);
    return -1;
  }

  while ((entry = readdir(dir)) != 0) {
    struct ifaddrs* node;
    struct crt_linux_ifreq flags_req;

    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }
    node = crt_ifaddrs_new_node(entry->d_name);
    if (node == 0) {
      goto fail;
    }

    memset(&flags_req, 0, sizeof(flags_req));
    strncpy(flags_req.ifr_name, entry->d_name, IFNAMSIZ - 1);
    if (ioctl(fd, CRT_SIOCGIFFLAGS, &flags_req) == 0) {
      node->ifa_flags = (unsigned int)(unsigned short)flags_req.ifr_ifru.flags;
    }

    node->ifa_addr = crt_linux_ioctl_addr(fd, entry->d_name, CRT_SIOCGIFADDR);
    node->ifa_netmask = crt_linux_ioctl_addr(fd, entry->d_name, CRT_SIOCGIFNETMASK);
    if ((node->ifa_flags & IFF_BROADCAST) != 0) {
      node->ifa_broadaddr = crt_linux_ioctl_addr(fd, entry->d_name, CRT_SIOCGIFBRDADDR);
    }

    crt_ifaddrs_append(&head, &tail, node);
  }

  close(fd);
  closedir(dir);
  *ifap = head;
  return 0;

fail:
  close(fd);
  closedir(dir);
  freeifaddrs(head);
  errno = ENOMEM;
  return -1;
}

#elif defined(CRT_TARGET_OS_MACOS)
#include <private/crt_macho_symbol.h>

/* Darwin's real struct sockaddr_in (see socket.c's own copy of this same
 * shape and comment): every Darwin sockaddr starts with a 1-byte length
 * then a 1-byte family, unlike this project's own Bionic/Linux-shaped
 * struct sockaddr. */
struct crt_darwin_sockaddr_in {
  unsigned char sin_len;
  unsigned char sin_family;
  unsigned short sin_port;
  struct in_addr sin_addr;
  unsigned char sin_zero[8];
};

struct crt_darwin_sockaddr_header {
  unsigned char sa_len;
  unsigned char sa_family;
};

/* Darwin's real struct ifaddrs: identical field shape/order/alignment to
 * this project's own struct ifaddrs above (same pointer widths, same
 * natural alignment on LP64) -- only the sockaddr* payloads it points to
 * are in Darwin's native (sa_len-prefixed) layout and need translating. */
struct crt_darwin_ifaddrs {
  struct crt_darwin_ifaddrs* ifa_next;
  char* ifa_name;
  unsigned int ifa_flags;
  void* ifa_addr;
  void* ifa_netmask;
  void* ifa_dstaddr;
  void* ifa_data;
};

typedef int (*crt_darwin_getifaddrs_fn)(struct crt_darwin_ifaddrs**);
typedef void (*crt_darwin_freeifaddrs_fn)(struct crt_darwin_ifaddrs*);

#define CRT_RTLD_NEXT ((void*)-1)
void* dlsym(void* handle, const char* symbol);

static struct sockaddr* crt_translate_darwin_sockaddr_in(const void* darwin_addr) {
  const struct crt_darwin_sockaddr_header* header;
  const struct crt_darwin_sockaddr_in* src;
  struct sockaddr_in* out;

  if (darwin_addr == 0) {
    return 0;
  }
  header = (const struct crt_darwin_sockaddr_header*)darwin_addr;
  if (header->sa_family != AF_INET) {
    return 0;
  }
  out = (struct sockaddr_in*)malloc(sizeof(*out));
  if (out == 0) {
    return 0;
  }
  src = (const struct crt_darwin_sockaddr_in*)darwin_addr;
  memset(out, 0, sizeof(*out));
  out->sin_family = (sa_family_t)src->sin_family;
  out->sin_port = src->sin_port;
  out->sin_addr = src->sin_addr;
  return out;
}

int getifaddrs(struct ifaddrs** ifap) {
  static crt_darwin_getifaddrs_fn real_getifaddrs;
  static crt_darwin_freeifaddrs_fn real_freeifaddrs;
  static int resolved;
  static int available;
  struct crt_darwin_ifaddrs* darwin_head = 0;
  struct crt_darwin_ifaddrs* it;
  struct ifaddrs* head = 0;
  struct ifaddrs* tail = 0;

  if (ifap == 0) {
    errno = EINVAL;
    return -1;
  }
  *ifap = 0;

  if (!resolved) {
    resolved = 1;
    real_getifaddrs = (crt_darwin_getifaddrs_fn)__crt_macho_find_symbol_in_loaded_image(
        "/usr/lib/system/libsystem_info.dylib", "getifaddrs");
    real_freeifaddrs = (crt_darwin_freeifaddrs_fn)__crt_macho_find_symbol_in_loaded_image(
        "/usr/lib/system/libsystem_info.dylib", "freeifaddrs");
    if (real_getifaddrs == 0) {
      real_getifaddrs = (crt_darwin_getifaddrs_fn)dlsym(CRT_RTLD_NEXT, "getifaddrs");
    }
    if (real_freeifaddrs == 0) {
      real_freeifaddrs = (crt_darwin_freeifaddrs_fn)dlsym(CRT_RTLD_NEXT, "freeifaddrs");
    }
    if (real_getifaddrs == (crt_darwin_getifaddrs_fn)getifaddrs) {
      real_getifaddrs = 0;
    }
    if (real_freeifaddrs == (crt_darwin_freeifaddrs_fn)freeifaddrs) {
      real_freeifaddrs = 0;
    }
    available = real_getifaddrs != 0 && real_freeifaddrs != 0;
  }
  if (!available) {
    errno = ENOSYS;
    return -1;
  }

  if (real_getifaddrs(&darwin_head) != 0) {
    return -1;
  }

  for (it = darwin_head; it != 0; it = it->ifa_next) {
    struct ifaddrs* node = crt_ifaddrs_new_node(it->ifa_name);

    if (node == 0) {
      goto fail;
    }
    node->ifa_flags = it->ifa_flags;
    node->ifa_addr = crt_translate_darwin_sockaddr_in(it->ifa_addr);
    node->ifa_netmask = crt_translate_darwin_sockaddr_in(it->ifa_netmask);
    node->ifa_broadaddr = crt_translate_darwin_sockaddr_in(it->ifa_dstaddr);
    crt_ifaddrs_append(&head, &tail, node);
  }

  real_freeifaddrs(darwin_head);
  *ifap = head;
  return 0;

fail:
  real_freeifaddrs(darwin_head);
  freeifaddrs(head);
  errno = ENOMEM;
  return -1;
}

#elif defined(CRT_TARGET_OS_WINDOWS)
typedef unsigned long DWORD;
typedef unsigned int UINT;
typedef int BOOL;
typedef unsigned char BYTE;
typedef void* HMODULE;

#if defined(_M_IX86) || defined(__i386__)
#define CRT_WINAPI __stdcall
#else
#define CRT_WINAPI
#endif

#define CRT_MAX_ADAPTER_NAME_LENGTH 256
#define CRT_MAX_ADAPTER_DESCRIPTION_LENGTH 128
#define CRT_MAX_ADAPTER_ADDRESS_LENGTH 8
#define CRT_ERROR_SUCCESS 0UL
#define CRT_ERROR_BUFFER_OVERFLOW 111UL

struct crt_win_ip_address_string {
  char string[16];
};

struct crt_win_ip_addr_string {
  struct crt_win_ip_addr_string* next;
  struct crt_win_ip_address_string ip_address;
  struct crt_win_ip_address_string ip_mask;
  DWORD context;
};

/* Matches the real Win32 IP_ADAPTER_INFO layout (iptypes.h) field for
 * field -- a small, non-versioned, long-stable struct (unlike the much
 * larger IP_ADAPTER_ADDRESSES used by the newer GetAdaptersAddresses()),
 * chosen specifically because its fixed, simple shape is much lower risk
 * to reason about correctly from documentation than a large versioned
 * struct would be. IPv4-only, which is why this backend is IPv4-only on
 * Windows for a real reason, not just this file's own AF_INET-only
 * translation scope on the other two hosts. */
struct crt_win_ip_adapter_info {
  struct crt_win_ip_adapter_info* next;
  DWORD combo_index;
  char adapter_name[CRT_MAX_ADAPTER_NAME_LENGTH + 4];
  char description[CRT_MAX_ADAPTER_DESCRIPTION_LENGTH + 4];
  UINT address_length;
  BYTE address[CRT_MAX_ADAPTER_ADDRESS_LENGTH];
  DWORD index;
  UINT type;
  UINT dhcp_enabled;
  struct crt_win_ip_addr_string* current_ip_address;
  struct crt_win_ip_addr_string ip_address_list;
  struct crt_win_ip_addr_string gateway_list;
  struct crt_win_ip_addr_string dhcp_server;
  BOOL have_wins;
  struct crt_win_ip_addr_string primary_wins_server;
  struct crt_win_ip_addr_string secondary_wins_server;
  long lease_obtained;
  long lease_expires;
};

typedef DWORD(CRT_WINAPI* crt_get_adapters_info_fn)(struct crt_win_ip_adapter_info*, DWORD*);

__declspec(dllimport) HMODULE CRT_WINAPI LoadLibraryA(const char* name);
__declspec(dllimport) void* CRT_WINAPI GetProcAddress(HMODULE module, const char* name);

static struct sockaddr* crt_win_parse_ipv4(const char* text) {
  struct sockaddr_in* out;

  if (text == 0 || text[0] == 0) {
    return 0;
  }
  out = (struct sockaddr_in*)malloc(sizeof(*out));
  if (out == 0) {
    return 0;
  }
  memset(out, 0, sizeof(*out));
  out->sin_family = AF_INET;
  if (inet_pton(AF_INET, text, &out->sin_addr) != 1) {
    free(out);
    return 0;
  }
  return (struct sockaddr*)out;
}

int getifaddrs(struct ifaddrs** ifap) {
  static crt_get_adapters_info_fn get_adapters_info;
  static int resolved;
  DWORD size = 0;
  DWORD result;
  struct crt_win_ip_adapter_info* buffer;
  struct crt_win_ip_adapter_info* it;
  struct ifaddrs* head = 0;
  struct ifaddrs* tail = 0;

  if (ifap == 0) {
    errno = EINVAL;
    return -1;
  }
  *ifap = 0;

  if (!resolved) {
    HMODULE module;

    resolved = 1;
    module = LoadLibraryA("iphlpapi.dll");
    if (module != 0) {
      get_adapters_info =
          (crt_get_adapters_info_fn)GetProcAddress(module, "GetAdaptersInfo");
    }
  }
  if (get_adapters_info == 0) {
    errno = ENOSYS;
    return -1;
  }

  result = get_adapters_info(0, &size);
  if (result != CRT_ERROR_BUFFER_OVERFLOW || size == 0) {
    errno = ENOSYS;
    return -1;
  }
  buffer = (struct crt_win_ip_adapter_info*)malloc(size);
  if (buffer == 0) {
    errno = ENOMEM;
    return -1;
  }
  result = get_adapters_info(buffer, &size);
  if (result != CRT_ERROR_SUCCESS) {
    free(buffer);
    errno = ENOSYS;
    return -1;
  }

  for (it = buffer; it != 0; it = it->next) {
    struct ifaddrs* node = crt_ifaddrs_new_node(it->adapter_name);
    struct crt_win_ip_addr_string* addr_entry;

    if (node == 0) {
      free(buffer);
      goto fail;
    }
    node->ifa_flags = IFF_UP | IFF_RUNNING;
    addr_entry = &it->ip_address_list;
    if (addr_entry->ip_address.string[0] != 0) {
      node->ifa_addr = crt_win_parse_ipv4(addr_entry->ip_address.string);
      node->ifa_netmask = crt_win_parse_ipv4(addr_entry->ip_mask.string);
      if (node->ifa_addr != 0 &&
          strncmp(addr_entry->ip_address.string, "127.", 4) == 0) {
        node->ifa_flags |= IFF_LOOPBACK;
      }
    }
    crt_ifaddrs_append(&head, &tail, node);
  }

  free(buffer);
  *ifap = head;
  return 0;

fail:
  freeifaddrs(head);
  errno = ENOMEM;
  return -1;
}

#else

int getifaddrs(struct ifaddrs** ifap) {
  if (ifap != 0) {
    *ifap = 0;
  }
  errno = ENOSYS;
  return -1;
}

#endif
