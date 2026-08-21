/* Vectored-exception-based safety net for hardware faults that occur
 * inside DWARF-CFI-compiled (-fdwarf-exceptions) CRT/libc++/libcxxabi/
 * libunwind code on Windows.
 *
 * Background (see docs/cxx_runtime.md's "Known cost: DWARF-compiled code
 * has zero Windows-native unwind info" and TODO.md's C++ runtime
 * prerequisite section, step 7): this project deliberately compiles its
 * own C++ runtime with -fdwarf-exceptions rather than native SEH (see
 * docs/cxx_runtime.md's own "why Windows C++ exceptions use DWARF CFI,
 * not native SEH" section), which means every non-leaf function in that
 * code -- throwing or not -- carries no `.pdata`/`.xdata` at all, only a
 * `.eh_frame` section only this project's own libunwind understands. The
 * Windows x64/ARM64 ABI assumes a function with no unwind-table entry is
 * a leaf (RSP unchanged, no register restoration needed); a real
 * non-leaf DWARF-only frame violates that assumption, so the OS's own
 * frame-based (SEH `__try`/`__except`) exception dispatch computes the
 * wrong caller context the moment it has to walk through one -- this is
 * undefined behavior by the ABI's own contract, confirmed for real (see
 * below), not a theoretical concern.
 *
 * THIS WAS EMPIRICALLY VERIFIED, not assumed, in three stages (2026-08-21
 * -- see HISTORY.md's dated entry for the full write-up and the
 * standalone repro this was distilled from):
 *
 * 1. A plain `-fseh-exceptions`-compiled boundary frame with a real
 *    `__try`/`__except` wrapped directly around a call into a chain of
 *    `-fdwarf-exceptions`-compiled functions (simulating CRT/libc++ code
 *    invoked from a native OS callback) does NOT catch a hardware fault
 *    (a null-pointer write) raised several frames deep in that DWARF
 *    chain -- the process crashes uncontrolled instead. This DISPROVES
 *    the "wrap every native-callback entry point in an SEH boundary
 *    shim" design TODO.md's item 7 originally sketched: a single SEH
 *    frame at the call-in point does not make the OS's own frame-based
 *    unwind able to cross the DWARF frames beneath it, because that walk
 *    fails at the FIRST untabled frame it reaches, long before ever
 *    getting back up to the boundary's own handler. A control run of the
 *    identical repro with the callee chain compiled WITHOUT
 *    -fdwarf-exceptions (real `.pdata` all the way down) confirms the
 *    test harness itself is sound: the same `__except` catches the same
 *    fault cleanly there.
 * 2. `AddVectoredExceptionHandler()` (VEH), unlike frame-based SEH, does
 *    NOT depend on walking the call stack via `.pdata` at all -- it is
 *    dispatched directly from the fault's own context by ntdll, before
 *    any frame-based search begins. Confirmed for real: a VEH handler
 *    (registered with either `First=1` or `First=0` -- both behave
 *    identically in this respect) DOES reliably fire for the exact same
 *    deep-DWARF-chain fault above, with no SEH boundary anywhere in the
 *    link at all. This is the real, correct, fully-documented mechanism
 *    this file uses.
 * 3. A naive "VEH handler that always logs and terminates" is too broad:
 *    confirmed for real that ANY VEH handler -- `First=1` or `First=0`
 *    alike -- runs unconditionally BEFORE frame-based SEH dispatch, so
 *    it would preempt and break a legitimate, fully-`.pdata`-backed
 *    application `__try`/`__except` elsewhere in the same process (e.g.
 *    third-party ported C code built through tools/crt-cc, which is
 *    never compiled with -fdwarf-exceptions and keeps real unwind
 *    tables throughout) that wants to catch and recover from its own
 *    hardware faults. `SetUnhandledExceptionFilter()` was considered as
 *    the alternative (its whole contract is "only called once nothing
 *    else handled it," so it can never preempt a working `__except`) but
 *    rejected: confirmed for real that it does NOT fire at all for the
 *    deep-DWARF-chain fault, for the same reason frame (1) above
 *    failed -- reaching the "nothing else handled it" determination
 *    itself requires completing the same broken frame-based walk.
 *    The fix: gate this file's own VEH handler on `RtlLookupFunctionEntry()`
 *    (the exact same table lookup the OS's own frame-based dispatch
 *    relies on) called on the faulting address itself. Confirmed for
 *    real, both directions: when the fault is in an untabled DWARF frame,
 *    the lookup returns NULL and this handler takes over; when the fault
 *    is in any properly-tabled frame (regardless of whether a real
 *    `__except` exists further up), the lookup succeeds and this handler
 *    immediately defers (`EXCEPTION_CONTINUE_SEARCH`), which was
 *    confirmed to let a real `__except` elsewhere in the chain catch the
 *    exception exactly as if this file were never linked in at all.
 *
 * What this DOES fix: a hardware fault (access violation, divide-by-
 * zero, illegal instruction, stack overflow, ...) whose faulting address
 * has no Windows-native unwind info -- i.e. is inside this project's own
 * DWARF-compiled C++ runtime, or any C++ user code built through
 * tools/crt-c++ -- now produces a controlled, deterministic process exit
 * (a brief diagnostic on stderr, then `ExitProcess()` with a `128 +
 * <POSIX signal number>` exit code matching this project's own existing
 * convention, see libc/src/signal.c's `abort()`) instead of undefined
 * behavior from a corrupted second-chance search. This is exactly what
 * makes CRT/libc++-compiled code safe to register directly as a raw
 * native OS callback (window proc, `CreateThread` entry point, COM
 * vtable method, ...) per TODO.md item 7 and docs/cxx_runtime.md's third
 * bullet -- and it needs no per-callsite boundary shim at all: this one
 * process-wide registration at CRT startup covers every thread and every
 * call path automatically, which turned out to be strictly simpler than
 * (and a real fix, unlike) the originally-sketched per-callback wrapper.
 *
 * What this does NOT fix (documented honestly, not glossed over):
 *   - It does not let a C++ `catch` recover from a hardware fault --
 *     never promised, matches this project's existing non-`/EHa`
 *     semantics (a hardware fault is not a C++ exception here, same as
 *     on Linux/macOS).
 *   - It does not fix third-party tools (a debugger's live call-stack
 *     view, Windows Error Reporting's own minidump writer, an ETW-based
 *     profiler) that do their own separate, OS-native frame-based walk --
 *     those still misbehave crossing a DWARF frame exactly as before.
 *     This file's own diagnostic write happens first and is unaffected
 *     by that, but cannot repair what a third-party tool shows afterward.
 *   - It does not attempt a real backtrace: only the single faulting
 *     address is reported. A DWARF-CFI-based backtrace (via this
 *     project's own libunwind) would be a real, valuable follow-up, but
 *     libunwind is only staged as an *optional* CRT_USE_IMPORTED_LIBCXX
 *     component, not a base libc dependency -- pulling it into every
 *     program's crash path would be a real layering change, deliberately
 *     not attempted here.
 *
 * Diagnostic output uses a raw Win32 WriteFile() call and ExitProcess(),
 * never this project's own stdio/write()/abort() -- same reasoning as
 * pseudo_reloc.c's own comment: a hardware-fault handler cannot assume
 * anything about the rest of this project's own libc state is intact at
 * the moment it runs, and (once linked into every shared DLL the same
 * way crt1_pseudo_reloc.o already is) must not risk pulling in fd.c's
 * bundled read()/write() the way that file's own regression did.
 *
 * Architecture note: empirically verified on x86_64 Windows only (the
 * only architecture actually testable in this dev session). ARM64
 * Windows uses the same table-based unwind model with the same
 * "no entry = leaf" assumption and the same AddVectoredExceptionHandler/
 * RtlLookupFunctionEntry APIs, so this is expected to behave identically
 * there, but that expectation is reasoned, not independently verified --
 * flagged the same way this project already flags other not-yet-
 * cross-architecture-verified code (see the crt-unverified-syscall-
 * trampolines discipline applied elsewhere in this project).
 */

