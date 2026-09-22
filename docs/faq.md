# CRT FAQ

Short answers about what CRT is, why it is shaped the way it is, and how it
differs from tools people often compare it with. For what works today, see the
[README](../README.md) and [`STATUS.md`](../STATUS.md); those are the evidence,
this page is the explanation. CRT is pre-1.0 developer software.

## Why CRT?

Linux, BSD, and Android-style native code is only as portable as the work done
to move it. Files, sockets, threads, thread-local storage, memory mapping,
dynamic loading, and process startup differ on every host, so each project
repeats the same porting effort.

CRT puts that effort in one place: a Bionic-shaped libc/libm/libdl and C++
runtime over a small per-host Platform Adaptation Layer (PAL). You rebuild the
same source and get native executables for Linux, Windows, and macOS. Upper
stages add a window and input layer, Skia, FFmpeg, and GPU presentation through
each host's own API (Wayland/Vulkan, Win32/D3D12, Cocoa/Metal).

The primary target is a Linux-kernel embedded device: set-top boxes, HMIs,
automotive IVI. Native desktop builds give those teams a fast development and
debugging loop.

## Why a Bionic-shaped interface?

Bionic is Android's libc, libm, libdl, and dynamic-linker stack, and it fits the
goal better than the alternatives:

- It is permissively (BSD) licensed. A low-level runtime is linked into
  everything, including closed-source software, so license friction matters.
- It already defines an Android-facing API and ABI surface, a syscall wrapper
  model, and a cleaned Linux kernel UAPI header flow.
- It keeps a small libc implementation style around Linux kernel interfaces.
- It is the natural baseline for Android NDK code and AOSP native libraries.

The goal is Bionic-compatible source portability, not generic POSIX
conformance. Bionic itself is not a POSIX superset (for example, some POSIX IPC
functions, `<aio.h>`, robust mutexes, and `pthread_cancel` are absent or
limited), so CRT does not claim full POSIX or glibc compatibility.

More detail: [`project_stacks.md`](project_stacks.md) and
[`project_meanings.md`](project_meanings.md).

## What is "rebuild-based source portability"?

You compile your source, with a Clang/LLD-based toolchain that you provide,
against a CRT sysroot for the host you are targeting. The result is an ordinary
native executable in that host's format.

- **Not binary compatibility.** Existing Linux binaries and Android APKs do not
  run on CRT.
- **Not a VM.** There is no guest kernel and no emulated hardware.
- **Not a container.** There is no image, namespace, or Linux environment at
  run time.
- **No bundled compiler.** A CRT distribution is a sysroot, startup objects,
  runtime libraries, wrappers, CMake configuration, and a manifest. See
  [`distribution.md`](distribution.md).

**Do I have to change my code?** Ideally not. When a library needs a
Bionic-compatible API that CRT lacks, the fix goes into CRT rather than into the
upstream source. Most ported projects carry no source patch, and the exceptions
are recorded, with reasons, in their recipes (see the Portability Proof in the
README). Code that needs an API CRT does not implement yet will not build until
it is added.

## How does CRT compare to...?

### musl

musl is a small, correctness-oriented libc (MIT licensed) that targets the
Linux syscall API. If you only target Linux and want that libc, use musl. CRT is
Bionic-shaped instead, adds Windows and macOS host adapters, and carries the
graphics and media stages above the libc. musl remains a useful reference for
behavior comparisons and tests.

### SDL

SDL is a mature cross-platform API for windows, input, audio, and graphics that
you write your application against. CRT's window and input layer is much
smaller and is not an SDL replacement. CRT addresses the layer below: making
Linux/Android-style C and C++ code, and the libraries it depends on, rebuild
natively. Choose SDL when you want a portable multimedia API for a new
application.

### Qt

Qt is a complete application framework with widgets, QML, and its own platform
plugins. CRT is not a UI toolkit and has no widget or layout system. Qt is not a
porting target for CRT; it is only a useful example of how hard host-specific
low-level porting can get. Choose Qt when you want a full UI framework.

### WSL

WSL runs Linux userspace inside a Linux environment on Windows, so existing
Linux binaries run as they are. CRT builds native Windows executables from
source, with no Linux environment at run time. Use WSL to run Linux software
unmodified; use CRT when you want a native Windows or macOS build of code you
can rebuild.

### Wine

Wine runs Windows applications on POSIX hosts by translating Windows API calls
to the host. CRT goes the other way: it rebuilds Linux/Android-style source as
native Windows and macOS programs, and it does not translate existing binaries.

### Cosmopolitan

Cosmopolitan Libc aims for one portable executable that runs on several
operating systems, using its own libc and executable format. CRT produces a
separate native executable per host (ELF, PE, or Mach-O) from a Bionic-shaped
libc. Choose Cosmopolitan when a single-file portable binary is the goal.

### Android / Bionic

Bionic is Android's libc; CRT follows its API and ABI shape and imports its
sources. Bionic assumes the Android/Linux kernel and is not an official Windows
or macOS target, which is the gap CRT's PAL fills. CRT is not Android: there is
no Android framework, APK runtime, ART, or Binder, and Android is not a current
CRT host.

## What CRT is not

- Not Android APK compatibility, and not an Android framework.
- Not an Electron clone. Electron and Chromium are, at most, a later portability
  benchmark.
- Not full POSIX compatibility, and not glibc binary compatibility.
- Not a VM, container, or WSL replacement.
- Not a direct port of Qt, GTK, or Enlightenment.
- Not a production-ready `1.0`: interfaces may still change.

## Is it ready to use?

It is a developer preview in preparation. The graphics/media stage
(`04-gfx-media`) is the current milestone, verified on Linux, Windows, and
macOS with the per-host limits listed in the README's capability matrix.
Hardware H.264 decode is verified on all three hosts (VideoToolbox on macOS,
D3D11VA on Windows, VA-API on Linux). The JavaScript stage (`05-js`) is
roadmap work, not a supported feature.

## What do I need to build with it?

Git, CMake 3.25+, Ninja, Python 3, Clang, and LLD on Linux, Windows 11, or macOS
(64-bit x86 and ARM), plus host graphics/audio prerequisites for the upper
stages. Windows also needs Developer Mode enabled. See the README's
[Prerequisites](../README.md#prerequisites) and
[Build](../README.md#build) sections.

## Where can I read more?

- [Project meaning](project_meanings.md) and [stack policy](project_stacks.md)
- [Distribution stages](distribution.md) and [runtime roadmap](runtime_roadmap.md)
- [Current status](../STATUS.md), [work queue](../TODO.md), and
  [history](../HISTORY.md)
