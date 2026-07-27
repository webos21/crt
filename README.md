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
- a tiny `libc.a` with `write` and `_exit`
- macOS x86_64/aarch64 startup and syscall assembly
- a freestanding `Hello World` test
- sysroot installation for headers, `crt1.o`, `libc.a`, and compiler-rt builtins

On macOS, normal Mach-O executables must still link `libSystem.dylib`. The test
does this explicitly while using this project's `_start`, `write`, `_exit`, and
direct Darwin syscall wrappers for the hello path.

Linux bring-up should remove that macOS-specific exception and verify a fully
project-owned ELF startup/libc path.

## Build

The currently verified preset is macOS host debug:

```sh
cmake --preset macos-host-debug
cmake --build --preset macos-host-debug
ctest --preset macos-host-debug
```

Install the project sysroot into the build directory:

```sh
cmake --build --preset macos-host-debug --target sysroot
```

The generated sysroot currently contains:

```text
out/macos-host-debug/sysroot/
  include/
    unistd.h
  lib/
    crt1.o
    libc.a
    libclang_rt.builtins.a
```

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