#include <stddef.h>
#include <stdint.h>

#if defined(_M_IX86) || defined(__i386__)
#define CRT_WINAPI __stdcall
#else
#define CRT_WINAPI
#endif

typedef unsigned long crt_dword;
typedef unsigned long long crt_dword64;
typedef long crt_long;
typedef int crt_bool;

/* EXCEPTION_RECORD / EXCEPTION_POINTERS: real, fixed, documented layout
 * (winnt.h) -- only the fields this file actually reads are given real
 * types; ContextRecord is never dereferenced, so it stays an opaque
 * pointer (matches EXCEPTION_MAXIMUM_PARAMETERS == 15, verified against
 * this machine's own installed Windows SDK winnt.h). */
typedef struct crt_exception_record {
  crt_dword ExceptionCode;
  crt_dword ExceptionFlags;
  struct crt_exception_record* ExceptionRecord;
  void* ExceptionAddress;
  crt_dword NumberParameters;
  uintptr_t ExceptionInformation[15];
} crt_exception_record;

typedef struct {
  crt_exception_record* ExceptionRecord;
  void* ContextRecord;
} crt_exception_pointers;

typedef crt_long(CRT_WINAPI* crt_veh_callback)(crt_exception_pointers*);

__declspec(dllimport) void* CRT_WINAPI AddVectoredExceptionHandler(crt_dword First, crt_veh_callback Handler);
__declspec(dllimport) void* CRT_WINAPI RtlLookupFunctionEntry(crt_dword64 ControlPc, crt_dword64* ImageBase,
                                                               void* HistoryTable);
