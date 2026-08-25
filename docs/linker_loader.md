# Linker And Loader Policy

This document records the long-term linker/loader direction for CRT.

## Goal

CRT ultimately needs a dynamic loading boundary that supports rebuilt
Bionic/Linux-style native code across Linux, Windows, macOS, and Android.

The first implementation does not try to replace every host loader. Instead, it
keeps two layers separate:

- `libdl`: the public `dlopen`/`dlsym`/`dlclose`/`dlerror` API surface used by
  application and library code.
- `linker/`: the future project-owned dynamic linker/loader implementation,
  primarily needed when host loaders cannot provide the Bionic/ELF behavior the
  CRT ABI expects.

## Current Policy

The current policy is host-loader-first, with an explicit exception for Linux
freestanding executables:

- Windows uses the host PE loader through `LoadLibraryA`, `GetProcAddress`, and
  `FreeLibrary`.
- macOS uses dyld image and symbol APIs.
- Linux does not call host glibc/libdl in the freestanding profile yet. `dlopen`
  supports only the main-program handle and returns `dlerror` diagnostics for
  real object loading.

This keeps the CRT libc boundary clean. Pulling host glibc `dlopen` into a
freestanding CRT process would mix two libc implementations, two errno/TLS
models, two allocator worlds, and two loader states before the project has
defined ownership rules for that mixture.

## Loader Kinds

### Host-Native Loader

The host-native loader is the first choice when the loaded module is native to
the host executable format:

- PE/COFF DLLs on Windows;
- Mach-O dylibs/bundles on macOS;
- possibly host ELF shared objects in a future non-freestanding Linux bridge
  profile.

Host-native loading is useful for platform integration, such as Windows SDK
DLLs, system frameworks, plugins, and native C ABI bridges.

Host-native loading does not by itself provide Android linker behavior. It does
not imply Android namespaces, Bionic search paths, ELF relocation semantics,
ELF TLS module handling, or Bionic symbol/version policy.

### Project ELF Loader

A project ELF loader becomes necessary when CRT needs to load rebuilt
Bionic/Linux-style ELF shared objects without depending on host glibc's loader.

This is most important for:

- Linux freestanding CRT executables;
- Android/Bionic-like `libdl` behavior;
- shared `libc.so`, `libm.so`, `libdl.so`, and C++ runtime experiments;
- ELF TLS and `pthread`/`errno` integration;
- symbol interposition and `RTLD_GLOBAL`/`RTLD_LOCAL`;
- `DT_NEEDED` dependency loading;
- relocation processing;
- `.init_array` and `.fini_array`;
- future `linker/` compatibility tests.

The project ELF loader should start on Linux first. Windows and macOS should not
try to execute ELF as a normal host-native module unless the project explicitly
chooses an ELF-in-host-process model later.

## Android/Bionic Boundary

Android Bionic's `libdl` is a frontend to Android's dynamic linker. A
Bionic-compatible public API does not mean the project already implements the
Android linker.

The Android/Bionic boundary should be split into three compatibility levels:

- **API shape**: headers, constants, and basic functions exist. This is the
  current `libdl` level.
- **Source compatibility**: common rebuilt code can call `dlopen`, `dlsym`,
  and `dlclose` and receive predictable errors or host-native behavior.
- **Runtime behavior compatibility**: ELF loading, dependency resolution,
  namespaces, TLS, relocation, init/fini arrays, and linker diagnostics behave
  close enough to Bionic for complex rebuilt libraries.

The current project is at the API shape/source compatibility level. Runtime
behavior compatibility is a future `linker/` milestone.

## Boundary Rules

The loader boundary must preserve these rules:

- `libdl` owns the public API. Applications should not call private `linker/`
  entry points directly.
- `linker/` owns ELF loader state once the project ELF loader exists.
- Host-native modules and project-loaded ELF modules are different module
  worlds unless a bridge explicitly connects them.
- C ABI is the default safe boundary between host-native modules and CRT-loaded
  modules.
- C++ objects, exceptions, RTTI, STL containers, and allocator ownership must
  not cross loader worlds without a documented ABI bridge.
- `errno`, TLS, pthread keys, thread local C++ destructors, and `__cxa_atexit`
  must have one clear owner for every loaded module.
- Search paths must be explicit. Do not silently inherit Android, glibc, dyld,
  or Windows DLL search behavior and call it Bionic compatibility.

## Completed Foundation

The following prerequisites from the original loader plan are complete:

- `libdl` provides the public host-adapter surface with permanent compile and
  runtime tests, including `dladdr()` and `dl_iterate_phdr()` coverage.
- CRT libraries are produced and exercised in both static and host-native
  shared forms on Linux, macOS, and Windows.
- startup and shared-library work covers init/fini arrays and the current
  imported C++ runtime path.
- the host-native and future project-ELF loader ownership boundary is recorded
  in this document and `docs/shared_libraries.md`.

These accomplishments do not imply that Linux can yet load arbitrary CRT-built
ELF DSOs through a project-owned loader.

## Decision Points

The project should start implementing a real ELF loader when at least one of
these becomes blocking:

- Linux `dlopen` needs to load CRT-built `.so` files.
- Linux must load CRT-built ELF DSOs without depending on the host glibc
  loader.
- ELF TLS is needed for compiler TLS, `errno`, C++ thread locals, or imported
  libraries.
- complex libraries require `DT_NEEDED`, `RPATH`/`RUNPATH`, or init/fini arrays.
- Android linker API behavior such as namespaces or `android_dlopen_ext` becomes
  a concrete porting requirement.

Until then, host-native loading remains acceptable for Windows/macOS platform
integration and small `libdl` smoke tests.

The first shared-library artifact policy is documented in
`docs/shared_libraries.md`.

## Remaining Milestones

1. Prototype a Linux-only ELF loader that can map one dependency-free shared
   object and resolve a small symbol table.
2. Add relocation support for x86_64 and AArch64.
3. Add `DT_NEEDED`, init/fini arrays, and symbol lookup scopes.
4. Add ELF TLS and connect it to pthread, errno, and C++ runtime policy.
5. Revisit Android linker namespace behavior and `android_dlopen_ext`.

## Deferred Work

These are explicitly deferred:

- executing Android APKs;
- loading Android framework or HAL modules;
- using host glibc as the CRT Linux loader backend by default;
- cross-loading ELF as a normal PE/Mach-O module on Windows/macOS;
- C++ ABI object interop across loader worlds;
- full Android linker namespace compatibility.
