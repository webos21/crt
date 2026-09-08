#include <arpa/inet.h>
#include <android/api-level.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fenv.h>
#include <float.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

#if defined(CRT_TARGET_OS_MACOS)
#include <TargetConditionals.h>
#include <mach/mach.h>
#include <mach/vm_param.h>
#include <mach/machine/vm_param.h>
#endif

#define CRT_STATIC_ASSERT(name, expr) typedef char crt_static_assert_##name[(expr) ? 1 : -1]

CRT_STATIC_ASSERT(ssize_pointer_sized, sizeof(ssize_t) == sizeof(void*));
CRT_STATIC_ASSERT(off_64, sizeof(off_t) == 8);
CRT_STATIC_ASSERT(time_64, sizeof(time_t) == 8);
CRT_STATIC_ASSERT(dev_64, sizeof(dev_t) == 8);
CRT_STATIC_ASSERT(ino_64, sizeof(ino_t) == 8);
CRT_STATIC_ASSERT(nlink_64, sizeof(nlink_t) == 8);
CRT_STATIC_ASSERT(pid_32, sizeof(pid_t) == 4);
CRT_STATIC_ASSERT(socklen_32, sizeof(socklen_t) == 4);
CRT_STATIC_ASSERT(sa_family_16, sizeof(sa_family_t) == 2);
CRT_STATIC_ASSERT(in_port_16, sizeof(in_port_t) == 2);
CRT_STATIC_ASSERT(in_addr_32, sizeof(in_addr_t) == 4);
CRT_STATIC_ASSERT(fd_mask_ulong, sizeof(__fd_mask) == sizeof(unsigned long));
CRT_STATIC_ASSERT(pthread_t_pointer_sized, sizeof(pthread_t) == sizeof(void*));
CRT_STATIC_ASSERT(wchar_32, sizeof(wchar_t) == 4);
CRT_STATIC_ASSERT(stat_has_64_size, sizeof(((struct stat*)0)->st_size) == 8);
CRT_STATIC_ASSERT(timespec_has_64_sec, sizeof(((struct timespec*)0)->tv_sec) == 8);
CRT_STATIC_ASSERT(timeval_has_64_sec, sizeof(((struct timeval*)0)->tv_sec) == 8);
CRT_STATIC_ASSERT(sockaddr_size, sizeof(struct sockaddr) == 16);
CRT_STATIC_ASSERT(sockaddr_storage_size_bionic, sizeof(struct sockaddr_storage) == 128);
CRT_STATIC_ASSERT(sockaddr_storage_pointer_aligned, _Alignof(struct sockaddr_storage) == sizeof(void*));
CRT_STATIC_ASSERT(fd_set_1024, FD_SETSIZE == 1024);
CRT_STATIC_ASSERT(pollfd_layout, offsetof(struct pollfd, revents) > offsetof(struct pollfd, events));
CRT_STATIC_ASSERT(addrinfo_addrlen_socklen, sizeof(((struct addrinfo*)0)->ai_addrlen) == sizeof(socklen_t));
CRT_STATIC_ASSERT(statfs_has_64_blocks, sizeof(((struct statfs*)0)->f_blocks) == 8);
CRT_STATIC_ASSERT(statfs64_same_size, sizeof(struct statfs64) == sizeof(struct statfs));
CRT_STATIC_ASSERT(winsize_layout, sizeof(struct winsize) == 8);
CRT_STATIC_ASSERT(ioctl_fionread_linux_bionic, FIONREAD == 0x541b);
CRT_STATIC_ASSERT(ioctl_tiocgwinsz_linux_bionic, TIOCGWINSZ == 0x5413);
CRT_STATIC_ASSERT(ioctl_tiocswinsz_linux_bionic, TIOCSWINSZ == 0x5414);
CRT_STATIC_ASSERT(sysconf_open_max_bionic, _SC_OPEN_MAX == 0x000b);
CRT_STATIC_ASSERT(sysconf_mapped_files_bionic, _SC_MAPPED_FILES == 0x003b);
CRT_STATIC_ASSERT(sysconf_nprocessors_onln_bionic, _SC_NPROCESSORS_ONLN == 0x0061);
CRT_STATIC_ASSERT(sysconf_monotonic_clock_bionic, _SC_MONOTONIC_CLOCK == 0x0064);
CRT_STATIC_ASSERT(ai_numericsrv_bionic, AI_NUMERICSERV == 0x00000008);
CRT_STATIC_ASSERT(ai_addrconfig_bionic, AI_ADDRCONFIG == 0x00000400);
CRT_STATIC_ASSERT(ai_default_bionic, AI_DEFAULT == (AI_V4MAPPED_CFG | AI_ADDRCONFIG));
CRT_STATIC_ASSERT(eai_badflags_bionic, EAI_BADFLAGS == 3);
CRT_STATIC_ASSERT(eai_noname_bionic, EAI_NONAME == 8);
CRT_STATIC_ASSERT(eai_service_bionic, EAI_SERVICE == 9);
CRT_STATIC_ASSERT(eai_system_bionic, EAI_SYSTEM == 11);
CRT_STATIC_ASSERT(android_api_future_bionic, __ANDROID_API_FUTURE__ == 10000);

