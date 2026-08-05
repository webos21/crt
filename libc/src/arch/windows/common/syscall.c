#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/vfs.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include <private/crt_fd_table.h>
#include <private/crt_signal.h>
#include <private/crt_spawn.h>

typedef void* HANDLE;
typedef uintptr_t SOCKET;
typedef unsigned long DWORD;
typedef unsigned long ULONG;
typedef uint16_t WORD;
typedef int BOOL;
struct sockaddr;
struct crt_filetime;
typedef struct crt_overlapped {
  uintptr_t Internal;
  uintptr_t InternalHigh;
  union {
    struct {
      DWORD Offset;
      DWORD OffsetHigh;
    };
    void* Pointer;
  };
  HANDLE hEvent;
} OVERLAPPED;

struct crt_coord {
  short X;
  short Y;
};

struct crt_small_rect {
  short Left;
  short Top;
  short Right;
  short Bottom;
};

struct crt_console_screen_buffer_info {
  struct crt_coord dwSize;
  struct crt_coord dwCursorPosition;
  WORD wAttributes;
  struct crt_small_rect srWindow;
  struct crt_coord dwMaximumWindowSize;
};

struct crt_system_info {
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
  uintptr_t dwActiveProcessorMask;
  DWORD dwNumberOfProcessors;
  DWORD dwProcessorType;
  DWORD dwAllocationGranularity;
  WORD wProcessorLevel;
  WORD wProcessorRevision;
};

struct crt_memory_status_ex {
  DWORD dwLength;
  DWORD dwMemoryLoad;
  unsigned long long ullTotalPhys;
  unsigned long long ullAvailPhys;
  unsigned long long ullTotalPageFile;
  unsigned long long ullAvailPageFile;
  unsigned long long ullTotalVirtual;
  unsigned long long ullAvailVirtual;
  unsigned long long ullAvailExtendedVirtual;
};

#define CRT_FD_TABLE_SIZE 64

#define STD_INPUT_HANDLE ((DWORD)-10)
#define STD_OUTPUT_HANDLE ((DWORD)-11)
#define STD_ERROR_HANDLE ((DWORD)-12)
#define GENERIC_READ ((DWORD)0x80000000)
#define GENERIC_WRITE ((DWORD)0x40000000)
#define FILE_SHARE_READ 0x00000001
#define FILE_SHARE_WRITE 0x00000002
#define FILE_SHARE_DELETE 0x00000004
#define CREATE_NEW 1
#define CREATE_ALWAYS 2
#define OPEN_EXISTING 3
#define OPEN_ALWAYS 4
#define FILE_ATTRIBUTE_READONLY 0x00000001
#define FILE_ATTRIBUTE_NORMAL 0x00000080
#define MOVEFILE_REPLACE_EXISTING 0x00000001
#define FILE_ATTRIBUTE_DIRECTORY 0x00000010
#define FILE_ATTRIBUTE_REPARSE_POINT 0x00000400
#define FILE_TYPE_DISK 0x0001
#define FILE_TYPE_CHAR 0x0002
#define FILE_TYPE_PIPE 0x0003
#define INVALID_FILE_ATTRIBUTES ((DWORD)0xffffffffUL)
#define FILE_FLAG_BACKUP_SEMANTICS 0x02000000
#define FILE_WRITE_ATTRIBUTES 0x00000100
#define FILE_BEGIN 0
#define FILE_CURRENT 1
#define FILE_END 2
#define MEM_COMMIT 0x00001000
#define MEM_RESERVE 0x00002000
#define MEM_RELEASE 0x00008000
#define PAGE_NOACCESS 0x01
#define PAGE_READONLY 0x02
#define PAGE_READWRITE 0x04
#define PAGE_WRITECOPY 0x08
#define PAGE_EXECUTE 0x10
#define PAGE_EXECUTE_READ 0x20
#define PAGE_EXECUTE_READWRITE 0x40
#define PAGE_EXECUTE_WRITECOPY 0x80
#define FILE_MAP_COPY 0x00000001
#define FILE_MAP_WRITE 0x00000002
#define FILE_MAP_READ 0x00000004
#define FILE_MAP_EXECUTE 0x00000020
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define WINDOWS_TICK 10000000ULL
#define SEC_TO_UNIX_EPOCH 11644473600ULL
#define DUPLICATE_SAME_ACCESS 0x00000002
#define HANDLE_FLAG_INHERIT 0x00000001
#define LOCKFILE_FAIL_IMMEDIATELY 0x00000001
#define LOCKFILE_EXCLUSIVE_LOCK 0x00000002
#define INVALID_SOCKET ((SOCKET)~(uintptr_t)0)
#define SOCKET_ERROR (-1)
#define SD_RECEIVE 0
#define SD_SEND 1
#define SD_BOTH 2
#define CRT_PUBLIC_SOL_SOCKET 1
#define CRT_PUBLIC_SO_REUSEADDR 2
#define CRT_PUBLIC_SHUT_RD 0
#define CRT_PUBLIC_SHUT_WR 1
#define CRT_WS_SOL_SOCKET 0xffff
#define CRT_WS_SO_REUSEADDR 0x0004
#define CRT_WS_FIONREAD 0x4004667fUL
#define CRT_FD_KIND_NONE 0
#define CRT_FD_KIND_FILE 1
#define CRT_FD_KIND_SOCKET 2
#define CRT_WAIT_OBJECT_0 0
#define CRT_WAIT_FAILED 0xffffffffUL
#define CRT_INFINITE 0xffffffffUL
#define CRT_STARTF_USESTDHANDLES 0x00000100
#define CRT_CREATE_NEW_PROCESS_GROUP 0x00000200
#define CRT_CREATE_SUSPENDED 0x00000004
#define CRT_ERROR_BROKEN_PIPE 109
#define CRT_ERROR_HANDLE_EOF 38
#define CRT_STATUS_SUCCESS 0x00000000UL
#define CRT_STATUS_PROCESS_CLONED 0x00000129UL
#define CRT_RTL_CLONE_PROCESS_FLAGS_INHERIT_HANDLES 0x00000002UL
#define CRT_FROM_PROTOCOL_INFO (-1)
#define CRT_ENABLE_PROCESSED_INPUT 0x0001
#define CRT_ENABLE_LINE_INPUT 0x0002
#define CRT_ENABLE_ECHO_INPUT 0x0004
#define CRT_ENABLE_PROCESSED_OUTPUT 0x0001
#define CRT_ENABLE_WRAP_AT_EOL_OUTPUT 0x0002

#if defined(_M_IX86) || defined(__i386__)
#define CRT_WINAPI __stdcall
#else
#define CRT_WINAPI
#endif

struct crt_startupinfo {
  DWORD cb;
  char* lpReserved;
  char* lpDesktop;
  char* lpTitle;
  DWORD dwX;
  DWORD dwY;
  DWORD dwXSize;
  DWORD dwYSize;
  DWORD dwXCountChars;
  DWORD dwYCountChars;
  DWORD dwFillAttribute;
  DWORD dwFlags;
  WORD wShowWindow;
  WORD cbReserved2;
  unsigned char* lpReserved2;
  HANDLE hStdInput;
  HANDLE hStdOutput;
  HANDLE hStdError;
};

struct crt_process_information {
  HANDLE hProcess;
  HANDLE hThread;
  DWORD dwProcessId;
  DWORD dwThreadId;
};

struct crt_client_id {
  HANDLE UniqueProcess;
  HANDLE UniqueThread;
};

struct crt_rtl_user_process_information {
  ULONG Length;
  HANDLE Process;
  HANDLE Thread;
  struct crt_client_id ClientId;
  unsigned char ImageInformation[256];
};

__declspec(dllimport) HANDLE CRT_WINAPI GetStdHandle(DWORD nStdHandle);
__declspec(dllimport) DWORD CRT_WINAPI GetLastError(void);
__declspec(dllimport) void CRT_WINAPI SetLastError(DWORD dwErrCode);
__declspec(dllimport) BOOL CRT_WINAPI GetConsoleMode(HANDLE hConsoleHandle, DWORD* lpMode);
__declspec(dllimport) BOOL CRT_WINAPI SetConsoleMode(HANDLE hConsoleHandle, DWORD dwMode);
__declspec(dllimport) HANDLE CRT_WINAPI LoadLibraryA(const char* lpLibFileName);
__declspec(dllimport) void* CRT_WINAPI GetProcAddress(HANDLE hModule, const char* lpProcName);
__declspec(dllimport) DWORD CRT_WINAPI GetModuleFileNameA(
    HANDLE hModule,
    char* lpFilename,
    DWORD nSize);
__declspec(dllimport) BOOL CRT_WINAPI CreateProcessA(
    const char* lpApplicationName,
    char* lpCommandLine,
    void* lpProcessAttributes,
    void* lpThreadAttributes,
    BOOL bInheritHandles,
    DWORD dwCreationFlags,
    void* lpEnvironment,
    const char* lpCurrentDirectory,
    struct crt_startupinfo* lpStartupInfo,
    struct crt_process_information* lpProcessInformation);
__declspec(dllimport) DWORD CRT_WINAPI SearchPathA(
    const char* lpPath,
    const char* lpFileName,
    const char* lpExtension,
    DWORD nBufferLength,
    char* lpBuffer,
    char** lpFilePart);
__declspec(dllimport) DWORD CRT_WINAPI WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds);
__declspec(dllimport) BOOL CRT_WINAPI GetExitCodeProcess(HANDLE hProcess, DWORD* lpExitCode);
__declspec(dllimport) DWORD CRT_WINAPI ResumeThread(HANDLE hThread);
__declspec(dllimport) BOOL CRT_WINAPI TerminateProcess(HANDLE hProcess, unsigned int uExitCode);
__declspec(dllimport) HANDLE CRT_WINAPI CreateFileA(
    const char* lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    void* lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile);
__declspec(dllimport) BOOL CRT_WINAPI ReadFile(
    HANDLE hFile,
    void* lpBuffer,
    DWORD nNumberOfBytesToRead,
    DWORD* lpNumberOfBytesRead,
    void* lpOverlapped);
__declspec(dllimport) BOOL CRT_WINAPI WriteFile(
    HANDLE hFile,
    const void* lpBuffer,
    DWORD nNumberOfBytesToWrite,
    DWORD* lpNumberOfBytesWritten,
    void* lpOverlapped);
__declspec(dllimport) BOOL CRT_WINAPI CloseHandle(HANDLE hObject);
__declspec(dllimport) BOOL CRT_WINAPI DeleteFileA(const char* lpFileName);
__declspec(dllimport) BOOL CRT_WINAPI MoveFileExA(
    const char* lpExistingFileName,
    const char* lpNewFileName,
    DWORD dwFlags);
__declspec(dllimport) BOOL CRT_WINAPI CreateDirectoryA(
    const char* lpPathName,
    void* lpSecurityAttributes);
__declspec(dllimport) BOOL CRT_WINAPI RemoveDirectoryA(const char* lpPathName);
__declspec(dllimport) BOOL CRT_WINAPI SetCurrentDirectoryA(const char* lpPathName);
__declspec(dllimport) DWORD CRT_WINAPI GetCurrentDirectoryA(DWORD nBufferLength, char* lpBuffer);
__declspec(dllimport) DWORD CRT_WINAPI GetFileAttributesA(const char* lpFileName);
__declspec(dllimport) BOOL CRT_WINAPI SetFileAttributesA(const char* lpFileName, DWORD dwFileAttributes);
__declspec(dllimport) void CRT_WINAPI GetSystemInfo(struct crt_system_info* lpSystemInfo);
__declspec(dllimport) BOOL CRT_WINAPI GlobalMemoryStatusEx(
    struct crt_memory_status_ex* lpBuffer);
__declspec(dllimport) DWORD CRT_WINAPI GetFullPathNameA(
    const char* lpFileName,
    DWORD nBufferLength,
    char* lpBuffer,
    char** lpFilePart);
__declspec(dllimport) DWORD CRT_WINAPI GetFinalPathNameByHandleA(
    HANDLE hFile,
    char* lpszFilePath,
    DWORD cchFilePath,
    DWORD dwFlags);
__declspec(dllimport) HANDLE CRT_WINAPI GetCurrentProcess(void);
__declspec(dllimport) BOOL CRT_WINAPI DuplicateHandle(
    HANDLE hSourceProcessHandle,
    HANDLE hSourceHandle,
    HANDLE hTargetProcessHandle,
    HANDLE* lpTargetHandle,
    DWORD dwDesiredAccess,
    BOOL bInheritHandle,
    DWORD dwOptions);
__declspec(dllimport) BOOL CRT_WINAPI GetHandleInformation(
    HANDLE hObject,
    DWORD* lpdwFlags);
__declspec(dllimport) BOOL CRT_WINAPI SetHandleInformation(
    HANDLE hObject,
    DWORD dwMask,
    DWORD dwFlags);
__declspec(dllimport) BOOL CRT_WINAPI CreatePipe(
    HANDLE* hReadPipe,
    HANDLE* hWritePipe,
    void* lpPipeAttributes,
    DWORD nSize);
__declspec(dllimport) DWORD CRT_WINAPI GetFileType(HANDLE hFile);
__declspec(dllimport) BOOL CRT_WINAPI PeekNamedPipe(
    HANDLE hNamedPipe,
    void* lpBuffer,
    DWORD nBufferSize,
    DWORD* lpBytesRead,
    DWORD* lpTotalBytesAvail,
    DWORD* lpBytesLeftThisMessage);
__declspec(dllimport) BOOL CRT_WINAPI GetConsoleScreenBufferInfo(
    HANDLE hConsoleOutput,
    struct crt_console_screen_buffer_info* lpConsoleScreenBufferInfo);
__declspec(dllimport) BOOL CRT_WINAPI GetNumberOfConsoleInputEvents(
    HANDLE hConsoleInput,
    DWORD* lpcNumberOfEvents);
__declspec(dllimport) BOOL CRT_WINAPI SetFilePointerEx(
    HANDLE hFile,
    long long liDistanceToMove,
    long long* lpNewFilePointer,
    DWORD dwMoveMethod);
__declspec(dllimport) BOOL CRT_WINAPI SetEndOfFile(HANDLE hFile);
__declspec(dllimport) BOOL CRT_WINAPI FlushFileBuffers(HANDLE hFile);
__declspec(dllimport) BOOL CRT_WINAPI LockFileEx(
    HANDLE hFile,
    DWORD dwFlags,
    DWORD dwReserved,
    DWORD nNumberOfBytesToLockLow,
    DWORD nNumberOfBytesToLockHigh,
    OVERLAPPED* lpOverlapped);
__declspec(dllimport) BOOL CRT_WINAPI UnlockFileEx(
    HANDLE hFile,
    DWORD dwReserved,
    DWORD nNumberOfBytesToUnlockLow,
    DWORD nNumberOfBytesToUnlockHigh,
    OVERLAPPED* lpOverlapped);
__declspec(dllimport) BOOL CRT_WINAPI SetFileTime(
    HANDLE hFile,
    const struct crt_filetime* lpCreationTime,
    const struct crt_filetime* lpLastAccessTime,
    const struct crt_filetime* lpLastWriteTime);
__declspec(dllimport) BOOL CRT_WINAPI GetDiskFreeSpaceExA(
    const char* lpDirectoryName,
    unsigned long long* lpFreeBytesAvailableToCaller,
    unsigned long long* lpTotalNumberOfBytes,
    unsigned long long* lpTotalNumberOfFreeBytes);
__declspec(dllimport) void* CRT_WINAPI VirtualAlloc(
    void* lpAddress,
    size_t dwSize,
    DWORD flAllocationType,
    DWORD flProtect);
__declspec(dllimport) BOOL CRT_WINAPI VirtualFree(
    void* lpAddress,
    size_t dwSize,
    DWORD dwFreeType);
__declspec(dllimport) BOOL CRT_WINAPI VirtualProtect(
    void* lpAddress,
    size_t dwSize,
    DWORD flNewProtect,
    DWORD* lpflOldProtect);
__declspec(dllimport) HANDLE CRT_WINAPI CreateFileMappingA(
    HANDLE hFile,
    void* lpFileMappingAttributes,
    DWORD flProtect,
    DWORD dwMaximumSizeHigh,
    DWORD dwMaximumSizeLow,
    const char* lpName);
__declspec(dllimport) void* CRT_WINAPI MapViewOfFileEx(
    HANDLE hFileMappingObject,
    DWORD dwDesiredAccess,
    DWORD dwFileOffsetHigh,
    DWORD dwFileOffsetLow,
    size_t dwNumberOfBytesToMap,
    void* lpBaseAddress);
__declspec(dllimport) BOOL CRT_WINAPI UnmapViewOfFile(const void* lpBaseAddress);
__declspec(dllimport) BOOL CRT_WINAPI FlushViewOfFile(
    const void* lpBaseAddress,
    size_t dwNumberOfBytesToFlush);
__declspec(dllimport) BOOL CRT_WINAPI VirtualLock(void* lpAddress, size_t dwSize);
__declspec(dllimport) BOOL CRT_WINAPI VirtualUnlock(void* lpAddress, size_t dwSize);
struct crt_filetime {
  DWORD low;
  DWORD high;
};
struct crt_by_handle_file_information {
  DWORD file_attributes;
  struct crt_filetime creation_time;
  struct crt_filetime last_access_time;
  struct crt_filetime last_write_time;
  DWORD volume_serial_number;
  DWORD file_size_high;
  DWORD file_size_low;
  DWORD number_of_links;
  DWORD file_index_high;
  DWORD file_index_low;
};
__declspec(dllimport) void CRT_WINAPI GetSystemTimeAsFileTime(struct crt_filetime* lpSystemTimeAsFileTime);
__declspec(dllimport) BOOL CRT_WINAPI GetFileInformationByHandle(
    HANDLE hFile,
    struct crt_by_handle_file_information* lpFileInformation);
__declspec(dllimport) BOOL CRT_WINAPI QueryPerformanceCounter(long long* lpPerformanceCount);
__declspec(dllimport) BOOL CRT_WINAPI QueryPerformanceFrequency(long long* lpFrequency);
__declspec(dllimport) void CRT_WINAPI Sleep(DWORD dwMilliseconds);
__declspec(dllimport) DWORD CRT_WINAPI GetCurrentThreadId(void);
__declspec(dllimport) DWORD CRT_WINAPI GetCurrentProcessId(void);
__declspec(dllimport) void CRT_WINAPI ExitThread(DWORD dwExitCode);
__declspec(dllimport) void CRT_WINAPI ExitProcess(unsigned int uExitCode);
__declspec(dllimport) char* CRT_WINAPI GetEnvironmentStringsA(void);
__declspec(dllimport) BOOL CRT_WINAPI FreeEnvironmentStringsA(char* lpszEnvironmentBlock);

static HANDLE fd_table[CRT_FD_TABLE_SIZE];
static int fd_kind[CRT_FD_TABLE_SIZE];
static int fd_flags[CRT_FD_TABLE_SIZE];
static int fd_table_initialized;
static int winsock_initialized;
static int ntdll_initialized;
/* Set once, in the RtlCloneUserProcess child branch of __crt_sys_fork().
 * A process with this flag set is a raw NT-level clone that never went
 * through CreateProcess's CSRSS registration handshake: CreateProcessA
 * (and anything else that needs a working CSR connection) crashes if
 * called directly from here. See docs/windows_fork_emulation.md, "Chosen
 * Direction: Spawn Broker". */
