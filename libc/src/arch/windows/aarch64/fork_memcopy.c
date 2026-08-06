/* Windows aarch64 Cygwin/MSYS-style fork() replacement. See
 * libc/include/private/crt_fork_memcopy.h and
 * docs/windows_fork_emulation.md ("Spawn Broker Retired: Moving To Full
 * Cygwin/MSYS-Style fork()") for the design and the Phase B measurements
 * this depends on.
 *
 * Deliberately self-contained (own Win32 declarations, no <windows.h>,
 * no shared statics with syscall.c) -- same reasoning as the retired
 * spawn_broker.c: this is one coherent subsystem, not a scattering of
 * syscall backends.
 *
 * aarch64-only: the CONTEXT layout and mitigation-policy verification
 * below are ARM64-specific and have only been tested on real Windows
 * aarch64 hardware. x86_64 keeps the existing RtlCloneUserProcess-based
 * __crt_sys_fork() (syscall.c) until this is ported and verified there --
 * see TODO.md. */
#include <errno.h>
#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <private/crt_fork_memcopy.h>
#include <private/crt_tls.h>

typedef void* HANDLE;
typedef unsigned long DWORD;
typedef unsigned short WORD;
typedef int BOOL;
typedef unsigned long long DWORD64;
typedef unsigned long long SIZE_T;
typedef unsigned long long ULONG_PTR;

#define CRT_WINAPI

