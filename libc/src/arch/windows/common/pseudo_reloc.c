/* Windows/PE "runtime pseudo relocation" support for GNU ld/lld-link's
 * auto-import extension.
 *
 * When code built for the GNU/MinGW ABI (as this project's tools/crt-cc
 * does for third-party port builds, --target=*-w64-mingw32) references a
 * DATA symbol exported from another DLL WITHOUT an explicit
 * __declspec(dllimport) annotation -- routine for cross-platform C
 * libraries whose own headers assume "GCC auto-imports, only MSVC needs
 * dllimport" (confirmed directly: libffi's own ffi.h says exactly this,
 * and that is why porting/tests/libffi_call.c's `&ffi_type_sint`
 * reference needs this at all) -- the linker can still make the
 * reference work via "auto-import": it treats the reference as if it
 * went through the normal import-address-table (IAT) indirection, but
 * bakes a PLACEHOLDER value into the actual instruction/data bytes at
 * link time (the real cross-DLL address isn't known until the loader
 * resolves imports at process start), then emits a table of "pseudo
 * relocations" describing exactly which locations need a runtime fixup
 * once the IAT is populated.
 *
 * Real mingw-w64 crt0 processes this table via a function conventionally
 * named _pei386_runtime_relocator(), called as the very first thing at
 * process startup -- before anything else, since ANY code (including
 * this project's own __crt_env_set_initial()/__crt_child_bootstrap(),
 * or a third-party library's own __attribute__((constructor)) function)
 * could potentially touch an auto-imported data reference before it's
 * fixed up. lld-link REFUSES to produce a final image containing a
 * non-empty pseudo-reloc table unless a symbol with this exact name
 * exists SOMEWHERE in the link -- confirmed directly: linking a program
 * against libffi.dll.a (which triggers this for `&ffi_type_sint`) fails
 * with "output image has runtime pseudo relocations, but the function
 * _pei386_runtime_relocator is missing" without this file linked in. A
 * deliberate safety check: a linked pseudo-reloc table nobody ever
 * processes at runtime would silently leave broken addresses in the
 * final binary.
 *
 * Table format (confirmed against two independent, cross-checked
 * sources: mingw-w64's own reference decoder,
 * mingw-w64-crt/crt/pseudo-reloc.c, and this exact toolchain's own
 * encoder, LLVM lld's PseudoRelocTableChunk in lld/COFF/Chunks.cpp --
 * both agree): a 12-byte v2 header (three 32-bit fields: magic1=0,
 * magic2=0, version=1) followed by zero or more 12-byte entries (three
 * 32-bit fields: sym = RVA of the IAT slot holding the real imported
 * address, target = RVA of the location to patch, flags = low byte is
 * the bit-width of the relocation: 8/16/32/64). RVAs are always 32-bit
 * here, even in an x86_64/aarch64 build -- a basic PE format property,
 * unrelated to CPU pointer width. The list's own start/end addresses
 * are provided automatically by the linker as
 * __RUNTIME_PSEUDO_RELOC_LIST__/__RUNTIME_PSEUDO_RELOC_LIST_END__ -- no
 * custom sentinel objects needed here, unlike this project's own
 * .ctors/.dtors walker (init_fini_array.c), which has to provide its own
 * because plain (non-$-grouped) COFF sections have no such automatic
 * linker-provided boundary symbols. This is architecture-independent
 * (only pointer width matters, and both this project's Windows targets,
 * x86_64 and aarch64, are 64-bit), so it lives in common/, not either
 * arch directory.
 *
 * Both the executable's own entry point (mainCRTStartup, crt1.c) and the
 * DLL entry point (crtDllMainCRTStartup, dllcrt.c, on DLL_PROCESS_ATTACH)
 * call this. The DLL side was added once a concrete case actually needed
 * it: libc++.dll importing an auto-imported data symbol from
 * libunwind.dll (see libstdc++/third_party/libcxx/recipe.json's own
 * notes) -- lld-link refuses to produce a DLL with a non-empty
 * pseudo-reloc table unless this symbol exists in the link, the exact
 * same check it applies to executables.
 *
 * Diagnostic output below goes through a raw Win32 WriteFile() call
 * rather than fprintf(stderr, ...) or even this project's own write():
 * this function runs before absolutely anything else (see above),
 * including -- once linked into a DLL rather than only an executable --
 * possibly before the DLL that owns the process's `stderr` FILE* has
 * finished its own DLL_PROCESS_ATTACH. Confirmed for real, twice: first
 * linking this file into m.dll/dl.dll/c++.dll failed with "lld-link:
 * error: undefined symbol: stderr" (a cross-DLL DATA reference from a
 * small, low-level DLL that does not itself implement stdio, unlike
 * c.dll); switching to this project's own write() fixed that but broke
 * something more subtle -- libc/src/fd.c bundles read()/write()/every
 * other fd primitive into one translation unit, so any reference to
 * write() anywhere in a link pulls in that whole object, including its
 * own real read(). tests/windows_dll_symbol_priority_dll.c deliberately
 * defines its own conflicting read() to test DLL symbol-priority
 * resolution (see that file's own comment) -- crt1_pseudo_reloc.o now
 * being linked into every shared DLL (see the DLL-analogue paragraph
 * above) meant fd.c's read() started colliding with it: "duplicate
 * symbol: read" building that regression fixture's own DLL, a real
 * failure this file's own change caused, not a pre-existing one. A raw
 * WriteFile() call needs no part of this project's own libc at all --
 * genuinely nothing beyond a function-call IAT thunk to kernel32, never
 * triggering the pseudo-relocation auto-import machinery this whole
 * file exists to fix up in the first place, and never pulling in
 * anything from c.lib. abort() (libc/src/signal.c) got the exact same
 * treatment for the exact same reason, replaced with a direct
 * ExitProcess() kernel32 call: switching only write() away from c.lib
 * was not enough on its own to stop pulling in fd.c.obj (this project's
 * own archive-member resolution evidently still reached it via some
 * other path from abort()'s own call chain, not worth fully tracing
 * once the fix is "depend on zero c.lib symbols, not on tracing exactly
 * which one" -- confirmed for real: the duplicate-read failure persisted
 * with write() alone changed, and disappeared once abort() was too).
 * The two messages that used to carry numeric detail (magic/version/bits
 * values) are static strings -- these are die-immediately-after paths
 * for a toolchain-emitted pseudo-reloc table this project's own build
 * always controls, not a runtime data condition worth a hand-rolled
 * formatter for.
 */