static int windows_unregistered_clone;
static HANDLE child_process_table[CRT_FD_TABLE_SIZE];
static DWORD child_pid_table[CRT_FD_TABLE_SIZE];
static HANDLE private_wait_process;
static DWORD private_wait_pid;

long __crt_sys_geteuid(void);
static HANDLE get_fd_handle(int fd);
static void init_fd_table(void);
static long init_winsock(void);
static long init_ntdll(void);
static long close_fd_slot(int fd);

struct ntdll_api {
  ULONG (CRT_WINAPI* RtlCloneUserProcess)(
      ULONG,
      void*,
      void*,
      HANDLE,
      struct crt_rtl_user_process_information*);
};

static struct ntdll_api ntdll;

struct winsock_api {
  int (CRT_WINAPI* WSAStartup)(WORD wVersionRequested, void* lpWSAData);
  int (CRT_WINAPI* WSAGetLastError)(void);
  SOCKET (CRT_WINAPI* socket)(int af, int type, int protocol);
  int (CRT_WINAPI* bind)(SOCKET s, const struct sockaddr* name, int namelen);
  int (CRT_WINAPI* listen)(SOCKET s, int backlog);
  SOCKET (CRT_WINAPI* accept)(SOCKET s, struct sockaddr* addr, int* addrlen);
  int (CRT_WINAPI* connect)(SOCKET s, const struct sockaddr* name, int namelen);
  int (CRT_WINAPI* send)(SOCKET s, const char* buf, int len, int flags);
  int (CRT_WINAPI* recv)(SOCKET s, char* buf, int len, int flags);
  int (CRT_WINAPI* sendto)(
      SOCKET s,
      const char* buf,
      int len,
      int flags,
      const struct sockaddr* to,
      int tolen);
  int (CRT_WINAPI* recvfrom)(
      SOCKET s,
      char* buf,
      int len,
      int flags,
      struct sockaddr* from,
      int* fromlen);
  int (CRT_WINAPI* getsockname)(SOCKET s, struct sockaddr* name, int* namelen);
  int (CRT_WINAPI* setsockopt)(
      SOCKET s,
      int level,
      int optname,
      const char* optval,
      int optlen);
  int (CRT_WINAPI* shutdown)(SOCKET s, int how);
  int (CRT_WINAPI* closesocket)(SOCKET s);
  int (CRT_WINAPI* ioctlsocket)(SOCKET s, long cmd, unsigned long* argp);
  int (CRT_WINAPI* WSADuplicateSocketA)(SOCKET s, DWORD dwProcessId, void* lpProtocolInfo);
  SOCKET (CRT_WINAPI* WSASocketA)(
      int af,
      int type,
      int protocol,
      void* lpProtocolInfo,
      unsigned int g,
      DWORD dwFlags);
};

static struct winsock_api winsock;

static int map_windows_error(DWORD error) {
  switch (error) {
    case 0:
      return 0;
    case 2:
    case 3:
    case 15:
    case 18:
    /* ERROR_INVALID_NAME (a path contains a character Windows never
     * allows in a real filename, e.g. '*', '?', '"', '<', '>', '|'):
     * such a path can never correspond to a real file, which is the
     * same thing ENOENT means to POSIX callers -- most concretely, `rm
     * -f` on a glob that failed to match anything (so the shell passes
     * the literal pattern through unchanged, e.g. `rm -f '*.core'`) must
     * see ENOENT here to silently succeed the way -f promises, not EIO. */
    case 123:
      return ENOENT;
    case 4:
      return EMFILE;
    case 5:
      return EACCES;
    case 6:
      return EBADF;
    case 7:
    case 8:
    case 9:
    case 14:
    case 1816:
      return ENOMEM;
    case 10:
      return E2BIG;
    case 11:
      return ENOEXEC;
    case 12:
      return EACCES;
    case 13:
    case 24:
    case 87:
      return EINVAL;
    case 16:
      return EACCES;
    case 17:
      return EXDEV;
    case 19:
      return EROFS;
    case 20:
      return ENODEV;
    case 21:
      return ENXIO;
    case 22:
    case 23:
    case 25:
    case 26:
    case 27:
    case 28:
    case 29:
    case 30:
    case 31:
      return EIO;
    case 32:
      return EBUSY;
    case 33:
      return EACCES;
    case 36:
      return EAGAIN;
    case 38:
      return ENODATA;
    case 80:
    case 183:
      return EEXIST;
    case 109:
    case 232:
    case 233:
      return EPIPE;
    case 145:
      return ENOTEMPTY;
    case 148:
    case 170:
    case 231:
      return EBUSY;
    case 206:
      return ENAMETOOLONG;
    case 267:
      return ENOTDIR;
    default:
      return EIO;
  }
}

static int map_wsa_error(int error) {
  switch (error) {
    case 10004:
      return EINTR;
    case 10009:
      return EBADF;
    case 10013:
      return EACCES;
    case 10014:
      return EFAULT;
    case 10022:
      return EINVAL;
    case 10035:
      return EAGAIN;
    case 10036:
      return EINPROGRESS;
    case 10037:
      return EALREADY;
    case 10038:
      return ENOTSOCK;
    case 10039:
      return EDESTADDRREQ;
    case 10040:
      return EMSGSIZE;
    case 10041:
      return EPROTOTYPE;
    case 10042:
      return ENOPROTOOPT;
    case 10043:
      return EPROTONOSUPPORT;
    case 10044:
      return ESOCKTNOSUPPORT;
    case 10047:
      return EAFNOSUPPORT;
    case 10048:
      return EADDRINUSE;
    case 10049:
      return EADDRNOTAVAIL;
    case 10050:
      return ENETDOWN;
    case 10051:
      return ENETUNREACH;
    case 10052:
      return ENETRESET;
    case 10053:
      return ECONNABORTED;
    case 10054:
      return ECONNRESET;
    case 10055:
      return ENOBUFS;
    case 10056:
      return EISCONN;
    case 10057:
      return ENOTCONN;
    case 10058:
      return ESHUTDOWN;
    case 10059:
      return ETOOMANYREFS;
    case 10060:
      return ETIMEDOUT;
    case 10061:
      return ECONNREFUSED;
    case 10064:
      return EHOSTDOWN;
    case 10065:
      return EHOSTUNREACH;
    default:
      return EIO;
  }
}

static long fail_last_error(void) {
  return -map_windows_error(GetLastError());
}

static int ascii_tolower(int c) {
  return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c;
}

static int ascii_strcasecmp(const char* a, const char* b) {
  size_t i;

  for (i = 0; a[i] != 0 && b[i] != 0; ++i) {
    int ac = ascii_tolower((unsigned char)a[i]);
    int bc = ascii_tolower((unsigned char)b[i]);

    if (ac != bc) {
      return ac - bc;
    }
  }
  return ascii_tolower((unsigned char)a[i]) - ascii_tolower((unsigned char)b[i]);
}

static int windows_path_prefix_equal(const char* a, const char* b) {
  size_t i;

  if (a == 0 || b == 0) {
    return 0;
  }
  for (i = 0; b[i] != 0; ++i) {
    int ac = a[i];
    int bc = b[i];

    if (ac == '/') {
      ac = '\\';
    }
    if (bc == '/') {
      bc = '\\';
    }
    if (ascii_tolower(ac) != ascii_tolower(bc)) {
      return 0;
    }
  }
  return a[i] == 0 || a[i] == '/' || a[i] == '\\';
}

