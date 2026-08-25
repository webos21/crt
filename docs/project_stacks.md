# Project Stacks

## Base Runtime Choice

The base runtime for this project is Android Bionic libc.

This choice is driven by both licensing and technical fit. The project is not
trying to build a generic POSIX libc from scratch. It is trying to build a
Bionic-compatible OS Abstraction Runtime/PAL that lets Linux, BSD, and Android
native source code be rebuilt across Linux, Windows, macOS, and Android.

Because of that goal, Bionic is a better starting point than musl, newlib, or a
generic embedded libc.

## Why Bionic

Bionic has several properties that match the project goals:

- Bionic is Android's real libc/libm/libdl/dynamic-linker stack.
- Bionic is permissively licensed. Android's Bionic documentation describes the
  code as BSD licensed.
- Bionic already includes the Android-facing libc API and ABI surface.
- Bionic already has a syscall wrapper model based on `SYSCALLS.TXT` and
  generated syscall stubs.
- Bionic has an established process for using cleaned Linux kernel UAPI headers.
- Bionic is already designed around Linux kernel interfaces while keeping a small
  libc implementation style.
- Bionic is the natural baseline for Android native code and AOSP native
  libraries.

The license point is especially important. A low-level runtime will be linked
into many libraries and applications, including potentially commercial or
closed-source software. A permissive license reduces adoption friction and keeps
the runtime suitable as a broad portability foundation.

## Linux Syscall And Kernel Header Considerations

Linux kernel UAPI headers are a special licensing case. The Linux kernel is
GPL-2.0, but UAPI headers commonly use the `Linux-syscall-note` exception. The
kernel documentation describes the syscall boundary as a boundary that does not
extend GPL requirements to normal user programs using kernel services.

Android's Bionic stack already has a cleaned-kernel-header flow. The original
kernel headers live in Android's `external/kernel-headers`, and Bionic generates
clean userland headers from them. Android's documentation notes that these
cleaned headers are intended to be safely included by userland applications and
libraries.

For this project, that means the Linux syscall and UAPI surface should be handled
carefully but does not need to be avoided. The recommended policy is:

- Prefer Bionic's cleaned kernel headers where possible.
- Track original UAPI provenance and license metadata.
- Keep generated headers reproducible from known upstream kernel header inputs.
- Preserve SPDX/license notices and generation scripts.
- Avoid copying arbitrary internal Linux kernel headers.
- Treat syscall wrappers as part of the PAL contract rather than exposing
  uncontrolled Linux-specific behavior everywhere.

## Why Not musl As The Base

musl is a strong libc implementation and is MIT licensed. It is small, clean,
correctness-oriented, and targets the Linux syscall API. It is a good reference
for POSIX behavior, static linking, and simple implementation style.

However, musl is not the best base for this project because:

- musl is primarily a Linux/POSIX libc, not an Android/Bionic-compatible runtime.
- Android/Bionic ABI details would need to be reintroduced on top of musl.
- Bionic's linker/libdl/syscall/header conventions are more directly relevant.
- The project goal is not generic POSIX conformance first; it is
  Bionic-compatible source portability first.

musl should still be used as a reference for selected algorithms, behavior
comparisons, tests, and Linux compatibility questions, subject to license review.

## Why Not newlib As The Base

newlib is useful for embedded and bare-metal systems. It is designed so that a
platform can provide a small set of low-level OS routines such as `_open`,
`_read`, `_write`, `_sbrk`, `_close`, and related hooks.

That model is useful, but it is not the best fit here:

- This project targets large Unix-like native software, not primarily bare-metal
  or small embedded systems.
- newlib does not provide the Android/Bionic ABI surface.
- newlib's OS hook model is lower-level and less aligned with Linux/BSD/Android
  userspace portability.
- newlib is a collection of code under multiple free software licenses, which
  increases file-by-file license review work.

newlib may be useful as a reference for freestanding build patterns and simple
stdio/stdlib behavior, but it should not be the base runtime.

## Other Alternatives

