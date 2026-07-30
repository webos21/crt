#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

long __crt_sys_getpid(void);
long __crt_sys_getppid(void);
long __crt_sys_kill(long pid, int sig);
long __crt_sysconf_page_size(void);
long __crt_sysconf_nprocessors_conf(void);
long __crt_sysconf_nprocessors_onln(void);
long __crt_sysconf_phys_pages(void);
long __crt_sysconf_avphys_pages(void);

#define CRT_SYSTEM_CLK_TCK 100
#define CRT_SYSTEM_IOV_MAX 1024
#define CRT_SYSTEM_DELAYTIMER_MAX 2147483647L
#define CRT_SYSTEM_MQ_OPEN_MAX 8
#define CRT_SYSTEM_MQ_PRIO_MAX 32768
#define CRT_SYSTEM_SEM_NSEMS_MAX 256
#define CRT_SYSTEM_SEM_VALUE_MAX 0x3fffffffL
#define CRT_SYSTEM_SIGQUEUE_MAX 32
#define CRT_SYSTEM_TIMER_MAX 32
#define CRT_SYSTEM_LOGIN_NAME_MAX 256
#define CRT_SYSTEM_TTY_NAME_MAX 32
#define CRT_SYSTEM_ATEXIT_MAX 65536
#define CRT_SYSTEM_THREAD_KEYS_MAX 128
#define CRT_SYSTEM_THREAD_STACK_MIN 16384
#define CRT_SYSTEM_THREAD_THREADS_MAX 2048
#define CRT_SYSTEM_OPEN_MAX 1024
#define CRT_SYSTEM_NGROUPS_MAX 32
#define CRT_SYSTEM_ARG_MAX 131072
#define CRT_SYSTEM_PASS_MAX 128
#define CRT_SYSTEM_RE_DUP_MAX 255
#define CRT_SYSTEM_TZNAME_MAX 6
#define CRT_SYSTEM_GET_R_SIZE_MAX 1024
#if defined(CRT_TARGET_OS_LINUX)
static int matches_cpu_number(const char* s) {
  size_t i;

  if (s == 0 || s[0] != 'c' || s[1] != 'p' || s[2] != 'u' || s[3] < '0' || s[3] > '9') {
    return 0;
  }
  for (i = 4; s[i] != 0; ++i) {
    if (s[i] < '0' || s[i] > '9') {
      return 0;
    }
  }
  return 1;
}

long __crt_sysconf_page_size(void) {
  return 4096;
}

long __crt_sysconf_nprocessors_conf(void) {
  DIR* dir = opendir("/sys/devices/system/cpu");
  struct dirent* entry;
  long count = 0;

  if (dir == 0) {
    return 1;
  }
  while ((entry = readdir(dir)) != 0) {
    if ((entry->d_type == DT_DIR || entry->d_type == DT_UNKNOWN) &&
        matches_cpu_number(entry->d_name)) {
      ++count;
    }
  }
  closedir(dir);
  return count > 0 ? count : 1;
}

long __crt_sysconf_nprocessors_onln(void) {
  int fd = open("/proc/stat", O_RDONLY);
  char buffer[4096];
  ssize_t bytes;
  size_t i = 0;
  long count = 0;

  if (fd < 0) {
    return 1;
  }
  bytes = read(fd, buffer, sizeof(buffer) - 1);
  close(fd);
  if (bytes <= 0) {
    return 1;
  }
  buffer[bytes] = 0;
  while (i < (size_t)bytes) {
    size_t line_start = i;

    while (i < (size_t)bytes && buffer[i] != '\n') {
      ++i;
    }
    if (i > line_start) {
      char saved = buffer[i];
      buffer[i] = 0;
      if (matches_cpu_number(buffer + line_start)) {
        ++count;
      }
      buffer[i] = saved;
    }
    if (i < (size_t)bytes && buffer[i] == '\n') {
      ++i;
    }
  }
  return count > 0 ? count : 1;
}