#include <stddef.h>
#include <stdint.h>

typedef unsigned long crt_dword;
typedef int crt_bool;
__declspec(dllimport) void* __stdcall GetStdHandle(crt_dword nStdHandle);
__declspec(dllimport) crt_bool __stdcall WriteFile(
    void* hFile, const void* lpBuffer, crt_dword nNumberOfBytesToWrite,
    crt_dword* lpNumberOfBytesWritten, void* lpOverlapped);
__declspec(dllimport) void __stdcall ExitProcess(crt_dword uExitCode);

static void pseudo_reloc_die(const char* msg, size_t len) {
  crt_dword written = 0;
  /* STD_ERROR_HANDLE, a real, stable Win32 constant. */
  WriteFile(GetStdHandle((crt_dword)-12), msg, (crt_dword)len, &written, 0);
  /* Not this project's own abort(): that lives in libc/src/signal.c and
   * pulls in enough of c.lib's own archive-member resolution to collide
   * with a client's own shadow read() (see the file comment above for
   * the full story). ExitProcess() needs nothing beyond kernel32. */
  ExitProcess((crt_dword)-1);
}
#define PSEUDO_RELOC_DIE(literal) pseudo_reloc_die(literal, sizeof(literal) - 1)

typedef struct {
  uint32_t sym;
  uint32_t target;
  uint32_t flags;
} crt_pseudo_reloc_item_v2;

typedef struct {
  uint32_t magic1;
  uint32_t magic2;
  uint32_t version;
} crt_pseudo_reloc_v2_header;