__declspec(dllimport) void* CRT_WINAPI GetStdHandle(crt_dword nStdHandle);
__declspec(dllimport) crt_bool CRT_WINAPI WriteFile(void* hFile, const void* lpBuffer, crt_dword nNumberOfBytesToWrite,
                                                     crt_dword* lpNumberOfBytesWritten, void* lpOverlapped);
__declspec(dllimport) void CRT_WINAPI ExitProcess(crt_dword uExitCode);

#define CRT_VEH_CONTINUE_SEARCH ((crt_long)0)

/* Real STATUS_.../EXCEPTION_... values (winnt.h) -- verified against
 * this machine's own installed Windows SDK, not transcribed from
 * memory. */
#define CRT_EXCEPTION_ACCESS_VIOLATION 0xC0000005ul
#define CRT_EXCEPTION_IN_PAGE_ERROR 0xC0000006ul
#define CRT_EXCEPTION_ILLEGAL_INSTRUCTION 0xC000001Dul
#define CRT_EXCEPTION_ARRAY_BOUNDS_EXCEEDED 0xC000008Cul
#define CRT_EXCEPTION_FLT_DENORMAL_OPERAND 0xC000008Dul
#define CRT_EXCEPTION_FLT_DIVIDE_BY_ZERO 0xC000008Eul
#define CRT_EXCEPTION_FLT_INEXACT_RESULT 0xC000008Ful
#define CRT_EXCEPTION_FLT_INVALID_OPERATION 0xC0000090ul
#define CRT_EXCEPTION_FLT_OVERFLOW 0xC0000091ul
#define CRT_EXCEPTION_FLT_STACK_CHECK 0xC0000092ul
#define CRT_EXCEPTION_FLT_UNDERFLOW 0xC0000093ul
#define CRT_EXCEPTION_INT_DIVIDE_BY_ZERO 0xC0000094ul
#define CRT_EXCEPTION_INT_OVERFLOW 0xC0000095ul
#define CRT_EXCEPTION_PRIV_INSTRUCTION 0xC0000096ul
#define CRT_EXCEPTION_STACK_OVERFLOW 0xC00000FDul

/* Matches include/signal.h's own Bionic/Linux numbering (not transcribed
 * as a header #include: this file follows pseudo_reloc.c's own
 * zero-libc-dependency discipline -- see this file's own top comment --
 * and these three numbers are simple, stable, freestanding constants). */
#define CRT_SIGILL 4
#define CRT_SIGFPE 8
#define CRT_SIGSEGV 11

static void veh_write(const char* s, size_t len) {
  crt_dword written = 0;
  /* STD_ERROR_HANDLE, a real, stable Win32 constant (matches
   * pseudo_reloc.c's own use of the same value). */
  WriteFile(GetStdHandle((crt_dword)-12), s, (crt_dword)len, &written, 0);
}
#define VEH_WRITE_LITERAL(literal) veh_write((literal), sizeof(literal) - 1)

static char hex_nibble(unsigned v) {
  return (char)((v < 10) ? ('0' + v) : ('a' + (v - 10)));
}

/* Fixed-width "0x" + 16 hex digits -- deliberately not trimmed: a crash
 * handler is not the place for a general-purpose formatter, and a
 * constant-width field is easier to eyeball/grep in a log. */
static void format_hex64(char* out, uint64_t value) {
  size_t i;
  out[0] = '0';
  out[1] = 'x';
  for (i = 0; i < 16; i++) {
    out[2 + i] = hex_nibble((unsigned)((value >> ((15 - i) * 4)) & 0xf));
  }
}

static void format_hex32(char* out, crt_dword value) {
  size_t i;
  out[0] = '0';
  out[1] = 'x';
  for (i = 0; i < 8; i++) {
    out[2 + i] = hex_nibble((unsigned)((value >> ((7 - i) * 4)) & 0xf));
  }
}

