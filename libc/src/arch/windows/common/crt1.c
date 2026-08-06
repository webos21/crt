#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <private/crt_fd_table.h>

#if defined(_M_IX86) || defined(__i386__)
#define CRT_WINAPI __stdcall
#else
#define CRT_WINAPI
#endif

#define CRT_ARG_MAX 256
#define CRT_COMMAND_LINE_MAX 8192

__declspec(dllimport) char* CRT_WINAPI GetCommandLineA(void);

int main(int argc, char** argv, char** envp);
void __crt_env_set_initial(char** envp);
void __crt_rootfs_bootstrap(int argc, char** argv);
void exit(int status);

#if defined(__aarch64__) || defined(_M_ARM64)
/* Every process built by this CRT for Windows aarch64 relaunches itself
 * once under the mitigation policy that makes memory-copy fork()
 * (libc/src/arch/windows/aarch64/fork_memcopy.c) viable at all: without
 * this, only fork()'s freshly-spawned *children* get deterministic
 * heap/stack addresses -- the original process itself still gets
 * ordinary, per-launch-randomized ASLR addresses, so a later fork()
 * call's parent-side addresses would never line up with what a
 * mitigated child gets. Every program pays one extra process-launch's
 * worth of startup latency for this, whether or not it ever calls
 * fork() -- see docs/windows_fork_emulation.md, "Spawn Broker Retired".
 * Best-effort: any failure here just falls through to normal (non-
 * relaunched, fork()-incapable) startup rather than aborting outright. */
#define CRT_FORK_MITIGATED_ENV "CRT_FORK_MITIGATED"
#define CRT_EXTENDED_STARTUPINFO_PRESENT 0x00080000UL
#define CRT_PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY 0x00020007ULL
#define CRT_MITIGATION_BOTTOM_UP_ASLR_ALWAYS_OFF (0x2ULL << 16)
#define CRT_MITIGATION_HIGH_ENTROPY_ASLR_ALWAYS_OFF (0x2ULL << 20)
#define CRT_STARTF_USESTDHANDLES 0x00000100UL
#define CRT_STD_INPUT_HANDLE ((DWORD)-10)
#define CRT_STD_OUTPUT_HANDLE ((DWORD)-11)
#define CRT_STD_ERROR_HANDLE ((DWORD)-12)
#define CRT_INFINITE 0xFFFFFFFFUL

typedef void* HANDLE;
typedef unsigned long DWORD;
typedef unsigned short WORD;
typedef int BOOL;
typedef unsigned long long DWORD64;
typedef unsigned long long SIZE_T;

struct crt_startupinfoex_relaunch {
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
  void* lpAttributeList;
};

struct crt_process_information_relaunch {
  HANDLE hProcess;
  HANDLE hThread;
  DWORD dwProcessId;
  DWORD dwThreadId;
};

__declspec(dllimport) DWORD CRT_WINAPI GetModuleFileNameA(HANDLE hModule, char* lpFilename, DWORD nSize);
__declspec(dllimport) BOOL CRT_WINAPI SetEnvironmentVariableA(const char* lpName, const char* lpValue);
__declspec(dllimport) HANDLE CRT_WINAPI GetStdHandle(DWORD nStdHandle);
__declspec(dllimport) BOOL CRT_WINAPI CreateProcessA(
    const char* lpApplicationName,
    char* lpCommandLine,
    void* lpProcessAttributes,
    void* lpThreadAttributes,
    BOOL bInheritHandles,
    DWORD dwCreationFlags,
    void* lpEnvironment,
    const char* lpCurrentDirectory,
    struct crt_startupinfoex_relaunch* lpStartupInfo,
    struct crt_process_information_relaunch* lpProcessInformation);
__declspec(dllimport) DWORD CRT_WINAPI WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds);
__declspec(dllimport) BOOL CRT_WINAPI GetExitCodeProcess(HANDLE hProcess, DWORD* lpExitCode);
__declspec(dllimport) BOOL CRT_WINAPI CloseHandle(HANDLE hObject);
__declspec(dllimport) BOOL CRT_WINAPI InitializeProcThreadAttributeList(
    void* lpAttributeList, DWORD dwAttributeCount, DWORD dwFlags, SIZE_T* lpSize);
__declspec(dllimport) BOOL CRT_WINAPI UpdateProcThreadAttribute(
    void* lpAttributeList,
    DWORD dwFlags,
    unsigned long long Attribute,
    void* lpValue,
    SIZE_T cbSize,
    void* lpPreviousValue,
    SIZE_T* lpReturnSize);
__declspec(dllimport) void CRT_WINAPI DeleteProcThreadAttributeList(void* lpAttributeList);
char* getenv(const char* name);

static char relaunch_command_line[CRT_COMMAND_LINE_MAX];

