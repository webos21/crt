#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <private/crt_fd_table.h>
#include "crt_spawn_broker.h"

/*
 * See libc/include/private/crt_spawn_broker.h for the protocol and the
 * "why does this file exist" background, and
 * docs/windows_fork_emulation.md ("Chosen Direction: Spawn Broker") for
 * the full design writeup.
 *
 * This file is deliberately self-contained (its own local Win32
 * declarations, its own small read/write-exact helpers) rather than
 * sharing statics with syscall.c, matching this codebase's existing
 * per-file style for Windows PAL glue (see signal_backend.c).
 */

typedef void* HANDLE;
typedef unsigned long DWORD;
typedef int BOOL;

#if defined(_M_IX86) || defined(__i386__)
#define CRT_WINAPI __stdcall
#else
#define CRT_WINAPI
#endif

struct crt_broker_startupinfo {
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
  unsigned short wShowWindow;
  unsigned short cbReserved2;
  unsigned char* lpReserved2;
  HANDLE hStdInput;
  HANDLE hStdOutput;
  HANDLE hStdError;
};

struct crt_broker_process_information {
  HANDLE hProcess;
  HANDLE hThread;
  DWORD dwProcessId;
  DWORD dwThreadId;
};

struct crt_broker_overlapped {
  uintptr_t Internal;
  uintptr_t InternalHigh;
  DWORD Offset;
  DWORD OffsetHigh;
  HANDLE hEvent;
};

#define CRT_INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define CRT_INFINITE 0xffffffffUL
#define CRT_STARTF_USESTDHANDLES 0x00000100UL
#define CRT_ERROR_FILE_NOT_FOUND 2UL
#define CRT_ERROR_PIPE_BUSY 231UL
#define CRT_ERROR_PIPE_CONNECTED 535UL
#define CRT_PIPE_ACCESS_DUPLEX 0x00000003UL
#define CRT_PIPE_TYPE_BYTE 0x00000000UL
#define CRT_PIPE_READMODE_BYTE 0x00000000UL
#define CRT_PIPE_WAIT 0x00000000UL
#define CRT_PIPE_UNLIMITED_INSTANCES 255UL
#define CRT_GENERIC_READ 0x80000000UL
#define CRT_GENERIC_WRITE 0x40000000UL
#define CRT_OPEN_EXISTING 3UL
#define CRT_PROCESS_DUP_HANDLE 0x00000040UL
#define CRT_DUPLICATE_SAME_ACCESS 0x00000002UL
#define CRT_HANDLE_FLAG_INHERIT 0x00000001UL
#define CRT_FILE_FLAG_OVERLAPPED 0x40000000UL
#define CRT_ERROR_IO_PENDING 997UL
#define CRT_WAIT_TIMEOUT 258UL
#define CRT_WAIT_OBJECT_0 0UL
#define CRT_CREATE_NEW 1UL
#define CRT_CREATE_ALWAYS 2UL
#define CRT_OPEN_ALWAYS 4UL
#define CRT_FILE_SHARE_READ 0x00000001UL
#define CRT_FILE_SHARE_WRITE 0x00000002UL
#define CRT_FILE_SHARE_DELETE 0x00000004UL
#define CRT_FILE_ATTRIBUTE_NORMAL 0x00000080UL
#define CRT_FILE_ATTRIBUTE_DIRECTORY 0x00000010UL
#define CRT_FILE_FLAG_BACKUP_SEMANTICS 0x02000000UL
#define CRT_INVALID_FILE_ATTRIBUTES 0xffffffffUL
/* Generous but finite: comfortably covers even a slow CreateProcessA
 * round trip through the broker, while guaranteeing a stuck request can
 * never wedge a caller (or the broker itself) forever. See the
 * __crt_windows_spawn_broker_main() comment for why "forever" was a real,
 * observed failure mode -- not just a theoretical one. */
#define CRT_BROKER_IO_TIMEOUT_MS 20000UL

__declspec(dllimport) DWORD CRT_WINAPI GetLastError(void);
__declspec(dllimport) void CRT_WINAPI SetLastError(DWORD dwErrCode);
__declspec(dllimport) void CRT_WINAPI ExitProcess(unsigned int uExitCode);
__declspec(dllimport) void CRT_WINAPI Sleep(DWORD dwMilliseconds);
__declspec(dllimport) DWORD CRT_WINAPI GetCurrentProcessId(void);
__declspec(dllimport) HANDLE CRT_WINAPI GetCurrentProcess(void);
__declspec(dllimport) BOOL CRT_WINAPI CloseHandle(HANDLE hObject);
__declspec(dllimport) DWORD CRT_WINAPI GetModuleFileNameA(
    HANDLE hModule, char* lpFilename, DWORD nSize);
__declspec(dllimport) HANDLE CRT_WINAPI OpenProcess(
    DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwProcessId);
__declspec(dllimport) BOOL CRT_WINAPI DuplicateHandle(
    HANDLE hSourceProcessHandle, HANDLE hSourceHandle, HANDLE hTargetProcessHandle,
    HANDLE* lpTargetHandle, DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwOptions);
__declspec(dllimport) HANDLE CRT_WINAPI CreateFileA(
    const char* lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
    void* lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile);
__declspec(dllimport) BOOL CRT_WINAPI ReadFile(
    HANDLE hFile, void* lpBuffer, DWORD nNumberOfBytesToRead, DWORD* lpNumberOfBytesRead,
    void* lpOverlapped);
