/*
 * Minimal Windows/MSVC ABI compiler helper definitions for freestanding CRT
 * links. Clang emits _fltused for x86_64 MSVC-targeted objects that use
 * floating-point operations; normally the MSVC runtime provides it.
 */

#include <stdint.h>

void __stack_chk_fail(void);

#if defined(__x86_64__) || defined(_M_X64)
__attribute__((weak)) int _fltused = 0;
#endif

/* __clear_cache(start, end): the standard GCC/Clang builtin-runtime symbol
 * (compiler-rt's builtins archive normally provides it) that
 * __builtin___clear_cache()/any hand-written self-modifying-code generator
 * calls after writing new executable code, to flush the range from the
 * instruction cache before it's ever executed -- otherwise the CPU may run
 * stale cached instructions from before the write. This project's Windows
 * builds have no compiler-rt builtins archive at all (clang's own
 * --print-libgcc-file-name comes up empty for this LLVM install/target,
 * confirmed via CMakeLists.txt's own "compiler-rt builtins not found"
 * message), so nothing has ever provided this symbol -- unnoticed until
 * libffi's closures.c (a genuine JIT-style trampoline generator) linked
 * against it and came up with "undefined symbol: __clear_cache".
 *
 * FlushInstructionCache() is the real Win32 API for exactly this purpose,
 * already correct for every architecture Windows itself supports (it
 * handles the aarch64 IC/DC-instruction-and-ISB sequence or the x86/x64
 * "practically a no-op, self-modifying code is naturally coherent there"
 * case internally) -- delegating to it is simpler and more robust than
 * hand-rolling per-architecture cache-maintenance instructions ourselves. */
typedef void* CRT_HANDLE;
typedef int CRT_BOOL;
typedef unsigned long long CRT_SIZE_T;
__declspec(dllimport) CRT_HANDLE __stdcall GetCurrentProcess(void);
__declspec(dllimport) CRT_BOOL __stdcall FlushInstructionCache(
    CRT_HANDLE hProcess, const void* lpBaseAddress, CRT_SIZE_T dwSize);

void __clear_cache(void* start, void* end) {
  CRT_SIZE_T size = (CRT_SIZE_T)((char*)end - (char*)start);

  FlushInstructionCache(GetCurrentProcess(), start, size);
}

/* __main(): another compiler-inserted, not user-written, call -- clang (like
 * real GCC before it) targeting *-w64-mingw32 unconditionally emits a call
 * to __main() at the very top of every translation unit's own main(), a
 * decades-old MinGW/Cygwin convention originally meant to run constructors
 * queued in a .ctors section on a PE loader that (unlike a real ELF dynamic
 * linker) never ran them itself. This project's own CRT startup
 * (src/arch/windows/common/crt1.c) already runs constructors correctly
 * through its own mechanism before main() is ever reached, so __main()
 * itself has nothing left to do -- matching modern mingw-w64's own runtime,
 * which keeps __main() around only as an empty, ABI-compatible stub for
 * exactly this reason. Needed by any third-party port built through
 * tools/crt-cc (which targets *-w64-mingw32 specifically so configure/
 * libtool probes see a GNU-like toolchain -- see tools/crt-cc's own
 * comment) that defines a plain main(), e.g. GNU make's src/main.c. This
 * project's own sources never call it (crt1.c doesn't target
 * *-w64-mingw32), so it is otherwise dead weight, not a duplicate
 * constructor-running path. */
void __main(void) {}

/* Clang's Windows compiler-rt builtins use the same documented MSVC ABI
 * stack-cookie pair as /GS-instrumented objects.  The normal MSVC startup
 * object initializes this state, but CRT-owned crt1/dllcrt deliberately
 * replaces that startup path.  Keep the cookie in the CRT ABI object so it
 * is present in both libc.a and libc.dll, alongside the other compiler-
 * generated symbol shims in this file. */
uintptr_t __security_cookie = (uintptr_t)0x9e3779b97f4a7c15ULL;

void __security_check_cookie(uintptr_t cookie) {
  if (cookie != __security_cookie) {
    __stack_chk_fail();
  }
}
