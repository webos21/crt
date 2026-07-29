# Dynamic Loading

This document records the first `libdl` policy for CRT.

## Goal

The `libdl` surface gives rebuilt Linux/Bionic-style source code a common
dynamic loading API across the project targets:

- `dlopen`
- `dlsym`
- `dlclose`
- `dlerror`

This is a source-portability layer. It is not yet an Android linker
implementation, an ELF loader, or a full Bionic namespace-compatible dynamic
linker.

## Public Surface

The first public header is `include/dlfcn.h`. It defines the common flags and
handles needed by configure probes and small runtime tests:

- `RTLD_LAZY`
- `RTLD_NOW`
- `RTLD_LOCAL`
- `RTLD_GLOBAL`
- `RTLD_DEFAULT`
- `RTLD_NEXT`

`dlerror` follows the usual one-shot behavior: after a failing operation,
`dlerror()` returns a process-local diagnostic string once, then returns `NULL`
until the next failure.

## Host Backend Policy

### Windows

Windows maps `libdl` calls to Kernel32:

- `dlopen(path, flags)` -> `LoadLibraryA(path)`
- `dlopen(NULL, flags)` -> `GetModuleHandleA(NULL)`
- `dlsym(handle, symbol)` -> `GetProcAddress`
- `dlclose(handle)` -> `FreeLibrary`

`RTLD_DEFAULT` maps to the main module. `RTLD_NEXT` is not implemented.

The flags are accepted for source compatibility but are not yet mapped to loader
policy. Windows does not expose ELF-style local/global symbol scopes through
this backend.

### macOS

macOS currently uses dyld's lower-level image APIs instead of importing the
host `dlopen` symbol:

- `NSAddImage`
- `NSLookupSymbolInImage`
- `NSAddressOfSymbol`
- `_dyld_image_count`
- `_dyld_get_image_header`

Mach-O C symbols are looked up with the required leading underscore. `dlopen`
with a `NULL` path returns a private main-program handle, and `dlsym` on that
handle scans the loaded dyld images.

`dlclose` validates the handle and then returns success for loaded image handles;
this first tranche does not unload Mach-O images.

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

Recommended next work:

1. Add a `libdl` ABI/header compile test that checks public constants and
   prototypes across all targets.
2. Define whether Linux grows a project ELF linker first or a temporary
   host-libdl bridge.
3. Add `dladdr` and, if needed, Android/Bionic extension placeholders with
   documented `ENOTSUP` behavior.
4. Decide the shared library build model for `libc.so`, `libm.so`, and
   `libdl.so`.
5. Start the `linker/` tranche once ELF loading becomes the main target.
