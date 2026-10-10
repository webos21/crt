# Shared Library Artifacts

This document owns shared-artifact naming, linkage, and export policy.
[Dynamic loading](dynamic_loading.md) owns runtime loading and the deferred ELF
loader plan; [distribution](../guides/distribution.md) owns SDK packaging checks.

## Goal

CRT now builds both static and host-native shared artifacts for the core runtime
libraries:

- `libc`
- `libm`
- `libdl`
- `libc++`

The current test executables still link against the static archives by default.
Shared artifacts are produced and installed for shared consumers and
static/shared porting tests alongside the freestanding static test path.

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
success for all attach/detach events; on process attach it also runs the
project's PE pseudo-relocator and installs the DWARF-unwind hardware-fault
safety net. It remains the hook for any later DLL-local TLS/destructor policy.

These pre-1.0 artifacts do not yet promise a final stable dynamic ABI.

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

Core exports remain broad. Windows core DLL targets use CMake automatic
exports, with focused hygiene checks for helpers such as `_fltused`. Linux
`libc` and `libdl` already use `CRT_1.0` version scripts in their respective
CMake files to separate CRT symbol binding from host glibc. Their `global: *`
rule is not a public-symbol allowlist. Upper libraries can apply stricter rules;
for example, [crtui privacy acceptance](../acceptance/crtui_acceptance.md) checks
its explicit public exports on ELF, Mach-O, and PE.

Before calling these libraries ABI-stable, the project needs:

- an exported symbol allowlist;
- hidden visibility for private helpers;
- per-library symbol-version and visibility policy beyond broad namespaces;
- macOS exported symbols list/install name policy;
- Windows `.def` or explicit `__declspec(dllexport)` policy;
- tests that compare exported symbols across targets.

The first Windows export hygiene test is `windows_export_hygiene_runs`. It reads
the generated PE export tables and fails if `_fltused` leaks into a DLL public
surface.

## Loader Boundary

Producing a native shared artifact does not implement runtime `dlopen` of
arbitrary CRT-built ELF objects. See [host backend policy](dynamic_loading.md#host-backend-policy)
and the [project ELF-loader decision](dynamic_loading.md#project-elf-loader-decision-and-milestones).

## Next Steps

Static/shared artifacts are part of the default build graph, Windows has a
permanent export-hygiene regression, and port recipes exercise real shared
load/run paths on all three hosts. The current distribution dependency/path
hardening gates are complete; the broader shared-library and ABI roadmap is:

1. Add equivalent exported-symbol allowlist/visibility checks on Linux and
   macOS, then tighten the current broad Windows exports.
2. Define stable soname/install-name/version policy for core and imported C++
   runtime libraries.
3. Add a small project-owned shared probe when it can test behavior not already
   covered by the real porting shared round trips.