__declspec(dllimport) BOOL CRT_WINAPI WriteFile(
    HANDLE hFile, const void* lpBuffer, DWORD nNumberOfBytesToWrite, DWORD* lpNumberOfBytesWritten,
    void* lpOverlapped);
__declspec(dllimport) BOOL CRT_WINAPI WaitNamedPipeA(const char* lpNamedPipeName, DWORD nTimeOut);
__declspec(dllimport) BOOL CRT_WINAPI CreatePipe(
    HANDLE* hReadPipe, HANDLE* hWritePipe, void* lpPipeAttributes, DWORD nSize);
__declspec(dllimport) BOOL CRT_WINAPI SetHandleInformation(HANDLE hObject, DWORD dwMask, DWORD dwFlags);
__declspec(dllimport) HANDLE CRT_WINAPI CreateNamedPipeA(
    const char* lpName, DWORD dwOpenMode, DWORD dwPipeMode, DWORD nMaxInstances,
    DWORD nOutBufferSize, DWORD nInBufferSize, DWORD nDefaultTimeOut, void* lpSecurityAttributes);
__declspec(dllimport) BOOL CRT_WINAPI ConnectNamedPipe(HANDLE hNamedPipe, void* lpOverlapped);
__declspec(dllimport) BOOL CRT_WINAPI DisconnectNamedPipe(HANDLE hNamedPipe);
__declspec(dllimport) BOOL CRT_WINAPI CreateProcessA(
    const char* lpApplicationName, char* lpCommandLine, void* lpProcessAttributes,
    void* lpThreadAttributes, BOOL bInheritHandles, DWORD dwCreationFlags, void* lpEnvironment,
    const char* lpCurrentDirectory, struct crt_broker_startupinfo* lpStartupInfo,
    struct crt_broker_process_information* lpProcessInformation);
__declspec(dllimport) BOOL CRT_WINAPI TerminateProcess(HANDLE hProcess, unsigned int uExitCode);
__declspec(dllimport) BOOL CRT_WINAPI FlushFileBuffers(HANDLE hFile);
__declspec(dllimport) HANDLE CRT_WINAPI CreateEventA(
    void* lpEventAttributes, BOOL bManualReset, BOOL bInitialState, const char* lpName);
__declspec(dllimport) DWORD CRT_WINAPI WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds);
__declspec(dllimport) BOOL CRT_WINAPI CancelIoEx(HANDLE hFile, struct crt_broker_overlapped* lpOverlapped);
__declspec(dllimport) BOOL CRT_WINAPI GetOverlappedResult(
    HANDLE hFile, struct crt_broker_overlapped* lpOverlapped, DWORD* lpNumberOfBytesTransferred,
    BOOL bWait);
__declspec(dllimport) DWORD CRT_WINAPI GetFileAttributesA(const char* lpFileName);

/* Waits (bounded by timeout_ms) for an overlapped ReadFile/WriteFile that
 * returned FALSE/ERROR_IO_PENDING to actually finish, and reports how many
 * bytes it transferred. On timeout, cancels the operation first -- an
 * event that never gets signaled after a plain WaitForSingleObject
 * timeout would otherwise leave the *kernel's* I/O request still pending
 * against `handle`, which is exactly the kind of thing that would corrupt
 * the next operation on the same handle. */
static int overlapped_wait(HANDLE handle, struct crt_broker_overlapped* ov, DWORD timeout_ms,
                            DWORD* out_bytes) {
  if (WaitForSingleObject(ov->hEvent, timeout_ms) != CRT_WAIT_OBJECT_0) {
    CancelIoEx(handle, ov);
    GetOverlappedResult(handle, ov, out_bytes, 1);
    return -ETIMEDOUT;
  }
  if (!GetOverlappedResult(handle, ov, out_bytes, 1)) {
    return -EIO;
  }
  return 0;
}

/* All broker pipe handles (both ends) are opened with
 * FILE_FLAG_OVERLAPPED specifically so these two helpers can bound how
 * long they wait: without a timeout, a request that the broker never
 * responds to (a bug on either side, an unlucky race, the broker process
 * having died) leaves the caller blocked in ReadFile/WriteFile forever --
 * observed in practice as an mksh subshell that never exits, invisible in
 * any log because it isn't crashing, just waiting. See
 * __crt_windows_spawn_broker_main() for the corresponding server-side
 * accept-side reasoning. */
static int write_exact(HANDLE handle, const void* buffer, DWORD size, DWORD timeout_ms) {
  const char* in = (const char*)buffer;
  DWORD offset = 0;
  HANDLE ev;
  int rc = 0;

  if (size == 0) {
    return 0;
  }
  ev = CreateEventA(0, 1, 0, 0);
  if (ev == 0) {
    return -EIO;
  }
  while (offset < size) {
    struct crt_broker_overlapped ov;
    DWORD wrote = 0;

    memset(&ov, 0, sizeof(ov));
    ov.hEvent = ev;
    if (!WriteFile(handle, in + offset, size - offset, &wrote, &ov)) {
      if (GetLastError() != CRT_ERROR_IO_PENDING) {
        rc = -EIO;
        break;
      }
      rc = overlapped_wait(handle, &ov, timeout_ms, &wrote);
      if (rc != 0) {
        break;
      }
    }
    if (wrote == 0) {
      rc = -EIO;
      break;
    }
    offset += wrote;
  }
  CloseHandle(ev);
  return rc;
}

