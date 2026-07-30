# Shared Library Artifacts

This document records the first shared-library artifact policy for CRT.

## Goal

CRT now builds both static and host-native shared artifacts for the core runtime
libraries:

- `libc`
- `libm`
- `libdl`
- `libc++`

The current test executables still link against the static archives by default.
Shared artifacts are produced and installed so the project can start validating
symbol ownership, loader boundaries, and future `dlopen`/linker work without
destabilizing the existing freestanding test path.

## Artifact Policy

Static libraries remain the bootstrap baseline:

- `libc.a`
- `libm.a`
- `libdl.a`
- `libc++.a`

Shared libraries are host-native artifacts:

- Linux: `.so`
- macOS: `.dylib`
- Windows: `.dll` plus import library

On Windows the DLL import libraries intentionally use distinct names such as
`c_dll.lib`, `m_dll.lib`, `dl_dll.lib`, and `c++_dll.lib`. This avoids
colliding with the static archives `c.lib`, `m.lib`, `dl.lib`, and `c++.lib` in
the same `lib/` output directory.

Windows shared libraries also use a project-owned minimal DLL entry point,
`crtDllMainCRTStartup`, because the CRT build links with `-nostdlib` and does
not import MSVC's `_DllMainCRTStartup`. The entry point currently returns
success for all attach/detach events. It is the future hook for DLL-local
runtime initialization, TLS, and destructor policy.

These are not yet final ABI-stable shared runtimes. They are build artifacts for
the next compatibility tranche.

## Link Policy

Shared runtime targets use the same freestanding build flags as the static
targets and are linked with explicit runtime boundaries:

- `-nostdlib`
- `-nodefaultlibs`
- compiler-rt builtins where needed
- `libSystem` only where macOS requires host loader/system support
- Windows SDK import libraries only for the explicit Win32 boundary

The purpose is to prevent hosted libc or C++ runtime libraries from silently
entering the shared CRT boundary.

## Dependency Policy

The first dependency graph is:

- `libc` is the base runtime.
- `libm` is built as an independent math artifact for now.
- `libdl` links against shared `libc`.
- `libc++` links against shared `libc`.

This is an artifact policy, not a final dynamic dependency ABI. Future work must
decide exact sonames/install names, symbol visibility, versioning, and whether
`libm`, `libdl`, and `libc++` should depend on shared `libc` or remain partially
self-contained for specific bootstrap profiles.

## Export Policy

Exports are intentionally broad in this tranche. Windows uses CMake's automatic
export support for the first DLL artifacts, with focused hygiene checks for
known private compiler helpers such as `_fltused`. Linux/macOS do not yet use
version scripts or export lists.

Before calling these libraries ABI-stable, the project needs:

- an exported symbol allowlist;
- hidden visibility for private helpers;
- Linux version script policy;
- macOS exported symbols list/install name policy;
- Windows `.def` or explicit `__declspec(dllexport)` policy;
- tests that compare exported symbols across targets.

The first Windows export hygiene test is `windows_export_hygiene_runs`. It reads
the generated PE export tables and fails if `_fltused` leaks into a DLL public
surface.

## Loader Boundary

Shared artifacts do not mean the project ELF loader exists yet.

Windows and macOS shared libraries are loaded by the host-native loader. Linux
shared libraries can be built, but freestanding Linux `dlopen` still does not
call host glibc/libdl by default. Loading CRT-built ELF shared objects remains a
future `linker/` milestone documented in `docs/linker_loader.md`.

## Next Steps

Recommended next work:

1. Add artifact presence tests for static and shared runtime outputs.
2. Add exported-symbol inspection tests per host.
3. Define soname/install name/import library naming policy.
4. Add a tiny project-owned shared probe library for host-native loader smoke
   tests on Windows/macOS.
5. Decide the first Linux ELF loader milestone before enabling Linux runtime
   `dlopen` of CRT-built `.so` files.
