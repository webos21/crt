#include <errno.h>
#include <string.h>
#include <sys/utsname.h>

#if defined(CRT_TARGET_OS_LINUX)
long __crt_sys_uname(struct utsname* buf);
static int normalize_syscall_result(long result) {
  if (result < 0 && result >= -4095) {
    return __set_errno((int)-result);
  }
  return (int)result;
}
#elif defined(CRT_TARGET_OS_MACOS)
#define CRT_DARWIN_CTL_KERN 1
#define CRT_DARWIN_KERN_OSTYPE 1
#define CRT_DARWIN_KERN_OSRELEASE 2
#define CRT_DARWIN_KERN_VERSION 4
#define CRT_DARWIN_KERN_HOSTNAME 10
#define CRT_DARWIN_CTL_HW 6
#define CRT_DARWIN_HW_MACHINE 1

long __crt_sys_macos_sysctl(
    int* name,
    unsigned int namelen,
    void* oldp,
    unsigned long* oldlenp,
    void* newp,
    unsigned long newlen);
#elif defined(CRT_TARGET_OS_WINDOWS)
#ifdef __i386__
#define CRT_WINAPI __stdcall
#else
#define CRT_WINAPI
#endif

typedef void* HANDLE;
typedef unsigned long DWORD;
typedef unsigned short WORD;
typedef int BOOL;

struct crt_windows_system_info {
  union {
    DWORD dwOemId;
    struct {
      WORD wProcessorArchitecture;
      WORD wReserved;
    };
  };
  DWORD dwPageSize;
  void* lpMinimumApplicationAddress;
  void* lpMaximumApplicationAddress;
  unsigned long long dwActiveProcessorMask;
  DWORD dwNumberOfProcessors;
  DWORD dwProcessorType;
  DWORD dwAllocationGranularity;
  WORD wProcessorLevel;
  WORD wProcessorRevision;
};

struct crt_windows_osversioninfo {
  DWORD dwOSVersionInfoSize;
  DWORD dwMajorVersion;
  DWORD dwMinorVersion;
  DWORD dwBuildNumber;
  DWORD dwPlatformId;
  unsigned short szCSDVersion[128];
};

__declspec(dllimport) BOOL CRT_WINAPI GetComputerNameA(char* lpBuffer, DWORD* nSize);
__declspec(dllimport) void CRT_WINAPI GetNativeSystemInfo(struct crt_windows_system_info* lpSystemInfo);
__declspec(dllimport) HANDLE CRT_WINAPI LoadLibraryA(const char* lpLibFileName);
__declspec(dllimport) void* CRT_WINAPI GetProcAddress(HANDLE hModule, const char* lpProcName);
#endif

#if !defined(CRT_TARGET_OS_LINUX)
static void uts_copy(char dst[_UTSNAME_LENGTH], const char* src) {
  size_t i;

  for (i = 0; i + 1 < _UTSNAME_LENGTH && src[i] != 0; ++i) {
    dst[i] = src[i];
  }
  dst[i] = 0;
}
#endif

#if defined(CRT_TARGET_OS_MACOS)
static void macos_sysctl_string(int mib0, int mib1, char dst[_UTSNAME_LENGTH], const char* fallback) {
  int name[2];
  unsigned long length = _UTSNAME_LENGTH;

  name[0] = mib0;
  name[1] = mib1;
  dst[0] = 0;
  if (__crt_sys_macos_sysctl(name, 2, dst, &length, 0, 0) < 0 || dst[0] == 0) {
    uts_copy(dst, fallback);
  } else {
    dst[_UTSNAME_LENGTH - 1] = 0;
  }
}
#elif defined(CRT_TARGET_OS_WINDOWS)
static void uts_append_decimal(char dst[_UTSNAME_LENGTH], unsigned long value) {
  char reversed[16];
  size_t out = strlen(dst);
  int pos = 0;

  do {
    reversed[pos++] = (char)('0' + (value % 10));
    value /= 10;
  } while (value != 0 && pos < (int)sizeof(reversed));

  while (pos > 0 && out + 1 < _UTSNAME_LENGTH) {
    dst[out++] = reversed[--pos];
  }
  dst[out] = 0;
}

static void uts_append_char(char dst[_UTSNAME_LENGTH], char ch) {
  size_t len = strlen(dst);

  if (len + 1 < _UTSNAME_LENGTH) {
    dst[len] = ch;
    dst[len + 1] = 0;
  }
}

