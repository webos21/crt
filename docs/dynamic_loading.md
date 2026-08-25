# Dynamic Loading

This document records the first `libdl` policy for CRT.

## Goal

The `libdl` surface gives rebuilt Linux/Bionic-style source code a common
dynamic loading API across the project targets:

- `dlopen`
- `dlsym`
- `dlclose`
- `dlerror`
- `dladdr`
- `dl_iterate_phdr`

This is a source-portability layer. It is not yet an Android linker
implementation, an ELF loader, or a full Bionic namespace-compatible dynamic
linker.

## Public Surface

The public surface is split between `include/dlfcn.h` and `include/link.h`.
It defines the common flags, handles, image lookup, and loaded-image iteration
needed by configure probes, libunwind, and runtime tests:

- `RTLD_LAZY`
- `RTLD_NOW`
- `RTLD_LOCAL`
- `RTLD_GLOBAL`
- `RTLD_DEFAULT`
- `RTLD_NEXT`

`dlerror` follows the usual one-shot behavior: after a failing operation,
`dlerror()` returns a process-local diagnostic string once, then returns `NULL`
until the next failure.

`dladdr()` is implemented on all three hosts. `dl_iterate_phdr()` reports real
ELF program headers on Linux; macOS/Windows use the documented empty-iteration
result because Mach-O/PE images do not have ELF program headers. The permanent
`dl_iterate_phdr_dladdr_test` covers those host-specific contracts.

## Source Layout

`libdl/src/dl.c` is the host-independent dispatcher: it owns `dlerror()`
state and the shared handle validation, then calls into a per-host backend
under `libdl/src/arch/{linux,macos,windows}/dl_*.c`. Each backend implements
`crt_dl_backend_open`/`crt_dl_backend_sym`/`crt_dl_backend_close`
(`libdl/src/dl_internal.h`); all three return the shared `CRT_DL_MAIN_HANDLE`
sentinel from `dlopen(NULL, ...)` rather than a real per-host handle.

`dl.c`'s `dlclose()` rejects `RTLD_DEFAULT`/`RTLD_NEXT` as invalid handles, but
treats `CRT_DL_MAIN_HANDLE` as a harmless no-op success without ever calling
into a backend -- matching real `dlopen()`/`dlclose()` behavior on Linux and
macOS, where `dlopen(NULL)` does not correspond to an actual loadable/
unloadable resource. This matters most on Windows: `crt_dl_backend_open()`
deliberately returns the sentinel instead of `GetModuleHandleA(0)` so that
`dlclose()` can never reach `FreeLibrary()` with the process's own main
executable module, which does not carry a `LoadLibrary`-style reference count
and must not be freed this way. (The reference Windows `dlfcn` implementation,
dlfcn-win32, special-cases this the same way, just per-call inside its own
`dlclose()` instead of once centrally.) An earlier version of this dispatcher
rejected `CRT_DL_MAIN_HANDLE` in `dlclose()` on all three hosts instead of
treating it as success; that broke the documented Linux contract below
(`tests/dl_test.c` catches this on every host now, not just Linux).

## Host Backend Policy

### Windows

Windows maps `libdl` calls to Kernel32:

- `dlopen(path, flags)` -> `LoadLibraryA(path)`
- `dlopen(NULL, flags)` -> the shared `CRT_DL_MAIN_HANDLE` sentinel (see
  Source Layout above), not `GetModuleHandleA(NULL)` directly
- `dlsym(handle, symbol)` -> `GetProcAddress`, resolving `RTLD_DEFAULT` and
  the sentinel to `GetModuleHandleA(NULL)` first
- `dlclose(handle)` -> `FreeLibrary`, except the sentinel, which `dl.c`
  handles centrally as a no-op success and never forwards here

`RTLD_DEFAULT` maps to the main module only, not every loaded DLL: unlike
dlfcn-win32 (the reference Windows `dlfcn` implementation), this backend does
not walk `EnumProcessModules()` to search all loaded modules. `RTLD_NEXT` is
not implemented.

The flags are accepted for source compatibility but are not yet mapped to loader
policy. Windows does not expose ELF-style local/global symbol scopes through
this backend.

### macOS

`dlopen`/`dlsym` use dyld's public image-introspection API plus a
project-owned Mach-O export-trie parser, instead of importing the host
`dlopen` symbol or relying on the legacy `NSAddImage`/`NSLookupSymbolInImage`/
`NSAddressOfSymbol` API (deprecated since Mac OS X 10.5):

