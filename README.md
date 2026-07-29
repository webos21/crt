# CRT

CRT is an experimental Bionic-compatible OS Abstraction Runtime / PAL.

The project aims to provide a low-level runtime surface, based on Android Bionic
libc, that makes Linux, BSD, and Android native source code easier to rebuild
across Linux, Windows, macOS, and Android.

The first deliverable looks like a C runtime library, but the broader goal is a
portable low-level foundation for native libraries and applications.

## Goal

CRT exposes a Bionic-compatible libc/API/ABI surface while hiding host operating
system differences behind explicit platform adaptation layers.

The central idea is:

> If Linux/BSD/Android-style low-level runtime facilities are made portable at
> the libc/PAL layer, upper layers can focus on graphics, application lifecycle,
> event integration, packaging, and host UX instead of repeatedly solving files,
> sockets, threads, TLS, memory mapping, dynamic loading, errno, signals, clocks,
> and process basics for each OS.

This project focuses on rebuild-based source portability. It is not a container,
VM, subsystem, or full foreign userspace emulator.

## Non-Goals

CRT is not trying to provide:

- Docker/LXC-style Linux execution.
- A Windows Subsystem for Linux style environment.
- Unmodified execution of arbitrary Linux binaries.
- Unmodified execution of Android APKs.
- A complete Android framework, HAL, Binder service, package manager, or ART
  environment.
- Full glibc binary ABI compatibility for existing Linux binaries.
- Direct ports of specific GUI toolkits such as Qt, GTK, or Enlightenment.

Large projects such as Qt, GTK, Enlightenment, Chromium, or Chrome are useful as
examples and long-term compatibility benchmarks, not as immediate porting
targets.

## Scope

The intended runtime scope includes:

- `libc`
- `libm`
- `libdl`
- `libstdc++` / C++ ABI support
- dynamic linker support, later
- platform adaptation layers for Linux, Windows, macOS, and Android
- architecture support for x86_64 and aarch64 only

## Stack

The current baseline decisions are:

- Base runtime: Android Bionic libc
- Language: C99 + architecture-specific assembly
- Primary compiler: LLVM Clang
- Primary linker: LLD
- Compiler runtime: compiler-rt
- C++ runtime direction: libunwind and libc++
- Build system: CMake
- Default generator: Ninja
- Test integration: CTest

The project ABI intentionally follows the Linux/Bionic-style runtime surface
rather than each host OS ABI where those differ. In particular, all CRT targets
are built with `-Xclang -fwchar-type=int` so `wchar_t` and wide string literals
are signed 32-bit on Windows as well as on Linux/macOS. External libraries built
against the CRT sysroot must use the same flag.

Rust may be used later for tooling or optional internal modules behind a stable C
ABI, but the core runtime must remain buildable without requiring Rust.

## Current Status

The repository currently contains the first executable bring-up:

- a minimal public `unistd.h`
- minimal public `errno.h`, `fcntl.h`, `string.h`, and `sys/types.h`
- a tiny `libc.a` with `_exit`, `errno`, `read`, `write`, `open`, `close`,
  `lseek`, `mmap`/`munmap`, bootstrap `malloc`/`free`/`calloc`/`realloc`, and
  first-tranche string/memory and fd-backed stdio functions
- macOS x86_64/aarch64 startup and syscall assembly
- Linux x86_64/aarch64 startup and syscall assembly
- Windows x86_64/ARM64 startup and Win32-backed low-level write/exit
  implementation
- freestanding `Hello World`, string/memory, fd/errno, malloc, stdio, stdio
  file, printf, mmap, math, locale, wchar/mbstate, and pthread-oriented tests
- sysroot installation for headers, `crt1.o`, `libc.a`, `libm.a`, and
  compiler-rt builtins

On macOS, normal Mach-O executables must still link `libSystem.dylib`. The test
does this explicitly while using this project's `_start`, `write`, `_exit`, and
direct Darwin syscall wrappers for the hello path.

On Linux, the current fd path uses direct Linux syscall wrappers.

On Windows, the current fd path uses a small POSIX-like fd table over Win32 APIs
such as `GetStdHandle`, `CreateFileA`, `ReadFile`, `WriteFile`, `CloseHandle`,
`SetFilePointerEx`, `ExitProcess`, and address-based wait/wake primitives, so
Windows executables link the relevant Windows SDK import libraries.