static void windows_release(char release[_UTSNAME_LENGTH], char version[_UTSNAME_LENGTH]) {
  typedef long (CRT_WINAPI *rtl_get_version_fn)(struct crt_windows_osversioninfo*);
  HANDLE ntdll;
  rtl_get_version_fn rtl_get_version;
  struct crt_windows_osversioninfo info;

  uts_copy(release, "Windows");
  uts_copy(version, "Windows");

  ntdll = LoadLibraryA("ntdll.dll");
  if (ntdll == 0) return;
  rtl_get_version = (rtl_get_version_fn)GetProcAddress(ntdll, "RtlGetVersion");
  if (rtl_get_version == 0) return;

  memset(&info, 0, sizeof(info));
  info.dwOSVersionInfoSize = sizeof(info);
  if (rtl_get_version(&info) != 0) return;

  uts_copy(release, "");
  uts_append_decimal(release, info.dwMajorVersion);
  uts_append_char(release, '.');
  uts_append_decimal(release, info.dwMinorVersion);
  uts_append_char(release, '.');
  uts_append_decimal(release, info.dwBuildNumber);

  uts_copy(version, "Windows ");
  uts_append_decimal(version, info.dwMajorVersion);
  uts_append_char(version, '.');
  uts_append_decimal(version, info.dwMinorVersion);
  uts_append_char(version, '.');
  uts_append_decimal(version, info.dwBuildNumber);
}

static void windows_machine(char machine[_UTSNAME_LENGTH]) {
  struct crt_windows_system_info info;

  memset(&info, 0, sizeof(info));
  GetNativeSystemInfo(&info);
  switch (info.wProcessorArchitecture) {
    case 9:
      uts_copy(machine, "x86_64");
      break;
    case 12:
      uts_copy(machine, "aarch64");
      break;
    case 0:
      uts_copy(machine, "i686");
      break;
    default:
      uts_copy(machine, "unknown");
      break;
  }
}
#endif

int uname(struct utsname* buf) {
  if (buf == 0) {
    return __set_errno(EFAULT);
  }

#if defined(CRT_TARGET_OS_LINUX)
  return normalize_syscall_result(__crt_sys_uname(buf));
#elif defined(CRT_TARGET_OS_MACOS)
  macos_sysctl_string(CRT_DARWIN_CTL_KERN, CRT_DARWIN_KERN_OSTYPE, buf->sysname, "Darwin");
  macos_sysctl_string(CRT_DARWIN_CTL_KERN, CRT_DARWIN_KERN_HOSTNAME, buf->nodename, "localhost");
  macos_sysctl_string(CRT_DARWIN_CTL_KERN, CRT_DARWIN_KERN_OSRELEASE, buf->release, "Darwin");
  macos_sysctl_string(CRT_DARWIN_CTL_KERN, CRT_DARWIN_KERN_VERSION, buf->version, "Darwin");
  macos_sysctl_string(CRT_DARWIN_CTL_HW, CRT_DARWIN_HW_MACHINE, buf->machine, "unknown");
  uts_copy(buf->domainname, "(none)");
  return 0;
#elif defined(CRT_TARGET_OS_WINDOWS)
  {
    DWORD size = _UTSNAME_LENGTH;

    uts_copy(buf->sysname, "Windows");
    if (GetComputerNameA(buf->nodename, &size) == 0 || buf->nodename[0] == 0) {
      uts_copy(buf->nodename, "localhost");
    } else {
      buf->nodename[_UTSNAME_LENGTH - 1] = 0;
    }
    windows_release(buf->release, buf->version);
    windows_machine(buf->machine);
    uts_copy(buf->domainname, "(none)");
    return 0;
  }
#else
  uts_copy(buf->sysname, "Linux");
  uts_copy(buf->nodename, "localhost");
  uts_copy(buf->release, "crt");
  uts_copy(buf->version, "Bionic-compatible CRT");
#if defined(__aarch64__) || defined(_M_ARM64)
  uts_copy(buf->machine, "aarch64");
#elif defined(__x86_64__) || defined(_M_X64)
  uts_copy(buf->machine, "x86_64");
#else
  uts_copy(buf->machine, "unknown");
#endif
  uts_copy(buf->domainname, "(none)");
  return 0;
#endif
}
