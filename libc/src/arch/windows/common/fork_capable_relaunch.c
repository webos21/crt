/* Windows startup self-relaunch under the fork()-enabling mitigation
 * policy. See libc/src/arch/windows/{aarch64,x86_64}/fork_memcopy.c and
 * docs/windows_fork_emulation.md ("Spawn Broker Retired") for why this is
 * needed at all: Phase B only verified that *children spawned under the
 * mitigation policy* get deterministic heap/stack addresses -- the
 * original process itself still gets ordinary, per-launch-randomized
 * ASLR addresses without this, so a later fork() call's parent-side
 * addresses would never line up with what a mitigated child gets.
 *
 * Lives in common/, not either arch directory: nothing in this file is
 * architecture-specific (plain Win32 API calls throughout, no CONTEXT/
 * register code -- that part lives in fork_memcopy.c instead), and both
 * aarch64 and x86_64's memory-copy fork() need the identical relaunch
 * dance.
 *
 * Deliberately NOT linked into every Windows process (unlike an earlier
 * version of this file, which lived directly in crt1.c and ran
 * unconditionally): every relaunch is a full extra process launch, and
 * doing it for processes that never call fork() at all -- toybox and any
 * other leaf external command -- turned out not to just be wasted
 * latency but actively harmful. A process reached this way that then
 * spawns a *further* external command (e.g. toybox's own multiplexer
 * dispatch) was observed losing that child's stdio and exiting silently
 * with no diagnosable output, root cause not fully isolated (suspected:
 * something about this project's posix_spawn()/set_native_spawn_stdio_
 * inherit() handle-inheritance machinery not surviving a *second*
 * process-generation hop the way it was designed for exactly one). Rather
 * than ship that regression to every external command on the system,
 * this file is opt-in: only linked into targets that actually call
 * fork() (crt_mksh -- see shell/CMakeLists.txt -- and the ctest suite --
 * see tests/CMakeLists.txt). crt1.c calls this through a weak symbol
 * reference that stays null (and is skipped) for every other target,
 * including toybox.
 *
 * TODO: root-cause the stdio-loss bug well enough to re-enable this
 * unconditionally (or scope it more precisely than "opt in per target"). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <private/crt_fd_table.h>

typedef void* HANDLE;
typedef unsigned long DWORD;
typedef unsigned short WORD;
typedef int BOOL;
typedef unsigned long long DWORD64;
typedef unsigned long long SIZE_T;

#define CRT_WINAPI

#define CRT_EXTENDED_STARTUPINFO_PRESENT 0x00080000UL
#define CRT_PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY 0x00020007ULL
#define CRT_MITIGATION_BOTTOM_UP_ASLR_ALWAYS_OFF (0x2ULL << 16)
#define CRT_MITIGATION_HIGH_ENTROPY_ASLR_ALWAYS_OFF (0x2ULL << 20)
#define CRT_STARTF_USESTDHANDLES 0x00000100UL
#define CRT_STD_INPUT_HANDLE ((DWORD)-10)
#define CRT_STD_OUTPUT_HANDLE ((DWORD)-11)
#define CRT_STD_ERROR_HANDLE ((DWORD)-12)
#define CRT_INFINITE 0xFFFFFFFFUL
#define CRT_HANDLE_FLAG_INHERIT 0x00000001UL
#define CRT_PROCESS_ASLR_POLICY 1
#define CRT_COMMAND_LINE_MAX 8192
#define CRT_CREATE_SUSPENDED 0x00000004UL

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

struct crt_process_mitigation_aslr_policy {
  DWORD Flags; /* bit 0: EnableBottomUpRandomization */
};