static int windows_native_absolute_path(const char* path) {
  if (path == 0 || path[0] == 0) {
    return 0;
  }
  if (((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
      path[1] == ':') {
    return 1;
  }
  return (path[0] == '\\' && path[1] == '\\');
}

static const char* windows_rootfs(void) {
  const char* root = getenv("CRT_ROOTFS");

  return root != 0 && root[0] != 0 ? root : 0;
}

static const char* translate_path_for_host(const char* path, char buffer[4096]) {
  const char* root;
  size_t root_len;
  size_t out;
  size_t in;

  if (path == 0) {
    return 0;
  }
  if (strcmp(path, "/dev/null") == 0) {
    return "NUL";
  }
  if (strcmp(path, "/dev/tty") == 0 || strcmp(path, "/dev/console") == 0) {
    return "CON";
  }
  if (strcmp(path, "/proc/self/exe") == 0) {
    DWORD result = GetModuleFileNameA(0, buffer, 4096);

    return result != 0 && result < 4096 ? buffer : path;
  }
  if (windows_native_absolute_path(path) || path[0] != '/') {
    return path;
  }
  root = windows_rootfs();
  if (root == 0) {
    return path;
  }
  root_len = strlen(root);
  if (root_len == 0 || root_len + strlen(path) + 1 >= 4096) {
    return path;
  }
  memcpy(buffer, root, root_len);
  out = root_len;
  while (out > 0 && (buffer[out - 1] == '/' || buffer[out - 1] == '\\')) {
    --out;
  }
  in = 0;
  while (path[in] == '/') {
    ++in;
  }
  buffer[out++] = '\\';
  while (path[in] != 0 && out + 1 < 4096) {
    buffer[out++] = path[in] == '/' ? '\\' : path[in];
    ++in;
  }
  buffer[out] = 0;
  return buffer;
}

static int path_is_dev_null(const char* path) {
  return path != 0 && strcmp(path, "/dev/null") == 0;
}

static int path_is_dev_tty(const char* path) {
  return path != 0 && (strcmp(path, "/dev/tty") == 0 || strcmp(path, "/dev/console") == 0);
}

static int windows_has_executable_extension(const char* path) {
  const char* dot = 0;
  const char* cursor;

  if (path == 0) {
    return 0;
  }
  for (cursor = path; *cursor != 0; ++cursor) {
    if (*cursor == '/' || *cursor == '\\') {
      dot = 0;
    } else if (*cursor == '.') {
      dot = cursor;
    }
  }
  if (dot == 0) {
    return 0;
  }
  return ascii_strcasecmp(dot, ".exe") == 0 ||
         ascii_strcasecmp(dot, ".com") == 0 ||
         ascii_strcasecmp(dot, ".bat") == 0 ||
         ascii_strcasecmp(dot, ".cmd") == 0;
}

static int windows_handle_has_mz_signature(HANDLE handle) {
  unsigned char magic[2];
  DWORD bytes_read = 0;
  long long saved = 0;
  int executable = 0;

  if (handle == INVALID_HANDLE_VALUE) {
    return 0;
  }
  if (!SetFilePointerEx(handle, 0, &saved, FILE_CURRENT)) {
    return 0;
  }
  if (SetFilePointerEx(handle, 0, 0, FILE_BEGIN) &&
      ReadFile(handle, magic, (DWORD)sizeof(magic), &bytes_read, 0) &&
      bytes_read == sizeof(magic) &&
      magic[0] == 'M' && magic[1] == 'Z') {
    executable = 1;
  }
  (void)SetFilePointerEx(handle, saved, 0, FILE_BEGIN);
  return executable;
}

static int windows_path_is_executable_file(const char* host_path, DWORD attrs) {
  HANDLE handle;
  int executable;

  if (host_path == 0 || (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0) {
    return 0;
  }
  if (windows_has_executable_extension(host_path)) {
    return 1;
  }
  handle = CreateFileA(host_path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE |
                       FILE_SHARE_DELETE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
  if (handle == INVALID_HANDLE_VALUE) {
    return 0;
  }
  executable = windows_handle_has_mz_signature(handle);
  CloseHandle(handle);
  return executable;
}

static int append_command_arg(char* buffer, size_t size, size_t* pos, const char* arg) {
  int needs_quotes = 0;
  size_t i;

  if (*pos != 0) {
    if (*pos + 1 >= size) {
      return -E2BIG;
    }
    buffer[(*pos)++] = ' ';
  }
  if (arg == 0) {
    arg = "";
  }
  for (i = 0; arg[i] != 0; ++i) {
    if (arg[i] == ' ' || arg[i] == '\t' || arg[i] == '"') {
      needs_quotes = 1;
      break;
    }
  }
  if (needs_quotes) {
    if (*pos + 1 >= size) {
      return -E2BIG;
    }
    buffer[(*pos)++] = '"';
  }
  for (i = 0; arg[i] != 0; ++i) {
    if (arg[i] == '"') {
      if (*pos + 2 >= size) {
        return -E2BIG;
      }
      buffer[(*pos)++] = '\\';
      buffer[(*pos)++] = '"';
    } else {
      if (*pos + 1 >= size) {
        return -E2BIG;
      }
      buffer[(*pos)++] = arg[i];
    }
  }
  if (needs_quotes) {
    if (*pos + 1 >= size) {
      return -E2BIG;
    }
    buffer[(*pos)++] = '"';
  }
  buffer[*pos] = 0;
  return 0;
}

static long resolve_process_application_path(
    const char* path,
    int search_path,
    char* buffer,
    size_t size) {
  char translated_path[4096];
  char searched_path[4096];
  const char* host_path = translate_path_for_host(path, translated_path);

  if (path == 0 || buffer == 0 || size == 0) {
    return -EINVAL;
  }
  if ((search_path || windows_rootfs() != 0) &&
      strchr(host_path, '/') == 0 &&
      strchr(host_path, '\\') == 0 &&
      !(host_path[0] != 0 && host_path[1] == ':')) {
    const char* env_path = getenv("PATH");
    if (env_path != 0) {
      const char* entry = env_path;

      while (*entry != 0) {
        char candidate[4096];
        char candidate_host_buffer[4096];
        const char* next = entry;
        const char* candidate_host;
        size_t entry_len;

        while (*next != 0 && *next != ':' && *next != ';') {
          ++next;
        }
        entry_len = (size_t)(next - entry);
        if (entry_len == 0) {
          candidate[0] = '.';
          candidate[1] = '/';
          entry_len = 1;
        } else if (entry_len + 1 < sizeof(candidate)) {
          memcpy(candidate, entry, entry_len);
          candidate[entry_len] = '/';
        }
        if (entry_len + 1 + strlen(path) < sizeof(candidate)) {
          memcpy(candidate + entry_len + 1, path, strlen(path) + 1);
          candidate_host = translate_path_for_host(candidate, candidate_host_buffer);
          if (windows_path_is_executable_file(candidate_host, GetFileAttributesA(candidate_host))) {
            if (strlen(candidate_host) >= size) {
              return -ENAMETOOLONG;
            }
            strcpy(buffer, candidate_host);
            return 0;
          }
        }
        entry = *next == 0 ? next : next + 1;
      }
    }
    DWORD found = SearchPathA(0, host_path, ".exe", (DWORD)sizeof(searched_path), searched_path, 0);

    if (found != 0 && found < sizeof(searched_path)) {
      host_path = searched_path;
    }
  }
  if (strlen(host_path) >= size) {
    return -ENAMETOOLONG;
  }
  strcpy(buffer, host_path);
  return 0;
}

static long build_process_command_line(
    const char* path,
    char* const argv[],
    char* buffer,
    size_t size) {
  size_t pos = 0;
  int result;
  size_t i;

  if (path == 0 || buffer == 0 || size == 0) {
    return -EINVAL;
  }
  if (argv == 0 || argv[0] == 0) {
    result = append_command_arg(buffer, size, &pos, path);
    return result == 0 ? 0 : result;
  }
  for (i = 0; argv[i] != 0; ++i) {
    result = append_command_arg(buffer, size, &pos, argv[i]);
    if (result != 0) {
      return result;
    }
  }
  return 0;
}

struct crt_env_extra {
  const char* name;
  const char* value;
};

static int env_entry_matches_name(const char* entry, const char* name) {
  size_t name_len = strlen(name);

  return strncmp(entry, name, name_len) == 0 && entry[name_len] == '=';
}

static int env_entry_is_bootstrap_extra(
    const char* entry,
    const struct crt_env_extra* extras,
    size_t extra_count) {
  size_t i;

  for (i = 0; i < extra_count; ++i) {
    if (extras[i].name != 0 && env_entry_matches_name(entry, extras[i].name)) {
      return 1;
    }
  }
  return env_entry_matches_name(entry, CRT_FD_SNAPSHOT_ENV) ||
         env_entry_matches_name(entry, CRT_CHILD_BOOTSTRAP_ENV) ||
         env_entry_matches_name(entry, CRT_BOOTSTRAP_CWD_ENV) ||
         env_entry_matches_name(entry, CRT_BOOTSTRAP_ROOTFS_ENV) ||
         env_entry_matches_name(entry, CRT_BOOTSTRAP_SIGMASK_ENV) ||
         env_entry_matches_name(entry, CRT_BOOTSTRAP_SIGDEFAULT_ENV);
}

static size_t environment_block_total(
    char* const envp[],
    const struct crt_env_extra* extras,
    size_t extra_count,
    char** host_block_out) {
  size_t total = 1;
  size_t i;
  char* host_block = 0;

  *host_block_out = 0;
  if (envp != 0) {
    for (i = 0; envp[i] != 0; ++i) {
      if (env_entry_is_bootstrap_extra(envp[i], extras, extra_count)) {
        continue;
      }
      total += strlen(envp[i]) + 1;
    }
  } else {
    char* entry;

    host_block = GetEnvironmentStringsA();
    if (host_block == 0) {
      return 0;
    }
    *host_block_out = host_block;
    for (entry = host_block; *entry != 0; entry += strlen(entry) + 1) {
      if (env_entry_is_bootstrap_extra(entry, extras, extra_count)) {
        continue;
      }
      total += strlen(entry) + 1;
    }
  }
  for (i = 0; i < extra_count; ++i) {
    if (extras[i].name != 0 && extras[i].value != 0) {
      total += strlen(extras[i].name) + 1 + strlen(extras[i].value) + 1;
    }
  }
  return total;
}

static char* build_windows_environment_block(
    char* const envp[],
    const struct crt_env_extra* extras,
    size_t extra_count) {
  char* host_block = 0;
  size_t total = environment_block_total(envp, extras, extra_count, &host_block);
  char* block;
  size_t offset = 0;
  size_t i;

  if (total == 0) {
    return 0;
  }
  block = (char*)malloc(total + 1);
  if (block == 0) {
    if (host_block != 0) {
      FreeEnvironmentStringsA(host_block);
    }
    return 0;
  }
  if (envp != 0) {
    for (i = 0; envp[i] != 0; ++i) {
      size_t len;

      if (env_entry_is_bootstrap_extra(envp[i], extras, extra_count)) {
        continue;
      }
      len = strlen(envp[i]);
      memcpy(block + offset, envp[i], len);
      offset += len;
      block[offset++] = 0;
    }
  } else {
    char* entry;

    for (entry = host_block; *entry != 0; entry += strlen(entry) + 1) {
      size_t len;

      if (env_entry_is_bootstrap_extra(entry, extras, extra_count)) {
        continue;
      }
      len = strlen(entry);
      memcpy(block + offset, entry, len);
      offset += len;
      block[offset++] = 0;
    }
  }
  for (i = 0; i < extra_count; ++i) {
    size_t name_len;
    size_t value_len;

    if (extras[i].name == 0 || extras[i].value == 0) {
      continue;
    }
    name_len = strlen(extras[i].name);
    value_len = strlen(extras[i].value);
    memcpy(block + offset, extras[i].name, name_len);
    offset += name_len;
    block[offset++] = '=';
    memcpy(block + offset, extras[i].value, value_len);
    offset += value_len;
    block[offset++] = 0;
  }
  if (host_block != 0) {
    FreeEnvironmentStringsA(host_block);
  }
  block[offset++] = 0;
  block[offset] = 0;
  return block;
}

static void format_hex_u64(unsigned long long value, char buffer[17]) {
  static const char hex[] = "0123456789abcdef";
  int i;

  for (i = 15; i >= 0; --i) {
    buffer[i] = hex[value & 0xfU];
    value >>= 4;
  }
  buffer[16] = 0;
}

static int parse_hex_u64(const char* text, unsigned long long* value) {
  unsigned long long result = 0;
  int digits = 0;

  if (text == 0 || value == 0) {
    return -EINVAL;
  }
  while (*text != 0) {
    unsigned int digit;

    if (*text >= '0' && *text <= '9') {
      digit = (unsigned int)(*text - '0');
    } else if (*text >= 'a' && *text <= 'f') {
      digit = (unsigned int)(*text - 'a' + 10);
    } else if (*text >= 'A' && *text <= 'F') {
      digit = (unsigned int)(*text - 'A' + 10);
    } else {
      return -EINVAL;
    }
    result = (result << 4) | digit;
    ++digits;
    ++text;
  }
  if (digits == 0 || digits > 16) {
    return -EINVAL;
  }
  *value = result;
  return 0;
}

static void add_env_extra(
    struct crt_env_extra* extras,
    size_t* count,
    const char* name,
    const char* value) {
  if (name != 0 && value != 0 && value[0] != 0) {
    extras[*count].name = name;
    extras[*count].value = value;
    ++*count;
  }
}

static int snapshot_kind_from_fd_kind(int kind) {
  if (kind == CRT_FD_KIND_FILE) {
    return CRT_FD_SNAPSHOT_KIND_FILE;
  }
  if (kind == CRT_FD_KIND_SOCKET) {
    return CRT_FD_SNAPSHOT_KIND_SOCKET;
  }
  return CRT_FD_SNAPSHOT_KIND_NONE;
}

static int fd_kind_from_snapshot_kind(int kind) {
  if (kind == CRT_FD_SNAPSHOT_KIND_FILE) {
    return CRT_FD_KIND_FILE;
  }
  if (kind == CRT_FD_SNAPSHOT_KIND_SOCKET) {
    return CRT_FD_KIND_SOCKET;
  }
  return CRT_FD_KIND_NONE;
}

int __crt_fd_snapshot_export(struct crt_fd_snapshot* snapshot) {
  unsigned int count = 0;
  int fd;

  if (snapshot == 0) {
    return EINVAL;
  }
  init_fd_table();
  memset(snapshot, 0, sizeof(*snapshot));
  snapshot->magic = CRT_FD_SNAPSHOT_MAGIC;
  snapshot->version = CRT_FD_SNAPSHOT_VERSION;
  snapshot->capacity = CRT_FD_SNAPSHOT_MAX;
  for (fd = 0; fd < CRT_FD_TABLE_SIZE; ++fd) {
    HANDLE duplicate = 0;

    if ((fd_kind[fd] != CRT_FD_KIND_FILE && fd_kind[fd] != CRT_FD_KIND_SOCKET) ||
        (fd_flags[fd] & FD_CLOEXEC) != 0 ||
        fd_table[fd] == 0 ||
        fd_table[fd] == INVALID_HANDLE_VALUE) {
      continue;
    }
    if (count == CRT_FD_SNAPSHOT_MAX) {
      __crt_fd_snapshot_dispose(snapshot);
      return EMFILE;
    }
    if (!DuplicateHandle(
            GetCurrentProcess(),
            fd_table[fd],
            GetCurrentProcess(),
            &duplicate,
            0,
            0,
            DUPLICATE_SAME_ACCESS)) {
      int error = map_windows_error(GetLastError());

      if (error == EBADF) {
        continue;
      }
      __crt_fd_snapshot_dispose(snapshot);
      return error;
    }
    snapshot->entries[count].fd = fd;
    snapshot->entries[count].kind = snapshot_kind_from_fd_kind(fd_kind[fd]);
    snapshot->entries[count].flags =
        (fd_kind[fd] == CRT_FD_KIND_FILE ? CRT_FD_SNAPSHOT_FLAG_INHERITABLE : 0) |
        ((fd_flags[fd] & O_APPEND) != 0 ? CRT_FD_SNAPSHOT_FLAG_APPEND : 0);
    snapshot->entries[count].handle = (uintptr_t)duplicate;
    ++count;
  }
  snapshot->count = count;
  return 0;
}

int __crt_fd_snapshot_import(const struct crt_fd_snapshot* snapshot) {
  unsigned int i;

  if (snapshot == 0 ||
      snapshot->magic != CRT_FD_SNAPSHOT_MAGIC ||
      snapshot->version != CRT_FD_SNAPSHOT_VERSION ||
      snapshot->count > snapshot->capacity ||
      snapshot->capacity > CRT_FD_SNAPSHOT_MAX) {
    return EINVAL;
  }
  init_fd_table();
  for (i = 0; i < snapshot->count; ++i) {
    const struct crt_fd_snapshot_entry* entry = &snapshot->entries[i];
    HANDLE duplicate = 0;
    int kind = fd_kind_from_snapshot_kind(entry->kind);

    if (entry->fd < 0 ||
        entry->fd >= CRT_FD_TABLE_SIZE ||
        kind == CRT_FD_KIND_NONE ||
        (((entry->flags & CRT_FD_SNAPSHOT_FLAG_SOCKET_DUPLICATED) == 0 ||
          kind != CRT_FD_KIND_SOCKET) &&
         (entry->handle == 0 || entry->handle == (uintptr_t)INVALID_HANDLE_VALUE))) {
      return EINVAL;
    }
    if (kind == CRT_FD_KIND_SOCKET &&
        (entry->flags & CRT_FD_SNAPSHOT_FLAG_SOCKET_DUPLICATED) != 0) {
      SOCKET socket_handle;

      if (entry->socket_protocol_info_size == 0 ||
          entry->socket_protocol_info_size > CRT_FD_SOCKET_PROTOCOL_INFO_SIZE) {
        return EINVAL;
      }
      if (init_winsock() < 0) {
        return ENOSYS;
      }
      socket_handle = winsock.WSASocketA(
          CRT_FROM_PROTOCOL_INFO,
          CRT_FROM_PROTOCOL_INFO,
          CRT_FROM_PROTOCOL_INFO,
          (void*)entry->socket_protocol_info,
          0,
          0);
      if (socket_handle == INVALID_SOCKET) {
        return map_wsa_error(winsock.WSAGetLastError());
      }
      duplicate = (HANDLE)(uintptr_t)socket_handle;
    } else {
      if (!DuplicateHandle(GetCurrentProcess(), (HANDLE)entry->handle, GetCurrentProcess(),
                           &duplicate, 0, 0, DUPLICATE_SAME_ACCESS)) {
        return map_windows_error(GetLastError());
      }
      if ((entry->flags & CRT_FD_SNAPSHOT_FLAG_REMOTE_PROCESS_HANDLE) != 0) {
        CloseHandle((HANDLE)entry->handle);
      }
    }
    if (fd_kind[entry->fd] != CRT_FD_KIND_NONE &&
        fd_table[entry->fd] != 0 &&
        fd_table[entry->fd] != INVALID_HANDLE_VALUE) {
      close_fd_slot(entry->fd);
    }
    fd_table[entry->fd] = duplicate;
    fd_kind[entry->fd] = kind;
    fd_flags[entry->fd] = (entry->flags & CRT_FD_SNAPSHOT_FLAG_APPEND) != 0 ? O_APPEND : 0;
  }
  return 0;
}

void __crt_fd_snapshot_dispose(struct crt_fd_snapshot* snapshot) {
  unsigned int i;

  if (snapshot == 0) {
    return;
  }
  for (i = 0; i < snapshot->count && i < CRT_FD_SNAPSHOT_MAX; ++i) {
    if (snapshot->entries[i].handle != 0 &&
        snapshot->entries[i].handle != (uintptr_t)INVALID_HANDLE_VALUE) {
      if ((snapshot->entries[i].flags & CRT_FD_SNAPSHOT_FLAG_REMOTE_PROCESS_HANDLE) != 0) {
        continue;
      }
      if (snapshot->entries[i].kind == CRT_FD_SNAPSHOT_KIND_SOCKET) {
        init_winsock();
        if (winsock.closesocket != 0) {
          winsock.closesocket((SOCKET)snapshot->entries[i].handle);
        }
      } else {
        CloseHandle((HANDLE)snapshot->entries[i].handle);
      }
    }
  }
  memset(snapshot, 0, sizeof(*snapshot));
}

static int fd_snapshot_has_socket(const struct crt_fd_snapshot* snapshot) {
  unsigned int i;

  if (snapshot == 0) {
    return 0;
  }
  for (i = 0; i < snapshot->count && i < CRT_FD_SNAPSHOT_MAX; ++i) {
    if (snapshot->entries[i].kind == CRT_FD_SNAPSHOT_KIND_SOCKET) {
      return 1;
    }
  }
  return 0;
}

static int envp_has_name(char* const envp[], const char* name) {
  size_t name_len = strlen(name);
  size_t i;

  if (envp == 0 || name == 0) {
    return 0;
  }
  for (i = 0; envp[i] != 0; ++i) {
    if (strncmp(envp[i], name, name_len) == 0 && envp[i][name_len] == '=') {
      return 1;
    }
  }
  return 0;
}

static long fd_snapshot_remove(struct crt_fd_snapshot* snapshot, int fd);

static long fd_snapshot_prepare_child_duplicates(
    struct crt_fd_snapshot* snapshot,
    HANDLE child_process,
    DWORD child_pid) {
  unsigned int i;
  long init_result;

  if (fd_snapshot_has_socket(snapshot)) {
    init_result = init_winsock();
    if (init_result < 0) {
      return init_result;
    }
  }
  for (i = 0; i < snapshot->count && i < CRT_FD_SNAPSHOT_MAX; ++i) {
    struct crt_fd_snapshot_entry* entry = &snapshot->entries[i];

    if (entry->kind == CRT_FD_SNAPSHOT_KIND_FILE) {
      HANDLE child_handle = 0;

      if (entry->handle == 0 || entry->handle == (uintptr_t)INVALID_HANDLE_VALUE) {
        return -EINVAL;
      }
      if (!DuplicateHandle(
              GetCurrentProcess(),
              (HANDLE)entry->handle,
              child_process,
              &child_handle,
              0,
              0,
              DUPLICATE_SAME_ACCESS)) {
        int error = map_windows_error(GetLastError());

        if (error == EBADF) {
          fd_snapshot_remove(snapshot, entry->fd);
          --i;
          continue;
        }
        return -error;
      }
      CloseHandle((HANDLE)entry->handle);
      entry->handle = (uintptr_t)child_handle;
      entry->flags &= ~CRT_FD_SNAPSHOT_FLAG_INHERITABLE;
      entry->flags |= CRT_FD_SNAPSHOT_FLAG_REMOTE_PROCESS_HANDLE;
      continue;
    }
    if (entry->kind != CRT_FD_SNAPSHOT_KIND_SOCKET) {
      continue;
    }
    memset(entry->socket_protocol_info, 0, sizeof(entry->socket_protocol_info));
    if (winsock.WSADuplicateSocketA(
            (SOCKET)entry->handle,
            child_pid,
            entry->socket_protocol_info) != 0) {
      return -map_wsa_error(winsock.WSAGetLastError());
    }
    if (entry->handle != 0 && entry->handle != (uintptr_t)INVALID_HANDLE_VALUE) {
      winsock.closesocket((SOCKET)entry->handle);
    }
    entry->handle = 0;
    entry->flags |= CRT_FD_SNAPSHOT_FLAG_SOCKET_DUPLICATED;
    entry->socket_protocol_info_size = CRT_FD_SOCKET_PROTOCOL_INFO_SIZE;
  }
  return 0;
}

/* Only __crt_sys_fork()'s RtlCloneUserProcess path calls this. */
static void fd_set_inherit_for_fork(unsigned char touched[CRT_FD_TABLE_SIZE],
                                    DWORD old_flags[CRT_FD_TABLE_SIZE]) {
  int fd;

  init_fd_table();
  memset(touched, 0, CRT_FD_TABLE_SIZE);
  for (fd = 0; fd < CRT_FD_TABLE_SIZE; ++fd) {
    DWORD flags = 0;

    if (fd_kind[fd] != CRT_FD_KIND_FILE ||
        fd_table[fd] == 0 ||
        fd_table[fd] == INVALID_HANDLE_VALUE) {
      continue;
    }
    if (!GetHandleInformation(fd_table[fd], &flags)) {
      continue;
    }
    touched[fd] = 1;
    old_flags[fd] = flags;
    if ((flags & HANDLE_FLAG_INHERIT) == 0) {
      (void)SetHandleInformation(fd_table[fd], HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
    }
  }
}

static void fd_restore_inherit_after_fork(const unsigned char touched[CRT_FD_TABLE_SIZE],
                                          const DWORD old_flags[CRT_FD_TABLE_SIZE]) {
  int fd;

  for (fd = 0; fd < CRT_FD_TABLE_SIZE; ++fd) {
    if (touched[fd] &&
        fd_kind[fd] == CRT_FD_KIND_FILE &&
        fd_table[fd] != 0 &&
        fd_table[fd] != INVALID_HANDLE_VALUE) {
      (void)SetHandleInformation(
          fd_table[fd], HANDLE_FLAG_INHERIT, old_flags[fd] & HANDLE_FLAG_INHERIT);
    }
  }
}

static void fd_clear_inherit_for_spawn(unsigned char touched[CRT_FD_TABLE_SIZE],
                                       DWORD old_flags[CRT_FD_TABLE_SIZE]) {
  int fd;

  init_fd_table();
  memset(touched, 0, CRT_FD_TABLE_SIZE);
  for (fd = 0; fd < CRT_FD_TABLE_SIZE; ++fd) {
    DWORD flags = 0;

    if (fd_kind[fd] != CRT_FD_KIND_FILE ||
        fd_table[fd] == 0 ||
        fd_table[fd] == INVALID_HANDLE_VALUE) {
      continue;
    }
    if (!GetHandleInformation(fd_table[fd], &flags)) {
      continue;
    }
    touched[fd] = 1;
    old_flags[fd] = flags;
    if ((flags & HANDLE_FLAG_INHERIT) != 0) {
      (void)SetHandleInformation(fd_table[fd], HANDLE_FLAG_INHERIT, 0);
    }
  }
}

static void fd_disable_inherit_in_current_process(void) {
  int fd;

  init_fd_table();
  for (fd = 0; fd < CRT_FD_TABLE_SIZE; ++fd) {
    if (fd_kind[fd] == CRT_FD_KIND_FILE &&
        fd_table[fd] != 0 &&
        fd_table[fd] != INVALID_HANDLE_VALUE) {
      (void)SetHandleInformation(fd_table[fd], HANDLE_FLAG_INHERIT, 0);
    }
  }
}

void __crt_fd_after_fork_child(void) {
  int i;

  fd_disable_inherit_in_current_process();
  for (i = 0; i < CRT_FD_TABLE_SIZE; ++i) {
    if (child_process_table[i] != 0 && child_process_table[i] != INVALID_HANDLE_VALUE) {
      CloseHandle(child_process_table[i]);
    }
    child_process_table[i] = 0;
    child_pid_table[i] = 0;
  }
  if (private_wait_process != 0 && private_wait_process != INVALID_HANDLE_VALUE) {
    CloseHandle(private_wait_process);
  }
  private_wait_process = 0;
  private_wait_pid = 0;
}

int __crt_fd_get_cloexec(int fd) {
  init_fd_table();
  if (fd < 0 || fd >= CRT_FD_TABLE_SIZE || fd_kind[fd] == CRT_FD_KIND_NONE) {
    errno = EBADF;
    return 0;
  }
  return (fd_flags[fd] & FD_CLOEXEC) != 0;
}

int __crt_fd_set_cloexec(int fd, int cloexec) {
  init_fd_table();
  if (fd < 0 || fd >= CRT_FD_TABLE_SIZE || fd_kind[fd] == CRT_FD_KIND_NONE) {
    errno = EBADF;
    return -1;
  }
  if (cloexec) {
    fd_flags[fd] |= FD_CLOEXEC;
  } else {
    fd_flags[fd] &= ~FD_CLOEXEC;
  }
  return 0;
}

static int bootstrap_read_exact(HANDLE handle, void* buffer, DWORD size) {
  char* out = (char*)buffer;
  DWORD offset = 0;

  while (offset < size) {
    DWORD got = 0;

    if (!ReadFile(handle, out + offset, size - offset, &got, 0) || got == 0) {
      return -EIO;
    }
    offset += got;
  }
  return 0;
}

static int bootstrap_write_exact(HANDLE handle, const void* buffer, DWORD size) {
  const char* in = (const char*)buffer;
  DWORD offset = 0;

  while (offset < size) {
    DWORD wrote = 0;

    if (!WriteFile(handle, in + offset, size - offset, &wrote, 0) || wrote == 0) {
      return -EIO;
    }
    offset += wrote;
  }
  return 0;
}

static int bootstrap_read_fd_snapshot_from_pipe(char* buffer, DWORD size) {
  const char* pipe_text = getenv(CRT_FD_SNAPSHOT_PIPE_ENV);
  unsigned long long handle_value = 0;
  uint32_t length = 0;
  HANDLE pipe_handle;

  if (pipe_text == 0 || pipe_text[0] == 0 || buffer == 0 || size == 0) {
    return ENOENT;
  }
  if (parse_hex_u64(pipe_text, &handle_value) != 0 ||
      handle_value == 0 ||
      handle_value == (uintptr_t)INVALID_HANDLE_VALUE) {
    return EINVAL;
  }
  pipe_handle = (HANDLE)(uintptr_t)handle_value;
  if (bootstrap_read_exact(pipe_handle, &length, (DWORD)sizeof(length)) != 0 ||
      length == 0 ||
      length >= size) {
    CloseHandle(pipe_handle);
    return EINVAL;
  }
  if (bootstrap_read_exact(pipe_handle, buffer, length) != 0) {
    CloseHandle(pipe_handle);
    return EIO;
  }
  buffer[length] = 0;
  CloseHandle(pipe_handle);
  return 0;
}

void __crt_child_bootstrap(void) {
  const char* marker = getenv(CRT_CHILD_BOOTSTRAP_ENV);
  const char* encoded = getenv(CRT_FD_SNAPSHOT_ENV);
  const char* cwd = getenv(CRT_BOOTSTRAP_CWD_ENV);
  const char* rootfs = getenv(CRT_BOOTSTRAP_ROOTFS_ENV);
  const char* sigmask = getenv(CRT_BOOTSTRAP_SIGMASK_ENV);
  const char* sigdefault = getenv(CRT_BOOTSTRAP_SIGDEFAULT_ENV);
  struct crt_fd_snapshot snapshot;
  char pipe_snapshot_text[65536];
  unsigned long long mask_value;
  unsigned long long default_value;

  if ((marker == 0 || marker[0] == 0) && (encoded == 0 || encoded[0] == 0)) {
    return;
  }
  if (rootfs != 0 && rootfs[0] != 0) {
    (void)setenv("CRT_ROOTFS", rootfs, 1);
  }
  if (cwd != 0 && cwd[0] != 0) {
    (void)SetCurrentDirectoryA(cwd);
  }
  if (sigmask != 0 && parse_hex_u64(sigmask, &mask_value) == 0) {
    __crt_signal_set_mask((sigset64_t)mask_value);
  }
  if (sigdefault != 0 && parse_hex_u64(sigdefault, &default_value) == 0) {
    __crt_signal_reset_defaults((sigset64_t)default_value);
  }
  if ((encoded == 0 || encoded[0] == 0) &&
      bootstrap_read_fd_snapshot_from_pipe(pipe_snapshot_text, sizeof(pipe_snapshot_text)) == 0) {
    encoded = pipe_snapshot_text;
  }
  if (encoded == 0 || encoded[0] == 0) {
    return;
  }
  if (__crt_fd_snapshot_decode(encoded, &snapshot) == 0) {
    if (__crt_fd_snapshot_import(&snapshot) == 0) {
      __crt_fd_snapshot_dispose(&snapshot);
    }
  }
}

static long open_spawn_action_handle(
    const char* path,
    int flags,
    unsigned int mode,
    HANDLE* out) {
  char translated_path[4096];
  const char* host_path = translate_path_for_host(path, translated_path);
  DWORD access = 0;
  DWORD disposition = OPEN_EXISTING;
  DWORD attrs;
  DWORD file_flags = FILE_ATTRIBUTE_NORMAL;
  struct {
    DWORD nLength;
    void* lpSecurityDescriptor;
    BOOL bInheritHandle;
  } security_attributes;
  (void)mode;

  if ((flags & O_RDWR) == O_RDWR) {
    access = GENERIC_READ | GENERIC_WRITE;
  } else if (flags & O_WRONLY) {
    access = GENERIC_WRITE;
  } else {
    access = GENERIC_READ;
  }
  if ((flags & O_CREAT) && (flags & O_EXCL)) {
    disposition = CREATE_NEW;
  } else if ((flags & O_CREAT) && (flags & O_TRUNC)) {
    disposition = CREATE_ALWAYS;
  } else if (flags & O_CREAT) {
    disposition = OPEN_ALWAYS;
  } else {
    disposition = OPEN_EXISTING;
  }
  attrs = GetFileAttributesA(host_path);
  if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0) {
    file_flags = FILE_FLAG_BACKUP_SEMANTICS;
  }
  security_attributes.nLength = sizeof(security_attributes);
  security_attributes.lpSecurityDescriptor = 0;
  security_attributes.bInheritHandle = 0;
  *out = CreateFileA(host_path, access, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                     &security_attributes, disposition, file_flags, 0);
  return *out == INVALID_HANDLE_VALUE ? fail_last_error() : 0;
}

static struct crt_fd_snapshot_entry* fd_snapshot_find_entry(struct crt_fd_snapshot* snapshot, int fd) {
  unsigned int i;

  for (i = 0; i < snapshot->count; ++i) {
    if (snapshot->entries[i].fd == fd) {
      return &snapshot->entries[i];
    }
  }
  return 0;
}

static const struct crt_fd_snapshot_entry* fd_snapshot_find_const_entry(
    const struct crt_fd_snapshot* snapshot,
    int fd) {
  unsigned int i;

  for (i = 0; i < snapshot->count; ++i) {
    if (snapshot->entries[i].fd == fd) {
      return &snapshot->entries[i];
    }
  }
  return 0;
}

static void fd_snapshot_close_entry(struct crt_fd_snapshot_entry* entry) {
  if (entry->handle != 0 && entry->handle != (uintptr_t)INVALID_HANDLE_VALUE) {
    if (entry->kind == CRT_FD_SNAPSHOT_KIND_SOCKET) {
      init_winsock();
      if (winsock.closesocket != 0) {
        winsock.closesocket((SOCKET)entry->handle);
      }
    } else {
      CloseHandle((HANDLE)entry->handle);
    }
  }
  entry->handle = 0;
  entry->kind = CRT_FD_SNAPSHOT_KIND_NONE;
  entry->flags = 0;
}

static long fd_snapshot_remove(struct crt_fd_snapshot* snapshot, int fd) {
  unsigned int i;

  if (fd < 0 || fd >= CRT_FD_TABLE_SIZE) {
    return -EBADF;
  }
  for (i = 0; i < snapshot->count; ++i) {
    if (snapshot->entries[i].fd == fd) {
      fd_snapshot_close_entry(&snapshot->entries[i]);
      if (i + 1 < snapshot->count) {
        snapshot->entries[i] = snapshot->entries[snapshot->count - 1];
      }
      --snapshot->count;
      memset(&snapshot->entries[snapshot->count], 0, sizeof(snapshot->entries[snapshot->count]));
      return 0;
    }
  }
  return 0;
}

static long fd_snapshot_set_handle(
    struct crt_fd_snapshot* snapshot,
    int fd,
    int kind,
    HANDLE handle) {
  struct crt_fd_snapshot_entry* entry;

  if (fd < 0 || fd >= CRT_FD_TABLE_SIZE) {
    return -EBADF;
  }
  if ((kind != CRT_FD_SNAPSHOT_KIND_FILE && kind != CRT_FD_SNAPSHOT_KIND_SOCKET) ||
      handle == 0 ||
      handle == INVALID_HANDLE_VALUE) {
    return -EINVAL;
  }
  entry = fd_snapshot_find_entry(snapshot, fd);
  if (entry == 0) {
    if (snapshot->count == CRT_FD_SNAPSHOT_MAX) {
      return -EMFILE;
    }
    entry = &snapshot->entries[snapshot->count++];
    memset(entry, 0, sizeof(*entry));
    entry->fd = fd;
  } else {
    fd_snapshot_close_entry(entry);
  }
  entry->fd = fd;
  entry->kind = kind;
  entry->flags = kind == CRT_FD_SNAPSHOT_KIND_FILE ? CRT_FD_SNAPSHOT_FLAG_INHERITABLE : 0;
  entry->handle = (uintptr_t)handle;
  return 0;
}

static HANDLE fd_snapshot_handle_for_fd(const struct crt_fd_snapshot* snapshot, int fd) {
  const struct crt_fd_snapshot_entry* entry = fd_snapshot_find_const_entry(snapshot, fd);

  if (entry == 0 ||
      entry->kind != CRT_FD_SNAPSHOT_KIND_FILE ||
      entry->handle == 0 ||
      entry->handle == (uintptr_t)INVALID_HANDLE_VALUE) {
    return INVALID_HANDLE_VALUE;
  }
  return (HANDLE)entry->handle;
}

static long fd_snapshot_dup2(struct crt_fd_snapshot* snapshot, int fd, int new_fd) {
  const struct crt_fd_snapshot_entry* source_entry = fd_snapshot_find_const_entry(snapshot, fd);
  HANDLE source;
  HANDLE duplicate = 0;

  if (fd < 0 || fd >= CRT_FD_TABLE_SIZE || new_fd < 0 || new_fd >= CRT_FD_TABLE_SIZE) {
    return -EBADF;
  }
  if (source_entry == 0 ||
      (source_entry->kind != CRT_FD_SNAPSHOT_KIND_FILE &&
       source_entry->kind != CRT_FD_SNAPSHOT_KIND_SOCKET) ||
      source_entry->handle == 0 ||
      source_entry->handle == (uintptr_t)INVALID_HANDLE_VALUE) {
    return -EBADF;
  }
  if (fd == new_fd) {
    return 0;
  }
  source = (HANDLE)source_entry->handle;
  if (!DuplicateHandle(
          GetCurrentProcess(),
          source,
          GetCurrentProcess(),
          &duplicate,
          0,
          0,
          DUPLICATE_SAME_ACCESS)) {
    return -map_windows_error(GetLastError());
  }
  return fd_snapshot_set_handle(snapshot, new_fd, source_entry->kind, duplicate);
}

static long prepare_spawn_startup(
    const posix_spawn_file_actions_t actions,
    const posix_spawnattr_t attr,
    struct crt_startupinfo* startup,
    const char** current_directory,
    char current_directory_buffer[4096],
    struct crt_fd_snapshot* fd_snapshot,
    DWORD* creation_flags) {
  struct __posix_spawn_file_action* action;
  HANDLE std_input;
  HANDLE std_output;
  HANDLE std_error;

  memset(startup, 0, sizeof(*startup));
  startup->cb = sizeof(*startup);
  *current_directory = 0;
  *creation_flags = 0;
  if (actions != 0) {
    for (action = actions->head; action != 0; action = action->next) {
      if (action->kind == CRT_SPAWN_ACTION_OPEN) {
        HANDLE handle = 0;
        long result;

        result = open_spawn_action_handle(action->path, action->flags, action->mode, &handle);
        if (result != 0) {
          return result;
        }
        result = fd_snapshot_set_handle(
            fd_snapshot, action->new_fd, CRT_FD_SNAPSHOT_KIND_FILE, handle);
        if (result != 0) {
          CloseHandle(handle);
          return result;
        }
      } else if (action->kind == CRT_SPAWN_ACTION_CLOSE) {
        long result = fd_snapshot_remove(fd_snapshot, action->fd);

        if (result != 0) {
          return result;
        }
      } else if (action->kind == CRT_SPAWN_ACTION_DUP2) {
        long result = fd_snapshot_dup2(fd_snapshot, action->fd, action->new_fd);

        if (result != 0) {
          return result;
        }
      } else if (action->kind == CRT_SPAWN_ACTION_CHDIR) {
        const char* host_path = translate_path_for_host(action->path, current_directory_buffer);

        *current_directory = host_path;
      } else {
        return -ENOTSUP;
      }
    }
  }
  if (attr != 0 &&
      (attr->flags & (POSIX_SPAWN_SETPGROUP | POSIX_SPAWN_SETSID)) != 0) {
    *creation_flags |= CRT_CREATE_NEW_PROCESS_GROUP;
  }
  if (attr != 0 &&
      (attr->flags & (POSIX_SPAWN_SETSCHEDPARAM | POSIX_SPAWN_SETSCHEDULER)) != 0) {
    return -ENOTSUP;
  }
  std_input = fd_snapshot_handle_for_fd(fd_snapshot, 0);
  std_output = fd_snapshot_handle_for_fd(fd_snapshot, 1);
  std_error = fd_snapshot_handle_for_fd(fd_snapshot, 2);
  startup->dwFlags = CRT_STARTF_USESTDHANDLES;
  startup->hStdInput = std_input != INVALID_HANDLE_VALUE ? std_input : INVALID_HANDLE_VALUE;
  startup->hStdOutput = std_output != INVALID_HANDLE_VALUE ? std_output : INVALID_HANDLE_VALUE;
  startup->hStdError = std_error != INVALID_HANDLE_VALUE ? std_error : INVALID_HANDLE_VALUE;
  return 0;
}

static long remember_child_process(DWORD pid, HANDLE process) {
  int i;

  for (i = 0; i < CRT_FD_TABLE_SIZE; ++i) {
    if (child_process_table[i] == 0) {
      child_process_table[i] = process;
      child_pid_table[i] = pid;
      return (long)pid;
    }
  }
  return -EMFILE;
}

static long prepare_native_windows_spawn_startup(
    const posix_spawn_file_actions_t actions,
    const posix_spawnattr_t attr,
    struct crt_startupinfo* startup,
    const char** current_directory,
    char current_directory_buffer[4096],
    struct crt_fd_snapshot* fd_snapshot,
    DWORD* creation_flags) {
  HANDLE std_input;
  HANDLE std_output;
  HANDLE std_error;

  long result = prepare_spawn_startup(
      actions,
      attr,
      startup,
      current_directory,
      current_directory_buffer,
      fd_snapshot,
      creation_flags);
  if (result != 0) {
    return result;
  }
  std_input = fd_snapshot_handle_for_fd(fd_snapshot, 0);
  std_output = fd_snapshot_handle_for_fd(fd_snapshot, 1);
  std_error = fd_snapshot_handle_for_fd(fd_snapshot, 2);
  startup->dwFlags = CRT_STARTF_USESTDHANDLES;
  startup->hStdInput = std_input != INVALID_HANDLE_VALUE ? std_input : INVALID_HANDLE_VALUE;
  startup->hStdOutput = std_output != INVALID_HANDLE_VALUE ? std_output : INVALID_HANDLE_VALUE;
  startup->hStdError = std_error != INVALID_HANDLE_VALUE ? std_error : INVALID_HANDLE_VALUE;
  return 0;
}

static void set_native_spawn_stdio_inherit(
    const struct crt_startupinfo* startup,
    HANDLE handles[3],
    DWORD old_flags[3],
    unsigned char touched[3]) {
  int i;

  handles[0] = startup->hStdInput;
  handles[1] = startup->hStdOutput;
  handles[2] = startup->hStdError;
  memset(touched, 0, 3);
  for (i = 0; i < 3; ++i) {
    DWORD flags = 0;

    if (handles[i] == 0 || handles[i] == INVALID_HANDLE_VALUE) {
      continue;
    }
    if (!GetHandleInformation(handles[i], &flags)) {
      continue;
    }
    touched[i] = 1;
    old_flags[i] = flags;
    if ((flags & HANDLE_FLAG_INHERIT) == 0) {
      (void)SetHandleInformation(handles[i], HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
    }
  }
}

static void restore_native_spawn_stdio_inherit(
    const HANDLE handles[3],
    const DWORD old_flags[3],
    const unsigned char touched[3]) {
  int i;

  for (i = 0; i < 3; ++i) {
    if (touched[i]) {
      (void)SetHandleInformation(handles[i], HANDLE_FLAG_INHERIT, old_flags[i]);
    }
  }
}

static HANDLE find_child_process(long pid, int* index) {
  int i;

  for (i = 0; i < CRT_FD_TABLE_SIZE; ++i) {
    if (child_process_table[i] != 0 &&
        (pid == -1 || child_pid_table[i] == (DWORD)pid)) {
      if (index != 0) {
        *index = i;
      }
      return child_process_table[i];
    }
  }
  return 0;
}

static void forget_child_process_at(int index) {
  child_process_table[index] = 0;
  child_pid_table[index] = 0;
}

static time_t filetime_to_time(const struct crt_filetime* ft) {
  unsigned long long ticks = ((unsigned long long)ft->high << 32) | ft->low;

  if (ticks < SEC_TO_UNIX_EPOCH * WINDOWS_TICK) {
    return 0;
  }
  ticks -= SEC_TO_UNIX_EPOCH * WINDOWS_TICK;
  return (time_t)(ticks / WINDOWS_TICK);
}

static void time_to_filetime(time_t seconds, long microseconds, struct crt_filetime* ft) {
  unsigned long long ticks;

  if (seconds < 0) {
    seconds = 0;
  }
  if (microseconds < 0) {
    microseconds = 0;
  }
  ticks = ((unsigned long long)seconds + SEC_TO_UNIX_EPOCH) * WINDOWS_TICK;
  ticks += (unsigned long long)microseconds * 10ULL;
  ft->low = (DWORD)(ticks & 0xffffffffU);
  ft->high = (DWORD)(ticks >> 32);
}

static void init_fd_table(void) {
  if (fd_table_initialized) {
    return;
  }
  fd_table[0] = GetStdHandle(STD_INPUT_HANDLE);
  fd_table[1] = GetStdHandle(STD_OUTPUT_HANDLE);
  fd_table[2] = GetStdHandle(STD_ERROR_HANDLE);
  fd_kind[0] = CRT_FD_KIND_FILE;
  fd_kind[1] = CRT_FD_KIND_FILE;
  fd_kind[2] = CRT_FD_KIND_FILE;
  fd_flags[0] = 0;
  fd_flags[1] = 0;
  fd_flags[2] = 0;
  fd_table_initialized = 1;
}

static long init_winsock(void) {
  HANDLE module;
  unsigned char data[512];

  if (winsock_initialized) {
    return 0;
  }
  module = LoadLibraryA("ws2_32.dll");
  if (module == 0) {
    return -ENOSYS;
  }
  winsock.WSAStartup = (int (CRT_WINAPI*)(WORD, void*))GetProcAddress(module, "WSAStartup");
  winsock.WSAGetLastError = (int (CRT_WINAPI*)(void))GetProcAddress(module, "WSAGetLastError");
  winsock.socket = (SOCKET(CRT_WINAPI*)(int, int, int))GetProcAddress(module, "socket");
  winsock.bind =
      (int (CRT_WINAPI*)(SOCKET, const struct sockaddr*, int))GetProcAddress(module, "bind");
  winsock.listen = (int (CRT_WINAPI*)(SOCKET, int))GetProcAddress(module, "listen");
  winsock.accept =
      (SOCKET(CRT_WINAPI*)(SOCKET, struct sockaddr*, int*))GetProcAddress(module, "accept");
  winsock.connect =
      (int (CRT_WINAPI*)(SOCKET, const struct sockaddr*, int))GetProcAddress(module, "connect");
  winsock.send =
      (int (CRT_WINAPI*)(SOCKET, const char*, int, int))GetProcAddress(module, "send");
  winsock.recv = (int (CRT_WINAPI*)(SOCKET, char*, int, int))GetProcAddress(module, "recv");
  winsock.sendto = (int (CRT_WINAPI*)(
      SOCKET,
      const char*,
      int,
      int,
      const struct sockaddr*,
      int))GetProcAddress(module, "sendto");
  winsock.recvfrom = (int (CRT_WINAPI*)(
      SOCKET,
      char*,
      int,
      int,
      struct sockaddr*,
      int*))GetProcAddress(module, "recvfrom");
  winsock.getsockname =
      (int (CRT_WINAPI*)(SOCKET, struct sockaddr*, int*))GetProcAddress(module, "getsockname");
  winsock.setsockopt = (int (CRT_WINAPI*)(
      SOCKET,
      int,
      int,
      const char*,
      int))GetProcAddress(module, "setsockopt");
  winsock.shutdown = (int (CRT_WINAPI*)(SOCKET, int))GetProcAddress(module, "shutdown");
  winsock.closesocket = (int (CRT_WINAPI*)(SOCKET))GetProcAddress(module, "closesocket");
  winsock.ioctlsocket =
      (int (CRT_WINAPI*)(SOCKET, long, unsigned long*))GetProcAddress(module, "ioctlsocket");
  winsock.WSADuplicateSocketA =
      (int (CRT_WINAPI*)(SOCKET, DWORD, void*))GetProcAddress(module, "WSADuplicateSocketA");
  winsock.WSASocketA =
      (SOCKET(CRT_WINAPI*)(int, int, int, void*, unsigned int, DWORD))GetProcAddress(
          module, "WSASocketA");

  if (winsock.WSAStartup == 0 ||
      winsock.WSAGetLastError == 0 ||
      winsock.socket == 0 ||
      winsock.bind == 0 ||
      winsock.listen == 0 ||
      winsock.accept == 0 ||
      winsock.connect == 0 ||
      winsock.send == 0 ||
      winsock.recv == 0 ||
      winsock.sendto == 0 ||
      winsock.recvfrom == 0 ||
      winsock.getsockname == 0 ||
      winsock.setsockopt == 0 ||
      winsock.shutdown == 0 ||
      winsock.closesocket == 0 ||
      winsock.ioctlsocket == 0 ||
      winsock.WSADuplicateSocketA == 0 ||
      winsock.WSASocketA == 0) {
    return -ENOSYS;
  }
  if (winsock.WSAStartup((WORD)0x0202, data) != 0) {
    return -map_wsa_error(winsock.WSAGetLastError());
  }
  winsock_initialized = 1;
  return 0;
}

/* Only __crt_sys_fork()'s RtlCloneUserProcess path calls this. */
static long init_ntdll(void) {
  HANDLE module;

  if (ntdll_initialized) {
    return 0;
  }
  module = LoadLibraryA("ntdll.dll");
  if (module == 0) {
    return fail_last_error();
  }
  ntdll.RtlCloneUserProcess =
      (ULONG (CRT_WINAPI*)(ULONG, void*, void*, HANDLE, struct crt_rtl_user_process_information*))
          GetProcAddress(module, "RtlCloneUserProcess");
  if (ntdll.RtlCloneUserProcess == 0) {
    return -ENOTSUP;
  }
  ntdll_initialized = 1;
  return 0;
}

static HANDLE get_fd_handle(int fd) {
  init_fd_table();
  if (fd < 0 || fd >= CRT_FD_TABLE_SIZE || fd_kind[fd] != CRT_FD_KIND_FILE ||
      fd_table[fd] == 0 ||
      fd_table[fd] == INVALID_HANDLE_VALUE) {
    return INVALID_HANDLE_VALUE;
  }
  return fd_table[fd];
}

static SOCKET get_fd_socket(int fd) {
  init_fd_table();
  if (fd < 0 || fd >= CRT_FD_TABLE_SIZE || fd_kind[fd] != CRT_FD_KIND_SOCKET) {
    return INVALID_SOCKET;
  }
  return (SOCKET)(uintptr_t)fd_table[fd];
}

static int alloc_fd(HANDLE handle) {
  int fd;

  init_fd_table();
  for (fd = 3; fd < CRT_FD_TABLE_SIZE; ++fd) {
    if (fd_table[fd] == 0) {
      fd_table[fd] = handle;
      fd_kind[fd] = CRT_FD_KIND_FILE;
      fd_flags[fd] = 0;
      return fd;
    }
  }
  return -1;
}

static int alloc_socket_fd(SOCKET socket_handle) {
  int fd;

  init_fd_table();
  for (fd = 3; fd < CRT_FD_TABLE_SIZE; ++fd) {
    if (fd_kind[fd] == CRT_FD_KIND_NONE) {
      fd_table[fd] = (HANDLE)(uintptr_t)socket_handle;
      fd_kind[fd] = CRT_FD_KIND_SOCKET;
      fd_flags[fd] = 0;
      return fd;
    }
  }
  return -1;
}

long __crt_sys_read(int fd, void* buf, unsigned long count) {
  HANDLE handle = get_fd_handle(fd);
  DWORD bytes_read = 0;

  if (fd >= 0 && fd < CRT_FD_TABLE_SIZE && fd_kind[fd] == CRT_FD_KIND_SOCKET) {
    int result = winsock.recv((SOCKET)(uintptr_t)fd_table[fd], (char*)buf, (int)count, 0);
    return result == SOCKET_ERROR ? -map_wsa_error(winsock.WSAGetLastError()) : result;
  }
  if (handle == INVALID_HANDLE_VALUE) {
    return -EBADF;
  }
  if (!ReadFile(handle, buf, (DWORD)count, &bytes_read, 0)) {
    DWORD error = GetLastError();
    if (error == CRT_ERROR_BROKEN_PIPE || error == CRT_ERROR_HANDLE_EOF) {
      return 0;
    }
    return fail_last_error();
  }
  return (long)bytes_read;
}

long __crt_sys_write(int fd, const void* buf, unsigned long count) {
  HANDLE handle = get_fd_handle(fd);
  DWORD written = 0;

  if (fd >= 0 && fd < CRT_FD_TABLE_SIZE && fd_kind[fd] == CRT_FD_KIND_SOCKET) {
    int result = winsock.send((SOCKET)(uintptr_t)fd_table[fd], (const char*)buf, (int)count, 0);
    return result == SOCKET_ERROR ? -map_wsa_error(winsock.WSAGetLastError()) : result;
  }
  if (handle == INVALID_HANDLE_VALUE) {
    return -EBADF;
  }
  if (fd >= 0 && fd < CRT_FD_TABLE_SIZE && (fd_flags[fd] & O_APPEND) != 0 &&
      !SetFilePointerEx(handle, 0, 0, FILE_END)) {
    return fail_last_error();
  }
  if (!WriteFile(handle, buf, (DWORD)count, &written, 0)) {
    return fail_last_error();
  }
  return (long)written;
}

long __crt_sys_pread(int fd, void* buf, unsigned long count, long long offset) {
  HANDLE handle = get_fd_handle(fd);
  OVERLAPPED overlapped;
  DWORD bytes_read = 0;

  if (fd >= 0 && fd < CRT_FD_TABLE_SIZE && fd_kind[fd] == CRT_FD_KIND_SOCKET) {
    return -ESPIPE;
  }
  if (handle == INVALID_HANDLE_VALUE) {
    return -EBADF;
  }
  if (offset < 0) {
    return -EINVAL;
  }
  memset(&overlapped, 0, sizeof(overlapped));
  overlapped.Offset = (DWORD)((uint64_t)offset & 0xffffffffU);
  overlapped.OffsetHigh = (DWORD)(((uint64_t)offset >> 32) & 0xffffffffU);
  if (!ReadFile(handle, buf, (DWORD)count, &bytes_read, &overlapped)) {
    if (GetLastError() == CRT_ERROR_HANDLE_EOF) {
      return 0;
    }
    return fail_last_error();
  }
  return (long)bytes_read;
}

long __crt_sys_pwrite(int fd, const void* buf, unsigned long count, long long offset) {
  HANDLE handle = get_fd_handle(fd);
  OVERLAPPED overlapped;
  DWORD written = 0;

  if (fd >= 0 && fd < CRT_FD_TABLE_SIZE && fd_kind[fd] == CRT_FD_KIND_SOCKET) {
    return -ESPIPE;
  }
  if (handle == INVALID_HANDLE_VALUE) {
    return -EBADF;
  }
  if (offset < 0) {
    return -EINVAL;
  }
  memset(&overlapped, 0, sizeof(overlapped));
  overlapped.Offset = (DWORD)((uint64_t)offset & 0xffffffffU);
  overlapped.OffsetHigh = (DWORD)(((uint64_t)offset >> 32) & 0xffffffffU);
  if (!WriteFile(handle, buf, (DWORD)count, &written, &overlapped)) {
    return fail_last_error();
  }
  return (long)written;
}

static long windows_lock_start(HANDLE handle, const struct flock* lock, long long* start) {
  long long current = 0;
  long long end = 0;

  if (lock->l_whence == SEEK_SET) {
    *start = lock->l_start;
  } else if (lock->l_whence == SEEK_CUR) {
    if (!SetFilePointerEx(handle, 0, &current, FILE_CURRENT)) {
      return fail_last_error();
    }
    *start = current + lock->l_start;
  } else if (lock->l_whence == SEEK_END) {
    if (!SetFilePointerEx(handle, 0, &end, FILE_END)) {
      return fail_last_error();
    }
    *start = end + lock->l_start;
  } else {
    return -EINVAL;
  }
  if (*start < 0 || lock->l_len < 0) {
    return -EINVAL;
  }
  return 0;
}

long __crt_sys_fcntl(int fd, int cmd, void* arg) {
  HANDLE handle = get_fd_handle(fd);
  struct flock* lock = (struct flock*)arg;
  OVERLAPPED overlapped;
  unsigned long long length;
  long long start;
  DWORD flags = 0;
  long result;

  if (handle == INVALID_HANDLE_VALUE) {
    return -EBADF;
  }
  if (lock == 0) {
    return -EINVAL;
  }
  result = windows_lock_start(handle, lock, &start);
  if (result < 0) {
    return result;
  }
  length = lock->l_len == 0 ? 0xffffffffffffffffULL - (unsigned long long)start
                            : (unsigned long long)lock->l_len;
  memset(&overlapped, 0, sizeof(overlapped));
  overlapped.Offset = (DWORD)((uint64_t)start & 0xffffffffU);
  overlapped.OffsetHigh = (DWORD)(((uint64_t)start >> 32) & 0xffffffffU);

  if (cmd == F_GETLK) {
    flags = lock->l_type == F_WRLCK ? LOCKFILE_EXCLUSIVE_LOCK : 0;
    flags |= LOCKFILE_FAIL_IMMEDIATELY;
    if (LockFileEx(handle, flags, 0, (DWORD)(length & 0xffffffffU),
                   (DWORD)(length >> 32), &overlapped)) {
      (void)UnlockFileEx(handle, 0, (DWORD)(length & 0xffffffffU), (DWORD)(length >> 32),
                         &overlapped);
      lock->l_type = F_UNLCK;
      lock->l_pid = 0;
      return 0;
    }
    result = fail_last_error();
    if (result == -EAGAIN || result == -EACCES) {
      lock->l_type = F_WRLCK;
      lock->l_pid = 0;
      return 0;
    }
    return result;
  }
  if (cmd == F_SETLK || cmd == F_SETLKW) {
    if (lock->l_type == F_UNLCK) {
      return UnlockFileEx(handle, 0, (DWORD)(length & 0xffffffffU), (DWORD)(length >> 32),
                          &overlapped)
                 ? 0
                 : fail_last_error();
    }
    if (lock->l_type != F_RDLCK && lock->l_type != F_WRLCK) {
      return -EINVAL;
    }
    if (lock->l_type == F_WRLCK) {
      flags |= LOCKFILE_EXCLUSIVE_LOCK;
    }
    if (cmd == F_SETLK) {
      flags |= LOCKFILE_FAIL_IMMEDIATELY;
    }
    return LockFileEx(handle, flags, 0, (DWORD)(length & 0xffffffffU),
                      (DWORD)(length >> 32), &overlapped)
               ? 0
               : fail_last_error();
  }
  return -EINVAL;
}

long __crt_sys_open(const char* path, int flags, unsigned int mode) {
  char translated_path[4096];
  const char* host_path = translate_path_for_host(path, translated_path);
  DWORD access = 0;
  DWORD disposition = OPEN_EXISTING;
  DWORD file_flags = FILE_ATTRIBUTE_NORMAL;
  DWORD attrs;
  HANDLE handle;
  int fd;

  (void)mode;
  if ((flags & O_RDWR) == O_RDWR) {
    access = GENERIC_READ | GENERIC_WRITE;
  } else if (flags & O_WRONLY) {
    access = GENERIC_WRITE;
  } else {
    access = GENERIC_READ;
  }

  if ((flags & O_CREAT) && (flags & O_EXCL)) {
    disposition = CREATE_NEW;
  } else if ((flags & O_CREAT) && (flags & O_TRUNC)) {
    disposition = CREATE_ALWAYS;
  } else if (flags & O_CREAT) {
    disposition = OPEN_ALWAYS;
  } else {
    disposition = OPEN_EXISTING;
  }

  attrs = GetFileAttributesA(host_path);
  if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0) {
    file_flags = FILE_FLAG_BACKUP_SEMANTICS;
  }

  handle = CreateFileA(host_path, access, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       0, disposition, file_flags, 0);
  if (handle == INVALID_HANDLE_VALUE && path_is_dev_tty(path)) {
    if ((access & GENERIC_WRITE) != 0 && (access & GENERIC_READ) == 0) {
      handle = CreateFileA("CONOUT$", access, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    } else {
      handle = CreateFileA("CONIN$", GENERIC_READ | GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, 0);
      if (handle == INVALID_HANDLE_VALUE) {
        handle = CreateFileA("CONIN$", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
      }
    }
  }
  if (handle == INVALID_HANDLE_VALUE) {
    return fail_last_error();
  }

  fd = alloc_fd(handle);
  if (fd < 0) {
    CloseHandle(handle);
    return -EMFILE;
  }
  if ((flags & O_APPEND) != 0) {
    fd_flags[fd] |= O_APPEND;
  }
  return fd;
}

static long close_fd_slot(int fd) {
  HANDLE handle;

  init_fd_table();
  if (fd >= 0 && fd < CRT_FD_TABLE_SIZE && fd_kind[fd] == CRT_FD_KIND_SOCKET) {
    SOCKET socket_handle = (SOCKET)(uintptr_t)fd_table[fd];
    fd_table[fd] = 0;
    fd_kind[fd] = CRT_FD_KIND_NONE;
    fd_flags[fd] = 0;
    return winsock.closesocket(socket_handle) == SOCKET_ERROR
               ? -map_wsa_error(winsock.WSAGetLastError())
               : 0;
  }
  handle = get_fd_handle(fd);
  if (handle == INVALID_HANDLE_VALUE) {
    return -EBADF;
  }
  if (fd >= 0 && fd <= 2) {
    fd_table[fd] = 0;
    fd_kind[fd] = CRT_FD_KIND_NONE;
    fd_flags[fd] = 0;
    return 0;
  }
  fd_table[fd] = 0;
  fd_kind[fd] = CRT_FD_KIND_NONE;
  fd_flags[fd] = 0;
  if (!CloseHandle(handle)) {
    return fail_last_error();
  }
  return 0;
}

long __crt_sys_close(int fd) {
  return close_fd_slot(fd);
}

long long __crt_sys_lseek(int fd, long long offset, int whence) {
  HANDLE handle = get_fd_handle(fd);
  DWORD method;
  long long new_position = 0;

  if (handle == INVALID_HANDLE_VALUE) {
    return -EBADF;
  }

  if (whence == SEEK_SET) {
    method = FILE_BEGIN;
  } else if (whence == SEEK_CUR) {
    method = FILE_CURRENT;
  } else if (whence == SEEK_END) {
    method = FILE_END;
  } else {
    return -EINVAL;
  }

  if (!SetFilePointerEx(handle, offset, &new_position, method)) {
    return fail_last_error();
  }
  return new_position;
}

long __crt_sys_ftruncate(int fd, long long length) {
  HANDLE handle = get_fd_handle(fd);
  long long saved_position = 0;

  if (handle == INVALID_HANDLE_VALUE) {
    return -EBADF;
  }
  if (length < 0) {
    return -EINVAL;
  }
  if (!SetFilePointerEx(handle, 0, &saved_position, FILE_CURRENT)) {
    return fail_last_error();
  }
  if (!SetFilePointerEx(handle, length, 0, FILE_BEGIN)) {
    return fail_last_error();
  }
  if (!SetEndOfFile(handle)) {
    (void)SetFilePointerEx(handle, saved_position, 0, FILE_BEGIN);
    return fail_last_error();
  }
  if (!SetFilePointerEx(handle, saved_position, 0, FILE_BEGIN)) {
    return fail_last_error();
  }
  return 0;
}

long __crt_sys_fsync(int fd) {
  HANDLE handle = get_fd_handle(fd);

  if (handle == INVALID_HANDLE_VALUE) {
    return -EBADF;
  }
  if (!FlushFileBuffers(handle)) {
    return fail_last_error();
  }
  return 0;
}

static void windows_statfs_fill_generic(struct statfs* buf) {
  memset(buf, 0, sizeof(*buf));
  buf->f_bsize = 4096;
  buf->f_frsize = 4096;
  buf->f_namelen = 255;
}

static const char* windows_root_for_path(const char* path, char root[8]) {
  if (path != 0 && ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
      path[1] == ':' && (path[2] == '\\' || path[2] == '/')) {
    root[0] = path[0];
    root[1] = ':';
    root[2] = '\\';
    root[3] = 0;
    return root;
  }
  return 0;
}

long __crt_sys_statfs(const char* path, struct statfs* buf) {
  char translated_path[4096];
  const char* host_path = translate_path_for_host(path, translated_path);
  DWORD attrs;
  char absolute[4096];
  char root[8];
  const char* query_root;
  unsigned long long available = 0;
  unsigned long long total = 0;
  unsigned long long free_bytes = 0;

  if (path == 0 || buf == 0) {
    return -EINVAL;
  }
  attrs = GetFileAttributesA(host_path);
  if (attrs == INVALID_FILE_ATTRIBUTES) {
    return fail_last_error();
  }
  windows_statfs_fill_generic(buf);
  if (GetFullPathNameA(host_path, (DWORD)sizeof(absolute), absolute, 0) == 0) {
    return 0;
  }
  query_root = windows_root_for_path(absolute, root);
  if (query_root != 0 && GetDiskFreeSpaceExA(query_root, &available, &total, &free_bytes)) {
    buf->f_blocks = total / buf->f_frsize;
    buf->f_bfree = free_bytes / buf->f_frsize;
    buf->f_bavail = available / buf->f_frsize;
  }
  return 0;
}

long __crt_sys_fstatfs(int fd, struct statfs* buf) {
  if (buf == 0) {
    return -EINVAL;
  }
  if (get_fd_handle(fd) == INVALID_HANDLE_VALUE) {
    return -EBADF;
  }
  windows_statfs_fill_generic(buf);
  return 0;
}

static void timeval_pair_to_filetime_pair(
    const struct timeval times[2],
    struct crt_filetime out[2]) {
  if (times == 0) {
    GetSystemTimeAsFileTime(&out[0]);
    out[1] = out[0];
    return;
  }
  time_to_filetime(times[0].tv_sec, times[0].tv_usec, &out[0]);
  time_to_filetime(times[1].tv_sec, times[1].tv_usec, &out[1]);
}

long __crt_sys_utimes(const char* path, const struct timeval times[2]) {
  char translated_path[4096];
  const char* host_path = translate_path_for_host(path, translated_path);
  HANDLE handle;
  struct crt_filetime ft[2];

  if (path == 0) {
    return -EINVAL;
  }
  if (times != 0 &&
      (times[0].tv_usec < 0 || times[0].tv_usec >= 1000000L ||
       times[1].tv_usec < 0 || times[1].tv_usec >= 1000000L)) {
    return -EINVAL;
  }
  handle = CreateFileA(host_path, FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE |
                       FILE_SHARE_DELETE, 0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
  if (handle == INVALID_HANDLE_VALUE) {
    return fail_last_error();
  }
  timeval_pair_to_filetime_pair(times, ft);
  if (!SetFileTime(handle, 0, &ft[0], &ft[1])) {
    long result = fail_last_error();
    CloseHandle(handle);
    return result;
  }
  CloseHandle(handle);
  return 0;
}

long __crt_sys_futimes(int fd, const struct timeval times[2]) {
  HANDLE handle = get_fd_handle(fd);
  struct crt_filetime ft[2];

  if (handle == INVALID_HANDLE_VALUE) {
    return -EBADF;
  }
  if (times != 0 &&
      (times[0].tv_usec < 0 || times[0].tv_usec >= 1000000L ||
       times[1].tv_usec < 0 || times[1].tv_usec >= 1000000L)) {
    return -EINVAL;
  }
  timeval_pair_to_filetime_pair(times, ft);
  return SetFileTime(handle, 0, &ft[0], &ft[1]) ? 0 : fail_last_error();
}

long __crt_sys_access(const char* path, int mode) {
  char translated_path[4096];
  const char* host_path = translate_path_for_host(path, translated_path);
  DWORD attrs;

  if (path_is_dev_null(path)) {
    return 0;
  }
  attrs = GetFileAttributesA(host_path);
  if (attrs == INVALID_FILE_ATTRIBUTES) {
    return fail_last_error();
  }
  if ((mode & W_OK) != 0 &&
      (attrs & FILE_ATTRIBUTE_READONLY) != 0 &&
      (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) {
    return -EACCES;
  }
  if ((mode & X_OK) != 0 && !windows_path_is_executable_file(host_path, attrs)) {
    return -EACCES;
  }
  return 0;
}

long __crt_sys_mkdir(const char* path, unsigned int mode) {
  char translated_path[4096];
  const char* host_path = translate_path_for_host(path, translated_path);
  (void)mode;
  if (!CreateDirectoryA(host_path, 0)) {
    return fail_last_error();
  }
  return 0;
}

long __crt_sys_rmdir(const char* path) {
  char translated_path[4096];
  const char* host_path = translate_path_for_host(path, translated_path);

  if (!RemoveDirectoryA(host_path)) {
    return fail_last_error();
  }
  return 0;
}

long __crt_sys_chdir(const char* path) {
  char translated_path[4096];
  const char* host_path = translate_path_for_host(path, translated_path);

  if (!SetCurrentDirectoryA(host_path)) {
    return fail_last_error();
  }
  return 0;
}

long __crt_sys_fchdir(int fd) {
  (void)fd;
  return -ENOTSUP;
}

long __crt_sys_chmod(const char* path, unsigned int mode) {
  char translated_path[4096];
  const char* host_path = translate_path_for_host(path, translated_path);
  DWORD attrs = GetFileAttributesA(host_path);

  if (attrs == INVALID_FILE_ATTRIBUTES) {
    return fail_last_error();
  }
  if ((mode & (S_IWUSR | S_IWGRP | S_IWOTH)) != 0) {
    attrs &= ~FILE_ATTRIBUTE_READONLY;
  } else {
    attrs |= FILE_ATTRIBUTE_READONLY;
  }
  if (!SetFileAttributesA(host_path, attrs)) {
    return fail_last_error();
  }
  return 0;
}

long __crt_sys_fchmod(int fd, unsigned int mode) {
  HANDLE handle = get_fd_handle(fd);
  (void)mode;

  if (handle == INVALID_HANDLE_VALUE) {
    return -EBADF;
  }
  return 0;
}

long __crt_sys_getcwd(char* buf, unsigned long size) {
  char host_cwd[4096];
  const char* root = windows_rootfs();
  DWORD result = GetCurrentDirectoryA((DWORD)sizeof(host_cwd), host_cwd);
  size_t root_len;
  size_t out = 0;
  size_t in;

  if (result == 0) {
    return fail_last_error();
  }
  if (result >= (DWORD)sizeof(host_cwd)) {
    return -ERANGE;
  }
  if (root != 0 && windows_path_prefix_equal(host_cwd, root)) {
    root_len = strlen(root);
    while (root_len > 0 && (root[root_len - 1] == '/' || root[root_len - 1] == '\\')) {
      --root_len;
    }
    in = root_len;
    while (host_cwd[in] == '/' || host_cwd[in] == '\\') {
      ++in;
    }
    if (size < 2) {
      return -ERANGE;
    }
    buf[out++] = '/';
    while (host_cwd[in] != 0) {
      if (out + 1 >= size) {
        return -ERANGE;
      }
      buf[out++] = host_cwd[in] == '\\' ? '/' : host_cwd[in];
      ++in;
    }
    if (out > 1 && buf[out - 1] == '/') {
      --out;
    }
    buf[out] = 0;
    return (long)out;
  }
  if (result >= (DWORD)size) {
    return -ERANGE;
  }
  memcpy(buf, host_cwd, (size_t)result + 1);
  return (long)result;
}

long __crt_sys_dup(int oldfd) {
  HANDLE handle = get_fd_handle(oldfd);
  HANDLE duplicate = 0;
  int fd;

  if (handle == INVALID_HANDLE_VALUE) {
    return -EBADF;
  }
  if (!DuplicateHandle(GetCurrentProcess(), handle, GetCurrentProcess(), &duplicate, 0, 0, DUPLICATE_SAME_ACCESS)) {
    return fail_last_error();
  }
  fd = alloc_fd(duplicate);
  if (fd < 0) {
    CloseHandle(duplicate);
    return -EMFILE;
  }
  if (oldfd >= 0 && oldfd < CRT_FD_TABLE_SIZE) {
    fd_flags[fd] = fd_flags[oldfd] & ~FD_CLOEXEC;
  }
  return fd;
}

long __crt_sys_dup2(int oldfd, int newfd) {
  HANDLE handle = get_fd_handle(oldfd);
  HANDLE duplicate = 0;

  init_fd_table();
  if (handle == INVALID_HANDLE_VALUE) {
    return -EBADF;
  }
  if (newfd < 0 || newfd >= CRT_FD_TABLE_SIZE) {
    return -EBADF;
  }
  if (oldfd == newfd) {
    return newfd;
  }
  if (!DuplicateHandle(GetCurrentProcess(), handle, GetCurrentProcess(), &duplicate, 0, 0, DUPLICATE_SAME_ACCESS)) {
    return fail_last_error();
  }
  if (fd_table[newfd] != 0 && fd_table[newfd] != INVALID_HANDLE_VALUE) {
    if (fd_kind[newfd] == CRT_FD_KIND_SOCKET) {
      winsock.closesocket((SOCKET)(uintptr_t)fd_table[newfd]);
    } else {
      CloseHandle(fd_table[newfd]);
    }
  }
  fd_table[newfd] = duplicate;
  fd_kind[newfd] = CRT_FD_KIND_FILE;
  fd_flags[newfd] = oldfd >= 0 && oldfd < CRT_FD_TABLE_SIZE ? fd_flags[oldfd] & ~FD_CLOEXEC : 0;
  return newfd;
}

long __crt_sys_pipe(int pipefd[2]) {
  HANDLE read_handle = 0;
  HANDLE write_handle = 0;
  struct {
    DWORD nLength;
    void* lpSecurityDescriptor;
    BOOL bInheritHandle;
  } security_attributes;
  int read_fd;
  int write_fd;

  security_attributes.nLength = sizeof(security_attributes);
  security_attributes.lpSecurityDescriptor = 0;
  security_attributes.bInheritHandle = 1;
  if (!CreatePipe(&read_handle, &write_handle, &security_attributes, 0)) {
    return fail_last_error();
  }
  read_fd = alloc_fd(read_handle);
  if (read_fd < 0) {
    CloseHandle(read_handle);
    CloseHandle(write_handle);
    return -EMFILE;
  }
  write_fd = alloc_fd(write_handle);
  if (write_fd < 0) {
    fd_table[read_fd] = 0;
    fd_kind[read_fd] = CRT_FD_KIND_NONE;
    fd_flags[read_fd] = 0;
    CloseHandle(read_handle);
    CloseHandle(write_handle);
    return -EMFILE;
  }
  pipefd[0] = read_fd;
  pipefd[1] = write_fd;
  return 0;
}

static short poll_handle(HANDLE handle, short events) {
  DWORD file_type;
  DWORD bytes_available = 0;
  short revents = 0;

  if ((events & POLLOUT) != 0) {
    revents |= POLLOUT;
  }
  if ((events & (POLLIN | POLLPRI)) == 0) {
    return revents;
  }

  file_type = GetFileType(handle);
  if (file_type == FILE_TYPE_DISK || file_type == FILE_TYPE_CHAR) {
    revents |= (short)(events & (POLLIN | POLLPRI));
    return revents;
  }
  if (file_type == FILE_TYPE_PIPE) {
    if (PeekNamedPipe(handle, 0, 0, 0, &bytes_available, 0)) {
      if (bytes_available != 0) {
        revents |= (short)(events & POLLIN);
      }
      return revents;
    }
    return POLLERR;
  }
  return (short)(revents | POLLERR);
}

static short poll_socket(SOCKET socket_handle, short events) {
  unsigned long bytes_available = 0;
  short revents = 0;

  if ((events & POLLOUT) != 0) {
    revents |= POLLOUT;
  }
  if ((events & POLLIN) != 0) {
    if (winsock.ioctlsocket(socket_handle, CRT_WS_FIONREAD, &bytes_available) == SOCKET_ERROR) {
      return POLLERR;
    }
    if (bytes_available != 0) {
      revents |= POLLIN;
    }
  }
  return revents;
}

long __crt_sys_poll(struct pollfd* fds, unsigned long nfds, int timeout) {
  unsigned long i;
  long ready;
  DWORD slept = 0;

  if (fds == 0 && nfds != 0) {
    return -EFAULT;
  }
  do {
    ready = 0;
    for (i = 0; i < nfds; ++i) {
      HANDLE handle;

      fds[i].revents = 0;
      if (fds[i].fd < 0) {
        continue;
      }
      if (fds[i].fd < CRT_FD_TABLE_SIZE && fd_kind[fds[i].fd] == CRT_FD_KIND_SOCKET) {
        fds[i].revents = poll_socket((SOCKET)(uintptr_t)fd_table[fds[i].fd], fds[i].events);
      } else {
        handle = get_fd_handle(fds[i].fd);
        if (handle == INVALID_HANDLE_VALUE) {
          fds[i].revents = POLLNVAL;
        } else {
          fds[i].revents = poll_handle(handle, fds[i].events);
        }
      }
      if (fds[i].revents != 0) {
        ++ready;
      }
    }
    if (ready != 0 || timeout == 0) {
      return ready;
    }
    if (timeout < 0) {
      Sleep(1);
      continue;
    }
    if (slept >= (DWORD)timeout) {
      return 0;
    }
    Sleep(1);
    ++slept;
  } while (1);
}

static int translate_socket_level(int level) {
  return level == CRT_PUBLIC_SOL_SOCKET ? CRT_WS_SOL_SOCKET : level;
}

static int translate_socket_option(int level, int optname) {
  if (level == CRT_PUBLIC_SOL_SOCKET && optname == CRT_PUBLIC_SO_REUSEADDR) {
    return CRT_WS_SO_REUSEADDR;
  }
  return optname;
}

long __crt_sys_socket(int domain, int type, int protocol) {
  SOCKET socket_handle;
  int fd;
  long init_result = init_winsock();

  if (init_result != 0) {
    return init_result;
  }
  socket_handle = winsock.socket(domain, type, protocol);
  if (socket_handle == INVALID_SOCKET) {
    return -map_wsa_error(winsock.WSAGetLastError());
  }
  fd = alloc_socket_fd(socket_handle);
  if (fd < 0) {
    winsock.closesocket(socket_handle);
    return -EMFILE;
  }
  return fd;
}

long __crt_sys_bind(int sockfd, const void* addr, unsigned int addrlen) {
  SOCKET socket_handle = get_fd_socket(sockfd);

  if (socket_handle == INVALID_SOCKET) {
    return -EBADF;
  }
  return winsock.bind(socket_handle, (const struct sockaddr*)addr, (int)addrlen) == SOCKET_ERROR
             ? -map_wsa_error(winsock.WSAGetLastError())
             : 0;
}

long __crt_sys_listen(int sockfd, int backlog) {
  SOCKET socket_handle = get_fd_socket(sockfd);

  if (socket_handle == INVALID_SOCKET) {
    return -EBADF;
  }
  return winsock.listen(socket_handle, backlog) == SOCKET_ERROR
             ? -map_wsa_error(winsock.WSAGetLastError())
             : 0;
}

long __crt_sys_accept(int sockfd, void* addr, unsigned int* addrlen) {
  SOCKET socket_handle = get_fd_socket(sockfd);
  SOCKET accepted;
  int len = addrlen != 0 ? (int)*addrlen : 0;
  int fd;

  if (socket_handle == INVALID_SOCKET) {
    return -EBADF;
  }
  accepted = winsock.accept(socket_handle, (struct sockaddr*)addr, addrlen != 0 ? &len : 0);
  if (accepted == INVALID_SOCKET) {
    return -map_wsa_error(winsock.WSAGetLastError());
  }
  fd = alloc_socket_fd(accepted);
  if (fd < 0) {
    winsock.closesocket(accepted);
    return -EMFILE;
  }
  if (addrlen != 0) {
    *addrlen = (unsigned int)len;
  }
  return fd;
}

long __crt_sys_connect(int sockfd, const void* addr, unsigned int addrlen) {
  SOCKET socket_handle = get_fd_socket(sockfd);

  if (socket_handle == INVALID_SOCKET) {
    return -EBADF;
  }
  return winsock.connect(socket_handle, (const struct sockaddr*)addr, (int)addrlen) == SOCKET_ERROR
             ? -map_wsa_error(winsock.WSAGetLastError())
             : 0;
}

long __crt_sys_sendto(
    int sockfd,
    const void* buf,
    unsigned long len,
    int flags,
    const void* dest_addr,
    unsigned int addrlen) {
  SOCKET socket_handle = get_fd_socket(sockfd);
  int result;

  if (socket_handle == INVALID_SOCKET) {
    return -EBADF;
  }
  result = winsock.sendto(
      socket_handle,
      (const char*)buf,
      (int)len,
      flags,
      (const struct sockaddr*)dest_addr,
      (int)addrlen);
  return result == SOCKET_ERROR ? -map_wsa_error(winsock.WSAGetLastError()) : result;
}

long __crt_sys_recvfrom(
    int sockfd,
    void* buf,
    unsigned long len,
    int flags,
    void* src_addr,
    unsigned int* addrlen) {
  SOCKET socket_handle = get_fd_socket(sockfd);
  int inout_len = addrlen != 0 ? (int)*addrlen : 0;
  int result;

  if (socket_handle == INVALID_SOCKET) {
    return -EBADF;
  }
  result = winsock.recvfrom(
      socket_handle,
      (char*)buf,
      (int)len,
      flags,
      (struct sockaddr*)src_addr,
      addrlen != 0 ? &inout_len : 0);
  if (result == SOCKET_ERROR) {
    return -map_wsa_error(winsock.WSAGetLastError());
  }
  if (addrlen != 0) {
    *addrlen = (unsigned int)inout_len;
  }
  return result;
}

long __crt_sys_getsockname(int sockfd, void* addr, unsigned int* addrlen) {
  SOCKET socket_handle = get_fd_socket(sockfd);
  int len = addrlen != 0 ? (int)*addrlen : 0;

  if (socket_handle == INVALID_SOCKET) {
    return -EBADF;
  }
  if (winsock.getsockname(socket_handle, (struct sockaddr*)addr, &len) == SOCKET_ERROR) {
    return -map_wsa_error(winsock.WSAGetLastError());
  }
  if (addrlen != 0) {
    *addrlen = (unsigned int)len;
  }
  return 0;
}

long __crt_sys_setsockopt(int sockfd, int level, int optname, const void* optval, unsigned int optlen) {
  SOCKET socket_handle = get_fd_socket(sockfd);

  if (socket_handle == INVALID_SOCKET) {
    return -EBADF;
  }
  return winsock.setsockopt(
             socket_handle,
             translate_socket_level(level),
             translate_socket_option(level, optname),
             (const char*)optval,
             (int)optlen) == SOCKET_ERROR
             ? -map_wsa_error(winsock.WSAGetLastError())
             : 0;
}

long __crt_sys_shutdown(int sockfd, int how) {
  SOCKET socket_handle = get_fd_socket(sockfd);
  int mapped_how =
      how == CRT_PUBLIC_SHUT_RD ? SD_RECEIVE : (how == CRT_PUBLIC_SHUT_WR ? SD_SEND : SD_BOTH);

  if (socket_handle == INVALID_SOCKET) {
    return -EBADF;
  }
  return winsock.shutdown(socket_handle, mapped_how) == SOCKET_ERROR
             ? -map_wsa_error(winsock.WSAGetLastError())
             : 0;
}

long __crt_sys_realpath_path(const char* path, char* resolved_path, unsigned long size) {
  char translated_path[4096];
  const char* host_path = translate_path_for_host(path, translated_path);
  DWORD result = GetFullPathNameA(host_path, (DWORD)size, resolved_path, 0);

  if (result == 0) {
    return fail_last_error();
  }
  if (result >= (DWORD)size) {
    return -ERANGE;
  }
  return 0;
}

static long normalize_final_handle_path(char* path, unsigned long size) {
  static const char dos_prefix[] = "\\\\?\\";
  static const char unc_prefix[] = "\\\\?\\UNC\\";
  size_t prefix_len;
  size_t path_len;

  if (strncmp(path, unc_prefix, sizeof(unc_prefix) - 1) == 0) {
    prefix_len = sizeof(unc_prefix) - 1;
    path_len = strlen(path + prefix_len);
    if (path_len + 3 > size) {
      return -ERANGE;
    }
    memmove(path + 2, path + prefix_len, path_len + 1);
    path[0] = '\\';
    path[1] = '\\';
    return 0;
  }
  if (strncmp(path, dos_prefix, sizeof(dos_prefix) - 1) == 0) {
    prefix_len = sizeof(dos_prefix) - 1;
    memmove(path, path + prefix_len, strlen(path + prefix_len) + 1);
  }
  return 0;
}

long __crt_sys_realpath_fd(int fd, char* resolved_path, unsigned long size) {
  HANDLE handle = get_fd_handle(fd);
  DWORD result;

  if (resolved_path == 0 || size == 0) {
    return -EINVAL;
  }
  if (handle == INVALID_HANDLE_VALUE) {
    return -EBADF;
  }
  result = GetFinalPathNameByHandleA(handle, resolved_path, (DWORD)size, 0);
  if (result == 0) {
    return fail_last_error();
  }
  if (result >= (DWORD)size) {
    return -ERANGE;
  }
  return normalize_final_handle_path(resolved_path, size);
}

long __crt_sys_readlink(const char* path, char* buf, unsigned long size) {
  (void)path;
  (void)buf;
  (void)size;
  return -ENOSYS;
}

long __crt_sys_symlink(const char* target, const char* linkpath) {
  (void)target;
  (void)linkpath;
  return -ENOSYS;
}

static long stat_from_handle(HANDLE handle, struct stat* st) {
  struct crt_by_handle_file_information info;
  uint64_t size;

  if (!GetFileInformationByHandle(handle, &info)) {
    return fail_last_error();
  }
  memset(st, 0, sizeof(*st));
  st->st_dev = info.volume_serial_number;
  st->st_ino = ((uint64_t)info.file_index_high << 32) | info.file_index_low;
  st->st_nlink = info.number_of_links;
  st->st_uid = (unsigned int)__crt_sys_geteuid();
  st->st_gid = 0;
  if ((info.file_attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
    st->st_mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO;
  } else {
    st->st_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
    if ((info.file_attributes & FILE_ATTRIBUTE_READONLY) == 0) {
      st->st_mode |= S_IWUSR | S_IWGRP | S_IWOTH;
    }
    if (windows_handle_has_mz_signature(handle)) {
      st->st_mode |= S_IXUSR | S_IXGRP | S_IXOTH;
    }
  }
  size = ((uint64_t)info.file_size_high << 32) | info.file_size_low;
  st->st_size = (off_t)size;
  st->st_blksize = 4096;
  st->st_blocks = (blkcnt_t)((size + 511) / 512);
  st->st_atime = filetime_to_time(&info.last_access_time);
  st->st_mtime = filetime_to_time(&info.last_write_time);
  st->st_ctime = filetime_to_time(&info.creation_time);
  st->st_atim.tv_sec = st->st_atime;
  st->st_atim.tv_nsec = 0;
  st->st_mtim.tv_sec = st->st_mtime;
  st->st_mtim.tv_nsec = 0;
  st->st_ctim.tv_sec = st->st_ctime;
  st->st_ctim.tv_nsec = 0;
  return 0;
}

static long stat_virtual_dev_null(struct stat* st) {
  if (st == 0) {
    return -EFAULT;
  }
  memset(st, 0, sizeof(*st));
  st->st_mode = S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
  st->st_nlink = 1;
  st->st_blksize = 4096;
  return 0;
}

static long stat_virtual_dev_tty(struct stat* st) {
  if (st == 0) {
    return -EFAULT;
  }
  memset(st, 0, sizeof(*st));
  st->st_mode = S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
  st->st_nlink = 1;
  st->st_rdev = 1;
  st->st_blksize = 4096;
  return 0;
}

long __crt_sys_fstat(int fd, struct stat* st) {
  HANDLE handle = get_fd_handle(fd);

  if (handle == INVALID_HANDLE_VALUE) {
    return -EBADF;
  }
  if (GetFileType(handle) == FILE_TYPE_CHAR) {
    return stat_virtual_dev_tty(st);
  }
  return stat_from_handle(handle, st);
}

long __crt_sys_isatty(int fd) {
  HANDLE handle = get_fd_handle(fd);
  DWORD file_type;

  if (handle == INVALID_HANDLE_VALUE) {
    return -EBADF;
  }
  file_type = GetFileType(handle);
  return file_type == FILE_TYPE_CHAR ? 1 : 0;
}

static long windows_ioctl_fionread_handle(HANDLE handle, int* value) {
  DWORD file_type;
  DWORD available = 0;

  if (value == 0) {
    return -EFAULT;
  }
  file_type = GetFileType(handle);
  if (file_type == FILE_TYPE_PIPE) {
    if (!PeekNamedPipe(handle, 0, 0, 0, &available, 0)) {
      return fail_last_error();
    }
    *value = (int)available;
    return 0;
  }
  if (file_type == FILE_TYPE_CHAR) {
    if (GetNumberOfConsoleInputEvents(handle, &available)) {
      *value = (int)available;
      return 0;
    }
    return -ENOTTY;
  }
  return -ENOTTY;
}

static long windows_ioctl_get_winsize(HANDLE handle, struct winsize* value) {
  struct crt_console_screen_buffer_info info;

  if (value == 0) {
    return -EFAULT;
  }
  if (GetFileType(handle) != FILE_TYPE_CHAR) {
    return -ENOTTY;
  }
  if (!GetConsoleScreenBufferInfo(handle, &info)) {
    return -ENOTTY;
  }
  value->ws_col = (unsigned short)(info.srWindow.Right - info.srWindow.Left + 1);
  value->ws_row = (unsigned short)(info.srWindow.Bottom - info.srWindow.Top + 1);
  value->ws_xpixel = 0;
  value->ws_ypixel = 0;
  return 0;
}

long __crt_sys_ioctl(int fd, unsigned long request, void* arg) {
  HANDLE handle;

  init_fd_table();
  if (fd < 0 || fd >= CRT_FD_TABLE_SIZE || fd_kind[fd] == CRT_FD_KIND_NONE ||
      fd_table[fd] == 0 ||
      fd_table[fd] == INVALID_HANDLE_VALUE) {
    return -EBADF;
  }
  if (fd_kind[fd] == CRT_FD_KIND_SOCKET) {
    unsigned long available = 0;

    if (request != FIONREAD) {
      return -ENOTTY;
    }
    if (arg == 0) {
      return -EFAULT;
    }
    if (winsock.ioctlsocket((SOCKET)(uintptr_t)fd_table[fd], CRT_WS_FIONREAD, &available) ==
        SOCKET_ERROR) {
      return -map_wsa_error(winsock.WSAGetLastError());
    }
    *(int*)arg = (int)available;
    return 0;
  }

  handle = fd_table[fd];
  switch (request) {
    case FIONREAD:
      return windows_ioctl_fionread_handle(handle, (int*)arg);
    case TIOCGWINSZ:
      return windows_ioctl_get_winsize(handle, (struct winsize*)arg);
    case TIOCSWINSZ:
      return GetFileType(handle) == FILE_TYPE_CHAR ? -ENOTSUP : -ENOTTY;
    default:
      return -ENOTTY;
  }
}

long __crt_sys_tcgetattr(int fd, struct termios* termios_p) {
  HANDLE handle = get_fd_handle(fd);
  DWORD mode = 0;

  if (termios_p == 0) {
    return -EFAULT;
  }
  if (handle == INVALID_HANDLE_VALUE) {
    return -EBADF;
  }
  if (GetFileType(handle) != FILE_TYPE_CHAR || !GetConsoleMode(handle, &mode)) {
    return -ENOTTY;
  }
  memset(termios_p, 0, sizeof(*termios_p));
  termios_p->c_iflag = ICRNL | IXON;
  termios_p->c_oflag = OPOST | ONLCR;
  termios_p->c_cflag = CREAD | CS8;
  if ((mode & CRT_ENABLE_PROCESSED_INPUT) != 0) {
    termios_p->c_lflag |= ISIG | IEXTEN;
  }
  if ((mode & CRT_ENABLE_LINE_INPUT) != 0) {
    termios_p->c_lflag |= ICANON;
  }
  if ((mode & CRT_ENABLE_ECHO_INPUT) != 0) {
    termios_p->c_lflag |= ECHO | ECHOE | ECHOK;
  }
  termios_p->c_ispeed = B38400;
  termios_p->c_ospeed = B38400;
  return 0;
}

long __crt_sys_tcsetattr(int fd, const struct termios* termios_p) {
  HANDLE handle = get_fd_handle(fd);
  DWORD mode = 0;

  if (termios_p == 0) {
    return -EFAULT;
  }
  if (handle == INVALID_HANDLE_VALUE) {
    return -EBADF;
  }
  if (GetFileType(handle) != FILE_TYPE_CHAR || !GetConsoleMode(handle, &mode)) {
    return -ENOTTY;
  }
  if ((termios_p->c_lflag & ISIG) != 0) {
    mode |= CRT_ENABLE_PROCESSED_INPUT;
  } else {
    mode &= ~CRT_ENABLE_PROCESSED_INPUT;
  }
  if ((termios_p->c_lflag & ICANON) != 0) {
    mode |= CRT_ENABLE_LINE_INPUT;
  } else {
    mode &= ~CRT_ENABLE_LINE_INPUT;
  }
  if ((termios_p->c_lflag & ECHO) != 0) {
    mode |= CRT_ENABLE_ECHO_INPUT;
  } else {
    mode &= ~CRT_ENABLE_ECHO_INPUT;
  }
  return SetConsoleMode(handle, mode) ? 0 : fail_last_error();
}

long __crt_sys_stat_path(const char* path, struct stat* st) {
  char translated_path[4096];
  const char* host_path = translate_path_for_host(path, translated_path);
  HANDLE handle;
  DWORD attrs;
  long result;

  if (path_is_dev_null(path)) {
    return stat_virtual_dev_null(st);
  }
  if (path_is_dev_tty(path)) {
    return stat_virtual_dev_tty(st);
  }
  handle = CreateFileA(host_path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS, 0);
  if (handle == INVALID_HANDLE_VALUE) {
    return fail_last_error();
  }
  result = stat_from_handle(handle, st);
  CloseHandle(handle);
  attrs = GetFileAttributesA(host_path);
  if (result == 0 && S_ISREG(st->st_mode) && attrs != INVALID_FILE_ATTRIBUTES &&
      windows_path_is_executable_file(host_path, attrs)) {
    st->st_mode |= S_IXUSR | S_IXGRP | S_IXOTH;
  }
  return result;
}

long __crt_sys_lstat_path(const char* path, struct stat* st) {
  char translated_path[4096];
  const char* host_path = translate_path_for_host(path, translated_path);
  DWORD attrs;
  long result = __crt_sys_stat_path(path, st);

  if (result != 0) {
    return result;
  }
  attrs = GetFileAttributesA(host_path);
  if (attrs == INVALID_FILE_ATTRIBUTES) {
    return fail_last_error();
  }
  if ((attrs & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
    st->st_mode = (st->st_mode & ~S_IFMT) | S_IFLNK;
  }
  return 0;
}

long __crt_sys_unlink(const char* path) {
  char translated_path[4096];
  const char* host_path = translate_path_for_host(path, translated_path);

  if (!DeleteFileA(host_path)) {
    return fail_last_error();
  }
  return 0;
}

long __crt_sys_rename(const char* old_path, const char* new_path) {
  char translated_old_path[4096];
  char translated_new_path[4096];
  const char* host_old_path = translate_path_for_host(old_path, translated_old_path);
  const char* host_new_path = translate_path_for_host(new_path, translated_new_path);

  if (!MoveFileExA(host_old_path, host_new_path, MOVEFILE_REPLACE_EXISTING)) {
    return fail_last_error();
  }
  return 0;
}

static DWORD windows_page_protect(int prot) {
  if ((prot & PROT_WRITE) != 0 && (prot & PROT_EXEC) != 0) {
    return PAGE_EXECUTE_READWRITE;
  } else if ((prot & PROT_WRITE) != 0) {
    return PAGE_READWRITE;
  } else if ((prot & PROT_EXEC) != 0 && (prot & PROT_READ) != 0) {
    return PAGE_EXECUTE_READ;
  } else if ((prot & PROT_EXEC) != 0) {
    return PAGE_EXECUTE;
  } else if ((prot & PROT_READ) != 0) {
    return PAGE_READONLY;
  }
  return PAGE_NOACCESS;
}

static DWORD windows_mapping_protect(int prot, int flags) {
  if ((flags & MAP_PRIVATE) != 0 && (prot & PROT_WRITE) != 0) {
    return (prot & PROT_EXEC) != 0 ? PAGE_EXECUTE_WRITECOPY : PAGE_WRITECOPY;
  }
  return windows_page_protect(prot);
}

static DWORD windows_mapping_access(int prot, int flags) {
  DWORD access = 0;

  if ((flags & MAP_PRIVATE) != 0 && (prot & PROT_WRITE) != 0) {
    access |= FILE_MAP_COPY;
  } else {
    if ((prot & PROT_READ) != 0) {
      access |= FILE_MAP_READ;
    }
    if ((prot & PROT_WRITE) != 0) {
      access |= FILE_MAP_WRITE;
    }
  }
  if ((prot & PROT_EXEC) != 0) {
    access |= FILE_MAP_EXECUTE;
  }
  return access == 0 ? FILE_MAP_READ : access;
}

void* __crt_sys_mmap(void* addr, unsigned long length, int prot, int flags, int fd, long long offset) {
  DWORD protect;
  void* result;

  if ((flags & MAP_TYPE) != MAP_PRIVATE && (flags & MAP_TYPE) != MAP_SHARED) {
    return (void*)(intptr_t)-EINVAL;
  }

  if ((flags & MAP_ANONYMOUS) != 0) {
    if (fd != -1) {
      return (void*)(intptr_t)-EINVAL;
    }
    protect = windows_page_protect(prot);
    result = VirtualAlloc(addr, (size_t)length, MEM_RESERVE | MEM_COMMIT, protect);
    if (result == 0) {
      return (void*)(intptr_t)-map_windows_error(GetLastError());
    }
    return result;
  }

  {
    HANDLE file = get_fd_handle(fd);
    HANDLE mapping;
    DWORD access;
    unsigned long long end = (unsigned long long)offset + (unsigned long long)length;

    if (file == INVALID_HANDLE_VALUE) {
      return (void*)(intptr_t)-EBADF;
    }
    if (offset < 0) {
      return (void*)(intptr_t)-EINVAL;
    }
    protect = windows_mapping_protect(prot, flags);
    mapping = CreateFileMappingA(file, 0, protect, (DWORD)(end >> 32), (DWORD)end, 0);
    if (mapping == 0) {
      return (void*)(intptr_t)-map_windows_error(GetLastError());
    }
    access = windows_mapping_access(prot, flags);
    result = MapViewOfFileEx(mapping, access, (DWORD)((unsigned long long)offset >> 32),
                             (DWORD)(unsigned long long)offset, (size_t)length,
                             (flags & MAP_FIXED) != 0 ? addr : 0);
    CloseHandle(mapping);
    if (result == 0) {
      return (void*)(intptr_t)-map_windows_error(GetLastError());
    }
    return result;
  }
}

long __crt_sys_mprotect(void* addr, unsigned long length, int prot) {
  DWORD old_protect;

  if (!VirtualProtect(addr, (size_t)length, windows_page_protect(prot), &old_protect)) {
    return fail_last_error();
  }
  return 0;
}

long __crt_sys_munmap(void* addr, unsigned long length) {
  (void)length;
  if (!VirtualFree(addr, 0, MEM_RELEASE)) {
    if (!UnmapViewOfFile(addr)) {
      return fail_last_error();
    }
  }
  return 0;
}

long __crt_sys_msync(void* addr, unsigned long length, int flags) {
  (void)flags;
  if (!FlushViewOfFile(addr, (size_t)length)) {
    return fail_last_error();
  }
  return 0;
}

void* __crt_sys_mremap(void* old_addr, unsigned long old_size, unsigned long new_size, int flags, void* new_addr) {
  (void)old_addr;
  (void)old_size;
  (void)new_size;
  (void)flags;
  (void)new_addr;
  return (void*)(intptr_t)-ENOSYS;
}

long __crt_sys_mlockall(int flags) {
  (void)flags;
  return -ENOSYS;
}

long __crt_sys_munlockall(void) {
  return -ENOSYS;
}

long __crt_sys_mlock(const void* addr, unsigned long length) {
  if (!VirtualLock((void*)addr, (size_t)length)) {
    return fail_last_error();
  }
  return 0;
}

long __crt_sys_mlock2(const void* addr, unsigned long length, int flags) {
  if (flags != 0) {
    return -EINVAL;
  }
  return __crt_sys_mlock(addr, length);
}

long __crt_sys_munlock(const void* addr, unsigned long length) {
  if (!VirtualUnlock((void*)addr, (size_t)length)) {
    return fail_last_error();
  }
  return 0;
}

long __crt_sys_mincore(void* addr, unsigned long length, unsigned char* vector) {
  (void)addr;
  (void)length;
  (void)vector;
  return -ENOSYS;
}

long __crt_sys_madvise(void* addr, unsigned long length, int advice) {
  (void)addr;
  (void)length;
  if (advice == MADV_NORMAL || advice == MADV_RANDOM || advice == MADV_SEQUENTIAL ||
      advice == MADV_WILLNEED || advice == MADV_DONTNEED || advice == POSIX_MADV_NORMAL ||
      advice == POSIX_MADV_RANDOM || advice == POSIX_MADV_SEQUENTIAL ||
      advice == POSIX_MADV_WILLNEED || advice == POSIX_MADV_DONTNEED) {
    return 0;
  }
  return -EINVAL;
}

long __crt_sys_gettimeofday(struct timeval* tv) {
  struct crt_filetime ft;
  unsigned long long ticks;

  GetSystemTimeAsFileTime(&ft);
  ticks = ((unsigned long long)ft.high << 32) | ft.low;
  ticks -= SEC_TO_UNIX_EPOCH * WINDOWS_TICK;
  tv->tv_sec = (time_t)(ticks / WINDOWS_TICK);
  tv->tv_usec = (long)((ticks % WINDOWS_TICK) / 10ULL);
  return 0;
}

long __crt_sys_clock_gettime(clockid_t clock_id, struct timespec* tp) {
  if (clock_id == CLOCK_REALTIME) {
    struct timeval tv;
    long result = __crt_sys_gettimeofday(&tv);
    if (result != 0) {
      return result;
    }
    tp->tv_sec = tv.tv_sec;
    tp->tv_nsec = tv.tv_usec * 1000L;
    return 0;
  }

  if (clock_id == CLOCK_MONOTONIC) {
    long long count;
    long long frequency;
    long long seconds;
    long long remainder;

    if (!QueryPerformanceFrequency(&frequency) || frequency <= 0 ||
        !QueryPerformanceCounter(&count)) {
      return fail_last_error();
    }
    seconds = count / frequency;
    remainder = count % frequency;
    tp->tv_sec = (time_t)seconds;
    tp->tv_nsec = (long)((remainder * 1000000000LL) / frequency);
    return 0;
  }

  return -EINVAL;
}

long __crt_sys_nanosleep(const struct timespec* req, struct timespec* rem) {
  unsigned long long ms;
  (void)rem;

  if (req->tv_sec < 0 || req->tv_nsec < 0 || req->tv_nsec >= 1000000000L) {
    return -EINVAL;
  }
  ms = (unsigned long long)req->tv_sec * 1000ULL + ((unsigned long long)req->tv_nsec + 999999ULL) / 1000000ULL;
  if (ms > 0xffffffffULL) {
    ms = 0xffffffffULL;
  }
  Sleep((DWORD)ms);
  return 0;
}

long __crt_sys_sched_yield(void) {
  Sleep(0);
  return 0;
}

long __crt_sys_thread_id(void) {
  return (long)GetCurrentThreadId();
}

long __crt_sys_fork(void) {
  struct crt_rtl_user_process_information info;
  unsigned char inherit_touched[CRT_FD_TABLE_SIZE];
  DWORD old_inherit_flags[CRT_FD_TABLE_SIZE];
  ULONG status;
  long result;
  DWORD child_pid;

  result = init_ntdll();
  if (result < 0) {
    return result;
  }
  memset(&info, 0, sizeof(info));
  info.Length = sizeof(info);
  memset(old_inherit_flags, 0, sizeof(old_inherit_flags));
  fd_set_inherit_for_fork(inherit_touched, old_inherit_flags);
  status = ntdll.RtlCloneUserProcess(
      CRT_RTL_CLONE_PROCESS_FLAGS_INHERIT_HANDLES,
      0,
      0,
      0,
      &info);
  fd_restore_inherit_after_fork(inherit_touched, old_inherit_flags);
  if (status == CRT_STATUS_PROCESS_CLONED) {
    windows_unregistered_clone = 1;
    return 0;
  }
  if (status != CRT_STATUS_SUCCESS) {
    return -ENOTSUP;
  }
  child_pid = (DWORD)(uintptr_t)info.ClientId.UniqueProcess;
  if (child_pid == 0) {
    child_pid = GetCurrentProcessId();
  }
  if (info.Thread != 0 && info.Thread != INVALID_HANDLE_VALUE) {
    CloseHandle(info.Thread);
  }
  if (info.Process == 0 || info.Process == INVALID_HANDLE_VALUE) {
    return -ECHILD;
  }
  return remember_child_process(child_pid, info.Process);
}

int __crt_windows_is_unregistered_clone(void) {
  return windows_unregistered_clone;
}

long __crt_sys_getpid(void) {
  return (long)GetCurrentProcessId();
}

long __crt_sys_getppid(void) {
  return 0;
}

long __crt_sysconf_page_size(void) {
  struct crt_system_info info;

  memset(&info, 0, sizeof(info));
  GetSystemInfo(&info);
  return info.dwPageSize != 0 ? (long)info.dwPageSize : 4096;
}

long __crt_sysconf_nprocessors_conf(void) {
  struct crt_system_info info;

  memset(&info, 0, sizeof(info));
  GetSystemInfo(&info);
  return info.dwNumberOfProcessors != 0 ? (long)info.dwNumberOfProcessors : 1;
}

long __crt_sysconf_nprocessors_onln(void) {
  return __crt_sysconf_nprocessors_conf();
}

static long windows_phys_pages(int available) {
  struct crt_memory_status_ex status;
  unsigned long long bytes;
  long page_size = __crt_sysconf_page_size();

  if (page_size <= 0) {
    return -1;
  }
  memset(&status, 0, sizeof(status));
  status.dwLength = sizeof(status);
  if (!GlobalMemoryStatusEx(&status)) {
    return -1;
  }
  bytes = available ? status.ullAvailPhys : status.ullTotalPhys;
  return (long)(bytes / (unsigned long long)page_size);
}

long __crt_sysconf_phys_pages(void) {
  return windows_phys_pages(0);
}

long __crt_sysconf_avphys_pages(void) {
  return windows_phys_pages(1);
}

long __crt_sys_geteuid(void) {
  return 1;
}

long __crt_sys_fchown(int fd, unsigned int owner, unsigned int group) {
  HANDLE handle = get_fd_handle(fd);

  (void)owner;
  (void)group;
  if (handle == INVALID_HANDLE_VALUE) {
    return -EBADF;
  }
  return 0;
}

long __crt_sys_kill(long pid, int sig) {
  if (sig == 0 && pid == (long)GetCurrentProcessId()) {
    return 0;
  }
  return -ENOSYS;
}

struct crt_windows_spawn_context {
  char application_path[4096];
  char command_line[8192];
  struct crt_startupinfo startup;
  struct crt_process_information process;
  char current_directory_buffer[4096];
  char fd_snapshot_text[65536];
  char bootstrap_cwd[4096];
};

long __crt_sys_posix_spawn(
    const char* path,
    char* const argv[],
    char* const envp[],
    long* pid,
    int search_path,
    const posix_spawn_file_actions_t actions,
    const posix_spawnattr_t attr) {
  struct crt_windows_spawn_context* ctx;
  const char* current_directory;
  DWORD creation_flags;
  char* environment_block;
  struct crt_fd_snapshot fd_snapshot;
  char bootstrap_pipe_text[17];
  char sigmask_text[17];
  char sigdefault_text[17];
  struct crt_env_extra extras[7];
  size_t extra_count = 0;
  sigset64_t sigmask;
  sigset64_t sigdefault;
  int fd_snapshot_ready = 0;
  int fd_snapshot_pipe_mode = 0;
  int native_windows_spawn = 0;
  HANDLE fd_snapshot_pipe_read = 0;
  HANDLE fd_snapshot_pipe_write = 0;
  unsigned char spawn_inherit_touched[CRT_FD_TABLE_SIZE];
  DWORD spawn_old_inherit_flags[CRT_FD_TABLE_SIZE];
  HANDLE native_stdio_handles[3];
  DWORD native_stdio_old_flags[3];
  unsigned char native_stdio_touched[3];
  BOOL process_created;
  DWORD process_error;
  long result;
  long remembered;

  if (path == 0) {
    return -EINVAL;
  }
#define application_path (ctx->application_path)
#define command_line (ctx->command_line)
#define startup (ctx->startup)
#define process (ctx->process)
#define current_directory_buffer (ctx->current_directory_buffer)
#define fd_snapshot_text (ctx->fd_snapshot_text)
#define bootstrap_cwd (ctx->bootstrap_cwd)
#define RETURN(value) do { long crt_spawn_return_value__ = (value); free(ctx); return crt_spawn_return_value__; } while (0)
  ctx = (struct crt_windows_spawn_context*)calloc(1, sizeof(*ctx));
  if (ctx == 0) {
    return -ENOMEM;
  }
  result = resolve_process_application_path(
      path, search_path, application_path, sizeof(application_path));
  if (result != 0) {
    RETURN(result);
  }
  result = build_process_command_line(path, argv, command_line, sizeof(command_line));
  if (result != 0) {
    RETURN(result);
  }
  native_windows_spawn = getenv("CRT_SPAWN_NATIVE_WINDOWS") != 0 ||
                         envp_has_name(envp, "CRT_SPAWN_NATIVE_WINDOWS");
  if (native_windows_spawn) {
    extras[extra_count].name = "CRT_SPAWN_NATIVE_WINDOWS";
    extras[extra_count].value = 0;
    ++extra_count;
    result = __crt_fd_snapshot_export(&fd_snapshot);
    if (result != 0) {
      RETURN(-result);
    }
    fd_snapshot_ready = 1;
    result = prepare_native_windows_spawn_startup(
        actions,
        attr,
        &startup,
        &current_directory,
        current_directory_buffer,
        &fd_snapshot,
        &creation_flags);
    if (result != 0) {
      __crt_fd_snapshot_dispose(&fd_snapshot);
      RETURN(result);
    }
    environment_block = build_windows_environment_block(envp, extras, extra_count);
    if (environment_block == 0) {
      __crt_fd_snapshot_dispose(&fd_snapshot);
      RETURN(-ENOMEM);
    }
    memset(&process, 0, sizeof(process));
    memset(native_stdio_old_flags, 0, sizeof(native_stdio_old_flags));
    set_native_spawn_stdio_inherit(
        &startup, native_stdio_handles, native_stdio_old_flags, native_stdio_touched);
    process_created = CreateProcessA(
            application_path,
            command_line,
            0,
            0,
            1,
            creation_flags,
            environment_block,
            current_directory,
            &startup,
            &process);
    process_error = process_created ? 0 : GetLastError();
    restore_native_spawn_stdio_inherit(
        native_stdio_handles, native_stdio_old_flags, native_stdio_touched);
    __crt_fd_snapshot_dispose(&fd_snapshot);
    fd_snapshot_ready = 0;
    free(environment_block);
    if (!process_created) {
      SetLastError(process_error);
      RETURN(fail_last_error());
    }
    CloseHandle(process.hThread);
    if (pid != 0 && *pid == CRT_SPAWN_PRIVATE_WAIT_PID) {
      private_wait_process = process.hProcess;
      private_wait_pid = process.dwProcessId;
      *pid = (long)process.dwProcessId;
      RETURN(0);
    }
    remembered = remember_child_process(process.dwProcessId, process.hProcess);
    if (remembered < 0) {
      CloseHandle(process.hProcess);
      RETURN(remembered);
    }
    if (pid != 0) {
      *pid = remembered;
    }
    RETURN(0);
  }
  result = __crt_fd_snapshot_export(&fd_snapshot);
  if (result != 0) {
    RETURN(result);
  }
  result = prepare_spawn_startup(
      actions, attr, &startup, &current_directory, current_directory_buffer, &fd_snapshot,
      &creation_flags);
  if (result != 0) {
    __crt_fd_snapshot_dispose(&fd_snapshot);
    RETURN(result);
  }
  fd_snapshot_pipe_mode = 1;
  startup.dwFlags &= ~CRT_STARTF_USESTDHANDLES;
  startup.hStdInput = INVALID_HANDLE_VALUE;
  startup.hStdOutput = INVALID_HANDLE_VALUE;
  startup.hStdError = INVALID_HANDLE_VALUE;
  if (fd_snapshot_pipe_mode) {
    struct {
      DWORD nLength;
      void* lpSecurityDescriptor;
      BOOL bInheritHandle;
    } security_attributes;

    security_attributes.nLength = sizeof(security_attributes);
    security_attributes.lpSecurityDescriptor = 0;
    security_attributes.bInheritHandle = 1;
    if (!CreatePipe(&fd_snapshot_pipe_read, &fd_snapshot_pipe_write, &security_attributes, 0)) {
      __crt_fd_snapshot_dispose(&fd_snapshot);
      RETURN(fail_last_error());
    }
    (void)SetHandleInformation(fd_snapshot_pipe_write, HANDLE_FLAG_INHERIT, 0);
    format_hex_u64((unsigned long long)(uintptr_t)fd_snapshot_pipe_read, bootstrap_pipe_text);
    creation_flags |= CRT_CREATE_SUSPENDED;
  } else {
    result = __crt_fd_snapshot_encode(&fd_snapshot, fd_snapshot_text, sizeof(fd_snapshot_text));
    if (result != 0) {
      __crt_fd_snapshot_dispose(&fd_snapshot);
      RETURN(-result);
    }
  }
  fd_snapshot_ready = 1;
  if (current_directory != 0) {
    size_t len = strlen(current_directory);

    if (len >= sizeof(bootstrap_cwd)) {
      __crt_fd_snapshot_dispose(&fd_snapshot);
      if (fd_snapshot_pipe_read != 0) {
        CloseHandle(fd_snapshot_pipe_read);
      }
      if (fd_snapshot_pipe_write != 0) {
        CloseHandle(fd_snapshot_pipe_write);
      }
      RETURN(-ENAMETOOLONG);
    }
    memcpy(bootstrap_cwd, current_directory, len + 1);
  } else {
    DWORD cwd_result = GetCurrentDirectoryA((DWORD)sizeof(bootstrap_cwd), bootstrap_cwd);

    if (cwd_result == 0) {
      __crt_fd_snapshot_dispose(&fd_snapshot);
      if (fd_snapshot_pipe_read != 0) {
        CloseHandle(fd_snapshot_pipe_read);
      }
      if (fd_snapshot_pipe_write != 0) {
        CloseHandle(fd_snapshot_pipe_write);
      }
      RETURN(fail_last_error());
    }
    if (cwd_result >= (DWORD)sizeof(bootstrap_cwd)) {
      __crt_fd_snapshot_dispose(&fd_snapshot);
      if (fd_snapshot_pipe_read != 0) {
        CloseHandle(fd_snapshot_pipe_read);
      }
      if (fd_snapshot_pipe_write != 0) {
        CloseHandle(fd_snapshot_pipe_write);
      }
      RETURN(-ERANGE);
    }
  }
  if (attr != 0 && (attr->flags & POSIX_SPAWN_SETSIGMASK) != 0) {
    sigmask = attr->sigmask64;
  } else {
    __crt_signal_get_mask(&sigmask);
  }
  sigdefault = attr != 0 && (attr->flags & POSIX_SPAWN_SETSIGDEF) != 0 ? attr->sigdefault64 : 0;
  format_hex_u64((unsigned long long)sigmask, sigmask_text);
  format_hex_u64((unsigned long long)sigdefault, sigdefault_text);
  add_env_extra(extras, &extra_count, CRT_CHILD_BOOTSTRAP_ENV, CRT_CHILD_BOOTSTRAP_VERSION);
  if (fd_snapshot_pipe_mode) {
    add_env_extra(extras, &extra_count, CRT_FD_SNAPSHOT_PIPE_ENV, bootstrap_pipe_text);
  } else if (fd_snapshot_ready) {
    add_env_extra(extras, &extra_count, CRT_FD_SNAPSHOT_ENV, fd_snapshot_text);
  }
  add_env_extra(extras, &extra_count, CRT_BOOTSTRAP_CWD_ENV, bootstrap_cwd);
  add_env_extra(extras, &extra_count, CRT_BOOTSTRAP_ROOTFS_ENV, getenv("CRT_ROOTFS"));
  add_env_extra(extras, &extra_count, CRT_BOOTSTRAP_SIGMASK_ENV, sigmask_text);
  add_env_extra(extras, &extra_count, CRT_BOOTSTRAP_SIGDEFAULT_ENV, sigdefault_text);
  environment_block = build_windows_environment_block(envp, extras, extra_count);
  if (environment_block == 0) {
    if (fd_snapshot_ready) {
      __crt_fd_snapshot_dispose(&fd_snapshot);
    }
    if (fd_snapshot_pipe_read != 0) {
      CloseHandle(fd_snapshot_pipe_read);
    }
    if (fd_snapshot_pipe_write != 0) {
      CloseHandle(fd_snapshot_pipe_write);
    }
    RETURN(-ENOMEM);
  }
  memset(&process, 0, sizeof(process));
  memset(spawn_old_inherit_flags, 0, sizeof(spawn_old_inherit_flags));
  fd_clear_inherit_for_spawn(spawn_inherit_touched, spawn_old_inherit_flags);
  process_created = CreateProcessA(
          application_path,
          command_line,
          0,
          0,
          1,
          creation_flags,
          environment_block,
          current_directory,
          &startup,
          &process);
  process_error = process_created ? 0 : GetLastError();
  fd_restore_inherit_after_fork(spawn_inherit_touched, spawn_old_inherit_flags);
  if (!process_created) {
    free(environment_block);
    if (fd_snapshot_ready) {
      __crt_fd_snapshot_dispose(&fd_snapshot);
    }
    if (fd_snapshot_pipe_read != 0) {
      CloseHandle(fd_snapshot_pipe_read);
    }
    if (fd_snapshot_pipe_write != 0) {
      CloseHandle(fd_snapshot_pipe_write);
    }
    SetLastError(process_error);
    RETURN(fail_last_error());
  }
  free(environment_block);
  if (fd_snapshot_pipe_mode) {
    uint32_t length;

    CloseHandle(fd_snapshot_pipe_read);
    fd_snapshot_pipe_read = 0;
    result = fd_snapshot_prepare_child_duplicates(
        &fd_snapshot, process.hProcess, process.dwProcessId);
    if (result == 0) {
      result = __crt_fd_snapshot_encode(&fd_snapshot, fd_snapshot_text, sizeof(fd_snapshot_text));
      if (result != 0) {
        result = -result;
      }
    }
    if (result == 0) {
      length = (uint32_t)strlen(fd_snapshot_text);
      if (bootstrap_write_exact(fd_snapshot_pipe_write, &length, (DWORD)sizeof(length)) != 0 ||
          bootstrap_write_exact(fd_snapshot_pipe_write, fd_snapshot_text, length) != 0) {
        result = -EIO;
      }
    }
    CloseHandle(fd_snapshot_pipe_write);
    fd_snapshot_pipe_write = 0;
    if (result != 0) {
      TerminateProcess(process.hProcess, 127);
      CloseHandle(process.hThread);
      CloseHandle(process.hProcess);
      if (fd_snapshot_ready) {
        __crt_fd_snapshot_dispose(&fd_snapshot);
      }
      RETURN(result);
    }
    if (ResumeThread(process.hThread) == (DWORD)0xffffffffUL) {
      result = fail_last_error();
      TerminateProcess(process.hProcess, 127);
      CloseHandle(process.hThread);
      CloseHandle(process.hProcess);
      if (fd_snapshot_ready) {
        __crt_fd_snapshot_dispose(&fd_snapshot);
      }
      RETURN(result);
    }
  }
  if (fd_snapshot_ready) {
    __crt_fd_snapshot_dispose(&fd_snapshot);
  }
  CloseHandle(process.hThread);
  if (pid != 0 && *pid == CRT_SPAWN_PRIVATE_WAIT_PID) {
    private_wait_process = process.hProcess;
    private_wait_pid = process.dwProcessId;
    *pid = (long)process.dwProcessId;
    RETURN(0);
  }
  remembered = remember_child_process(process.dwProcessId, process.hProcess);
  if (remembered < 0) {
    CloseHandle(process.hProcess);
    RETURN(remembered);
  }
  if (pid != 0) {
    *pid = remembered;
  }
  RETURN(0);
#undef RETURN
#undef bootstrap_cwd
#undef fd_snapshot_text
#undef current_directory_buffer
#undef process
#undef startup
#undef command_line
#undef application_path
}

long __crt_sys_waitpid(long pid, int* status, int options) {
  HANDLE process;
  DWORD wait_result;
  DWORD exit_code = 127;
  int index = -1;
  DWORD timeout = CRT_INFINITE;
  DWORD child_pid;

  if ((options & ~WNOHANG) != 0) {
    return -ENOTSUP;
  }
  if ((options & WNOHANG) != 0) {
    timeout = 0;
  }
  if (pid != -1 && private_wait_process != 0 && private_wait_pid == (DWORD)pid) {
    process = private_wait_process;
    child_pid = private_wait_pid;
    wait_result = WaitForSingleObject(process, timeout);
    if (wait_result != CRT_WAIT_OBJECT_0 && (options & WNOHANG) != 0) {
      return 0;
    }
    if (wait_result == CRT_WAIT_FAILED) {
      return fail_last_error();
    }
    if (wait_result != CRT_WAIT_OBJECT_0) {
      return -ECHILD;
    }
    if (!GetExitCodeProcess(process, &exit_code)) {
      return fail_last_error();
    }
    CloseHandle(process);
    private_wait_process = 0;
    private_wait_pid = 0;
    if (status != 0) {
      *status = ((int)exit_code & 0xff) << 8;
    }
    return (long)child_pid;
  }
  process = find_child_process(pid, &index);
  if (process == 0) {
    return -ECHILD;
  }
  child_pid = child_pid_table[index];
  wait_result = WaitForSingleObject(process, timeout);
  if (wait_result != CRT_WAIT_OBJECT_0 && (options & WNOHANG) != 0) {
    return 0;
  }
  if (wait_result == CRT_WAIT_FAILED) {
    return fail_last_error();
  }
  if (wait_result != CRT_WAIT_OBJECT_0) {
    return -ECHILD;
  }
  if (!GetExitCodeProcess(process, &exit_code)) {
    long result = fail_last_error();
    return result;
  }
  forget_child_process_at(index);
  CloseHandle(process);
  if (status != 0) {
    *status = ((int)exit_code & 0xff) << 8;
  }
  return (long)child_pid;
}

void __crt_sys_thread_exit(int status) {
  ExitThread((DWORD)status);
}

void __crt_sys_exit(int status) {
  ExitProcess((unsigned int)status);
}