### LLVM libc

LLVM libc is a serious alternative and a valuable reference. It is modular,
multiplatform, and licensed under Apache-2.0 with LLVM exceptions. Its license
also includes a patent grant, which is attractive for broad reuse.

However, LLVM libc is not Bionic-based. Its full host support is strongest on
Linux, while macOS and Windows support are documented as partial and less
continuously tested. It is best treated as a design and implementation reference,
especially for modular entrypoints, platform bring-up, testing, and correctness.

### BSD libcs

FreeBSD and NetBSD libc code is generally attractive from a licensing
perspective because BSD-family projects prefer permissive licenses. BSD libc code
can be a good reference for behavior, headers, and historical Unix semantics.

However, BSD libc implementations are naturally shaped around BSD kernels and BSD
system interfaces. They are not a direct fit for Android/Bionic compatibility or
Linux syscall-oriented PAL design.

### Picolibc

Picolibc is a small embedded libc derived from newlib and AVR libc, with mostly
BSD-like licensing. It is interesting for small runtime design and embedded
configuration.

It is not a strong base for this project because the target is large
Linux/BSD/Android native source portability rather than constrained embedded
systems.

### mlibc

mlibc, used by Managarm, is particularly interesting as a design reference. Its
documentation describes it as a libc designed with portability in mind, with a
clean syscall abstraction layer for new OS ports.

This is close to the architectural shape of this project. However, mlibc is not
Bionic-based and does not provide Android's libc/linker/libdl conventions.

Use mlibc as a PAL and portability-design reference, not as the base runtime.

## Stack Policy

The recommended stack policy is:

- Base implementation: Bionic.
- Kernel interface source: Bionic cleaned kernel headers and carefully tracked
  Linux UAPI inputs.
- PAL design references: mlibc, LLVM libc, Drawbridge, and Gramine/Graphene.
- Compatibility references: musl, BSD libcs, Cosmopolitan Libc, libhybris, and
  gVisor.
- Avoid as base: glibc, uClibc-ng, and other LGPL/GPL-heavy libc stacks unless a
  specific component has a clearly isolated and acceptable license boundary.

## Language Policy

The primary implementation language should be C99, with architecture-specific
assembly where necessary.

This is not because Rust is unsuitable for systems programming. Rust can be used
to implement libc-like components, and Redox OS provides an important precedent:
its `relibc` is a C/POSIX library written largely in Rust. Rust is also becoming
increasingly relevant for low-level systems work through Rust for Linux.

However, this project's public surface is a C ABI. It must provide predictable C
headers, symbol names, structure layouts, calling conventions, startup behavior,
TLS behavior, errno behavior, signal behavior, pthread behavior, and dynamic
linker interactions. These are naturally aligned with C, Bionic's existing code,
and small pieces of assembly.

For that reason, the core runtime should be C99-first:

- libc/libm/libdl public ABI should be implemented and exposed as C ABI.
- Bionic-derived code should stay close to its original C/C++ shape unless there
  is a strong reason to rewrite it.
- syscall stubs, startup code, setjmp/longjmp, atomics, TLS, and calling
  convention-sensitive pieces should use C and assembly.
- Public headers should remain C/C++ consumable without requiring Rust tooling.
- The project should not require Rust just to build the minimal libc/PAL.

Rust can still be valuable in selected places:

- build tools, code generators, syscall table processors, and validation tools;
- internal PAL modules with a stable `extern "C"` boundary;
- parsers and state machines where memory safety is especially valuable;
- optional compatibility services;
- future graphics/application runtime components;
- test harnesses and fuzzing tools.

If Rust is used inside the runtime, it should follow strict rules:

- expose only `extern "C"` interfaces across the C/Rust boundary;
- use `#[repr(C)]` for shared data structures;
- avoid Rust panics across FFI boundaries, preferably using `panic = "abort"`;
- avoid depending on Rust `std` in core runtime code;
- prefer `no_std` plus explicit allocator and panic behavior for low-level
  modules;