static int read_exact(HANDLE handle, void* buffer, DWORD size, DWORD timeout_ms) {
  char* out = (char*)buffer;
  DWORD offset = 0;
  HANDLE ev;
  int rc = 0;

  if (size == 0) {
    return 0;
  }
  ev = CreateEventA(0, 1, 0, 0);
  if (ev == 0) {
    return -EIO;
  }
  while (offset < size) {
    struct crt_broker_overlapped ov;
    DWORD got = 0;

    memset(&ov, 0, sizeof(ov));
    ov.hEvent = ev;
    if (!ReadFile(handle, out + offset, size - offset, &got, &ov)) {
      if (GetLastError() != CRT_ERROR_IO_PENDING) {
        rc = -EIO;
        break;
      }
      rc = overlapped_wait(handle, &ov, timeout_ms, &got);
      if (rc != 0) {
        break;
      }
    }
    if (got == 0) {
      rc = -EIO;
      break;
    }
    offset += got;
  }
  CloseHandle(ev);
  return rc;
}

/* ---- client side: lazily start the broker, once per fork lineage ---- */

static HANDLE g_broker_process;
static DWORD g_broker_pid;
static char g_broker_pipe_name[CRT_SPAWN_BROKER_PIPE_NAME_MAX];
static int g_broker_started;

/* Registered with atexit() the first time this process starts a broker.
 * Only meaningful in the process that actually started the broker: the
 * resulting hProcess is never marked inheritable, so a clone that
 * inherited g_broker_process via the RtlCloneUserProcess copy-on-write
 * clone holds a value that is not a valid handle in its own handle
 * table, and TerminateProcess on it simply (harmlessly) fails there.
 * Without this, the broker would otherwise sit forever in
 * ConnectNamedPipe() after its starting process exits, since Windows
 * does not cascade-terminate child processes by default -- which is
 * exactly what made ctest hang waiting for an orphaned broker during
 * this feature's own bring-up. */
static void shutdown_spawn_broker_atexit(void) {
  if (g_broker_process != 0) {
    TerminateProcess(g_broker_process, 0);
    CloseHandle(g_broker_process);
    g_broker_process = 0;
  }
}

long __crt_windows_ensure_spawn_broker(void) {
  char exe_path[CRT_SPAWN_BROKER_PATH_MAX];
  char command_line[CRT_SPAWN_BROKER_PATH_MAX + 2];
  char environment_block[512];
  size_t offset;
  size_t written;
  struct crt_broker_startupinfo startup;
  struct crt_broker_process_information process;
  BOOL created;

  if (g_broker_started) {
    return 0;
  }
  if (GetModuleFileNameA(0, exe_path, (DWORD)sizeof(exe_path)) == 0) {
    return -EIO;
  }
  written = (size_t)snprintf(
      g_broker_pipe_name, sizeof(g_broker_pipe_name),
      "\\\\.\\pipe\\crt_spawn_broker_%lu", (unsigned long)GetCurrentProcessId());
  if (written == 0 || written >= sizeof(g_broker_pipe_name)) {
    return -ENAMETOOLONG;
  }

  command_line[0] = '"';
  offset = 1;
  written = strlen(exe_path);
  if (written >= sizeof(command_line) - 2) {
    return -ENAMETOOLONG;
  }
  memcpy(command_line + offset, exe_path, written);
  offset += written;
  command_line[offset++] = '"';
  command_line[offset] = 0;

  offset = 0;
#define ADD_ENV_VAR(name, value)                                                   \
  do {                                                                             \
    size_t name_len = strlen(name);                                                \
    size_t value_len = strlen(value);                                              \
    if (offset + name_len + 1 + value_len + 1 >= sizeof(environment_block)) {      \
      return -ENAMETOOLONG;                                                        \
    }                                                                              \
    memcpy(environment_block + offset, name, name_len);                            \
    offset += name_len;                                                            \
    environment_block[offset++] = '=';                                             \
    memcpy(environment_block + offset, value, value_len);                          \
    offset += value_len;                                                           \
    environment_block[offset++] = 0;                                               \
  } while (0)
  ADD_ENV_VAR(CRT_SPAWN_BROKER_MODE_ENV, "1");
  ADD_ENV_VAR(CRT_SPAWN_BROKER_PIPE_ENV, g_broker_pipe_name);
#undef ADD_ENV_VAR
  environment_block[offset++] = 0;

  memset(&startup, 0, sizeof(startup));
  startup.cb = (DWORD)sizeof(startup);
  memset(&process, 0, sizeof(process));

  created = CreateProcessA(
      exe_path, command_line, 0, 0, /* bInheritHandles */ 0, 0, environment_block, 0, &startup,
      &process);
  if (!created) {
    return -EIO;
  }
  CloseHandle(process.hThread);
  g_broker_process = process.hProcess;
  g_broker_pid = process.dwProcessId;
  g_broker_started = 1;
  atexit(shutdown_spawn_broker_atexit);
  return 0;
}

/* ---- client side: send one spawn request, wait for the response ---- */

