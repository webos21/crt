/*
 * Minimal Windows/MSVC ABI compiler helper definitions for freestanding CRT
 * links. Clang emits _fltused for x86_64 MSVC-targeted objects that use
 * floating-point operations; normally the MSVC runtime provides it.
 */

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
