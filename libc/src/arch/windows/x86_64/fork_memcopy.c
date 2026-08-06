/* Windows x86_64 Cygwin/MSYS-style fork() replacement. See
 * libc/include/private/crt_fork_memcopy.h,
 * libc/src/arch/windows/aarch64/fork_memcopy.c (the original port this
 * mirrors), and docs/windows_fork_emulation.md ("Spawn Broker Retired") for
 * the design and the Phase B measurements this depends on.
 *
 * Deliberately self-contained (own Win32 declarations, no <windows.h>,
 * no shared statics with syscall.c) -- same reasoning as the aarch64
 * version and the retired spawn_broker.c: this is one coherent subsystem,
 * not a scattering of syscall backends.
 *
 * Everything here is a straight port of the aarch64 file, with three
 * things actually specific to this architecture:
 *   1. The CONTEXT layout is CONTEXT_AMD64 (winnt.h), not ARM64_NT_CONTEXT
 *      -- transcribed field-for-field below and cross-checked directly
 *      against a real Windows SDK winnt.h (P1Home.. through
 *      LastExceptionFromRip, XSAVE_FORMAT/M128A included) rather than
 *      from memory, given how costly a wrong offset would be here.
 *   2. Windows x64 has no equivalent of aarch64's reserved X18 platform
 *      register for the current thread's TEB pointer -- the TEB is
 *      instead reached via the GS segment base directly, and
 *      TEB+0x30 happens to hold a self-pointer (NT_TIB.Self, a carry-over
 *      from the 32-bit FS:0x18 convention), so `%gs:0x30` alone yields the
 *      TEB address with no extra register needed.
 *   3. setjmp()'s save layout (libc/src/arch/windows/x86_64/setjmp.S) is
 *      the standard Windows x64 callee-saved set: rbx/rbp/rdi/rsi/
 *      r12-r15/rsp/return-address, plus xmm6-xmm15 -- and unlike aarch64
 *      (which only preserves the low 64 bits of v8-v15 per AAPCS64), the
 *      Windows x64 ABI preserves xmm6-xmm15 in full (128 bits each), so
 *      both halves of each are copied.
 *
 * NT_TIB (ExceptionList/StackBase/StackLimit at the same +0x00/+0x08/+0x10
 * offsets as aarch64 uses) and the PE section-table/heap/stack/TLS copy
 * logic below are architecture-independent and unchanged from the aarch64
 * version. */
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
typedef unsigned char BYTE;
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

/* M128A (winnt.h): a 128-bit SSE register value. */
typedef struct __attribute__((aligned(16))) {
  unsigned long long Low;
  long long High;
} crt_m128a;

/* XSAVE_FORMAT / XMM_SAVE_AREA32 (winnt.h, _WIN64 branch), the classic
 * FXSAVE layout -- 512 bytes total. */
struct __attribute__((aligned(16))) crt_xsave_format {
  WORD ControlWord;    /* +0x000 */
  WORD StatusWord;     /* +0x002 */
  BYTE TagWord;         /* +0x004 */
  BYTE Reserved1;       /* +0x005 */
  WORD ErrorOpcode;    /* +0x006 */
  DWORD ErrorOffset;    /* +0x008 */
  WORD ErrorSelector;  /* +0x00C */
  WORD Reserved2;      /* +0x00E */
  DWORD DataOffset;     /* +0x010 */
  WORD DataSelector;   /* +0x014 */
  WORD Reserved3;      /* +0x016 */
  DWORD MxCsr;          /* +0x018 */
  DWORD MxCsr_Mask;     /* +0x01C */
  crt_m128a FloatRegisters[8]; /* +0x020 */
  crt_m128a XmmRegisters[16];  /* +0x0A0 */
  BYTE Reserved4[96];   /* +0x1A0, ends at +0x200 (512 bytes total) */
};

/* CONTEXT (winnt.h, AMD64): transcribed field-for-field and cross-checked
 * directly against a real Windows SDK winnt.h. sizeof == 1232 bytes. */