long __crt_windows_spawn_broker_request(
    const char* application_path,
    const char* command_line,
    const char* current_directory,
    const char* environment_block,
    unsigned int environment_length,
    unsigned long creation_flags,
    void* std_input,
    void* std_output,
    void* std_error,
    int want_fd_snapshot_pipe,
    unsigned long* out_pid,
    void** out_process_handle,
    void** out_thread_handle,
    void** out_fd_snapshot_pipe_write) {
  struct crt_spawn_broker_request_header header;
  struct crt_spawn_broker_response response;
  HANDLE pipe = CRT_INVALID_HANDLE_VALUE;
  int attempt;
  size_t application_path_length = strlen(application_path);
  size_t command_line_length = strlen(command_line);
  size_t current_directory_length = current_directory != 0 ? strlen(current_directory) : 0;
  int rc;

  if (!g_broker_started || g_broker_pipe_name[0] == 0) {
    return -ECHILD;
  }
  if (application_path_length >= CRT_SPAWN_BROKER_PATH_MAX ||
      command_line_length >= CRT_SPAWN_BROKER_CMDLINE_MAX ||
      current_directory_length >= CRT_SPAWN_BROKER_CWD_MAX ||
      environment_length >= CRT_SPAWN_BROKER_ENV_MAX) {
    return -ENAMETOOLONG;
  }

  for (attempt = 0; attempt < 100; ++attempt) {
    pipe = CreateFileA(
        g_broker_pipe_name, CRT_GENERIC_READ | CRT_GENERIC_WRITE, 0, 0, CRT_OPEN_EXISTING,
        CRT_FILE_FLAG_OVERLAPPED, 0);
    if (pipe != CRT_INVALID_HANDLE_VALUE) {
      break;
    }
    if (GetLastError() != CRT_ERROR_FILE_NOT_FOUND && GetLastError() != CRT_ERROR_PIPE_BUSY) {
      return -EIO;
    }
    WaitNamedPipeA(g_broker_pipe_name, 50);
    Sleep(10);
  }
  if (pipe == CRT_INVALID_HANDLE_VALUE) {
    return -EIO;
  }

  memset(&header, 0, sizeof(header));
  header.magic = CRT_SPAWN_BROKER_MAGIC;
  header.version = CRT_SPAWN_BROKER_VERSION;
  header.client_pid = (uint32_t)GetCurrentProcessId();
  header.creation_flags = (uint32_t)creation_flags;
  header.std_input = (uint64_t)(uintptr_t)std_input;
  header.std_output = (uint64_t)(uintptr_t)std_output;
  header.std_error = (uint64_t)(uintptr_t)std_error;
  header.want_fd_snapshot_pipe = want_fd_snapshot_pipe ? 1U : 0U;
  header.application_path_length = (uint32_t)application_path_length;
  header.command_line_length = (uint32_t)command_line_length;
  header.current_directory_length = (uint32_t)current_directory_length;
  header.environment_length = (uint32_t)environment_length;

  rc = write_exact(pipe, &header, (DWORD)sizeof(header), CRT_BROKER_IO_TIMEOUT_MS);
  if (rc == 0 && application_path_length != 0) {
    rc = write_exact(pipe, application_path, (DWORD)application_path_length, CRT_BROKER_IO_TIMEOUT_MS);
  }
  if (rc == 0 && command_line_length != 0) {
    rc = write_exact(pipe, command_line, (DWORD)command_line_length, CRT_BROKER_IO_TIMEOUT_MS);
  }
  if (rc == 0 && current_directory_length != 0) {
    rc = write_exact(pipe, current_directory, (DWORD)current_directory_length, CRT_BROKER_IO_TIMEOUT_MS);
  }
  if (rc == 0 && environment_length != 0) {
    rc = write_exact(pipe, environment_block, (DWORD)environment_length, CRT_BROKER_IO_TIMEOUT_MS);
  }
  if (rc == 0) {
    rc = read_exact(pipe, &response, (DWORD)sizeof(response), CRT_BROKER_IO_TIMEOUT_MS);
  }
  CloseHandle(pipe);
  if (rc != 0) {
    return rc;
  }
  if (response.magic != CRT_SPAWN_BROKER_MAGIC || response.version != CRT_SPAWN_BROKER_VERSION) {
    return -EIO;
  }
  if (response.result != 0) {
    return response.result;
  }
  if (out_pid != 0) {
    *out_pid = (unsigned long)response.process_id;
  }
  if (out_process_handle != 0) {
    *out_process_handle = (void*)(uintptr_t)response.process_handle;
  }
  if (out_thread_handle != 0) {
    *out_thread_handle = (void*)(uintptr_t)response.thread_handle;
  }
  if (out_fd_snapshot_pipe_write != 0) {
    *out_fd_snapshot_pipe_write = (void*)(uintptr_t)response.fd_snapshot_pipe_write;
  }
  return 0;
}

/* ---- client side: ask the broker for a plain, unattached pipe ---- */