static long linux_meminfo_pages(const char* key) {
  int fd = open("/proc/meminfo", O_RDONLY);
  char buffer[4096];
  ssize_t bytes;
  size_t key_len = strlen(key);
  size_t i = 0;
  long page_size = __crt_sysconf_page_size();

  if (fd < 0) {
    return -1;
  }
  bytes = read(fd, buffer, sizeof(buffer) - 1);
  close(fd);
  if (bytes <= 0 || page_size <= 0) {
    return -1;
  }
  buffer[bytes] = 0;
  while (i < (size_t)bytes) {
    size_t line_start = i;

    while (i < (size_t)bytes && buffer[i] != '\n') {
      ++i;
    }
    if (i > line_start && strncmp(buffer + line_start, key, key_len) == 0) {
      char* end = 0;
      long kb = strtol(buffer + line_start + key_len, &end, 10);

      if (end != buffer + line_start + key_len && kb >= 0) {
        return (kb * 1024L) / page_size;
      }
    }
    if (i < (size_t)bytes && buffer[i] == '\n') {
      ++i;
    }
  }
  return -1;
}

long __crt_sysconf_phys_pages(void) {
  return linux_meminfo_pages("MemTotal:");
}

long __crt_sysconf_avphys_pages(void) {
  return linux_meminfo_pages("MemFree:");
}
#elif defined(CRT_TARGET_OS_MACOS)
#define CRT_DARWIN_CTL_HW 6
#define CRT_DARWIN_HW_NCPU 3
#define CRT_DARWIN_HW_PAGESIZE 7
#define CRT_DARWIN_HW_MEMSIZE 24

long __crt_sys_macos_sysctl(
    int* name,
    unsigned int namelen,
    void* oldp,
    unsigned long* oldlenp,
    void* newp,
    unsigned long newlen);

static long macos_sysctl_long(int mib0, int mib1) {
  int name[2];
  long value = 0;
  unsigned long length = sizeof(value);
  long result;

  name[0] = mib0;
  name[1] = mib1;
  result = __crt_sys_macos_sysctl(name, 2, &value, &length, 0, 0);
  return result < 0 ? -1 : value;
}

long __crt_sysconf_page_size(void) {
  long value = macos_sysctl_long(CRT_DARWIN_CTL_HW, CRT_DARWIN_HW_PAGESIZE);

  return value > 0 ? value : 4096;
}

long __crt_sysconf_nprocessors_conf(void) {
  long value = macos_sysctl_long(CRT_DARWIN_CTL_HW, CRT_DARWIN_HW_NCPU);

  return value > 0 ? value : 1;
}

long __crt_sysconf_nprocessors_onln(void) {
  return __crt_sysconf_nprocessors_conf();
}

long __crt_sysconf_phys_pages(void) {
  long memory_size = macos_sysctl_long(CRT_DARWIN_CTL_HW, CRT_DARWIN_HW_MEMSIZE);
  long page_size = __crt_sysconf_page_size();

  if (memory_size <= 0 || page_size <= 0) {
    return -1;
  }
  return memory_size / page_size;
}

long __crt_sysconf_avphys_pages(void) {
  return -1;
}
#endif

static long normalize_syscall_result(long result) {
  if (result < 0 && result >= -4095) {
    errno = (int)-result;
    return -1;
  }
  return result;
}

pid_t getpid(void) {
  return (pid_t)normalize_syscall_result(__crt_sys_getpid());
}

pid_t getppid(void) {
  return (pid_t)normalize_syscall_result(__crt_sys_getppid());
}

int kill(pid_t pid, int sig) {
  long self;

  if (sig < 0) {
    errno = EINVAL;
    return -1;
  }
  self = __crt_sys_getpid();
  if (pid == (pid_t)self && sig != 0) {
    return raise(sig);
  }
  return (int)normalize_syscall_result(__crt_sys_kill((long)pid, sig));
}