struct __attribute__((aligned(16))) crt_context_amd64 {
  DWORD64 P1Home;  /* +0x000 */
  DWORD64 P2Home;  /* +0x008 */
  DWORD64 P3Home;  /* +0x010 */
  DWORD64 P4Home;  /* +0x018 */
  DWORD64 P5Home;  /* +0x020 */
  DWORD64 P6Home;  /* +0x028 */
  DWORD ContextFlags; /* +0x030 */
  DWORD MxCsr;         /* +0x034 */
  WORD SegCs;         /* +0x038 */
  WORD SegDs;         /* +0x03A */
  WORD SegEs;         /* +0x03C */
  WORD SegFs;         /* +0x03E */
  WORD SegGs;         /* +0x040 */
  WORD SegSs;         /* +0x042 */
  DWORD EFlags;        /* +0x044 */
  DWORD64 Dr0;     /* +0x048 */
  DWORD64 Dr1;     /* +0x050 */
  DWORD64 Dr2;     /* +0x058 */
  DWORD64 Dr3;     /* +0x060 */
  DWORD64 Dr6;     /* +0x068 */
  DWORD64 Dr7;     /* +0x070 */
  DWORD64 Rax;     /* +0x078 */
  DWORD64 Rcx;     /* +0x080 */
  DWORD64 Rdx;     /* +0x088 */
  DWORD64 Rbx;     /* +0x090 */
  DWORD64 Rsp;     /* +0x098 */
  DWORD64 Rbp;     /* +0x0A0 */
  DWORD64 Rsi;     /* +0x0A8 */
  DWORD64 Rdi;     /* +0x0B0 */
  DWORD64 R8;      /* +0x0B8 */
  DWORD64 R9;      /* +0x0C0 */
  DWORD64 R10;     /* +0x0C8 */
  DWORD64 R11;     /* +0x0D0 */
  DWORD64 R12;     /* +0x0D8 */
  DWORD64 R13;     /* +0x0E0 */
  DWORD64 R14;     /* +0x0E8 */
  DWORD64 R15;     /* +0x0F0 */
  DWORD64 Rip;     /* +0x0F8 */
  struct crt_xsave_format FltSave; /* +0x100, 512 bytes, ends at +0x300 */
  crt_m128a VectorRegister[26];    /* +0x300, ends at +0x4A0 */
  DWORD64 VectorControl;           /* +0x4A0 */
  DWORD64 DebugControl;            /* +0x4A8 */
  DWORD64 LastBranchToRip;         /* +0x4B0 */
  DWORD64 LastBranchFromRip;       /* +0x4B8 */
  DWORD64 LastExceptionToRip;      /* +0x4C0 */
  DWORD64 LastExceptionFromRip;    /* +0x4C8, ends at +0x4D0 (1232 bytes) */
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
__declspec(dllimport) BOOL CRT_WINAPI GetThreadContext(HANDLE hThread, struct crt_context_amd64* lpContext);
__declspec(dllimport) BOOL CRT_WINAPI SetThreadContext(HANDLE hThread, const struct crt_context_amd64* lpContext);
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
#define CRT_CONTEXT_AMD64 0x00100000UL
#define CRT_CONTEXT_AMD64_CONTROL (CRT_CONTEXT_AMD64 | 0x1UL)
#define CRT_CONTEXT_AMD64_INTEGER (CRT_CONTEXT_AMD64 | 0x2UL)
#define CRT_CONTEXT_AMD64_FLOATING_POINT (CRT_CONTEXT_AMD64 | 0x8UL)
#define CRT_CONTEXT_AMD64_FULL \
    (CRT_CONTEXT_AMD64_CONTROL | CRT_CONTEXT_AMD64_INTEGER | CRT_CONTEXT_AMD64_FLOATING_POINT)
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

/* Windows x64 (unlike aarch64, which dedicates X18 as a platform register)
 * reaches the current thread's TEB via the GS segment base directly:
 * GS:0x30 is NT_TIB.Self, a pointer the TEB stores to itself (a carry-over
 * from the 32-bit FS:0x18 convention), so this alone yields the TEB
 * address with no extra register reservation needed anywhere else in the
 * build (unlike aarch64's -ffixed-x18, which has to apply globally). */
static void* read_teb(void) {
  void* teb;
  __asm__ volatile("movq %%gs:0x30, %0" : "=r"(teb));
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
 * for decades and this project otherwise avoids pulling in <windows.h>.
 * Architecture-independent (PE32+ section headers are identical on x64
 * and ARM64); unchanged from the aarch64 version. */
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
 * parent's TEB happens to say. Architecture-independent (NT_TIB's
 * StackBase/StackLimit sit at the same +0x08/+0x10 offsets on x64 as on
 * ARM64); unchanged from the aarch64 version. */
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
  if (commit_region_into_child(process, copy_low, size) != 0) {
    fprintf(stderr,
            "fork_memcopy: stack commit failed: stack_limit=%p stack_base=%p child_sp=%p "
            "AllocationBase=%p mbi.State=%#lx copy_low=%p size=%zu err=%lu\n",
            stack_limit, stack_base, (void*)(ULONG_PTR)child_initial_sp, mbi.AllocationBase,
            (unsigned long)mbi.State, copy_low, size, (unsigned long)GetLastError());
    return -1;
  }
  return 0;
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
  struct crt_context_amd64 ctx;
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
    fprintf(stderr, "fork_memcopy: GetModuleFileNameA failed err=%lu\n", (unsigned long)GetLastError());
    return -EAGAIN;
  }

  InitializeProcThreadAttributeList(0, 1, 0, &attr_list_size);
  attr_list = malloc((size_t)attr_list_size);
  if (attr_list == 0) {
    fprintf(stderr, "fork_memcopy: malloc(attr_list) failed\n");
    return -ENOMEM;
  }
  if (!InitializeProcThreadAttributeList(attr_list, 1, 0, &attr_list_size) ||
      !UpdateProcThreadAttribute(attr_list, 0, CRT_PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY,
                                  &policy, sizeof(policy), 0, 0)) {
    fprintf(stderr, "fork_memcopy: attr list setup failed err=%lu\n", (unsigned long)GetLastError());
    free(attr_list);
    return -EAGAIN;
  }

  memset(&si, 0, sizeof(si));
  si.cb = sizeof(si);
  si.lpAttributeList = attr_list;
  snprintf(cmdline, sizeof(cmdline), "\"%s\"", self_path);

  if (!CreateProcessA(self_path, cmdline, 0, 0, 1, CRT_EXTENDED_STARTUPINFO_PRESENT | CRT_CREATE_SUSPENDED, 0, 0,
                       &si, &pi)) {
    fprintf(stderr, "fork_memcopy: CreateProcessA failed err=%lu self_path=%s\n", (unsigned long)GetLastError(),
            self_path);
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
  ctx.ContextFlags = CRT_CONTEXT_AMD64_FULL;
  if (!GetThreadContext(pi.hThread, &ctx)) {
    fprintf(stderr, "fork_memcopy: GetThreadContext failed err=%lu\n", (unsigned long)GetLastError());
    TerminateProcess(pi.hProcess, (unsigned int)-1);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return -EAGAIN;
  }

  /* Order matters: every destination region below must be committed and
   * written *before* SetThreadContext()/ResumeThread(), since the child
   * runs immediately and unsupervised as soon as it is resumed. */
  {
    int heap_rc = copy_heap_chunks(pi.hProcess);
    int image_rc = heap_rc == 0 ? copy_image_data_sections(pi.hProcess) : heap_rc;
    int stack_rc = image_rc == 0 ? copy_current_stack(pi.hProcess, ctx.Rsp, &stack_low, &stack_high) : image_rc;

    if (stack_rc != 0) {
      fprintf(stderr, "fork_memcopy: copy failed heap_rc=%d image_rc=%d stack_rc=%d err=%lu\n", heap_rc,
              image_rc, stack_rc, (unsigned long)GetLastError());
      TerminateProcess(pi.hProcess, (unsigned int)-1);
      CloseHandle(pi.hThread);
      CloseHandle(pi.hProcess);
      return -EAGAIN;
    }
  }

  /* The current thread's own crt_thread_context block (tls.c) is a
   * standalone VirtualAlloc(), not part of the heap/stack/image regions
   * above -- copy it explicitly. */
  tls_context = __crt_thread_get_current();
  if (copy_region_into_child(pi.hProcess, tls_context, sizeof(*tls_context)) != 0) {
    fprintf(stderr, "fork_memcopy: tls_context copy failed err=%lu\n", (unsigned long)GetLastError());
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
      fprintf(stderr, "fork_memcopy: tls index patch failed err=%lu\n", (unsigned long)GetLastError());
      TerminateProcess(pi.hProcess, (unsigned int)-1);
      CloseHandle(pi.hThread);
      CloseHandle(pi.hProcess);
      return -EAGAIN;
    }
  }

  /* Directly construct the child's initial CONTEXT from the jmp_buf
   * setjmp() just captured, instead of redirecting Rip to a trampoline
   * that would call longjmp() itself. Equivalent in principle, but does
   * not depend on the child's own code correctly reading resume state
   * back out of copied memory at just the right moment -- the parent
   * already has every value it needs in its own registers/locals right
   * here, so it sets them directly. Offsets match
   * libc/src/arch/windows/x86_64/setjmp.S's store layout exactly:
   * rbx/rbp/rdi/rsi/r12-r15 (buf[0..7]), rsp (buf[8]), return address
   * (buf[9]), xmm6-xmm15 -- full 128 bits each, unlike aarch64's v8-v15
   * which only preserve the low 64 -- (buf[10..29]). */
  ctx.Rbx = (DWORD64)resume[0];
  ctx.Rbp = (DWORD64)resume[1];
  ctx.Rdi = (DWORD64)resume[2];
  ctx.Rsi = (DWORD64)resume[3];
  ctx.R12 = (DWORD64)resume[4];
  ctx.R13 = (DWORD64)resume[5];
  ctx.R14 = (DWORD64)resume[6];
  ctx.R15 = (DWORD64)resume[7];
  ctx.Rsp = (DWORD64)resume[8];
  ctx.Rip = (DWORD64)resume[9]; /* return address -- becomes the resume PC directly */
  ctx.FltSave.XmmRegisters[6].Low = (unsigned long long)resume[10];
  ctx.FltSave.XmmRegisters[6].High = (long long)resume[11];
  ctx.FltSave.XmmRegisters[7].Low = (unsigned long long)resume[12];
  ctx.FltSave.XmmRegisters[7].High = (long long)resume[13];
  ctx.FltSave.XmmRegisters[8].Low = (unsigned long long)resume[14];
  ctx.FltSave.XmmRegisters[8].High = (long long)resume[15];
  ctx.FltSave.XmmRegisters[9].Low = (unsigned long long)resume[16];
  ctx.FltSave.XmmRegisters[9].High = (long long)resume[17];
  ctx.FltSave.XmmRegisters[10].Low = (unsigned long long)resume[18];
  ctx.FltSave.XmmRegisters[10].High = (long long)resume[19];
  ctx.FltSave.XmmRegisters[11].Low = (unsigned long long)resume[20];
  ctx.FltSave.XmmRegisters[11].High = (long long)resume[21];
  ctx.FltSave.XmmRegisters[12].Low = (unsigned long long)resume[22];
  ctx.FltSave.XmmRegisters[12].High = (long long)resume[23];
  ctx.FltSave.XmmRegisters[13].Low = (unsigned long long)resume[24];
  ctx.FltSave.XmmRegisters[13].High = (long long)resume[25];
  ctx.FltSave.XmmRegisters[14].Low = (unsigned long long)resume[26];
  ctx.FltSave.XmmRegisters[14].High = (long long)resume[27];
  ctx.FltSave.XmmRegisters[15].Low = (unsigned long long)resume[28];
  ctx.FltSave.XmmRegisters[15].High = (long long)resume[29];
  /* setjmp()'s own return value convention (setjmp.S) for a nonzero
   * longjmp value: Rax = 1 (the value a caller reads right after `call
   * setjmp` returns -- resuming directly at that return address, as this
   * does, needs Rax pre-loaded with what that call would have produced). */
  ctx.Rax = 1;
  if (!SetThreadContext(pi.hThread, &ctx)) {
    fprintf(stderr, "fork_memcopy: SetThreadContext failed err=%lu\n", (unsigned long)GetLastError());
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