/* The PE image's own DOS header lives at its load base -- the standard,
 * zero-API-call way CRT startup code on Windows finds its own base
 * address before anything else (including kernel32) is guaranteed
 * ready. The linker always defines this symbol automatically for every
 * PE image; only the extern declaration is needed here, never a
 * definition, and the symbol's content is never read -- only its own
 * address is used. */
extern char __ImageBase;

/* Weak, not a hard reference: lld only defines these automatically when
 * the link actually contains a non-empty pseudo-reloc table to bound --
 * exactly like ELF's __init_array_start/_end only existing when a
 * .init_array section does (see libc/src/arch/linux/common/
 * init_fini_array.c's own comment for that side of the same pattern).
 * Confirmed directly: linking this project's own CMake-native code
 * (which never references anything needing auto-import) failed with
 * "undefined symbol: __RUNTIME_PSEUDO_RELOC_LIST__" before these were
 * made weak. Genuinely unresolved-and-weak COFF symbols resolve to a
 * NULL address on this toolchain -- already relied on elsewhere (see
 * exit.c's __crt_run_fini_array). */
extern char __RUNTIME_PSEUDO_RELOC_LIST__ __attribute__((weak));
extern char __RUNTIME_PSEUDO_RELOC_LIST_END__ __attribute__((weak));

#if defined(_M_IX86) || defined(__i386__)
#define CRT_WINAPI __stdcall
#else
#define CRT_WINAPI
#endif

#define CRT_PAGE_SIZE 4096u
#define CRT_PAGE_EXECUTE_READWRITE 0x40ul

__declspec(dllimport) int CRT_WINAPI VirtualProtect(void* lpAddress, size_t dwSize, unsigned long flNewProtect,
                                                     unsigned long* lpflOldProtect);

static intptr_t read_signed(const unsigned char* p, int bits) {
  switch (bits) {
    case 8: {
      int8_t v;
      __builtin_memcpy(&v, p, sizeof(v));
      return v;
    }
    case 16: {
      uint16_t v;
      __builtin_memcpy(&v, p, sizeof(v));
      return (int16_t)v;
    }
    case 32: {
      uint32_t v;
      __builtin_memcpy(&v, p, sizeof(v));
      return (int32_t)v;
    }
    case 64: {
      uint64_t v;
      __builtin_memcpy(&v, p, sizeof(v));
      return (int64_t)v;
    }
    default:
      return 0;
  }
}

static void write_truncated(unsigned char* p, int bits, intptr_t value) {
  switch (bits) {
    case 8: {
      int8_t v = (int8_t)value;
      __builtin_memcpy(p, &v, sizeof(v));
      break;
    }
    case 16: {
      int16_t v = (int16_t)value;
      __builtin_memcpy(p, &v, sizeof(v));
      break;
    }
    case 32: {
      int32_t v = (int32_t)value;
      __builtin_memcpy(p, &v, sizeof(v));
      break;
    }
    case 64: {
      int64_t v = (int64_t)value;
      __builtin_memcpy(p, &v, sizeof(v));
      break;
    }
    default:
      break;
  }
}

/* The target location a pseudo relocation patches is frequently inside
 * .text, not .data/.rdata: a compile-time-constant address-of
 * expression like `&ffi_type_sint` gets embedded directly as the
 * immediate operand of a `movabs`-style instruction rather than loaded
 * from a data section, when the compiler can fold it that way. .text is
 * mapped PAGE_EXECUTE_READ, not writable, by default -- confirmed
 * directly: without this VirtualProtect() wrapping, patching such an
 * entry access-violated immediately. Real mingw-w64's own
 * _pei386_runtime_relocator() does the same temporary unprotect/
 * restore dance for the same reason. */
static void unprotect_and_write(unsigned char* target_addr, int bits, intptr_t value) {
  size_t byte_count = (size_t)(bits > 0 ? bits / 8 : 0);
  uintptr_t page_start = ((uintptr_t)target_addr) & ~(uintptr_t)(CRT_PAGE_SIZE - 1);
  uintptr_t span_end = (((uintptr_t)target_addr + byte_count) + (CRT_PAGE_SIZE - 1)) & ~(uintptr_t)(CRT_PAGE_SIZE - 1);
  size_t span = (size_t)(span_end - page_start);
  unsigned long old_protect = 0;

  if (!VirtualProtect((void*)page_start, span, CRT_PAGE_EXECUTE_READWRITE, &old_protect)) {
    PSEUDO_RELOC_DIE("_pei386_runtime_relocator: VirtualProtect (unprotect) failed\n");
  }
  write_truncated(target_addr, bits, value);
  if (!VirtualProtect((void*)page_start, span, old_protect, &old_protect)) {
    PSEUDO_RELOC_DIE("_pei386_runtime_relocator: VirtualProtect (restore) failed\n");
  }
}

