# Project Meanings

## Goal

CRT is a Bionic-compatible OS Abstraction Runtime / PAL for Linux, Windows,
and macOS on x86_64 and aarch64. Its first visible layer is a C runtime, while
its purpose is to let Linux/BSD/Android-style native source rebuild across
those hosts with less repeated low-level porting work. Android supplies the
libc/API/ABI reference; it is not a target operating system.

The central hypothesis is that a common contract for files, sockets, threads,
TLS, memory mapping, errno, signals, clocks, process basics, and dynamic loading
lets upper layers concentrate on graphics, input, application lifecycle,
packaging, and host UX. Compatibility claims apply to implemented and tested
surface, not all POSIX, glibc, or Android behavior.

## Product And Development Workflow

The intended consumers include Linux-kernel embedded products such as set-top
boxes, industrial HMIs, IVI systems, kiosks, and small game consoles. Native
Linux, Windows, and macOS builds provide an application development and
debugging loop using the same source. Separate desktop product builds are also
within scope.

A desktop run validates the application and the exercised CRT contracts; it
does not emulate a board's timing, drivers, memory footprint, or input devices.
Board-specific graphics, audio, capture, and controller support still require
an implemented backend and target-device acceptance. Raspberry Pi is a possible
product use case, not a blanket board-support claim. Host GPU adapters also
retain host driver dependencies; an in-process worker thread is not a crash
isolation boundary.

Deliverables are cumulative SDK stages with headers, startup objects, runtime
libraries, wrappers, and build metadata. Consumers provide the external
compiler/toolchain and choose the capability level they need. The authoritative
stage order and scope live in the [runtime roadmap](runtime_roadmap.md); package
contents and predecessor-only builds live in the
[distribution contract](../guides/distribution.md).

## Architecture

1. **Bionic-compatible public surface:** C headers, types, symbols, errno,
   pthreads, and C/C++ ABI expectations.
2. **Runtime core:** allocation, stdio, locale/time/path policy, process
   abstractions, and shared helpers.
3. **PAL and architecture code:** OS-specific mechanisms and calling-convention,
   TLS, atomics, and startup code under each library's
   `src/arch/{linux,macos,windows}/{x86_64,aarch64,common}`.
4. **Consumer-driven compatibility modules:** add a facility when a real
   rebuilt consumer requires it. Bionic ancestry alone does not require Binder,
   Android properties, Android framework services, or an entire Linux desktop.
5. **Optional upper stages:** `libcrtgfx`, `libcrtmedia`, `libcrtui`, and
   `libcrtweb`, keeping graphics/media/UI/web contracts above libc/PAL.

The core C runtime remains usable without the upper stages. Graphics separates
window/input/presentation from Skia drawing; media owns its frame contracts and
graphics consumes them through explicit bridges. Public headers do not inherit
host SDK types or a private dependency's object model. The
[Host ABI firewall](host_abi_firewall.md) defines object and allocator ownership.

## Source Portability Contract

The development loop is an upstream configure/build/link/run attempt, followed
by inspection of the corresponding Bionic public API and behavior, a CRT/PAL
fix, and replay of the same consumer. Do not mask missing runtime support with
host headers or an undocumented upstream patch. Port-specific exceptions need
recorded reasons and removal conditions. See [sysroot porting](../porting/sysroot_ports.md)
and [AGENTS.md](../../AGENTS.md).

WebKit is the direct `06-web` porting target, with WPE WebKit as its reference
and `PlatformCRT` as the CRT port. Qt, GTK, Enlightenment, and Chromium are
examples or later portability benchmarks. A complete desktop environment,
mandatory QuickJS stage, or Electron replacement is outside the current plan.

Existing Linux/glibc binaries, Android APKs, and source-unavailable foreign
libraries do not become portable merely through a Bionic-shaped ABI. The
runtime is not a VM, container, WSL environment, or hardware simulator. It makes
no safety-certification, zero-overhead, fixed-boot-time, or cross-device
pixel-identity guarantee. Capability and performance claims need the relevant
host/configuration evidence.

The reserved `linker/` directory is separate from native executable delivery.
A project ELF loader remains deferred; [dynamic loading](dynamic_loading.md)
defines when to revisit it and how module ownership must work.

## Design References

The original research informs the boundary design rather than adding product
requirements:

- [Bionic](https://android.googlesource.com/platform/bionic/+/main) supplies the
  source/API baseline; AOSP's pinned
  [`linux_bionic` build metadata](https://android.googlesource.com/platform/build/bazel/+/4442aebf/platforms/BUILD.bazel)
  was an early Linux-host reference.
- [Drawbridge](https://www.microsoft.com/en-us/research/project/drawbridge/overview/)
  and [Graphene/Gramine](https://oscarlab.github.io/projects/graphene/) are
  Library OS/PAL separation references.
- [libhybris](https://github.com/libhybris/libhybris) is a reference for
  Bionic/host-runtime boundary problems, not CRT's binary-compatibility contract.
- [Stack choices](project_stacks.md) records libc, compiler, language, and build
  alternatives; [graphics boundary research](libcrtgfx_wayland_plan.md) records
  compositor references. [FAQ](../guides/faq.md) holds the short comparisons.

Current support belongs in [STATUS](../../STATUS.md), open work in
[TODO](../../TODO.md), and completed investigations in
[history](../history/README.md). Those records supersede early feasibility
ratings and speculative market-positioning drafts.
