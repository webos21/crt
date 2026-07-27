# Project Meanings

## Goal

The project aims to build a Bionic-based OS Abstraction Runtime that can be used
on Linux, Windows, macOS, and Android. Its first visible form is a C Runtime
Library, but the real target is broader than a simple libc port.

The runtime should expose a Bionic-compatible libc/API/ABI surface while hiding
host OS differences behind explicit platform adaptation layers. If this low-level
abstraction is provided, libraries and applications originally written for Linux,
BSD, or Android should become easier to rebuild and move to Windows, macOS,
Linux, and Android with fewer source changes.

The central hypothesis is:

> If Linux/BSD/Android-style low-level runtime facilities are made portable at
> the libc/PAL layer, then upper layers can focus on graphics, application
> lifecycle, event integration, packaging, and host UX instead of repeatedly
> solving files, sockets, threads, memory mapping, TLS, dynamic loading, and
> related OS primitives.

The intended scope includes libc, libm, libdl, libstdc++, and eventually the
dynamic linker, with support limited to 64-bit x86 and 64-bit ARM.

In short, the project is best described as:

> A Bionic-compatible OS Abstraction Runtime, or a Bionic-flavored Library OS/PAL,
> for improving rebuild-based source portability of Linux/BSD/Android native code
> across Linux, Windows, macOS, and Android.

## Similar Projects And Prior Art

There does not appear to be a single mature project with exactly the same goal.
However, several projects overlap with important parts of this direction.