- `dlopen(path, flags)` first looks for `path` among dyld's already-loaded
  images (`_dyld_image_count`/`_dyld_get_image_name`/`_dyld_get_image_header`),
  matching by full path or basename. Every system library qualifies -- dyld
  loads them all before `main()` runs, whether or not they exist as plain
  on-disk files (they do not, on macOS 11 "Big Sur" and later: system
  libraries live only inside the dyld shared cache). `NSAddImage` is kept only
  as a fallback for genuinely not-yet-loaded images.
- `dlopen(NULL, flags)` returns a private main-program handle, and `dlsym` on
  that handle (or `RTLD_DEFAULT`) scans all loaded dyld images in order.
- `dlsym(handle, symbol)` walks the target image's export trie directly:
  it locates `LC_DYLD_EXPORTS_TRIE` (falling back to the older
  `LC_DYLD_INFO[_ONLY]` `export_off`/`export_size` for images built before
  that load command existed), converts the ULEB128-encoded trie the way
  dyld's own `MachOLoaded::trieWalk()`/`findExportedSymbol()` do
  (`apple-oss-distributions/dyld`, `dyld3/MachOLoaded.cpp`, reimplemented in
  C), and follows re-exports two ways: an individual trie node carrying the
  `REEXPORT` flag (a renamed re-export of one specific symbol), and -- the
  common case for umbrella libraries -- falling through to every
  `LC_REEXPORT_DYLIB` dependency's own trie when the symbol is not present in
  the current image's trie at all. `libSystem.B.dylib` itself is a good
  example of the latter: its own export trie is only ~120 bytes despite
  exposing thousands of symbols, because it re-exports essentially everything
  from ~39 `LC_REEXPORT_DYLIB` dependencies such as `libsystem_kernel.dylib`
  and `libsystem_c.dylib`.
- Address computation uses the image's real ASLR/shared-cache slide from
  `_dyld_get_image_vmaddr_slide`, not the mach header's own runtime address --
  those coincide for a traditional standalone PIE dylib (where `__TEXT`'s
  vmaddr is 0), but not for a dylib living in the dyld shared cache, where
  `__TEXT.vmaddr` is some large cache-relative base instead. Getting this
  wrong was the first version of this fix; it read from a wild pointer
  computed against the wrong base and crashed immediately.
- Only 64-bit Mach-O is parsed, matching this project's x86_64/aarch64-only
  scope.

`dlclose` validates the handle and then returns success for loaded image
handles; this first tranche does not unload Mach-O images.

### Linux

Linux is intentionally not wired to host glibc/libdl in this tranche.

CRT Linux executables are linked with `-nostdlib`, `-nostartfiles`, and
`-nodefaultlibs`, and the project owns its startup, syscall, pthread, errno,
stdio, and public ABI boundary. Calling the host glibc `dlopen` would pull the
process across two libc worlds and would create symbol ownership and ABI
questions before the CRT dynamic linker exists.

For now, Linux supports only:

- `dlopen(NULL, flags)` returning a private main-program handle;
- `dlclose` on that private handle;
- clear `dlerror` diagnostics for unsupported real loads/lookups.

True Linux `dlopen` support should be implemented in one of two explicit future
directions:

- a project ELF dynamic linker under `linker/`, with `libdl` forwarding into it;
- a narrowly documented host-libdl bridge for tooling-only builds, kept separate
  from the freestanding CRT runtime profile.

The first option better matches the project goal.

The broader linker/loader policy is documented in `docs/linker_loader.md`.

## Bionic Compatibility Notes

Android Bionic `libdl` is a frontend to Android's dynamic linker. That stack
includes behavior this tranche does not yet implement:

- ELF shared object loading and relocation;
- DT_NEEDED dependency loading;
- Android linker namespaces;
- Android-specific extension APIs such as `android_dlopen_ext`;
- `RTLD_NEXT` lookup;
- full `RTLD_LOCAL`/`RTLD_GLOBAL` behavior;
- symbol versioning policy;
- final unload/reference-count semantics;
- Android search paths, soname rules, and linker diagnostics.

The current project-owned implementation is therefore Bionic-shaped at the
public API level, but not Bionic-complete at the dynamic-linker behavior level.

## Next Steps

The public bootstrap, `dladdr()`, `dl_iterate_phdr()`, and host-native shared
artifact model are implemented. Remaining work is loader-level rather than
header-level:

1. Decide when Linux needs a project ELF linker rather than the current honest
   no-real-load policy; do not introduce a host-glibc bridge into the runtime
   profile as a shortcut.
2. Add `RTLD_NEXT`, scope/version/search-path behavior, and
   `android_dlopen_ext` only with a real loader implementation that can honor
   them.
3. Add reference counting/unload semantics after real multi-image loading is
   owned by the project.
4. Coordinate export/version policy with `docs/shared_libraries.md` before
   calling the dynamic ABI stable.