static void windows_aarch64_ensure_mitigated_relaunch(const char* command_line) {
  char self_path[1024];
  DWORD path_len;
  SIZE_T attr_list_size = 0;
  void* attr_list;
  DWORD64 policy = CRT_MITIGATION_BOTTOM_UP_ASLR_ALWAYS_OFF | CRT_MITIGATION_HIGH_ENTROPY_ASLR_ALWAYS_OFF;
  struct crt_startupinfoex_relaunch si;
  struct crt_process_information_relaunch pi;
  DWORD exit_code = 1;
  size_t length;

  if (getenv(CRT_FORK_MITIGATED_ENV) != 0) {
    return;
  }

  path_len = GetModuleFileNameA(0, self_path, (DWORD)sizeof(self_path));
  if (path_len == 0 || path_len >= sizeof(self_path)) {
    return;
  }

  InitializeProcThreadAttributeList(0, 1, 0, &attr_list_size);
  attr_list = malloc((size_t)attr_list_size);
  if (attr_list == 0) {
    return;
  }
  if (!InitializeProcThreadAttributeList(attr_list, 1, 0, &attr_list_size) ||
      !UpdateProcThreadAttribute(attr_list, 0, CRT_PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY,
                                  &policy, sizeof(policy), 0, 0)) {
    DeleteProcThreadAttributeList(attr_list);
    free(attr_list);
    return;
  }

  SetEnvironmentVariableA(CRT_FORK_MITIGATED_ENV, "1");

  length = command_line != 0 ? strlen(command_line) : 0;
  if (length >= sizeof(relaunch_command_line)) {
    length = sizeof(relaunch_command_line) - 1;
  }
  if (length > 0) {
    memcpy(relaunch_command_line, command_line, length);
  }
  relaunch_command_line[length] = 0;

  memset(&si, 0, sizeof(si));
  si.cb = sizeof(si);
  si.dwFlags = CRT_STARTF_USESTDHANDLES;
  si.hStdInput = GetStdHandle(CRT_STD_INPUT_HANDLE);
  si.hStdOutput = GetStdHandle(CRT_STD_OUTPUT_HANDLE);
  si.hStdError = GetStdHandle(CRT_STD_ERROR_HANDLE);
  si.lpAttributeList = attr_list;
  memset(&pi, 0, sizeof(pi));

  if (!CreateProcessA(self_path, relaunch_command_line, 0, 0, 1, CRT_EXTENDED_STARTUPINFO_PRESENT, 0, 0, &si,
                       &pi)) {
    DeleteProcThreadAttributeList(attr_list);
    free(attr_list);
    return;
  }
  DeleteProcThreadAttributeList(attr_list);
  free(attr_list);

  WaitForSingleObject(pi.hProcess, CRT_INFINITE);
  GetExitCodeProcess(pi.hProcess, &exit_code);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  exit((int)exit_code);
}
#endif

static char command_line_storage[CRT_COMMAND_LINE_MAX];
static char* argv_storage[CRT_ARG_MAX];

static int command_line_space(int c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static int parse_windows_command_line(char* command_line, char** argv, int max_args) {
  int argc = 0;
  char* read = command_line;
  char* write = command_line;

  while (*read != 0 && argc + 1 < max_args) {
    int in_quotes = 0;

    while (command_line_space((unsigned char)*read)) {
      ++read;
    }
    if (*read == 0) {
      break;
    }
    argv[argc++] = write;
    while (*read != 0) {
      if (*read == '"') {
        in_quotes = !in_quotes;
        ++read;
        continue;
      }
      if (!in_quotes && command_line_space((unsigned char)*read)) {
        ++read;
        break;
      }
      if (*read == '\\' && read[1] == '"') {
        *write++ = '"';
        read += 2;
        continue;
      }
      *write++ = *read++;
    }
    *write++ = 0;
  }
  argv[argc] = 0;
  return argc;
}

void mainCRTStartup(void) {
  char* command_line = GetCommandLineA();
  int argc = 0;

  __crt_env_set_initial(0);
#if defined(__aarch64__) || defined(_M_ARM64)
  windows_aarch64_ensure_mitigated_relaunch(command_line);
#endif
  __crt_child_bootstrap();
  if (command_line != 0) {
    size_t length = strlen(command_line);

    if (length >= sizeof(command_line_storage)) {
      length = sizeof(command_line_storage) - 1;
    }
    memcpy(command_line_storage, command_line, length);
    command_line_storage[length] = 0;
    argc = parse_windows_command_line(command_line_storage, argv_storage, CRT_ARG_MAX);
  }
  if (argc == 0) {
    argv_storage[0] = "";
    argv_storage[1] = 0;
    argc = 1;
  }
  __crt_rootfs_bootstrap(argc, argv_storage);
  exit(main(argc, argv_storage, environ));
}