/* Neither purely-signed nor purely-unsigned range checks alone are
 * correct here: the value being validated is the runtime-resolved
 * address arithmetic result, which is legitimately either a small
 * negative displacement or a large "unsigned-looking" address bit
 * pattern depending on what kind of reference the compiler originally
 * emitted -- matching mingw-w64's own reference decoder, which accepts
 * a value fitting in EITHER interpretation of the target width. */
static int fits_in_bits(intptr_t value, int bits) {
  if (bits <= 0 || bits >= (int)(sizeof(intptr_t) * 8)) {
    return 1;
  }
  {
    intptr_t min_signed = -((intptr_t)1 << (bits - 1));
    intptr_t max_signed = ((intptr_t)1 << (bits - 1)) - 1;
    uintptr_t max_unsigned = ((uintptr_t)1 << bits) - 1;

    if (value >= min_signed && value <= max_signed) {
      return 1;
    }
    if ((uintptr_t)value <= max_unsigned) {
      return 1;
    }
  }
  return 0;
}

void _pei386_runtime_relocator(void) {
  unsigned char* base = (unsigned char*)&__ImageBase;
  unsigned char* list_start = (unsigned char*)&__RUNTIME_PSEUDO_RELOC_LIST__;
  unsigned char* list_end = (unsigned char*)&__RUNTIME_PSEUDO_RELOC_LIST_END__;
  crt_pseudo_reloc_v2_header header;
  unsigned char* cursor;

  if (list_start == list_end) {
    return;
  }
  if ((size_t)(list_end - list_start) < sizeof(header)) {
    PSEUDO_RELOC_DIE("_pei386_runtime_relocator: pseudo-reloc list too small\n");
  }
  __builtin_memcpy(&header, list_start, sizeof(header));
  if (header.magic1 != 0 || header.magic2 != 0 || header.version != 1) {
    PSEUDO_RELOC_DIE(
        "_pei386_runtime_relocator: unsupported pseudo-reloc list format "
        "-- this toolchain is only known to emit v2\n");
  }

  for (cursor = list_start + sizeof(header); cursor + sizeof(crt_pseudo_reloc_item_v2) <= list_end;
       cursor += sizeof(crt_pseudo_reloc_item_v2)) {
    crt_pseudo_reloc_item_v2 item;
    unsigned char* sym_addr;
    unsigned char* target_addr;
    intptr_t imp_addr;
    intptr_t old_value;
    intptr_t new_value;
    int bits;

    __builtin_memcpy(&item, cursor, sizeof(item));
    bits = (int)(item.flags & 0xffu);
    sym_addr = base + item.sym;
    target_addr = base + item.target;

    /* The IAT slot always holds a full native pointer, regardless of
     * "bits" (which only describes the width of the reference being
     * patched at target_addr, not the imported address itself). */
    __builtin_memcpy(&imp_addr, sym_addr, sizeof(imp_addr));
    old_value = read_signed(target_addr, bits);
    /* old_value was the compile-time placeholder: the reference as the
     * linker wrote it, which is sym_addr itself (the IAT slot's own,
     * link-time-computable address) plus whatever addend the original
     * reference carried (e.g. a field offset into a referenced struct).
     * Subtracting sym_addr and adding the real imported address swaps
     * the placeholder for the real one while preserving that addend. */
    new_value = old_value - (intptr_t)sym_addr + imp_addr;

    if (!fits_in_bits(new_value, bits)) {
      PSEUDO_RELOC_DIE("_pei386_runtime_relocator: relocation result out of range\n");
    }
    unprotect_and_write(target_addr, bits, new_value);
  }
}
