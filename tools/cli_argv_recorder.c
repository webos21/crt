/* Producer-side helper of tools/export_cli_toolchain.py on Windows: stands in for the host C/C++ compiler,
 * writes its argv as a JSON array (UTF-8) to %CAPTURE_ARGV% and exits 0. `-print-resource-dir` is forwarded to the
 * real compiler named by %REAL_CLANG% so the wrapper sees the genuine value. Never part of an SDK or the CLI.
 *
 * Freestanding on purpose: only kernel32.dll and shell32.dll, no C runtime, vcruntime or Windows/MSVC headers, so it
 * builds with a bare LLVM on every Windows architecture. A Windows/arm64 host with the Windows SDK but without the
 * Visual C++ ARM64 libraries (libcmt.lib) cannot link a hosted program. Declarations are written out for the
 * handful of functions used; the types are the LLP64 ones shared by x64 and arm64. */
typedef unsigned long DWORD;
typedef int BOOL;
typedef void* HANDLE;
typedef unsigned short WCHAR;
typedef unsigned short WORD;

typedef struct {
  DWORD cb;
  char* lpReserved;
  char* lpDesktop;
  char* lpTitle;
  DWORD dwX, dwY, dwXSize, dwYSize, dwXCountChars, dwYCountChars, dwFillAttribute, dwFlags;
  WORD wShowWindow, cbReserved2;
  unsigned char* lpReserved2;
  HANDLE hStdInput, hStdOutput, hStdError;
} STARTUPINFOA;

typedef struct {
  HANDLE hProcess, hThread;
  DWORD dwProcessId, dwThreadId;
} PROCESS_INFORMATION;

__declspec(dllimport) const WCHAR* GetCommandLineW(void);
__declspec(dllimport) WCHAR** CommandLineToArgvW(const WCHAR*, int*);
__declspec(dllimport) int WideCharToMultiByte(unsigned, DWORD, const WCHAR*, int, char*, int, const char*, BOOL*);
__declspec(dllimport) DWORD GetEnvironmentVariableA(const char*, char*, DWORD);
__declspec(dllimport) HANDLE CreateFileA(const char*, DWORD, DWORD, void*, DWORD, DWORD, HANDLE);
__declspec(dllimport) BOOL WriteFile(HANDLE, const void*, DWORD, DWORD*, void*);
__declspec(dllimport) BOOL CloseHandle(HANDLE);
__declspec(dllimport) HANDLE GetStdHandle(DWORD);
__declspec(dllimport) BOOL CreateProcessA(const char*, char*, void*, void*, BOOL, DWORD, void*, const char*,
                                         STARTUPINFOA*, PROCESS_INFORMATION*);
__declspec(dllimport) DWORD WaitForSingleObject(HANDLE, DWORD);
__declspec(dllimport) BOOL GetExitCodeProcess(HANDLE, DWORD*);
__declspec(dllimport) void ExitProcess(unsigned);

#define CP_UTF8 65001u
#define GENERIC_WRITE 0x40000000UL
#define CREATE_ALWAYS 2
#define FILE_ATTRIBUTE_NORMAL 0x80
#define STARTF_USESTDHANDLES 0x100
#define INFINITE 0xFFFFFFFFUL
#define STD_INPUT_HANDLE ((DWORD)-10)
#define STD_OUTPUT_HANDLE ((DWORD)-11)
#define STD_ERROR_HANDLE ((DWORD)-12)

/* A command line is at most 32,767 UTF-16 units; its UTF-8 form needs at most 3 bytes per unit and a JSON escape at
 * most 6 bytes per byte. Static storage keeps the stack small (no __chkstk) and zero-initialized. */
static char converted[32768 * 3];
static char json[(32768 * 3 + 2) * 6 + 16];
static char path[32768];
static char capture[32768];
static char command[32768 + 8];
static STARTUPINFOA startup;
static PROCESS_INFORMATION process;
static unsigned used;

static void put(char c) { json[used++] = c; }

static void put_string(const char* text) {
  static const char hex[] = "0123456789abcdef";
  put('"');
  for (; *text; ++text) {
    unsigned char c = (unsigned char)*text;
    if (c == '"' || c == '\\') { put('\\'); put((char)c); }
    else if (c < 0x20) { put('\\'); put('u'); put('0'); put('0'); put(hex[c >> 4]); put(hex[c & 15]); }
    else put((char)c);
  }
  put('"');
}

static int same(const char* a, const char* b) {
  while (*a && *a == *b) { ++a; ++b; }
  return *a == *b;
}

void recorder_main(void) {
  int argc = 0;
  WCHAR** wide = CommandLineToArgvW(GetCommandLineW(), &argc);
  if (!wide) ExitProcess(5);
  put('[');
  for (int i = 1; i < argc; ++i) {
    if (!WideCharToMultiByte(CP_UTF8, 0, wide[i], -1, converted, (int)sizeof converted, 0, 0)) ExitProcess(5);
    if (i > 1) put(',');
    put_string(converted);
  }
  put(']');
  if (argc == 2 && same(converted, "-print-resource-dir") && GetEnvironmentVariableA("REAL_CLANG", path, sizeof path)) {
    unsigned n = 0;
    command[n++] = '"';
    for (unsigned i = 0; path[i]; ++i) command[n++] = path[i];
    command[n++] = '"';
    for (const char* tail = " -print-resource-dir"; *tail; ++tail) command[n++] = *tail;
    startup.cb = sizeof startup;
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    if (!CreateProcessA(path, command, 0, 0, 1, 0, 0, 0, &startup, &process)) ExitProcess(6);
    DWORD status = 7;
    WaitForSingleObject(process.hProcess, INFINITE);
    GetExitCodeProcess(process.hProcess, &status);
    ExitProcess(status);
  }
  if (!GetEnvironmentVariableA("CAPTURE_ARGV", capture, sizeof capture)) ExitProcess(2);
  HANDLE out = CreateFileA(capture, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
  if (out == (HANDLE)-1) ExitProcess(3);
  DWORD written = 0;
  BOOL ok = WriteFile(out, json, used, &written, 0) && written == used;
  ok = CloseHandle(out) && ok;
  ExitProcess(ok ? 0 : 4);
}