__declspec(dllimport) DWORD CRT_WINAPI GetModuleFileNameA(HANDLE hModule, char* lpFilename, DWORD nSize);
__declspec(dllimport) HANDLE CRT_WINAPI GetStdHandle(DWORD nStdHandle);
__declspec(dllimport) HANDLE CRT_WINAPI GetCurrentProcess(void);
__declspec(dllimport) BOOL CRT_WINAPI GetProcessMitigationPolicy(
    HANDLE hProcess, int MitigationPolicy, void* lpBuffer, SIZE_T dwLength);
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
__declspec(dllimport) BOOL CRT_WINAPI SetHandleInformation(HANDLE hObject, DWORD dwMask, DWORD dwFlags);
__declspec(dllimport) DWORD CRT_WINAPI GetLastError(void);
__declspec(dllimport) BOOL CRT_WINAPI SetEnvironmentVariableA(const char* lpName, const char* lpValue);
__declspec(dllimport) BOOL CRT_WINAPI TerminateProcess(HANDLE hProcess, unsigned int uExitCode);
__declspec(dllimport) DWORD CRT_WINAPI ResumeThread(HANDLE hThread);
void exit(int status);

static char relaunch_command_line[CRT_COMMAND_LINE_MAX];

/* Best-effort throughout: any failure here just falls through to normal
 * (non-relaunched, fork()-incapable) startup rather than aborting
 * outright. */