long __crt_windows_broker_create_pipe(void** out_read, void** out_write) {
  struct crt_spawn_broker_request_header header;
  struct crt_spawn_broker_response response;
  HANDLE pipe = CRT_INVALID_HANDLE_VALUE;
  int attempt;
  int rc;

  if (!g_broker_started || g_broker_pipe_name[0] == 0) {
    return -ECHILD;
  }

  for (attempt = 0; attempt < 100; ++attempt) {
    pipe = CreateFileA(
        g_broker_pipe_name, CRT_GENERIC_READ | CRT_GENERIC_WRITE, 0, 0, CRT_OPEN_EXISTING,
        CRT_FILE_FLAG_OVERLAPPED, 0);
    if (pipe != CRT_INVALID_HANDLE_VALUE) {
      break;
    }
    if (GetLastError() != CRT_ERROR_FILE_NOT_FOUND && GetLastError() != CRT_ERROR_PIPE_BUSY) {
      return -EIO;
    }
    WaitNamedPipeA(g_broker_pipe_name, 50);
    Sleep(10);
  }
  if (pipe == CRT_INVALID_HANDLE_VALUE) {
    return -EIO;
  }

  memset(&header, 0, sizeof(header));
  header.magic = CRT_SPAWN_BROKER_MAGIC;
  header.version = CRT_SPAWN_BROKER_VERSION;
  header.client_pid = (uint32_t)GetCurrentProcessId();
  header.want_plain_pipe = 1U;

  rc = write_exact(pipe, &header, (DWORD)sizeof(header), CRT_BROKER_IO_TIMEOUT_MS);
  if (rc == 0) {
    rc = read_exact(pipe, &response, (DWORD)sizeof(response), CRT_BROKER_IO_TIMEOUT_MS);
  }
  CloseHandle(pipe);
  if (rc != 0) {
    return rc;
  }
  if (response.magic != CRT_SPAWN_BROKER_MAGIC || response.version != CRT_SPAWN_BROKER_VERSION) {
    return -EIO;
  }
  if (response.result != 0) {
    return response.result;
  }
  if (out_read != 0) {
    *out_read = (void*)(uintptr_t)response.plain_pipe_read;
  }
  if (out_write != 0) {
    *out_write = (void*)(uintptr_t)response.plain_pipe_write;
  }
  return 0;
}

/* ---- client side: ask the broker to open a file for writing ---- */

long __crt_windows_broker_open_file(
    const char* path, unsigned long open_flags, unsigned long open_mode, void** out_handle) {
  struct crt_spawn_broker_request_header header;
  struct crt_spawn_broker_response response;
  HANDLE pipe = CRT_INVALID_HANDLE_VALUE;
  int attempt;
  int rc;
  size_t path_length = strlen(path);

  if (!g_broker_started || g_broker_pipe_name[0] == 0) {
    return -ECHILD;
  }
  if (path_length >= CRT_SPAWN_BROKER_PATH_MAX) {
    return -ENAMETOOLONG;
  }

  for (attempt = 0; attempt < 100; ++attempt) {
    pipe = CreateFileA(
        g_broker_pipe_name, CRT_GENERIC_READ | CRT_GENERIC_WRITE, 0, 0, CRT_OPEN_EXISTING,
        CRT_FILE_FLAG_OVERLAPPED, 0);
    if (pipe != CRT_INVALID_HANDLE_VALUE) {
      break;
    }
    if (GetLastError() != CRT_ERROR_FILE_NOT_FOUND && GetLastError() != CRT_ERROR_PIPE_BUSY) {
      return -EIO;
    }
    WaitNamedPipeA(g_broker_pipe_name, 50);
    Sleep(10);
  }
  if (pipe == CRT_INVALID_HANDLE_VALUE) {
    return -EIO;
  }

  memset(&header, 0, sizeof(header));
  header.magic = CRT_SPAWN_BROKER_MAGIC;
  header.version = CRT_SPAWN_BROKER_VERSION;
  header.client_pid = (uint32_t)GetCurrentProcessId();
  header.want_open_file = 1U;
  header.open_flags = (uint32_t)open_flags;
  header.open_mode = (uint32_t)open_mode;
  header.application_path_length = (uint32_t)path_length;

  rc = write_exact(pipe, &header, (DWORD)sizeof(header), CRT_BROKER_IO_TIMEOUT_MS);
  if (rc == 0) {
    rc = write_exact(pipe, path, (DWORD)path_length, CRT_BROKER_IO_TIMEOUT_MS);
  }
  if (rc == 0) {
    rc = read_exact(pipe, &response, (DWORD)sizeof(response), CRT_BROKER_IO_TIMEOUT_MS);
  }
  CloseHandle(pipe);
  if (rc != 0) {
    return rc;
  }
  if (response.magic != CRT_SPAWN_BROKER_MAGIC || response.version != CRT_SPAWN_BROKER_VERSION) {
    return -EIO;
  }
  if (response.result != 0) {
    return response.result;
  }
  if (out_handle != 0) {
    *out_handle = (void*)(uintptr_t)response.open_file_handle;
  }
  return 0;
}

/* ---- broker (server) side ---- */

/* CRT_FD_SNAPSHOT_PIPE_ENV's value is always exactly 16 hex chars, written
 * by format_hex_u64() (see syscall.c) -- an in-place fixed-width byte
 * patch, no reallocation or restructuring of the block needed. Matches
 * the entry by exact length so it cannot misfire on some unrelated
 * variable that merely starts with the same prefix. */
static void patch_fd_snapshot_pipe_env(char* block, uint32_t block_length, uint64_t new_value) {
  static const char prefix[] = CRT_FD_SNAPSHOT_PIPE_ENV "=";
  static const char digits[] = "0123456789abcdef";
  const size_t prefix_len = sizeof(prefix) - 1;
  char* p = block;
  char* end = block + block_length;

  while (p < end && *p != 0) {
    size_t entry_len = strlen(p);

    if (entry_len == prefix_len + 16 && memcmp(p, prefix, prefix_len) == 0) {
      int i;

      for (i = 15; i >= 0; --i) {
        p[prefix_len + (size_t)i] = digits[new_value & 0xfU];
        new_value >>= 4;
      }
      return;
    }
    p += entry_len + 1;
  }
}

