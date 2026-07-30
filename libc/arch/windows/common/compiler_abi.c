/*
 * Minimal Windows/MSVC ABI compiler helper definitions for freestanding CRT
 * links. Clang emits _fltused for x86_64 MSVC-targeted objects that use
 * floating-point operations; normally the MSVC runtime provides it.
 */

#if defined(__x86_64__) || defined(_M_X64)
__attribute__((weak)) int _fltused = 0;
#endif