The current allocator is a VM-backed bootstrap heap with a locked free list. It
uses anonymous `mmap` chunks and is intended to support early libc/PAL tests, not
production allocation behavior. Future allocator work should evaluate the
appropriate Bionic allocator integration.

The current VM layer supports anonymous private `mmap` and `munmap`. Linux and
macOS use direct syscalls. Windows maps anonymous allocations to
`VirtualAlloc`/`VirtualFree`; file-backed mappings are not implemented yet.

The current pthread layer keeps a project-owned, Bionic-shaped public ABI across
Linux, macOS, and Windows. It supports create, join, detach, exit, once, keys
with destructor passes, mutexes, condition variables, rwlocks, spin locks,
barriers, and the first Bionic extension surface such as `pthread_getattr_np`
and `pthread_gettid_np`. Cancellation and robust mutexes intentionally return
`ENOTSUP`, matching the current project policy. Scheduler attributes are stored
like Bionic attr objects, but host scheduler application is deferred. User
stacks are accepted by `pthread_attr_setstack`; Linux and macOS can apply them,
while Windows reports `ENOTSUP` from `pthread_create` because `CreateThread`
cannot consume arbitrary caller-owned stacks.

The current stdio layer is intentionally minimal. It supports standard streams,
`fopen`/`fclose`, `fseek`/`ftell`, EOF/error state helpers, `remove`/`rename`,
and simple byte-oriented I/O, but it does not yet define a final `FILE` ABI or
buffering model. The current `printf` family is a small bootstrap formatter for
early tests, not a complete C/POSIX formatter.

The current `libm.a` has a bootstrap `math.h`, classification macros, absolute
value, sign handling, current-Bionic-style builtin `sqrt`/`sqrtf`, a
project-owned portable long-double bootstrap, and curated Bionic/FreeBSD msun
imports for the first accuracy tranches. It is not yet a full fdlibm/msun/Bionic
math import.

The default libm error policy is still `math_errhandling == 0`: math functions
do not promise per-function `errno` side effects or strict IEEE exception
raising yet. The `fenv` API itself is backed by hardware state where available:
x86_64 tracks MXCSR plus x87 control/status words, and AArch64 tracks FPCR/FPSR.

`long double` follows the active compiler target ABI. The build intentionally
does not pass `-mlong-double-64`, `-mlong-double-80`, or `-mlong-double-128`.
This means Linux AArch64 can expose 128-bit `long double`, x86_64 targets can
use their compiler-selected 80-bit or 128-bit mode, and Windows/macOS ARM64 can
keep their native double-sized `long double`. Bionic's 64-bit Android ABI uses
128-bit long double, but forcing that ABI uniformly is not portable across the
project's Windows/macOS/Linux target matrix.

Public scalar types are centralized through a small `bits/` layer. The first
file, `bits/crt_types.h`, fixes the Bionic/Linux-style ABI for shared types such
as `off_t`, `time_t`, `ssize_t`, `socklen_t`, and inode/device counters while
still allowing host data-model differences such as Windows LLP64 `long`. The
policy and test coverage are documented in `docs/header_abi.md`.

## Prerequisites

### Common

All platforms need:

- Git
- CMake 3.25 or newer
- LLVM Clang
- compiler-rt from the active Clang installation

Ninja is the default generator for all host presets.

### Linux

Install:

- CMake
- Ninja
- Clang/LLVM
- LLD
- compiler-rt

Example package names vary by distribution, but the required commands should be
available on `PATH`:

```sh
git --version
cmake --version
ninja --version
clang --version
ld.lld --version
```

### Windows 11

Install:

- Git for Windows
- CMake
- Ninja
- LLVM/Clang
- Visual Studio Build Tools 2022 with the Windows SDK

The Windows SDK provides import libraries such as `kernel32.lib` and
`synchronization.lib`, which are needed by the current Windows backend. Use a
Developer PowerShell or Developer Command Prompt so the Windows SDK library
paths are visible to the linker.