struct crt_startupinfoex {
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

struct crt_process_information {
  HANDLE hProcess;
  HANDLE hThread;
  DWORD dwProcessId;
  DWORD dwThreadId;
};

typedef struct {
  unsigned long long Low;
  long long High;
} crt_neon128;

/* ARM64_NT_CONTEXT (winnt.h), transcribed field-for-field and verified
 * against real hardware via the Phase C proof-of-concept probe (see
 * TODO.md): X[] holds X0..X28,Fp,Lr as X[0..30] per the SDK's
 * DUMMYUNIONNAME. */
struct __attribute__((aligned(16))) crt_context_arm64 {
  DWORD ContextFlags;  /* +0x000 */
  DWORD Cpsr;           /* +0x004 */
  DWORD64 X[31];         /* +0x008: X0..X28, Fp(X29), Lr(X30) */
  DWORD64 Sp;            /* +0x100 */
  DWORD64 Pc;            /* +0x108 */
  crt_neon128 V[32];     /* +0x110 */
  DWORD Fpcr;            /* +0x310 */
  DWORD Fpsr;            /* +0x314 */
  DWORD Bcr[8];          /* +0x318 */
  DWORD64 Bvr[8];        /* +0x338 */
  DWORD Wcr[2];          /* +0x378 */
  DWORD64 Wvr[2];        /* +0x380 */
};

__declspec(dllimport) DWORD CRT_WINAPI GetModuleFileNameA(HANDLE hModule, char* lpFilename, DWORD nSize);
__declspec(dllimport) HANDLE CRT_WINAPI GetModuleHandleA(const char* lpModuleName);
__declspec(dllimport) BOOL CRT_WINAPI CreateProcessA(
    const char* lpApplicationName,
    char* lpCommandLine,
    void* lpProcessAttributes,
    void* lpThreadAttributes,
    BOOL bInheritHandles,
    DWORD dwCreationFlags,
    void* lpEnvironment,
    const char* lpCurrentDirectory,
    struct crt_startupinfoex* lpStartupInfo,
    struct crt_process_information* lpProcessInformation);
__declspec(dllimport) BOOL CRT_WINAPI TerminateProcess(HANDLE hProcess, unsigned int uExitCode);
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
__declspec(dllimport) BOOL CRT_WINAPI GetThreadContext(HANDLE hThread, struct crt_context_arm64* lpContext);
__declspec(dllimport) BOOL CRT_WINAPI SetThreadContext(HANDLE hThread, const struct crt_context_arm64* lpContext);
__declspec(dllimport) DWORD CRT_WINAPI ResumeThread(HANDLE hThread);
__declspec(dllimport) void* CRT_WINAPI VirtualAllocEx(
    HANDLE hProcess, void* lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect);
__declspec(dllimport) BOOL CRT_WINAPI WriteProcessMemory(
    HANDLE hProcess, void* lpBaseAddress, const void* lpBuffer, SIZE_T nSize, SIZE_T* lpNumberOfBytesWritten);
struct crt_memory_basic_information {
  void* BaseAddress;
  void* AllocationBase;
  DWORD AllocationProtect;
  WORD PartitionId;
  SIZE_T RegionSize;
  DWORD State;
  DWORD Protect;
  DWORD Type;
};
__declspec(dllimport) SIZE_T CRT_WINAPI VirtualQueryEx(
    HANDLE hProcess, const void* lpAddress, struct crt_memory_basic_information* lpBuffer, SIZE_T dwLength);
__declspec(dllimport) DWORD CRT_WINAPI GetLastError(void);

#define CRT_EXTENDED_STARTUPINFO_PRESENT 0x00080000UL
#define CRT_CREATE_SUSPENDED 0x00000004UL
#define CRT_PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY 0x00020007ULL
#define CRT_MITIGATION_BOTTOM_UP_ASLR_ALWAYS_OFF (0x2ULL << 16)
#define CRT_MITIGATION_HIGH_ENTROPY_ASLR_ALWAYS_OFF (0x2ULL << 20)
#define CRT_MEM_COMMIT 0x1000UL
#define CRT_MEM_RESERVE 0x2000UL
#define CRT_PAGE_READWRITE 0x04UL
#define CRT_CONTEXT_ARM64 0x00400000UL
#define CRT_CONTEXT_ARM64_CONTROL (CRT_CONTEXT_ARM64 | 0x1UL)
#define CRT_CONTEXT_ARM64_INTEGER (CRT_CONTEXT_ARM64 | 0x2UL)
#define CRT_CONTEXT_ARM64_FULL (CRT_CONTEXT_ARM64_CONTROL | CRT_CONTEXT_ARM64_INTEGER | (CRT_CONTEXT_ARM64 | 0x4UL))
#define CRT_TLS_OUT_OF_INDEXES 0xffffffffUL

/* malloc.c accessors -- see the comment there. Forward-declared here
 * rather than via a header, matching this project's existing convention
 * for narrow, single-purpose cross-file hooks (e.g.
 * __crt_malloc_after_fork_child() in process.c). These walk OS-level
 * mmap()/VirtualAlloc() region boundaries, NOT malloc.c's internal
 * block_header split chain -- the latter can subdivide one 64KB region
 * into several non-64KB-aligned sub-blocks, and VirtualAllocEx() requires
 * lpAddress to be allocation-granularity-aligned. */
int __crt_malloc_os_region_count(void);
void* __crt_malloc_os_region_base(int index);
size_t __crt_malloc_os_region_size(int index);

/* Windows ARM64 (unlike Linux/generic AArch64, which uses TPIDRRO_EL0)
 * dedicates general register X18 as the platform register holding the
 * current thread's TEB pointer -- the ARM64 analogue of the GS segment
 * base on x64. Requires -ffixed-x18 globally (CMakeLists.txt) so no other
 * code in the process treats X18 as an ordinary scratch register. */
static void* read_teb(void) {
  void* teb;
  __asm__ volatile("mov %0, x18" : "=r"(teb));
  return teb;
}

/* Writes [addr, addr+size) from *this* process into `process` at the
 * identical address. Used for regions the loader has already mapped in
 * the child by the time CreateProcessA() returns (the image's own
 * sections) -- no VirtualAllocEx() needed or wanted there. */
static int write_region_into_child(HANDLE process, void* addr, size_t size) {
  SIZE_T written = 0;

  if (size == 0) {
    return 0;
  }
  if (!WriteProcessMemory(process, addr, addr, (SIZE_T)size, &written) || written != (SIZE_T)size) {
    return -1;
  }
  return 0;
}

/* Copies [dest_addr, dest_addr+size) from *this* process into `process`
 * at the identical address, committing it first since nothing is mapped
 * there yet in a CREATE_SUSPENDED child that has not executed a single
 * instruction (unlike the image's own sections -- see
 * write_region_into_child() above -- the heap, stack, and TLS context
 * block are runtime allocations the loader knows nothing about). Returns
 * 0 on success, -1 on failure. Requires addr to be allocation-
 * granularity-aligned (64KB) -- true for every caller here (mmap()/
 * VirtualAlloc() region bases). */
static int copy_region_into_child(HANDLE process, void* addr, size_t size) {
  if (size == 0) {
    return 0;
  }
  if (VirtualAllocEx(process, addr, (SIZE_T)size, CRT_MEM_COMMIT | CRT_MEM_RESERVE, CRT_PAGE_READWRITE) == 0) {
    return -1;
  }
  return write_region_into_child(process, addr, size);
}

/* Like copy_region_into_child(), but MEM_COMMIT only (no MEM_RESERVE) --
 * for the thread stack specifically, which the loader already reserves
 * as one block at thread creation (unlike heap/TLS regions, which are
 * fresh, previously-untouched VirtualAlloc(0, ...) reservations). Only
 * needs page (4KB) alignment, not allocation-granularity (64KB)
 * alignment, since it commits within an existing reservation rather than
 * creating a new one. */
static int commit_region_into_child(HANDLE process, void* addr, size_t size) {
  if (size == 0) {
    return 0;
  }
  if (VirtualAllocEx(process, addr, (SIZE_T)size, CRT_MEM_COMMIT, CRT_PAGE_READWRITE) == 0) {
    return -1;
  }
  return write_region_into_child(process, addr, size);
}

/* Walks the loaded image's PE section table for writable sections
 * (.data/.bss and anything else IMAGE_SCN_MEM_WRITE) and copies each one
 * into the child. Raw offset-based parsing instead of the full
 * IMAGE_DOS_HEADER/IMAGE_NT_HEADERS64/IMAGE_SECTION_HEADER struct
 * definitions -- the PE format's layout at these offsets has been stable
 * for decades and this project otherwise avoids pulling in <windows.h>. */
static int copy_image_data_sections(HANDLE process) {
  unsigned char* base = (unsigned char*)GetModuleHandleA(0);
  unsigned char* nt_headers;
  unsigned char* file_header;
  unsigned char* optional_header;
  unsigned char* section_table;
  unsigned short number_of_sections;
  unsigned short size_of_optional_header;
  int i;

  if (base == 0 || *(unsigned short*)(base + 0) != 0x5A4D /* 'MZ' */) {
    return -1;
  }
  nt_headers = base + *(int*)(base + 0x3C);
  if (*(unsigned int*)(nt_headers + 0) != 0x00004550U /* 'PE\0\0' */) {
    return -1;
  }
  file_header = nt_headers + 4;
  number_of_sections = *(unsigned short*)(file_header + 2);
  size_of_optional_header = *(unsigned short*)(file_header + 0x10);
  optional_header = file_header + 0x14;
  section_table = optional_header + size_of_optional_header;

  for (i = 0; i < (int)number_of_sections; ++i) {
    unsigned char* sh = section_table + (size_t)i * 40;
    unsigned int characteristics = *(unsigned int*)(sh + 0x24);
    unsigned int virtual_size = *(unsigned int*)(sh + 0x08);
    unsigned int virtual_address = *(unsigned int*)(sh + 0x0C);
    unsigned int raw_size = *(unsigned int*)(sh + 0x10);

    if ((characteristics & 0x80000000U) == 0) { /* IMAGE_SCN_MEM_WRITE */
      continue;
    }
    if (virtual_size == 0) {
      virtual_size = raw_size;
    }
    if (virtual_size == 0) {
      continue;
    }
    if (write_region_into_child(process, base + virtual_address, virtual_size) != 0) {
      return -1;
    }
  }
  return 0;
}

static int copy_heap_chunks(HANDLE process) {
  int count = __crt_malloc_os_region_count();
  int i;

  for (i = 0; i < count; ++i) {
    void* base = __crt_malloc_os_region_base(i);
    size_t size = __crt_malloc_os_region_size(i);

    if (copy_region_into_child(process, base, size) != 0) {
      return -1;
    }
  }
  return 0;
}

/* Copies the calling thread's entire committed stack, not just the
 * currently-used portion -- simpler and safer than computing exactly how
 * much is "live", at the cost of copying a bit more than strictly
 * necessary (bounded by the thread's stack reservation). Known
 * limitation: this does not preserve the guard page beyond whatever was
 * already committed, so the child cannot auto-grow its stack past what
 * the parent had committed at fork() time.
 *
 * The copy target must be clamped to the CHILD's own stack reservation,
 * not the parent's: a CREATE_SUSPENDED child's initial thread has not
 * run a single instruction yet, so its TEB.StackLimit still sits at
 * whatever the loader committed by default (often just one page above
 * the guard page) -- nowhere near matching the parent's StackLimit,
 * which has receded from real, deep call-stack usage by the time
 * fork() actually runs. VirtualQueryEx() on the child's own initial Sp
 * reports the AllocationBase of its *entire* stack reservation, which is
 * what actually needs to be checked/committed against -- not what the
 * parent's TEB happens to say. */
static int copy_current_stack(HANDLE process, DWORD64 child_initial_sp, void** out_low, void** out_high) {
  unsigned char* teb = (unsigned char*)read_teb();
  void* stack_base = *(void**)(teb + 0x08);
  void* stack_limit = *(void**)(teb + 0x10);
  struct crt_memory_basic_information mbi;
  void* copy_low = stack_limit;
  size_t size;

  memset(&mbi, 0, sizeof(mbi));
  if (VirtualQueryEx(process, (void*)(ULONG_PTR)child_initial_sp, &mbi, sizeof(mbi)) == sizeof(mbi) &&
      mbi.AllocationBase != 0 && (uintptr_t)mbi.AllocationBase > (uintptr_t)copy_low) {
    /* Parent has used more stack than the child's default reservation
     * covers -- clamp to what is actually available. Known limitation:
     * anything the parent had below the child's reservation base is not
     * copied, so a fork() from an unusually deep call stack may resume
     * with a truncated view of frames below this point. */
    copy_low = mbi.AllocationBase;
  }

  size = (size_t)((uintptr_t)stack_base - (uintptr_t)copy_low);
  *out_low = copy_low;
  *out_high = stack_base;
  return commit_region_into_child(process, copy_low, size);
}

long __crt_windows_memcopy_fork(unsigned long* out_child_pid, void** out_child_process) {
  jmp_buf resume;
  char self_path[1024];
  DWORD path_len;
  SIZE_T attr_list_size = 0;
  void* attr_list = 0;
  DWORD64 policy = CRT_MITIGATION_BOTTOM_UP_ASLR_ALWAYS_OFF | CRT_MITIGATION_HIGH_ENTROPY_ASLR_ALWAYS_OFF;
  struct crt_startupinfoex si;
  struct crt_process_information pi;
  char cmdline[1200];
  struct crt_context_arm64 ctx;
  crt_thread_context* tls_context;
  long* tls_index_ptr;
  void* stack_low;
  void* stack_high;

  if (setjmp(resume) != 0) {
    /* Resumed in the child: __crt_windows_memcopy_fork() itself never
     * actually ran again here -- the parent directly constructed this
     * process's initial CONTEXT below (register-for-register, from the
     * exact values setjmp() just captured) and resumed the thread
     * straight into this point, without the child's own code ever
     * running mainCRTStartup or reading any resume state from memory.
     * Every frame between here and fork()'s original caller is part of
     * the copied stack range and resumes exactly as the parent left it. */
    return 0;
  }

  memset(&pi, 0, sizeof(pi));

  path_len = GetModuleFileNameA(0, self_path, (DWORD)sizeof(self_path));
  if (path_len == 0 || path_len >= sizeof(self_path)) {
    return -EAGAIN;
  }

  InitializeProcThreadAttributeList(0, 1, 0, &attr_list_size);
  attr_list = malloc((size_t)attr_list_size);
  if (attr_list == 0) {
    return -ENOMEM;
  }
  if (!InitializeProcThreadAttributeList(attr_list, 1, 0, &attr_list_size) ||
      !UpdateProcThreadAttribute(attr_list, 0, CRT_PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY,
                                  &policy, sizeof(policy), 0, 0)) {
    free(attr_list);
    return -EAGAIN;
  }

  memset(&si, 0, sizeof(si));
  si.cb = sizeof(si);
  si.lpAttributeList = attr_list;
  snprintf(cmdline, sizeof(cmdline), "\"%s\"", self_path);

  if (!CreateProcessA(self_path, cmdline, 0, 0, 1, CRT_EXTENDED_STARTUPINFO_PRESENT | CRT_CREATE_SUSPENDED, 0, 0,
                       &si, &pi)) {
    DeleteProcThreadAttributeList(attr_list);
    free(attr_list);
    return -EAGAIN;
  }
  DeleteProcThreadAttributeList(attr_list);
  free(attr_list);

  /* Fetched early (before any copying) so copy_current_stack() can query
   * the child's own stack reservation bounds via its initial Sp -- the
   * parent's own TEB.StackLimit is not a valid stand-in (see the comment
   * on copy_current_stack()). */
  memset(&ctx, 0, sizeof(ctx));
  ctx.ContextFlags = CRT_CONTEXT_ARM64_FULL;
  if (!GetThreadContext(pi.hThread, &ctx)) {
    TerminateProcess(pi.hProcess, (unsigned int)-1);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return -EAGAIN;
  }

  /* Order matters: every destination region below must be committed and
   * written *before* SetThreadContext()/ResumeThread(), since the child
   * runs immediately and unsupervised as soon as it is resumed. */
  if (copy_heap_chunks(pi.hProcess) != 0 || copy_image_data_sections(pi.hProcess) != 0 ||
      copy_current_stack(pi.hProcess, ctx.Sp, &stack_low, &stack_high) != 0) {
    TerminateProcess(pi.hProcess, (unsigned int)-1);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return -EAGAIN;
  }

  /* The current thread's own crt_thread_context block (tls.c) is a
   * standalone VirtualAlloc(), not part of the heap/stack/image regions
   * above -- copy it explicitly. */
  tls_context = __crt_thread_get_current();
  if (copy_region_into_child(pi.hProcess, tls_context, sizeof(*tls_context)) != 0) {
    TerminateProcess(pi.hProcess, (unsigned int)-1);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return -EAGAIN;
  }

  /* The copied thread_tls_index (part of the .data copy above) is only
   * valid in the PARENT's Win32 TLS bitmap. Reset it to
   * CRT_TLS_OUT_OF_INDEXES in the child so __crt_atfork_child() (called
   * by fork() in process.c right after this returns 0) allocates a fresh,
   * legitimate index there instead of colliding with whatever the child
   * process itself allocates later. */
  tls_index_ptr = __crt_windows_tls_index_ptr();
  {
    DWORD out_of_indexes = (DWORD)CRT_TLS_OUT_OF_INDEXES;
    SIZE_T written = 0;
    if (!WriteProcessMemory(pi.hProcess, tls_index_ptr, &out_of_indexes, sizeof(out_of_indexes), &written) ||
        written != sizeof(out_of_indexes)) {
      TerminateProcess(pi.hProcess, (unsigned int)-1);
      CloseHandle(pi.hThread);
      CloseHandle(pi.hProcess);
      return -EAGAIN;
    }
  }

  /* Directly construct the child's initial CONTEXT from the jmp_buf
   * setjmp() just captured, instead of redirecting Pc to a trampoline
   * that would call longjmp() itself. Equivalent in principle, but does
   * not depend on the child's own code correctly reading resume state
   * back out of copied memory at just the right moment -- the parent
   * already has every value it needs in its own registers/locals right
   * here, so it sets them directly. Offsets match
   * libc/src/arch/windows/aarch64/setjmp.S's store layout exactly:
   * x19-x28 (buf[0..9]), fp/lr (buf[10..11]), sp (buf[12]),
   * d8-d15 (buf[13..20]). */
  ctx.X[19] = (DWORD64)resume[0];
  ctx.X[20] = (DWORD64)resume[1];
  ctx.X[21] = (DWORD64)resume[2];
  ctx.X[22] = (DWORD64)resume[3];
  ctx.X[23] = (DWORD64)resume[4];
  ctx.X[24] = (DWORD64)resume[5];
  ctx.X[25] = (DWORD64)resume[6];
  ctx.X[26] = (DWORD64)resume[7];
  ctx.X[27] = (DWORD64)resume[8];
  ctx.X[28] = (DWORD64)resume[9];
  ctx.X[29] = (DWORD64)resume[10]; /* Fp */
  ctx.Pc = (DWORD64)resume[11];    /* Lr -- becomes the resume PC directly */
  ctx.Sp = (DWORD64)resume[12];
  ctx.V[8].Low = (DWORD64)resume[13];
  ctx.V[9].Low = (DWORD64)resume[14];
  ctx.V[10].Low = (DWORD64)resume[15];
  ctx.V[11].Low = (DWORD64)resume[16];
  ctx.V[12].Low = (DWORD64)resume[17];
  ctx.V[13].Low = (DWORD64)resume[18];
  ctx.V[14].Low = (DWORD64)resume[19];
  ctx.V[15].Low = (DWORD64)resume[20];
  /* setjmp()'s own return value convention (setjmp.S) for a nonzero
   * longjmp value: X0 = 1. */
  ctx.X[0] = 1;
  if (!SetThreadContext(pi.hThread, &ctx)) {
    TerminateProcess(pi.hProcess, (unsigned int)-1);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return -EAGAIN;
  }

  ResumeThread(pi.hThread);
  CloseHandle(pi.hThread);

  *out_child_pid = (unsigned long)pi.dwProcessId;
  *out_child_process = pi.hProcess;
  return 1;
}