- keep initialization order, TLS use, allocator use, and unwinding behavior
  explicit;
- make Rust an optional dependency until the C99 base is stable.

The recommended language strategy is therefore:

- Core runtime: C99 + assembly.
- Existing Bionic code: preserve C/C++ structure.
- Tooling: C, Python, or Rust as practical.
- Optional safety-sensitive new internals: Rust is allowed behind C ABI.
- Public contract: always C ABI.

## Compiler And Build System Policy

The primary compiler toolchain should be LLVM Clang, with LLD as the primary
linker and compiler-rt as the preferred compiler runtime companion.

The primary build system should be CMake with Ninja as the default generator.
Tests should be integrated through CTest. Hand-written Makefiles may exist only
as convenience wrappers, not as the authoritative build graph.

The recommended baseline is:

- Language: C99 + assembly.
- Primary compiler: LLVM Clang.
- Primary linker: LLD.
- Compiler runtime: compiler-rt.
- Build system: CMake.
- Default generator: Ninja.
- Tests: CTest.
- Configuration: `CMakePresets.json` plus target-specific CMake toolchain files.

This choice is made for portability and long-term scale. The project must support
Linux, Windows, macOS, and Android across x86_64 and aarch64. Clang and LLD
provide a relatively consistent target-triple, sysroot, freestanding,
`-nostdlib`, and cross-compilation story across those hosts and targets.

CMake is a conservative choice for a large C/C++ runtime project because it can
generate Ninja, Make, Visual Studio, and Xcode projects; integrates with CTest;
works well with LLVM projects; and is widely supported by IDEs and native
library ecosystems. Ninja should be the default generator because it is fast,
simple, and predictable.

The intended user workflow should look like:

```sh
cmake --preset linux-x86_64-debug
cmake --build --preset linux-x86_64-debug
ctest --preset linux-x86_64-debug
```

The build layout should include explicit toolchain files, for example:

```text
cmake/
  toolchains/
    linux-x86_64-clang.cmake
    linux-aarch64-clang.cmake
    windows-x86_64-clang.cmake
    windows-aarch64-clang.cmake
    macos-x86_64-clang.cmake
    macos-aarch64-clang.cmake
    android-x86_64-clang.cmake
    android-aarch64-clang.cmake

CMakePresets.json
CMakeLists.txt
```

Secondary compiler validation should be added after the Clang build is stable:

- GCC on Linux, to catch accidental Clang-only assumptions.
- Apple Clang on macOS, where system integration requires it.
- MSVC ABI checks on Windows, especially for interoperability with native
  Windows C/C++ libraries, while keeping Clang as the primary compiler.

### Why Not Make As The Primary Build

Make is universally available and useful for small projects, but it should not be
the primary build graph for this project. The runtime will need many build
variants: host OS, target OS personality, architecture, sysroot, static/shared
libraries, freestanding flags, sanitizer options, generated headers, generated
syscall stubs, and platform-specific tests.

Maintaining that matrix directly in Make would become fragile. CMake plus Ninja
provides a more manageable model while still allowing a small top-level
`Makefile` as a convenience entrypoint if desired.

### Why Not Bazel First

Bazel is attractive for very large monorepos, remote caching, hermetic builds,
and multi-language builds. It may become useful later, especially if the project
needs tight integration with AOSP-style or Chromium-scale build workflows.

However, Bazel should not be the first-class build system at the beginning. It
adds significant complexity around C/C++ toolchain definitions, platform
configuration, sysroots, and Windows/macOS integration. Those costs are not
worth paying before the libc/PAL architecture is stable.

The recommended policy is:

- Start with CMake + Ninja as the authoritative build.
- Keep the source layout friendly to future Bazel integration.
- Consider Bazel later for remote cache, large CI, or AOSP/Chromium integration.

### Why Not Meson First

Meson is also a good build system. It is fast, modern, and has strong
cross-compilation support. It is a reasonable alternative.