void __crt_windows_ensure_fork_capable_relaunch(const char* command_line) {
  char self_path[1024];
  DWORD path_len;
  SIZE_T attr_list_size = 0;
  void* attr_list;
  DWORD64 policy = CRT_MITIGATION_BOTTOM_UP_ASLR_ALWAYS_OFF | CRT_MITIGATION_HIGH_ENTROPY_ASLR_ALWAYS_OFF;
  struct crt_startupinfoex_relaunch si;
  struct crt_process_information_relaunch pi;
  DWORD exit_code = 1;
  size_t length;
  struct crt_process_mitigation_aslr_policy aslr_policy;

  /* Whether the relaunch already happened is checked via
   * GetProcessMitigationPolicy(), NOT an inheritable environment variable
   * marker (an earlier version of this used one and it was wrong): a
   * process reached via this CRT's execve() -- itself an ordinary
   * CreateProcessA() call with no mitigation-policy attribute -- inherits
   * the calling process's environment block, including any such marker,
   * even though the *actual* new process image is not mitigated at all.
   * Querying this process's own real mitigation state directly has no
   * such gap. */
  memset(&aslr_policy, 0, sizeof(aslr_policy));
  if (GetProcessMitigationPolicy(GetCurrentProcess(), CRT_PROCESS_ASLR_POLICY, &aslr_policy, sizeof(aslr_policy)) &&
      (aslr_policy.Flags & 1U) == 0) {
    return;
  }

  path_len = GetModuleFileNameA(0, self_path, (DWORD)sizeof(self_path));
  if (path_len == 0 || path_len >= sizeof(self_path)) {
    fprintf(stderr, "fork_capable_relaunch: GetModuleFileNameA failed\n");
    return;
  }

  InitializeProcThreadAttributeList(0, 1, 0, &attr_list_size);
  attr_list = malloc((size_t)attr_list_size);
  if (attr_list == 0) {
    fprintf(stderr, "fork_capable_relaunch: malloc attr_list failed\n");
    return;
  }
  if (!InitializeProcThreadAttributeList(attr_list, 1, 0, &attr_list_size) ||
      !UpdateProcThreadAttribute(attr_list, 0, CRT_PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY,
                                  &policy, sizeof(policy), 0, 0)) {
    fprintf(stderr, "fork_capable_relaunch: attr list setup failed\n");
    DeleteProcThreadAttributeList(attr_list);
    free(attr_list);
    return;
  }

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

  /* This process's own std handles may not be marked inheritable -- e.g.
   * a handle this process itself inherited from *its* parent's
   * temporarily-marked-inheritable handle (posix_spawn()'s
   * set_native_spawn_stdio_inherit()/restore_native_spawn_stdio_inherit()
   * pattern in syscall.c, which restores the flag on the *parent's* copy
   * right after spawning, but says nothing about whether this process's
   * own inherited copy carries the flag). STARTF_USESTDHANDLES only
   * actually reaches the child if these handles are inheritable at the
   * moment CreateProcessA() below runs, so mark them explicitly rather
   * than relying on inheritance-of-inheritance. Not restored afterward:
   * this process is about to exit() unconditionally once its relaunched
   * child exits, so there is no "later" for these flags to matter for. */
  if (si.hStdInput != 0 && si.hStdInput != (HANDLE)(long long)-1) {
    SetHandleInformation(si.hStdInput, CRT_HANDLE_FLAG_INHERIT, CRT_HANDLE_FLAG_INHERIT);
  }
  if (si.hStdOutput != 0 && si.hStdOutput != (HANDLE)(long long)-1) {
    SetHandleInformation(si.hStdOutput, CRT_HANDLE_FLAG_INHERIT, CRT_HANDLE_FLAG_INHERIT);
  }
  if (si.hStdError != 0 && si.hStdError != (HANDLE)(long long)-1) {
    SetHandleInformation(si.hStdError, CRT_HANDLE_FLAG_INHERIT, CRT_HANDLE_FLAG_INHERIT);
  }

  /* Hand this process's current fd table (imported by __crt_child_
   * bootstrap() above, which now deliberately runs before this function
   * -- see crt1.c) across to the relaunched child the same way
   * __crt_sys_posix_spawn() hands it to any other spawned child:
   * duplicate each fd's handle into the child's own process and a
   * suspended-child + pipe handoff, not bare Windows handle inheritance.
   * Without this, any fd beyond the 3 standard ones (e.g. one delivered
   * via posix_spawn_file_actions_adddup2() to spawn *this* process) is
   * silently lost across this relaunch hop, since its handle value is
   * only meaningful in this process, not the relaunched one -- see
   * docs/windows_fork_emulation.md "Current Open Issues". Best-effort:
   * if this fails, fall back to relaunching without it, same as before
   * this fd handoff existed. */
  {
    unsigned long long pipe_read_handle = 0;
    int fd_handoff_active = __crt_windows_fd_snapshot_relaunch_begin(&pipe_read_handle) == 0;
    DWORD creation_flags = CRT_EXTENDED_STARTUPINFO_PRESENT;
    char pipe_handle_text[20];

    if (fd_handoff_active) {
      creation_flags |= CRT_CREATE_SUSPENDED;
      sprintf(pipe_handle_text, "%llx", pipe_read_handle);
      SetEnvironmentVariableA(CRT_FD_SNAPSHOT_PIPE_ENV, pipe_handle_text);
      /* Clear any stale direct-encoded snapshot inherited from further
       * back (e.g. this process's own original spawn, if it used the
       * non-pipe transport) -- __crt_child_bootstrap() checks
       * CRT_FD_SNAPSHOT_ENV before CRT_FD_SNAPSHOT_PIPE_ENV, so a
       * leftover value here would silently shadow the fresh one above. */
      SetEnvironmentVariableA(CRT_FD_SNAPSHOT_ENV, 0);
      SetEnvironmentVariableA(CRT_CHILD_BOOTSTRAP_ENV, CRT_CHILD_BOOTSTRAP_VERSION);
    }

    if (!CreateProcessA(self_path, relaunch_command_line, 0, 0, 1, creation_flags, 0, 0, &si, &pi)) {
      fprintf(stderr, "fork_capable_relaunch: CreateProcessA failed err=%lu\n", (unsigned long)GetLastError());
      DeleteProcThreadAttributeList(attr_list);
      free(attr_list);
      if (fd_handoff_active) {
        CloseHandle((HANDLE)(uintptr_t)pipe_read_handle);
        __crt_windows_fd_snapshot_relaunch_abort();
      }
      return;
    }
    DeleteProcThreadAttributeList(attr_list);
    free(attr_list);

    if (fd_handoff_active) {
      CloseHandle((HANDLE)(uintptr_t)pipe_read_handle);
      if (__crt_windows_fd_snapshot_relaunch_finish((unsigned long long)(uintptr_t)pi.hProcess, pi.dwProcessId) !=
          0) {
        fprintf(stderr, "fork_capable_relaunch: fd snapshot handoff failed\n");
        TerminateProcess(pi.hProcess, 127);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return;
      }
      if (ResumeThread(pi.hThread) == (DWORD)0xffffffffUL) {
        fprintf(stderr, "fork_capable_relaunch: ResumeThread failed err=%lu\n", (unsigned long)GetLastError());
        TerminateProcess(pi.hProcess, 127);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return;
      }
    }
  }

  WaitForSingleObject(pi.hProcess, CRT_INFINITE);
  GetExitCodeProcess(pi.hProcess, &exit_code);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  exit((int)exit_code);
}