#if defined(CRT_TARGET_OS_MACOS)
CRT_STATIC_ASSERT(mach_port_32, sizeof(mach_port_t) == 4);
CRT_STATIC_ASSERT(vm_address_pointer_sized, sizeof(vm_address_t) == sizeof(void*));
CRT_STATIC_ASSERT(vm_size_pointer_sized, sizeof(vm_size_t) == sizeof(void*));
CRT_STATIC_ASSERT(mach_page_max_16k, PAGE_MAX_SIZE == 16384);
CRT_STATIC_ASSERT(mach_page_min_4k, PAGE_MIN_SIZE == 4096);
CRT_STATIC_ASSERT(target_osx, TARGET_OS_OSX == 1);
CRT_STATIC_ASSERT(target_not_ios, TARGET_OS_IPHONE == 0);
CRT_STATIC_ASSERT(target_not_catalyst, TARGET_OS_MACCATALYST == 0);
CRT_STATIC_ASSERT(target_64_bit, TARGET_RT_64_BIT == 1);
#endif

static int fail(const char* message) {
  fprintf(stderr, "header_abi_test: %s\n", message);
  return 1;
}

int main(void) {
  fd_set fds;
  struct sockaddr_in addr;
  struct pollfd pfd;
  struct stat st;
  struct statfs sfs;
  struct timespec ts;
  struct timeval tv;
#if defined(CRT_TARGET_OS_MACOS)
  kern_return_t (*allocate_fn)(vm_map_t, vm_address_t*, vm_size_t, int);
  kern_return_t (*remap_fn)(vm_map_t, vm_address_t*, vm_size_t, vm_address_t, int, vm_map_t,
                            vm_address_t, boolean_t, vm_prot_t*, vm_prot_t*, vm_inherit_t);
#endif

  memset(&fds, 0, sizeof(fds));
  memset(&addr, 0, sizeof(addr));
  memset(&pfd, 0, sizeof(pfd));
  memset(&st, 0, sizeof(st));
  memset(&sfs, 0, sizeof(sfs));
  memset(&ts, 0, sizeof(ts));
  memset(&tv, 0, sizeof(tv));

  FD_ZERO(&fds);
  FD_SET(3, &fds);
  if (!FD_ISSET(3, &fds)) {
    return fail("fd_set macros");
  }
  FD_CLR(3, &fds);
  if (FD_ISSET(3, &fds)) {
    return fail("fd_set clear");
  }

  addr.sin_family = AF_INET;
  addr.sin_port = htons(1234);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (addr.sin_family != AF_INET || ntohs(addr.sin_port) != 1234 ||
      ntohl(addr.sin_addr.s_addr) != INADDR_LOOPBACK) {
    return fail("inet ABI");
  }
  if (EPERM != 1 ||
      ECHILD != 10 ||
      ENOTBLK != 15 ||
      ETXTBSY != 26 ||
      EDEADLK != 35 ||
      EUSERS != 87 ||
      ENOTSUP != 95 ||
      EOPNOTSUPP != 95 ||
      ENETRESET != 102 ||
      ESHUTDOWN != 108 ||
      EINPROGRESS != 115 ||
      ECANCELED != 125 ||
      EOWNERDEAD != 130 ||
      ENOTRECOVERABLE != 131 ||
      EHWPOISON != 133) {
    return fail("Bionic errno ABI");
  }

  pfd.fd = -1;
  pfd.events = POLLIN | POLLOUT;
  st.st_size = (off_t)123;
  sfs.f_namelen = 255;
  ts.tv_sec = (time_t)1;
  tv.tv_sec = (time_t)1;
  if (pfd.fd != -1 || st.st_size != 123 || sfs.f_namelen != 255 || ts.tv_sec != 1 ||
      tv.tv_sec != 1) {
    return fail("basic ABI assignment");
  }
  if (android_get_device_api_level() != __ANDROID_API_FUTURE__ ||
      android_get_application_target_sdk_version() != __ANDROID_API_FUTURE__) {
    return fail("host Android API level policy");
  }

#if defined(CRT_TARGET_OS_MACOS)
  allocate_fn = vm_allocate;
  remap_fn = vm_remap;
  if (allocate_fn == 0 || remap_fn == 0 || VM_FLAGS_ANYWHERE == VM_FLAGS_OVERWRITE ||
      VM_PROT_EXECUTE == VM_PROT_WRITE || VM_INHERIT_SHARE != 0 || mach_task_self() == 0) {
    return fail("mach ABI");
  }
#endif

  printf("header_abi_test: ok\n");
  return 0;
}
