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

Rust may be used later for tooling or optional internal modules behind a stable C
ABI, but the core runtime must remain buildable without requiring Rust.

## Current Status

The repository currently contains the first executable bring-up:

- a minimal public `unistd.h`
- minimal public `errno.h`, `fcntl.h`, `string.h`, and `sys/types.h`
- a tiny `libc.a` with `_exit`, `errno`, `read`, `write`, `open`, `close`,
  `lseek`, bootstrap `malloc`/`free`/`calloc`/`realloc`, and first-tranche
  string/memory and fd-backed stdio functions
- macOS x86_64/aarch64 startup and syscall assembly
- Linux x86_64/aarch64 startup and syscall assembly
- Windows x86_64/ARM64 startup and Win32-backed low-level write/exit
  implementation
- freestanding `Hello World`, string/memory, fd/errno, malloc, stdio, and
  stdio file tests
- sysroot installation for headers, `crt1.o`, `libc.a`, and compiler-rt builtins

On macOS, normal Mach-O executables must still link `libSystem.dylib`. The test
does this explicitly while using this project's `_start`, `write`, `_exit`, and
direct Darwin syscall wrappers for the hello path.

On Linux, the current fd path uses direct Linux syscall wrappers.

On Windows, the current fd path uses a small POSIX-like fd table over Win32 APIs
such as `GetStdHandle`, `CreateFileA`, `ReadFile`, `WriteFile`, `CloseHandle`,
`SetFilePointerEx`, and `ExitProcess`, so the executable links `kernel32`.

The current allocator is a fixed-size bootstrap heap with a simple free list. It
is intended to support early libc/PAL tests, not production allocation behavior.
Future allocator work should move to a host VM-backed heap and then evaluate the
appropriate Bionic allocator integration.

The current stdio layer is intentionally minimal. It supports standard streams,
`fopen`/`fclose`, `fseek`/`ftell`, and simple byte-oriented I/O, but it does not
yet define a final `FILE` ABI, buffering model, or `printf` family.

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

The Windows SDK provides `kernel32.lib`, which is needed by the current Windows
hello backend. Use a Developer PowerShell or Developer Command Prompt so the
Windows SDK library paths are visible to the linker.

The configure preset sets `CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY` because
CRT controls its own startup and C runtime boundary. This avoids CMake's default
compiler check trying to link a hosted MSVC runtime executable before CRT has
configured its own targets.

The build also disables CMake's default Windows C standard libraries for CRT
targets. Only `kernel32.lib` is linked for the current hello backend. CMake tries
to locate `kernel32.lib` from the installed Windows SDK. If that fails, either
run from a Visual Studio Developer shell or pass it explicitly:

```powershell
cmake --preset windows-host-ninja-debug -DCRT_WINDOWS_KERNEL32_LIB="C:\Path\To\kernel32.lib"
```

Useful checks:

```powershell
git --version
cmake --version
ninja --version
clang --version
lld-link --version
where kernel32.lib
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

## Build

### macOS

The currently verified preset is macOS host debug:

```sh
cmake --preset macos-host-ninja-debug
cmake --build --preset macos-host-ninja-debug
ctest --preset macos-host-ninja-debug
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