static int broker_handle_request(HANDLE pipe) {
  struct crt_spawn_broker_request_header header;
  struct crt_spawn_broker_response response;
  static char application_path[CRT_SPAWN_BROKER_PATH_MAX];
  static char command_line[CRT_SPAWN_BROKER_CMDLINE_MAX];
  static char current_directory[CRT_SPAWN_BROKER_CWD_MAX];
  static char environment_block[CRT_SPAWN_BROKER_ENV_MAX];
  HANDLE client_process = 0;
  HANDLE dup_std_input = 0;
  HANDLE dup_std_output = 0;
  HANDLE dup_std_error = 0;
  HANDLE fd_pipe_read = 0;
  HANDLE fd_pipe_write = 0;
  HANDLE dup_fd_pipe_write_for_client = 0;
  struct crt_broker_startupinfo startup;
  struct crt_broker_process_information process;
  BOOL created;
  int rc;

  memset(&response, 0, sizeof(response));
  response.magic = CRT_SPAWN_BROKER_MAGIC;
  response.version = CRT_SPAWN_BROKER_VERSION;

  rc = read_exact(pipe, &header, (DWORD)sizeof(header), CRT_BROKER_IO_TIMEOUT_MS);
  if (rc != 0) {
    return rc;
  }
  if (header.magic != CRT_SPAWN_BROKER_MAGIC || header.version != CRT_SPAWN_BROKER_VERSION ||
      header.application_path_length >= CRT_SPAWN_BROKER_PATH_MAX ||
      header.command_line_length >= CRT_SPAWN_BROKER_CMDLINE_MAX ||
      header.current_directory_length >= CRT_SPAWN_BROKER_CWD_MAX ||
      header.environment_length >= CRT_SPAWN_BROKER_ENV_MAX) {
    response.result = -EINVAL;
    write_exact(pipe, &response, (DWORD)sizeof(response), CRT_BROKER_IO_TIMEOUT_MS);
    return -EINVAL;
  }
  if (header.application_path_length != 0 &&
      read_exact(pipe, application_path, header.application_path_length, CRT_BROKER_IO_TIMEOUT_MS) != 0) {
    return -EIO;
  }
  application_path[header.application_path_length] = 0;
  if (header.command_line_length != 0 &&
      read_exact(pipe, command_line, header.command_line_length, CRT_BROKER_IO_TIMEOUT_MS) != 0) {
    return -EIO;
  }
  command_line[header.command_line_length] = 0;
  if (header.current_directory_length != 0 &&
      read_exact(pipe, current_directory, header.current_directory_length, CRT_BROKER_IO_TIMEOUT_MS) !=
          0) {
    return -EIO;
  }
  current_directory[header.current_directory_length] = 0;
  if (header.environment_length != 0 &&
      read_exact(pipe, environment_block, header.environment_length, CRT_BROKER_IO_TIMEOUT_MS) != 0) {
    return -EIO;
  }

  client_process = OpenProcess(CRT_PROCESS_DUP_HANDLE, 0, (DWORD)header.client_pid);
  if (client_process == 0) {
    response.result = -ESRCH;
    response.windows_error = (uint32_t)GetLastError();
    write_exact(pipe, &response, (DWORD)sizeof(response), CRT_BROKER_IO_TIMEOUT_MS);
    return 0;
  }

  if (header.want_plain_pipe != 0) {
    HANDLE plain_read = 0;
    HANDLE plain_write = 0;
    HANDLE dup_plain_read = 0;
    HANDLE dup_plain_write = 0;

    if (CreatePipe(&plain_read, &plain_write, 0, 0)) {
      DuplicateHandle(
          GetCurrentProcess(), plain_read, client_process, &dup_plain_read, 0, 0,
          CRT_DUPLICATE_SAME_ACCESS);
      DuplicateHandle(
          GetCurrentProcess(), plain_write, client_process, &dup_plain_write, 0, 0,
          CRT_DUPLICATE_SAME_ACCESS);
      CloseHandle(plain_read);
      CloseHandle(plain_write);
      response.result = 0;
      response.plain_pipe_read = (uint64_t)(uintptr_t)dup_plain_read;
      response.plain_pipe_write = (uint64_t)(uintptr_t)dup_plain_write;
    } else {
      response.result = -EIO;
      response.windows_error = (uint32_t)GetLastError();
    }
    CloseHandle(client_process);
    write_exact(pipe, &response, (DWORD)sizeof(response), CRT_BROKER_IO_TIMEOUT_MS);
    return 0;
  }

  if (header.want_open_file != 0) {
    /* Mirrors __crt_sys_open()'s CreateFileA() setup in syscall.c (kept
     * in sync by hand, not shared, matching this file's existing
     * self-contained style) for exactly the cases that can reach here:
     * always GENERIC_WRITE (see __crt_sys_open()'s call site below --
     * read-only opens never route through the broker at all, since only
     * GENERIC_WRITE was ever observed to fail inside an unregistered
     * clone). application_path is already the host-translated path
     * (translate_path_for_host() ran in the client before this request
     * was ever sent), so no path translation happens here. */
    DWORD open_access = (header.open_flags & O_RDWR) == O_RDWR
                             ? (CRT_GENERIC_READ | CRT_GENERIC_WRITE)
                             : CRT_GENERIC_WRITE;
    DWORD open_disposition;
    DWORD open_file_flags = CRT_FILE_ATTRIBUTE_NORMAL;
    DWORD open_attrs;
    HANDLE opened = CRT_INVALID_HANDLE_VALUE;
    HANDLE dup_opened = 0;

    if ((header.open_flags & O_CREAT) && (header.open_flags & O_EXCL)) {
      open_disposition = CRT_CREATE_NEW;
    } else if ((header.open_flags & O_CREAT) && (header.open_flags & O_TRUNC)) {
      open_disposition = CRT_CREATE_ALWAYS;
    } else if (header.open_flags & O_CREAT) {
      open_disposition = CRT_OPEN_ALWAYS;
    } else {
      open_disposition = CRT_OPEN_EXISTING;
    }
    open_attrs = GetFileAttributesA(application_path);
    if (open_attrs != CRT_INVALID_FILE_ATTRIBUTES &&
        (open_attrs & CRT_FILE_ATTRIBUTE_DIRECTORY) != 0) {
      open_file_flags = CRT_FILE_FLAG_BACKUP_SEMANTICS;
    }
    opened = CreateFileA(
        application_path, open_access,
        CRT_FILE_SHARE_READ | CRT_FILE_SHARE_WRITE | CRT_FILE_SHARE_DELETE, 0, open_disposition,
        open_file_flags, 0);
    if (opened != CRT_INVALID_HANDLE_VALUE) {
      DuplicateHandle(
          GetCurrentProcess(), opened, client_process, &dup_opened, 0, 0,
          CRT_DUPLICATE_SAME_ACCESS);
      CloseHandle(opened);
      response.result = 0;
      response.open_file_handle = (uint64_t)(uintptr_t)dup_opened;
    } else {
      response.result = -EIO;
      response.windows_error = (uint32_t)GetLastError();
    }
    CloseHandle(client_process);
    write_exact(pipe, &response, (DWORD)sizeof(response), CRT_BROKER_IO_TIMEOUT_MS);
    return 0;
  }

  if (header.std_input != 0) {
    DuplicateHandle(
        client_process, (HANDLE)(uintptr_t)header.std_input, GetCurrentProcess(), &dup_std_input,
        0, 1, CRT_DUPLICATE_SAME_ACCESS);
  }
  if (header.std_output != 0) {
    DuplicateHandle(
        client_process, (HANDLE)(uintptr_t)header.std_output, GetCurrentProcess(), &dup_std_output,
        0, 1, CRT_DUPLICATE_SAME_ACCESS);
  }
  if (header.std_error != 0) {
    DuplicateHandle(
        client_process, (HANDLE)(uintptr_t)header.std_error, GetCurrentProcess(), &dup_std_error,
        0, 1, CRT_DUPLICATE_SAME_ACCESS);
  }
  if (header.want_fd_snapshot_pipe != 0) {
    /* CreatePipe() here, not in the client: this is exactly the API that
     * was observed to fail with ERROR_INVALID_HANDLE when called from
     * inside an unregistered clone (see crt_spawn_broker.h). The broker
     * is a normal CreateProcessA-spawned process, so it works fine here.
     * The read end stays in the broker's own process, inheritable, so
     * the CreateProcessA call below hands it to the real target directly
     * -- at whatever numeric value it has *here*, which is why that
     * value (not anything the client could have precomputed) gets
     * patched into the environment block. Only the write end needs to
     * cross back to the client, via DuplicateHandle. */
    if (CreatePipe(&fd_pipe_read, &fd_pipe_write, 0, 0)) {
      SetHandleInformation(fd_pipe_read, CRT_HANDLE_FLAG_INHERIT, CRT_HANDLE_FLAG_INHERIT);
      patch_fd_snapshot_pipe_env(
          environment_block, header.environment_length, (uint64_t)(uintptr_t)fd_pipe_read);
    } else {
      fd_pipe_read = 0;
      fd_pipe_write = 0;
    }
  }

  memset(&startup, 0, sizeof(startup));
  startup.cb = (DWORD)sizeof(startup);
  if (dup_std_input != 0 || dup_std_output != 0 || dup_std_error != 0) {
    startup.dwFlags |= CRT_STARTF_USESTDHANDLES;
    startup.hStdInput = dup_std_input;
    startup.hStdOutput = dup_std_output;
    startup.hStdError = dup_std_error;
  }
  memset(&process, 0, sizeof(process));

  created = CreateProcessA(
      application_path[0] != 0 ? application_path : 0,
      command_line,
      0,
      0,
      /* bInheritHandles */ 1,
      (DWORD)header.creation_flags,
      header.environment_length != 0 ? environment_block : 0,
      header.current_directory_length != 0 ? current_directory : 0,
      &startup,
      &process);

  if (dup_std_input != 0) {
    CloseHandle(dup_std_input);
  }
  if (dup_std_output != 0) {
    CloseHandle(dup_std_output);
  }
  if (dup_std_error != 0) {
    CloseHandle(dup_std_error);
  }
  if (fd_pipe_read != 0) {
    /* The real target inherited its own independent copy; the broker's
     * copy is no longer needed regardless of whether CreateProcessA
     * succeeded. */
    CloseHandle(fd_pipe_read);
  }

  if (!created) {
    if (fd_pipe_write != 0) {
      CloseHandle(fd_pipe_write);
    }
    response.result = -EIO;
    response.windows_error = (uint32_t)GetLastError();
    write_exact(pipe, &response, (DWORD)sizeof(response), CRT_BROKER_IO_TIMEOUT_MS);
    CloseHandle(client_process);
    return 0;
  }

  if (fd_pipe_write != 0) {
    DuplicateHandle(
        GetCurrentProcess(), fd_pipe_write, client_process, &dup_fd_pipe_write_for_client, 0, 0,
        CRT_DUPLICATE_SAME_ACCESS);
    CloseHandle(fd_pipe_write);
  }

  {
    HANDLE dup_process = 0;
    HANDLE dup_thread = 0;

    DuplicateHandle(
        GetCurrentProcess(), process.hProcess, client_process, &dup_process, 0, 0,
        CRT_DUPLICATE_SAME_ACCESS);
    DuplicateHandle(
        GetCurrentProcess(), process.hThread, client_process, &dup_thread, 0, 0,
        CRT_DUPLICATE_SAME_ACCESS);
    CloseHandle(process.hProcess);
    CloseHandle(process.hThread);
    CloseHandle(client_process);

    response.result = 0;
    response.process_id = (uint32_t)process.dwProcessId;
    response.process_handle = (uint64_t)(uintptr_t)dup_process;
    response.thread_handle = (uint64_t)(uintptr_t)dup_thread;
    response.fd_snapshot_pipe_write = (uint64_t)(uintptr_t)dup_fd_pipe_write_for_client;
  }
  write_exact(pipe, &response, (DWORD)sizeof(response), CRT_BROKER_IO_TIMEOUT_MS);
  return 0;
}