The configure preset sets `CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY` because
CRT controls its own startup and C runtime boundary. This avoids CMake's default
compiler check trying to link a hosted MSVC runtime executable before CRT has
configured its own targets.

The build also disables CMake's default Windows C standard libraries for CRT
targets. CMake tries to locate the required Windows SDK import libraries from
the installed SDK. If that fails, either run from a Visual Studio Developer
shell or pass them explicitly:

```powershell
cmake --preset windows-host-ninja-debug `
  -DCRT_WINDOWS_KERNEL32_LIB="C:\Path\To\kernel32.lib" `
  -DCRT_WINDOWS_SYNCHRONIZATION_LIB="C:\Path\To\synchronization.lib"
```

Useful checks:

```powershell
git --version
cmake --version
ninja --version
clang --version
lld-link --version
where kernel32.lib
where synchronization.lib
```

### macOS

Install:

- Xcode or Xcode Command Line Tools
- CMake
- Ninja

Useful checks:

```sh
xcode-select -p
clang --version
cmake --version
ninja --version
```

## Provenance Checks

Imported Bionic and FreeBSD/msun source provenance is tracked in
`third_party/bionic/import_manifest.json`. After changing imported files or
their policy, run:

```sh
python3 tools/check_import_manifest.py
```

When CMake finds Python, the same check is available as:

```sh
cmake --build --preset macos-host-ninja-debug --target check-import-manifest
```

## Build

CTest reads generated metadata from each `out/<preset>/` directory. After adding
or removing tests, run the matching `cmake --preset ...` step on every host
before comparing test counts. The configure log prints `CRT registered tests`
for the selected OS preset; the count should match across macOS, Linux, and
Windows unless a test is intentionally gated by platform.

CTest does not build missing test executables. If CTest reports `Unable to find
executable`, rerun the matching build preset or use the workflow preset:

```sh
cmake --workflow --preset <os-host-ninja-debug>
```

### macOS

The currently verified preset is macOS host debug:

```sh
cmake --preset macos-host-ninja-debug
cmake --build --preset macos-host-ninja-debug
ctest --preset macos-host-ninja-debug
```

Or run configure, build, and test in one step:

```sh
cmake --workflow --preset macos-host-ninja-debug
```

Install the project sysroot into the build directory:

```sh
cmake --build --preset macos-host-ninja-debug --target sysroot
```

The generated sysroot currently contains:

```text
out/macos-host-ninja-debug/sysroot/
  include/
    errno.h
    fcntl.h
    stdio.h
    string.h
    stdlib.h
    sys/mman.h
    sys/types.h
    unistd.h
  lib/
    crt1.o
    libc.a
    libclang_rt.builtins.a
```

### Linux

On a Linux host with Clang and Ninja installed:

```sh
cmake --preset linux-host-ninja-debug
cmake --build --preset linux-host-ninja-debug
ctest --preset linux-host-ninja-debug
cmake --build --preset linux-host-ninja-debug --target sysroot
```

Or run configure, build, and test in one step:

```sh
cmake --workflow --preset linux-host-ninja-debug
```

The test executable is expected at:

```text
out/linux-host-ninja-debug/tests/hello_c
```

### Windows 11

Open a Developer PowerShell or Developer Command Prompt with the Windows SDK
environment configured, then run:

```powershell
cmake --preset windows-host-ninja-debug
cmake --build --preset windows-host-ninja-debug
ctest --preset windows-host-ninja-debug
cmake --build --preset windows-host-ninja-debug --target sysroot
```

Or run configure, build, and test in one step:

```powershell
cmake --workflow --preset windows-host-ninja-debug
```

The test executable is expected at:

```text
out/windows-host-ninja-debug/tests/hello_c.exe
```

The Windows bring-up intentionally links `kernel32` for the OS boundary while
still avoiding the hosted C runtime with `-nostdlib`, `-nostartfiles`, and
`-nodefaultlibs`.

## Repository Layout

```text
docs/
include/
platform/
arch/
cmake/
libc/
libm/
libdl/
libstdc++/
linker/
tests/
```

Only some of these directories exist today. The layout reflects the intended
long-term structure.

## Design Documents

See:

- `docs/project_meanings.md`
- `docs/project_stacks.md`
- `docs/hello_bringup.md`