long sysconf(int name) {
  switch (name) {
    case _SC_ARG_MAX:
      return CRT_SYSTEM_ARG_MAX;
    case _SC_CHILD_MAX:
      return _POSIX_CHILD_MAX;
    case _SC_CLK_TCK:
      return CRT_SYSTEM_CLK_TCK;
    case _SC_LINE_MAX:
      return 2048;
    case _SC_NGROUPS_MAX:
      return CRT_SYSTEM_NGROUPS_MAX;
    case _SC_OPEN_MAX:
      return CRT_SYSTEM_OPEN_MAX;
    case _SC_PASS_MAX:
      return CRT_SYSTEM_PASS_MAX;
    case _SC_2_C_BIND:
      return _POSIX_VERSION;
    case _SC_2_C_DEV:
      return -1;
    case _SC_2_C_VERSION:
      return _POSIX2_C_VERSION;
    case _SC_2_CHAR_TERM:
      return -1;
    case _SC_2_FORT_DEV:
    case _SC_2_FORT_RUN:
    case _SC_2_LOCALEDEF:
    case _SC_2_SW_DEV:
    case _SC_2_UPE:
      return -1;
    case _SC_2_VERSION:
      return _POSIX2_VERSION;
    case _SC_JOB_CONTROL:
      return _POSIX_JOB_CONTROL;
    case _SC_SAVED_IDS:
      return _POSIX_SAVED_IDS;
    case _SC_VERSION:
      return _POSIX_VERSION;
    case _SC_RE_DUP_MAX:
      return CRT_SYSTEM_RE_DUP_MAX;
    case _SC_STREAM_MAX:
      return FOPEN_MAX;
    case _SC_TZNAME_MAX:
      return CRT_SYSTEM_TZNAME_MAX;
    case _SC_XOPEN_CRYPT:
      return _XOPEN_CRYPT;
    case _SC_XOPEN_ENH_I18N:
      return _XOPEN_ENH_I18N;
    case _SC_XOPEN_SHM:
      return _XOPEN_SHM;
    case _SC_XOPEN_VERSION:
      return _XOPEN_VERSION;
    case _SC_XOPEN_XCU_VERSION:
      return _XOPEN_XCU_VERSION;
    case _SC_XOPEN_REALTIME:
      return _XOPEN_REALTIME;
    case _SC_XOPEN_REALTIME_THREADS:
      return _XOPEN_REALTIME_THREADS;
    case _SC_XOPEN_LEGACY:
      return _XOPEN_LEGACY;
    case _SC_ATEXIT_MAX:
      return CRT_SYSTEM_ATEXIT_MAX;
    case _SC_IOV_MAX:
      return CRT_SYSTEM_IOV_MAX;
    case _SC_PAGESIZE:
    case _SC_PAGE_SIZE:
      return __crt_sysconf_page_size();
    case _SC_XOPEN_UNIX:
      return _XOPEN_UNIX;
    case _SC_DELAYTIMER_MAX:
      return CRT_SYSTEM_DELAYTIMER_MAX;
    case _SC_MQ_OPEN_MAX:
      return CRT_SYSTEM_MQ_OPEN_MAX;
    case _SC_MQ_PRIO_MAX:
      return CRT_SYSTEM_MQ_PRIO_MAX;
    case _SC_RTSIG_MAX:
      return CRT_SYSTEM_SIGQUEUE_MAX;
    case _SC_SEM_NSEMS_MAX:
      return CRT_SYSTEM_SEM_NSEMS_MAX;
    case _SC_SEM_VALUE_MAX:
      return CRT_SYSTEM_SEM_VALUE_MAX;
    case _SC_SIGQUEUE_MAX:
      return CRT_SYSTEM_SIGQUEUE_MAX;
    case _SC_TIMER_MAX:
      return CRT_SYSTEM_TIMER_MAX;
    case _SC_ASYNCHRONOUS_IO:
      return -1;
    case _SC_FSYNC:
      return _POSIX_FSYNC;
    case _SC_MAPPED_FILES:
      return _POSIX_MAPPED_FILES;
    case _SC_MEMLOCK:
      return _POSIX_MEMLOCK;
    case _SC_MEMLOCK_RANGE:
      return _POSIX_MEMLOCK_RANGE;
    case _SC_MEMORY_PROTECTION:
      return _POSIX_MEMORY_PROTECTION;
    case _SC_MESSAGE_PASSING:
      return -1;
    case _SC_PRIORITIZED_IO:
      return -1;
    case _SC_PRIORITY_SCHEDULING:
      return _POSIX_PRIORITY_SCHEDULING;
    case _SC_REALTIME_SIGNALS:
      return _POSIX_REALTIME_SIGNALS;
    case _SC_SEMAPHORES:
      return _POSIX_SEMAPHORES;
    case _SC_SHARED_MEMORY_OBJECTS:
      return _POSIX_SHARED_MEMORY_OBJECTS;
    case _SC_SYNCHRONIZED_IO:
      return _POSIX_SYNCHRONIZED_IO;
    case _SC_TIMERS:
      return _POSIX_TIMERS;
    case _SC_GETGR_R_SIZE_MAX:
    case _SC_GETPW_R_SIZE_MAX:
      return CRT_SYSTEM_GET_R_SIZE_MAX;
    case _SC_LOGIN_NAME_MAX:
      return CRT_SYSTEM_LOGIN_NAME_MAX;
    case _SC_THREAD_DESTRUCTOR_ITERATIONS:
      return _POSIX_THREAD_DESTRUCTOR_ITERATIONS;
    case _SC_THREAD_KEYS_MAX:
      return CRT_SYSTEM_THREAD_KEYS_MAX;
    case _SC_THREAD_STACK_MIN:
      return CRT_SYSTEM_THREAD_STACK_MIN;
    case _SC_THREAD_THREADS_MAX:
      return CRT_SYSTEM_THREAD_THREADS_MAX;
    case _SC_TTY_NAME_MAX:
      return CRT_SYSTEM_TTY_NAME_MAX;
    case _SC_THREADS:
      return _POSIX_THREADS;
    case _SC_THREAD_ATTR_STACKADDR:
    case _SC_THREAD_ATTR_STACKSIZE:
      return -1;
    case _SC_THREAD_PRIORITY_SCHEDULING:
      return _POSIX_THREAD_PRIORITY_SCHEDULING;
    case _SC_THREAD_PRIO_INHERIT:
      return _POSIX_THREAD_PRIO_INHERIT;
    case _SC_THREAD_PRIO_PROTECT:
      return _POSIX_THREAD_PRIO_PROTECT;
    case _SC_THREAD_SAFE_FUNCTIONS:
      return _POSIX_THREAD_SAFE_FUNCTIONS;
    case _SC_NPROCESSORS_CONF:
      return __crt_sysconf_nprocessors_conf();
    case _SC_NPROCESSORS_ONLN:
      return __crt_sysconf_nprocessors_onln();
    case _SC_PHYS_PAGES:
      return __crt_sysconf_phys_pages();
    case _SC_AVPHYS_PAGES:
      return __crt_sysconf_avphys_pages();
    case _SC_MONOTONIC_CLOCK:
      return _POSIX_MONOTONIC_CLOCK;
    case _SC_ADVISORY_INFO:
      return -1;
    case _SC_BARRIERS:
      return _POSIX_BARRIERS;
    case _SC_CLOCK_SELECTION:
      return -1;
    case _SC_CPUTIME:
      return -1;
    case _SC_HOST_NAME_MAX:
      return _POSIX_HOST_NAME_MAX;
    case _SC_IPV6:
      return _POSIX_VERSION;
    case _SC_RAW_SOCKETS:
      return -1;
    case _SC_READER_WRITER_LOCKS:
      return _POSIX_READER_WRITER_LOCKS;
    case _SC_REGEXP:
      return _POSIX_VERSION;
    default:
      errno = ENOSYS;
      return -1;
  }
}