static HANDLE create_broker_pipe_instance(const char* pipe_name) {
  return CreateNamedPipeA(
      pipe_name, CRT_PIPE_ACCESS_DUPLEX | CRT_FILE_FLAG_OVERLAPPED,
      CRT_PIPE_TYPE_BYTE | CRT_PIPE_READMODE_BYTE | CRT_PIPE_WAIT, CRT_PIPE_UNLIMITED_INSTANCES, 65536,
      65536, 0, 0);
}

/* A pipe instance opened with FILE_FLAG_OVERLAPPED (required so
 * broker_handle_request()'s read_exact()/write_exact() calls on it can
 * time out) must itself be connected with an OVERLAPPED, or the call
 * fails outright. Unlike the read/write timeouts, this one waits
 * indefinitely: an idle broker with no pending request is supposed to
 * block here -- that is its entire job between requests, not a hang. */
static BOOL connect_broker_pipe_instance(HANDLE pipe) {
  struct crt_broker_overlapped ov;
  HANDLE ev;
  BOOL result;

  ev = CreateEventA(0, 1, 0, 0);
  if (ev == 0) {
    return 0;
  }
  memset(&ov, 0, sizeof(ov));
  ov.hEvent = ev;
  if (ConnectNamedPipe(pipe, &ov)) {
    result = 1;
  } else {
    DWORD err = GetLastError();

    if (err == CRT_ERROR_PIPE_CONNECTED) {
      result = 1;
    } else if (err == CRT_ERROR_IO_PENDING) {
      DWORD got = 0;

      result = WaitForSingleObject(ev, CRT_INFINITE) == CRT_WAIT_OBJECT_0 &&
               GetOverlappedResult(pipe, &ov, &got, 1);
    } else {
      result = 0;
    }
  }
  CloseHandle(ev);
  return result;
}