| Project | Similarity | Difference |
| --- | --- | --- |
| [AOSP Bionic `linux_bionic`](https://android.googlesource.com/platform/build/bazel/+/4442aebf/platforms/BUILD.bazel) | Android's official build metadata already defines `linux_bionic_x86_64` and `linux_bionic_arm64`, meaning a Linux kernel plus Bionic runtime without the rest of Android. | This is effectively Linux-focused and does not provide a Windows or macOS native CRT target. |
| [Bionic](https://android.googlesource.com/platform/bionic/+/main) | This is the real Android implementation of `libc`, `libm`, `libdl`, and the dynamic linker. It is the most important source baseline. | Bionic assumes Android/Linux kernel behavior. Windows and macOS are not official portability targets. |
| [Waydroid](https://waydro.id/) | Runs Android userspace on Linux with near-native behavior by using LXC, Linux namespaces, Binder, and Wayland integration. | It is Linux-only and container-based. It does not provide a portable CRT for native Windows or macOS programs. |
| [Redroid](https://hub.docker.com/r/redroid/redroid/) | Runs Android in Docker, Podman, or Kubernetes on Linux hosts, with x86_64 and arm64 support. | It depends on Linux kernel features such as Binder and ashmem. Windows and macOS are only clients, not native host targets. |
| [libhybris](https://github.com/libhybris/libhybris) | Bridges glibc or musl userspace with Android libraries that depend on Bionic, especially for reusing Android vendor libraries and HALs. | It is mainly a compatibility bridge for Android binary blobs, not a full Bionic-derived portable CRT. |
| [Winedroid](https://winedroid.soham.sh/) | Presents a very close conceptual goal: Android apps running on Linux and macOS without a VM, container, or full Android OS. | It focuses on APK/framework compatibility. It is not primarily a libc/linker/CRT project, and Windows does not appear to be a target. |
| [Cosmopolitan Libc](https://justine.lol/cosmopolitan/) | Provides a cross-platform libc and executable model for Linux, macOS, Windows, and other systems, including amd64 and arm64. | It is not based on Bionic. It uses its own libc and portable executable strategy. |
| [LLVM libc](https://libc.llvm.org/) | A modular, multiplatform libc project that targets Linux, bare metal, UEFI, GPU, macOS, and Windows. | It is not Bionic-based. Linux is the main full-build focus; macOS and Windows support are documented as partial and less continuously tested. |
| [Wine](https://www.winehq.org/about/) | A mature compatibility-layer model that translates one OS API surface into host-native calls without full hardware emulation. | It targets Windows applications on POSIX systems, not Android/Bionic programs. |
| [Darling](https://www.darlinghq.org/) | Similar compatibility-layer model for running macOS software on Linux, including Darwin, Mach, dyld, and launchd concepts. | It is Darwin/macOS-focused rather than Android/Bionic-focused. |
| [Drawbridge](https://www.microsoft.com/en-us/research/project/drawbridge/overview/) | Demonstrates the Library OS model: an OS personality runs in-process and talks to the host through a small ABI. | It implemented a Windows personality, not Android/Bionic. |
| [Gramine/Graphene](https://oscarlab.github.io/projects/graphene/) | Implements Linux compatibility through a Library OS plus a Platform Adaptation Layer. This is structurally close to this project's intended architecture. | It is Linux-personality oriented and historically glibc-centered, not Bionic-centered or cross-host desktop focused. |
| [gVisor](https://gvisor.dev/docs/) | Implements a substantial Linux system-call surface in userspace and demonstrates that partial syscall compatibility can support many real workloads. | It is a container sandbox/runtime, not a Bionic libc or cross-platform native CRT. |

## Important Observations

AOSP already contains useful clues for a Linux-first path. Its build platform
definitions include `linux_bionic_x86_64` and `linux_bionic_arm64`, and Bionic's
linker build files include a `linker_wrapper` path for host Linux Bionic
binaries. This suggests that Linux can be treated as the first practical target
for a standalone Bionic-based runtime.

By contrast, modern AOSP platform development is officially Linux-host oriented.
Android 11 and later platform development on macOS is not supported by the
official setup documentation. Windows is even farther from the default AOSP
build model. Therefore, making Android source build and run as native programs on
Windows and macOS should be treated as a separate portability project, not a
simple rebuild of AOSP.

## Evaluation

The project direction is technically meaningful and has strong precedent in
Library OS, compatibility layer, and portable libc work. However, the project
should not be framed merely as "porting Bionic libc". A more accurate framing is
to put the Bionic API/ABI surface at the top and design a small, explicit Host
ABI or Platform Adaptation Layer underneath it.

The strongest supporting prior art comes from Library OS research. Microsoft
Research's Drawbridge work shows that an OS personality can run inside an
application address space and communicate with the host through a small set of
abstractions. Its Windows 7 Library OS prototype ran large real applications with
lower overhead than a full virtual machine.

Graphene/Gramine reaches a similar conclusion from the Linux compatibility side.
It separates compatibility into two parts:

- API and syscall emulation in the Library OS.
- Host-specific behavior hidden behind a Platform Adaptation Layer.

This separation closely matches the desired shape of this project. The Bionic
runtime would provide the Android/Linux-facing personality, while
`platform/linux`, `platform/windows`, `platform/macos`, and `platform/android`
would provide the host adaptation.

The idea is especially attractive for source portability. Many Linux, BSD, and
Android native libraries depend on a familiar libc, pthreads, sockets, files,
memory mapping, TLS, dynamic loading, and errno behavior. A Bionic-compatible
runtime can reduce repeated per-project porting work by centralizing those
differences.

The main risk is that libc alone is not enough for modern source portability.
The paper "POSIX Abstractions in Modern Operating Systems" studied Android, OS
X, and Ubuntu and found that modern applications rely heavily on high-level
frameworks and non-standard abstractions. For this project, that does not mean
we must emulate a full Linux, Android, or BSD runtime. Instead, it means the PAL
must eventually cover the low-level facilities that large native codebases expect
when they are rebuilt: files, sockets, memory mapping, shared memory, threading,
TLS, atomics, time, process control, dynamic loading, and selected OS extension
points.

GUI toolkits such as Qt, GTK, and Enlightenment are useful examples of the
problem shape, but they are not direct project goals. They show how complicated
OS-specific porting becomes when low-level runtime behavior differs across hosts.
If this project succeeds, toolkit and application work should be able to start
above a more consistent low-level runtime and concentrate on graphics,
window-system integration, application lifecycle, input, fonts, clipboard,
accessibility, and packaging.

Bionic itself is also not a complete POSIX superset. Android's Bionic status
documentation lists unsupported or intentionally omitted functionality such as
some POSIX IPC functions, locale limitations, `<aio.h>`, robust mutexes, and
`pthread_cancel`. This is acceptable if Android/Bionic compatibility is the
primary target, but it means the runtime should not claim general glibc or full
POSIX compatibility without separate work.

## Compatibility Boundaries

This project is a good fit for:

- Android native code and NDK-style C/C++ source portability.
- Linux, BSD, and Android-oriented libraries that can be rebuilt against this
  runtime.
- AOSP native libraries that primarily depend on Bionic-level facilities.
- Centralizing host differences for files, sockets, memory, threads, TLS,
  signals, clocks, process basics, dynamic loading, and extension points.
- Moving source-based native library ecosystems from Linux/BSD/Android to
  Windows and macOS with fewer per-project patches.
- Providing a common low-level base for future graphics and application runtime
  definitions.
- Using large rebuilt libraries or applications as compatibility benchmarks.

The project is not primarily trying to provide:

- Container-style Linux execution such as Docker/LXC.
- A subsystem approach such as Windows Subsystem for Linux.
- Unmodified execution of arbitrary existing Linux binaries.
- Unmodified execution of arbitrary Android APKs.
- A complete Android framework, HAL, Binder service, package manager, or ART
  environment.
- Full glibc binary ABI compatibility for existing Linux binaries.
- Direct ports of specific GUI toolkits such as Qt, GTK, or Enlightenment.

Those systems solve a different problem: running an existing foreign userspace or
binary environment. This project instead focuses on making source-available
libraries and applications easier to rebuild on top of a common Bionic-compatible
PAL.

Large GUI toolkits and applications can still be used as validation benchmarks.
For example, a project like Qt can illustrate the kind of OS-specific low-level
porting pressure this PAL is meant to reduce, without becoming a direct porting
target. A very large application such as Chromium/Chrome can be treated as a
long-term stress test for whether the rebuilt dependency ecosystem is broad
enough.

## Practical Interpretation

This project is best understood as a combination of several ideas:

- Use Bionic's source layout and behavior as the reference for libc, libm, libdl,
  libstdc++, and the linker.
- Use AOSP's `linux_bionic` target as the first proof point for Linux x86_64 and
  arm64.
- Borrow platform-abstraction ideas from LLVM libc and Cosmopolitan Libc for
  host-specific files, threads, TLS, signals, memory, and dynamic loading.
- Study libhybris for Bionic/host-libc boundary problems and Android binary
  compatibility concerns.
- Study Waydroid and Redroid to understand which Android userspace assumptions
  depend directly on Linux kernel features.
- Treat Drawbridge, Graphene/Gramine, Wine, Darling, gVisor, and Winedroid as
  long-term examples of translating one OS runtime model into another host
  runtime model.
- Treat Qt, GTK, Enlightenment, Chromium, and similar large projects as examples
  or benchmarks that reveal portability gaps, not as initial porting targets.

## Recommended Direction

The first milestone should prioritize rebuild-based source portability rather
than unmodified binary compatibility. Binary compatibility requires a stable ABI,
dynamic linker, symbol/version policy, C++ ABI stability, executable format
handling, and host process model decisions, and is not the primary project
definition.

A practical order is:

1. Define the supported compatibility level: rebuild-based source portability
   first, selected runtime compatibility later, binary compatibility only where
   it directly helps source-built libraries.
2. Establish a Linux Bionic baseline for x86_64 and arm64.
3. Split all OS-dependent behavior into explicit `platform/linux`,
   `platform/windows`, `platform/macos`, and `platform/android` layers.
4. Build a small `libc.a` subset and run host-side tests.
5. Expand platform adaptation for files, sockets, memory, threads, TLS, signals,
   clocks, and errno.
6. Add `libm` and broader libc coverage.
7. Add shared-library support and `libdl` wrappers per host OS.
8. Add C++ ABI support through `libstdc++` or a minimal Android-compatible C++
   ABI support library.
9. Add optional compatibility modules such as Android logging, properties,
   Binder client primitives, shared memory, Linux/BSD extension shims, and other
   facilities needed by source-rebuilt libraries.
10. Define a separate upper-layer graphics and application runtime only after the
   libc/PAL layer is stable enough to support large native libraries.
11. Defer `linker/` to a later phase, starting with Linux ELF support before
   considering Windows PE/COFF or macOS Mach-O implications.

The most important architectural decision is the exact portability contract:

- source portability for Android native code,
- source portability for Linux/BSD native libraries,
- ABI compatibility only for libraries rebuilt as part of this runtime ecosystem,
- optional compatibility modules required by rebuilt libraries,
- and a later, separately defined graphics/application runtime layer.

The project should avoid being pulled toward Docker/LXC, WSL, or full Android
emulation goals unless a specific source-built library requires a targeted
compatibility module.

## Architecture Implication

The runtime should be divided into clear layers:

1. Bionic-compatible public surface: headers, libc/libm/libdl/libstdc++ symbols,
   errno, pthreads, C/C++ ABI expectations.
2. Runtime core: allocator, stdio, locale policy, time, path handling, process
   abstractions, dynamic loading policy, and shared internal helpers.
3. Platform Adaptation Layer: host-specific implementations for Linux, Windows,
   macOS, and Android.
4. Architecture layer: x86_64 and aarch64 assembly, atomics, TLS details,
   calling-convention-sensitive code, and startup code.
5. Optional compatibility modules: Android log, properties, Binder client
   primitives, ashmem/memfd-style shared memory, Linux/BSD extension shims, and
   application-specific portability support.
6. Later graphics/application runtime: window-system integration, graphics API
   binding, application lifecycle, input, fonts, clipboard, accessibility, and
   packaging. This layer should be defined separately after the libc/PAL contract
   is stable.

This framing keeps the project honest: the libc is the first deliverable, but the
long-term value is the OS abstraction boundary underneath it.