static crt_long CRT_WINAPI crt_dwarf_unwind_safety_net(crt_exception_pointers* info) {
  crt_exception_record* rec;
  crt_dword64 image_base = 0;
  const char* signal_name;
  size_t signal_name_len;
  int posix_signal;

  if (info == 0 || info->ExceptionRecord == 0) {
    return CRT_VEH_CONTINUE_SEARCH;
  }
  rec = info->ExceptionRecord;

  switch (rec->ExceptionCode) {
    case CRT_EXCEPTION_ACCESS_VIOLATION:
    case CRT_EXCEPTION_IN_PAGE_ERROR:
    case CRT_EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    case CRT_EXCEPTION_STACK_OVERFLOW:
      signal_name = "SIGSEGV";
      signal_name_len = 7;
      posix_signal = CRT_SIGSEGV;
      break;
    case CRT_EXCEPTION_ILLEGAL_INSTRUCTION:
    case CRT_EXCEPTION_PRIV_INSTRUCTION:
      signal_name = "SIGILL";
      signal_name_len = 6;
      posix_signal = CRT_SIGILL;
      break;
    case CRT_EXCEPTION_FLT_DENORMAL_OPERAND:
    case CRT_EXCEPTION_FLT_DIVIDE_BY_ZERO:
    case CRT_EXCEPTION_FLT_INEXACT_RESULT:
    case CRT_EXCEPTION_FLT_INVALID_OPERATION:
    case CRT_EXCEPTION_FLT_OVERFLOW:
    case CRT_EXCEPTION_FLT_STACK_CHECK:
    case CRT_EXCEPTION_FLT_UNDERFLOW:
    case CRT_EXCEPTION_INT_DIVIDE_BY_ZERO:
    case CRT_EXCEPTION_INT_OVERFLOW:
      signal_name = "SIGFPE";
      signal_name_len = 6;
      posix_signal = CRT_SIGFPE;
      break;
    default:
      /* Not one of the hardware-fault codes this file cares about (e.g.
       * a debugger breakpoint/single-step, or any software-raised
       * exception -- this project's own C++ throw/catch never reaches
       * here at all: it is implemented via libunwind's portable,
       * DWARF-CFI-walking _Unwind_RaiseException (UnwindLevel1.c), which
       * never calls Windows' RaiseException()/SEH machinery in the first
       * place, confirmed by this project's own choice of libunwind
       * backend -- see docs/cxx_runtime.md). Never touch anything else,
       * regardless of table state. */
      return CRT_VEH_CONTINUE_SEARCH;
  }

  /* The actual gap this file exists to close (see this file's own top
   * comment, empirical finding 3): only take over when the OS's own
   * frame-based (SEH __try/__except) search would already be broken at
   * the fault site, i.e. RtlLookupFunctionEntry finds no unwind info for
   * the faulting address. When real .pdata IS present -- regardless of
   * whether it has anything to do with this project's own CRT/libc++ --
   * defer completely and let normal SEH/default handling proceed exactly
   * as if this handler were never installed. */
  if (RtlLookupFunctionEntry((crt_dword64)(uintptr_t)rec->ExceptionAddress, &image_base, 0) != 0) {
    return CRT_VEH_CONTINUE_SEARCH;
  }

  if (rec->ExceptionCode != CRT_EXCEPTION_STACK_OVERFLOW) {
    /* Stack overflow: skip the diagnostic write entirely. Windows only
     * guarantees a small additional "safety" stack region past the
     * guard page that triggered this, and every WriteFile()/
     * GetStdHandle() call itself needs stack -- minimize what runs here
     * rather than risk re-faulting inside the handler itself. */
    char code_buf[10];
    char addr_buf[18];

    VEH_WRITE_LITERAL("crt: fatal exception ");
    format_hex32(code_buf, rec->ExceptionCode);
    veh_write(code_buf, sizeof(code_buf));
    VEH_WRITE_LITERAL(" (");
    veh_write(signal_name, signal_name_len);
    VEH_WRITE_LITERAL(") at ");
    format_hex64(addr_buf, (uint64_t)(uintptr_t)rec->ExceptionAddress);
    veh_write(addr_buf, sizeof(addr_buf));
    VEH_WRITE_LITERAL(
        " -- no Windows-native unwind info at the fault site (a DWARF-only\n"
        "CRT/libc++/libunwind frame); the OS's own second-chance search\n"
        "cannot safely cross it, so this is a controlled, deterministic\n"
        "exit instead of undefined behavior. See docs/cxx_runtime.md's\n"
        "\"Known cost: DWARF-compiled code has zero Windows-native unwind\n"
        "info\" section.\n");
  }

  ExitProcess((crt_dword)(128 + posix_signal));
  return CRT_VEH_CONTINUE_SEARCH; /* unreachable: ExitProcess() never returns */
}

void _crt_install_dwarf_unwind_safety_net(void) {
  AddVectoredExceptionHandler(1, crt_dwarf_unwind_safety_net);
}