void __crt_windows_spawn_broker_main(void) {
  const char* pipe_name = getenv(CRT_SPAWN_BROKER_PIPE_ENV);
  HANDLE pipe;

  if (pipe_name == 0 || pipe_name[0] == 0) {
    ExitProcess(1);
  }
  pipe = create_broker_pipe_instance(pipe_name);
  if (pipe == CRT_INVALID_HANDLE_VALUE) {
    ExitProcess(1);
  }
  for (;;) {
    HANDLE next_pipe;
    BOOL connected;

    connected = connect_broker_pipe_instance(pipe);
    /* Create the *next* waiting instance right after accepting this
     * connection, before servicing it -- not after, at the top of the
     * next loop iteration. broker_handle_request() below can run a full
     * CreateProcessA call and take a while; with the instance created
     * only afterward, there was a real window (this request's full
     * service time, plus the disconnect/close/recreate cleanup) with
     * zero pending instances on this pipe name. Any client racing to
     * connect during that window saw ERROR_FILE_NOT_FOUND -- observed in
     * practice as mksh's "can't create pipe - try again" from a pipe()
     * call unlucky enough to land in the gap, in the middle of a long
     * autoconf run doing many pipe()s back to back. */
    next_pipe = create_broker_pipe_instance(pipe_name);
    if (connected) {
      broker_handle_request(pipe);
      /* WriteFile() returning success only means the response bytes made
       * it into the kernel's pipe buffer, not that the client has
       * actually read them yet. DisconnectNamedPipe() tears the
       * connection down immediately regardless, discarding whatever the
       * client hasn't read yet -- a well-known named-pipe pitfall.
       * FlushFileBuffers() on a named pipe server handle blocks until the
       * client has read everything, so the disconnect below can never
       * race the client's read. Without this, the client's read_exact()
       * intermittently failed with ERROR_PIPE_NOT_CONNECTED (233) right
       * after a successful write_exact() on the server side -- confirmed
       * with per-PID debug logging showing exactly that sequence. */
      FlushFileBuffers(pipe);
    }
    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
    if (next_pipe == CRT_INVALID_HANDLE_VALUE) {
      ExitProcess(1);
    }
    pipe = next_pipe;
  }
}