The reason to prefer CMake is ecosystem conservatism and compatibility. CMake has
broader adoption across large C/C++ native projects, LLVM-related projects,
IDEs, package managers, and platform toolchains. For this runtime, broad
compatibility matters more than build-system elegance.

## Freestanding, Sysroot, And Runtime Library Policy

The build must explicitly control the C runtime boundary. This project is itself
providing libc/PAL pieces, so it must not accidentally depend on the host libc,
host startup files, or host default runtime libraries in core runtime builds.

Core runtime targets should be built with freestanding assumptions where
appropriate:

- Use `-ffreestanding` for low-level libc/PAL objects that must not assume a
  hosted C environment.
- Use `-fno-builtin` or targeted `-fno-builtin-<name>` when implementing symbols
  such as `memcpy`, `memmove`, `memset`, `strlen`, `malloc`, or other functions
  the compiler might otherwise treat specially.
- Use `-ffunction-sections` and `-fdata-sections` so unused runtime code and data
  can be removed by the linker.
- Use linker section garbage collection for final executable and shared-library
  links:
  - ELF/Linux: `-Wl,--gc-sections`.
  - Mach-O/macOS: `-Wl,-dead_strip`.
  - PE/COFF Windows: `-Wl,/OPT:REF`.
- Use `-nostdlib`, `-nostartfiles`, or `-nodefaultlibs` for final runtime and
  low-level test links where accidental host runtime linkage would invalidate
  the result.
- Keep hosted build tools separate from freestanding runtime objects.

The project should define its own sysroot layout early. The sysroot is the
contract consumed by rebuilt libraries and applications, and should contain this
runtime's headers, libraries, startup objects, and target metadata rather than
falling through to the host system layout.

A practical sysroot shape is:

```text
sysroot/
  include/
  lib/
    crt1.o
    crti.o
    crtn.o
    libc.a
    libc.so
    libm.a
    libm.so
    libdl.so
    libc++.so
    libunwind.a
    libclang_rt.builtins.a
```

Sysroot policy:

- Every target tuple should have an explicit sysroot.
- Toolchain files should pass `--sysroot` or the platform-equivalent setting.
- Header search paths should prefer the project sysroot before host headers.
- Library search paths should prefer the project sysroot before host libraries.
- Tests should include checks that forbidden host libraries are not linked into
  freestanding runtime artifacts.
- Generated headers and startup objects should be installed into the sysroot as
  part of the build.

LLVM runtime components should be integrated deliberately:

- Use compiler-rt builtins for compiler helper symbols such as integer division,
  atomics, overflow helpers, and architecture-specific compiler support.
- Use libunwind where stack unwinding is needed, especially for C++ exception
  support and backtraces.
- Use libc++ as the preferred C++ standard library when C++ standard library
  support is needed.
- Keep libc++ and libunwind integration separate from the minimal C libc/PAL
  bring-up so that the C runtime can boot independently.

Current implementation status (2026-08-25): the project builds its pinned
AOSP `libc++abi` and `libc++` lane for Linux, macOS, and Windows. The
project-owned `libunwind` lane is used on Linux and Windows; macOS deliberately
uses libSystem's unwinder while retaining the same imported Itanium C++ ABI
surface. The sequence below is therefore the completed bring-up order, not an
open task list.

The layering should be:

1. Minimal startup objects and freestanding libc/PAL.
2. compiler-rt builtins.
3. libm and broader libc.
4. libunwind.
5. libc++ ABI support and C++ standard library integration.
6. shared-library and dynamic-loading support.

This order keeps the base runtime testable before the project depends on C++
exceptions, unwinding, shared-library loading, or higher-level runtime behavior.

## Conclusion

Bionic is the right base because the project is not merely a libc project. It is
a Bionic-compatible low-level runtime for rebuilding Linux/BSD/Android native
software across host operating systems.

musl, newlib, LLVM libc, BSD libc, Picolibc, and mlibc all remain useful
references. But none of them combine Android API/ABI relevance, Linux syscall
orientation, cleaned UAPI header flow, libdl/linker context, and permissive
licensing as directly as Bionic does.
