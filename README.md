# CRT

**Linux-style C/C++ software. Native on Linux, Windows and macOS.**

Same source model. Native executables. Native GPUs. No VM or container.

CRT is a Bionic-compatible cross-platform C runtime and Platform Adaptation
Layer (PAL). It lets Linux/BSD/Android-style native libraries and applications
be rebuilt from the same source as native Linux, Windows, and macOS
executables.

The primary target is a Linux-kernel embedded device: set-top boxes,
Raspberry-Pi-class consoles, industrial HMIs, and automotive IVI systems.
Native desktop builds provide a fast development and debugging loop and can
also be shipped as desktop applications.

CRT is pre-1.0 developer software under active development. The current
milestone is the `04-gfx-media` stage (see [Where It Stands](#where-it-stands));
interfaces may still change, and it is not a production-ready `1.0`.

## Architecture

```text
Application
    |
    v
Skia / FFmpeg
    |
    v
CRT C++ Runtime
    |
    v
Bionic-compatible CRT / PAL
    |
    +-----------+-----------+
    |           |           |
  Linux       Windows      macOS
 Wayland       Win32       Cocoa
 Vulkan        D3D12       Metal
```

An application is rebuilt from the same source against CRT's Bionic-shaped
headers and libraries. Each host's PAL adapter reaches the native window
system, GPU API, and audio/video services directly, so the result is an
ordinary native executable rather than something running inside a VM,
container, or translation layer.

## Where It Stands

- **Current milestone: `04-gfx-media`.** libc/libm/libdl, the imported
  libc++/libc++abi/libunwind runtime, native window and input, Skia CPU and
  GPU rendering over Vulkan, D3D12, and Metal, FFmpeg software media, and native
  audio sinks are implemented, and the predecessor-only
  `03-gfx-simple -> 04-gfx-media` stage build is verified on Windows, macOS,
  and Linux/aarch64. The Linux/aarch64 acceptance host is a VM whose Vulkan
  device is virtio-gpu/lavapipe rather than a physical GPU; host details and
  limits are in [`STATUS.md`](STATUS.md).
- **Hardware H.264 decode is verified on macOS and Windows only.** VideoToolbox
  (macOS/arm64) and D3D11VA (Windows/x64) decode real hardware frames into the
  CPU-resident `crtmedia_frame`, with software decode as the default and the
  fallback. Linux VA-API is not yet verified because the hosts available so
  far have no working VA-API H.264 decoder; this is an environment limit, not a
  limit of the software graphics/media runtime. Hardware decode is therefore
  not claimed as cross-platform yet.
- **`05-js` is roadmap, not a supported feature.** It currently packages a
  `libcrtjs` skeleton; QuickJS, the event loop, modules, and JavaScript-visible
  graphics/media bindings are planned.

The per-capability matrix is under [What Already Works](#what-already-works).
The exact evidence, per-port results, and open work are in
[`STATUS.md`](STATUS.md), [`docs/porting_status.md`](docs/porting_status.md),
and [`TODO.md`](TODO.md). To try it from source, see
[Prerequisites](#prerequisites), [Build](#build), and
[Using A Distribution](#using-a-distribution).

## What Already Works

This is the evidence-based snapshot behind the claims above. Each cell is the
state on that host, not a promise.

- **Verified**: implemented, and an automated test or recorded acceptance run
  passed on that host.
- **Partial**: implemented and exercised, with a gap on that host stated in the
  last column.
- **In progress**: work under way, not accepted yet.
- **Planned**: not started, or skeleton only.

| Capability | Linux | Windows | macOS | Evidence and limits |
| --- | --- | --- | --- | --- |
| libc / libm (startup, stdio, files, processes, signals) | Verified | Verified | Verified | Bionic-shaped headers and ABI; full in-tree `ctest` suite. |
| libdl | Partial | Verified | Verified | `dlopen`/`dlsym` use `LoadLibrary` on Windows and dyld on macOS. Linux supports `dlopen(NULL)`, `dladdr`, `dl_iterate_phdr` and reports loading real libraries as unsupported (a CRT-owned loader is deferred). |
| libc++ / libc++abi / libunwind | Verified | Verified | Verified | Imported runtime; static and shared smoke tests including RTTI and exceptions. libunwind is project-built on Linux and Windows; macOS uses libSystem's unwinder. |
| pthread | Verified | Verified | Verified | `pthread_*` tests: mutexes, condition variables, barriers, attributes, thread-specific data, process-shared. |
| Sockets and DNS | Verified | Verified | Verified | `socket_network_test`, `dns_*`; libcurl + mbedTLS HTTP and HTTPS round trips to a real server on all three hosts. IPv4/UDP A-record resolver only (no IPv6, TCP fallback, or caching). |
| mmap | Verified | Verified | Verified | `mman_test`. |
| Thread-local storage | Verified | Verified | Verified | `pthread_tls_test`. (TLS as in HTTPS is covered by the sockets row.) |
| Native window and input | Verified | Verified | Verified | Wayland (`xdg-shell`), Win32, Cocoa; keyboard, pointer, resize, close, DPI. Linux needs a reachable compositor; WSLg is useful evidence but differs from a desktop compositor. |
| Skia CPU raster and text | Verified | Verified | Verified | Skia m148 plus FreeType text through the software frame. |
| Skia GPU (Ganesh) | Partial | Verified | Verified | Vulkan / D3D12 / Metal live presentation with pixel-exact and mid-stream resize checks. Linux evidence is from a VM with a virtio-gpu/lavapipe Vulkan device; no physical-GPU Linux run is recorded yet. |
| FFmpeg software media | Verified | Verified | Verified | Opt-in, narrow LGPL build: MOV/MP4/M4A, WAV, MP3 demux; H.264, AAC, MP3, PCM software decode; player and playback-pipeline tests. |
| Native audio output | Partial | Verified | Verified | WASAPI, CoreAudio, and ALSA or PulseAudio. Linux real-device behavior is environment dependent; WSLg's PulseAudio bridge is recorded as stopping to respond after about a second of continuous audio. |
| Hardware H.264 decode (frames delivered to CPU) | In progress | Verified | Verified | D3D11VA on Windows and VideoToolbox on macOS with real hardware frames; software decode stays the default and the fallback. Linux VA-API is blocked on a host with a working VA-API H.264 decoder. |
| Hardware decode to GPU texture (zero-copy) | Planned | Planned | Planned | Decoded surfaces are not shared with the graphics path yet. |
| Encode, capture, streaming | Planned | Planned | Planned | No mux/encode, capture, or network streaming layer exists. |
| JavaScript runtime (QuickJS) | Planned | Planned | Planned | `05-js` packages a `libcrtjs` skeleton only. |

Hosts: Linux is an aarch64 VM (the acceptance host) plus x86_64 under WSL2;
Windows is x86_64; macOS is arm64 (Apple Silicon).

Evidence dates: the full `ctest` suite last ran 149/149 on Windows/x64
(2026-09-19), 121/121 on Linux/x86_64 under WSL2 (2026-09-21; two TTY-dependent
termios tests excluded from that non-interactive run), and 132/132 on
macOS/arm64 (2026-09-18, as recorded in [`HISTORY.md`](HISTORY.md)). The
Linux/aarch64 results come from its recorded acceptance runs there. Details and
per-host limits stay in [`STATUS.md`](STATUS.md), which is authoritative if it
and this table ever disagree.

## Hardware Decode Status

CRT can decode H.264 on the platform's hardware decoder and hand each frame to
your code as a CPU-resident `crtmedia_frame`. It is opt-in, and software decode
is always the default and the fallback. Zero-copy sharing of decoded surfaces
with the GPU is not implemented yet.

| Host | Backend | Status |
| --- | --- | --- |
| macOS/arm64 | VideoToolbox | **Verified.** Real hardware frames, CPU transfer, clean end of stream, decoder flush/reuse, and 15 repeated create/decode/release cycles, all on hardware. |
| Windows/x64 | D3D11VA | **Verified** on a physical Intel GPU: the same checks, plus a GPU video-decode engine counter that reads zero when idle and non-zero while decoding. |
| Linux | VA-API | **Not verified yet.** Decoding works in software; requesting hardware falls back cleanly and reports `fallback=yes`. |

Linux VA-API is blocked by the hosts available so far, not by the software
graphics/media runtime. The aarch64 VM's Mesa driver exposes no H.264 decode
entrypoint. Under WSL2 on an Intel GPU, a real decode deadlocks inside Intel's
own WSL video driver even with a plain FFmpeg that does not involve CRT. Neither
counts as Linux hardware-decode evidence; a native Linux host with a working
VA-API H.264 decoder is still needed.

Hardware decode is therefore not claimed as cross-platform yet. Per-host
evidence and the exact result format are in
[`docs/crtmedia_hardware_decode_acceptance.md`](docs/crtmedia_hardware_decode_acceptance.md).

## Portability Proof

The portability claim is tested by rebuilding real upstream software through the
CRT sysroot with its own `configure`/`make` (or GN) flow and then running it.
Each port is a pinned, SHA-256-checked recipe under
[`porting/recipes/`](porting/recipes/), with a recorded result per host.

| Upstream | Version | Linux / Windows / macOS | What runs |
| --- | --- | --- | --- |
| zlib | 1.3.1 | static + shared pass on all three | Real compress/decompress round trip against both builds. |
| libpng | 1.6.57 | static + shared pass on all three | libpng create/write/destroy paths against both builds. |
| SQLite | 3.53.4 | amalgamation build pass on all three | Amalgamation builds without upstream source patching; recipe-level smoke and link checks only. |
| bzip2 | 1.0.8 | static + shared pass on all three | Compress/decompress round trip against both builds. |
| xz / liblzma | 5.8.3 | static + shared pass on all three | Compress/decompress round trip at the maximum preset (9, extreme) with CRC64 against both builds. |
| PCRE2 | 10.47 | static + shared pass on all three | 8-bit regular-expression matching against both builds. |
| mbedTLS | 3.6.7 | static + shared pass on all three | SHA-256 known-answer check and AES-128-CBC encrypt/decrypt round trip. |
| curl (libcurl) | 8.21.0 | static + shared pass on all three | Real HTTP and HTTPS requests through libcurl, zlib, mbedTLS, DNS, and sockets. |
| FreeType | 2.14.3 | static + shared pass on all three | Glyph rasterization from a bundled font; also feeds Skia text. |
| FFmpeg | 8.1.2 | configure/make port, pass on all three | Narrow LGPL build; `libcrtmedia` demux, software decode, and playback tests, plus hardware H.264 decode on macOS and Windows. |
| Skia | m148 | GN build against the CRT sysroot and imported libc++, pass on all three | CPU raster and text; Ganesh GPU presentation over Vulkan, D3D12, and Metal (see the Linux caveat above). |

libffi and expat also pass on all three hosts, and the toolchain `make` builds
from the same sysroot (a manual pass); per-port detail is in
[`docs/porting_status.md`](docs/porting_status.md).

Runtime evidence, beyond "it compiles":

- **curl**: real HTTP and HTTPS round trips to a public server (`example.com`),
  not a loopback test, over CRT's sockets, resolver, and non-blocking file
  descriptors, for both static and shared libcurl. These tests need network
  access.
- **Skia**: live native presentation with a machine-checkable pixel comparison
  (`SkSurface::readPixels()` against a shared reference scene), including a
  scripted mid-stream resize, on the Vulkan, D3D12, and Metal backends.
- **FFmpeg**: a real H.264 MP4 fixture demuxed and decoded frame by frame, and a
  WAV fixture decoded to an exact known sample count.

Upstream source is not patched to hide a missing CRT surface; the missing
Bionic-compatible behavior is implemented in CRT instead. Most recipes carry no
source patch. The exceptions are recorded with their reasons in each recipe: a
header guard for Windows constructor sections in xz, mbedTLS configuration and
Makefile edits, FreeType build-script fixes (paths with spaces, Mach-O
archives), and FFmpeg build-script edits for the Windows D3D11VA probes and macOS
VideoToolbox. Skia carries no source patch, only a build-time GN interpreter
pin.

To reproduce a result on your host:

```sh
cmake --build --preset <preset> --target port-test-<name>
cmake --build --preset <preset> --target port-test-recipes
```

## Why CRT?

Native Linux, BSD, and Android-style code is only as portable as the porting work
behind it: files, sockets, threads, thread-local storage, memory mapping, dynamic
loading, and startup differ on every host, and each project repeats that work.
CRT does it once, in a Bionic-shaped libc and C++ runtime over a small per-host
PAL, so the same source rebuilds as a native Linux, Windows, or macOS
executable.

- **Why Bionic-shaped?** Bionic is Android's real libc stack, permissively
  licensed, and already defines the API/ABI surface, syscall wrappers, and
  kernel-header flow that Linux/Android-style code expects. CRT aims for
  Bionic-compatible source portability, not generic POSIX conformance.
- **Rebuild, not translate.** You compile your source with your own Clang/LLD
  toolchain against a CRT sysroot. That is not binary compatibility, not a VM,
  and not a container; a CRT distribution never bundles a compiler.
- **What CRT is not.** Not Android APK compatibility, not an Electron clone, not
  full POSIX or glibc compatibility, and not a production-ready `1.0`.

For comparisons with musl, SDL, Qt, WSL, Wine, Cosmopolitan, and Android/Bionic,
see the [FAQ](docs/faq.md).

## Scope

CRT owns the low-level portability boundary: files, sockets, threads, TLS,
memory mapping, clocks, signals, process basics, dynamic-loading policy,
startup objects, libc/libm/libdl, and the C++ runtime. Higher layers add a
window/input/software-framebuffer API, accelerated graphics and media, and a
JavaScript runtime.

The compatibility model is source rebuilding against a Bionic-shaped public
surface. CRT does not aim to run unmodified glibc binaries or APKs, reproduce
Android Framework, provide a container/VM, or directly port Qt, GTK,
Enlightenment, Chromium, or Electron.

The `linker/` directory is retained for a possible future CRT-owned loader,
but linker implementation is outside the current roadmap.

## Staged Runtime

CRT is built and verified as cumulative distributions:

| Stage | Advertised surface |
| --- | --- |
| `01-c` | libc/libm/libdl, startup, shell, mksh, toybox, awk, make |
| `02-cxx` | `01-c` + libc++/libc++abi/libunwind |
| `03-gfx-simple` | `02-cxx` + window, keyboard/mouse, software framebuffer |
| `04-gfx-media` | `03-gfx-simple` + Skia CPU/GPU, Vulkan/D3D12/Metal, FFmpeg |
| `05-js` | `04-gfx-media` + JavaScript runtime/bindings layer |

Simple Graphics deliberately excludes Skia CPU raster/text and Skia GPU. Its
drawing surface is the CPU-writable framebuffer in `crtgfx/window.h`.

The normal repository build proves that every later binary package inherits
the preceding installed `dist` tree. The stronger source-stage boundary is
also implemented: a packaged predecessor SDK fetches a SHA-256-pinned CRT
GitHub Release source asset and builds/tests the next stage without using the
repository build tree. The complete predecessor-only chain through the
option-ON `03-gfx-simple -> 04-gfx-media` transition is verified on Windows,
macOS, and native Linux/aarch64. Linux passes the cold-cache Skia presentation
matrix with Vulkan validation both disabled and enabled, distribution
verification, and atomic publication. `04-gfx-media -> 05-js`
has not yet been added. Full details, artifact layout, package naming, and
acceptance rules are in
[`docs/distribution.md`](docs/distribution.md).

The current `05-js` package contains the installable `libcrtjs` skeleton. The
QuickJS engine, event loop, modules, and JavaScript-visible graphics/media
bindings remain planned work rather than a current completion claim. Likewise,
the ordinary developer preset keeps Skia and FFmpeg disabled by default;
release-grade `04-gfx-media` acceptance uses the separate option-ON isolated
stage path.

## Prerequisites

All hosts need Git, CMake 3.25+, Ninja, Python 3, Clang, LLD, and suitable
archive tools. These are build-machine prerequisites, not distribution
contents.

### Linux

For Debian/Ubuntu, a representative base setup is:

```sh
sudo apt install git cmake ninja-build python3 clang lld compiler-rt
```

Simple Graphics additionally needs a running Wayland compositor and the
Wayland/xkbcommon runtime. Advanced graphics normally adds:

```sh
sudo apt install libvulkan-dev libwayland-dev mesa-vulkan-drivers vulkan-tools
```

`mesa-vulkan-drivers` may be replaced with the GPU vendor's Vulkan ICD. The
current Linux FFmpeg recipe does not enable VA-API. When that path is enabled,
the usual development/runtime set is `libva-dev`, `libva2`, `libva-drm2`, a
vendor VA driver, and optional `vainfo`. Embedded images should satisfy the
same capabilities with board-vendor packages rather than copying desktop
package lists.

### Windows 11

Install Git, CMake, Ninja, Python, LLVM for Windows, and a Windows 10/11 SDK.
Run CMake from a shell where the SDK import libraries are discoverable. The
CRT Windows target is `*-w64-mingw32` for the common Itanium ABI lane even
though the host SDK supplies the native system and graphics import libraries.

Windows **Developer Mode must be enabled** for every supported CRT build and
porting environment. CRT deliberately implements Bionic/POSIX `symlink()` as
a real Windows filesystem symbolic link, and source extraction plus GNU-style
build/install steps rely on dangling, relative, and SONAME-style links. CRT
does not emulate these links as Cygwin marker files, MSYS shortcut files, or
MSYS2 deep copies. A Windows run without Developer Mode is outside the
supported build contract and may fail with `EPERM` or a host permission error
when it first creates a symbolic link.

### macOS

Install Xcode Command Line Tools, CMake, Ninja, and Python 3. Metal and the
required system frameworks come from the platform SDK.

## Build

The default workflow configures, builds, and tests only the C stage:

```sh
cmake --workflow --preset linux-host-ninja-debug
cmake --workflow --preset macos-host-ninja-debug
cmake --workflow --preset windows-host-ninja-debug
```

Focused stage commands use the matching build preset:

```sh
cmake --build --preset <preset> --target crt-c-build
cmake --build --preset <preset> --target crt-c-test
cmake --build --preset <preset> --target crt-c-dist

cmake --build --preset <preset> --target crt-libcxx-build
cmake --build --preset <preset> --target crt-libcxx-test
cmake --build --preset <preset> --target crt-libcxx-dist

cmake --build --preset <preset> --target crt-gfx-simple-build
cmake --build --preset <preset> --target crt-gfx-simple-test
cmake --build --preset <preset> --target crt-gfx-simple-dist

cmake --build --preset <preset> --target crt-gfx-media-build
cmake --build --preset <preset> --target crt-gfx-media-test
cmake --build --preset <preset> --target crt-gfx-media-dist

cmake --build --preset <preset> --target crt-js-build
cmake --build --preset <preset> --target crt-js-test
cmake --build --preset <preset> --target crt-js-dist
```

The outputs are cumulative directories and archives under
`out/<preset>/dist/`. A later `*-dist` target builds preceding stages as
dependencies. This dependency establishes cumulative package contents; the
source-stage bootstrap commands described under
[Release Engineering And Source Stages](#release-engineering-and-source-stages)
will separately verify that a freshly extracted predecessor SDK can produce
the next stage without reading repository headers, libraries, or build
outputs.

A stage is a self-contained sysroot for its advertised surface. Its
`include/`, `lib/`, and Windows `bin/` directories therefore include the
redistributable headers, link artifacts, and `.so`/`.dylib`/`.dll` runtime
files of external libraries required by that stage, not only CRT-owned files.
OS frameworks/system libraries and device-specific GPU/video drivers are
documented manifest prerequisites instead of silently copied or assumed. The
exact transitive packaging and licensing rule is in
[`docs/distribution.md`](docs/distribution.md).

Advanced graphics must be configured with a completed imported libc++ and
Skia build. The existing `crtgfx-skia-fetch`, `crtgfx-skia-configure`,
`crtgfx-skia-build`, and `crtgfx-skia-smoke` targets retain that explicit
bring-up path. FFmpeg remains controlled by `CRTMEDIA_ENABLE_FFMPEG`.

## Using A Distribution

Select a stage, then provide the external compiler tools:

```sh
export CRT_CC=/path/to/clang
export CRT_CXX=/path/to/clang++
export CRT_AR=/path/to/llvm-ar
export CRT_RANLIB=/path/to/llvm-ranlib
. out/<preset>/dist/02-cxx/activate.sh
```

On Windows use `call out\<preset>\dist\02-cxx\activate.cmd`. CMake consumers
may use the packaged `crt-toolchain.cmake`, which selects the packaged wrappers.
An embedded vendor toolchain that cannot use those Clang-compatible wrappers
should remain authoritative and consume CRT's packaged sysroot directly.
The Windows activation script discovers the SDK import-library directory from
a Developer Command Prompt, or accepts an explicit `CRT_WINDOWS_SDK_LIBPATH`.

The in-repository wrappers `tools/crt-cc` and `tools/crt-c++` enforce the
freestanding/sysroot/default-runtime boundary. Every stage also packages
optional Python port fetch/build drivers, pinned recipes, tests/shims, and the
examples appropriate to that stage. Configure/make work runs through the
packaged CRT mksh and Toybox tools; MSYS and Git Bash are not distribution
prerequisites. Upstream sources are not patched merely to hide a missing CRT
surface; the missing Bionic API/type/symbol/behavior is implemented in CRT
first.

## Toolchain Policy

Clang/LLVM, LLD, and compiler-rt are the primary development toolchain, but
CRT distributions **never bundle a compiler or linker**. Embedded devices use
their vendor/dedicated toolchain. A distribution contains the CRT sysroot,
startup objects, runtime libraries, wrappers, CMake configuration, and a
manifest that records the external toolchain contract.

The Windows C++ lane uses the Bionic/Itanium ABI and project-built libunwind.
Windows C++ exceptions are compiled as DWARF CFI (`-fdwarf-exceptions`) rather
than depending on the OS-owned SEH unwind engine. See
[`docs/cxx_runtime.md`](docs/cxx_runtime.md).

## Release Engineering And Source Stages

Release engineering creates an OS-qualified upper-stage source asset and its
recipe only after the component sources have been fetched at their pinned
revisions. For example:

```sh
python3 tools/create_stage_source.py --root . --stage 02-cxx \
  --target-os <linux|macos|windows> \
  --source-root out/<preset>/external/llvm-runtimes \
  --output-dir out/<preset>/stage-sources --release-tag <tag>
```

The command emits a deterministic source archive plus a recipe containing its
target OS, exact byte size, CRT commit, and SHA-256. Asset names include the
target OS because source payloads differ by backend. After the asset and recipe
are published, an extracted predecessor SDK consumes them with its packaged
`tools/crt-stage-build.py`; a local `--asset` override is available for
pre-publication acceptance without weakening digest verification.

## Repository Layout

```text
include/          Bionic-compatible public headers
libc/             libc, PAL, startup, OS/architecture backends
libm/             math runtime
libdl/            dynamic-loading API and host backends
libstdc++/        bootstrap ABI shim and imported libc++ build integration
shell/            tiny shell, mksh, toybox, and awk
libcrtgfx/        window/input/framebuffer and advanced Skia/GPU integration
libcrtmedia/      media runtime and optional FFmpeg integration
libcrtjs/         JavaScript runtime skeleton; QuickJS integration is planned
porting/recipes/  upstream porting test recipes
tools/            wrappers, rootfs/dist builders, porting automation
libc/tests/       libc, PAL, shell-level, ABI, and integration tests
libstdc++/tests/  C/C++ ABI-boundary tests
libcrtgfx/tests/  window, input, software, GPU, and Skia tests
libcrtmedia/tests/ frame, codec, player, audio, and FFmpeg tests
docs/             design, policy, roadmap, and verification documents
```

## Core Documents

- [FAQ](docs/faq.md)
- [Project meaning](docs/project_meanings.md)
- [Stack and toolchain policy](docs/project_stacks.md)
- [Distribution stages](docs/distribution.md) and the
  [developer-preview release contract](docs/release_preview.md)
- [Runtime roadmap](docs/runtime_roadmap.md)
- [C++ runtime](docs/cxx_runtime.md)
- [Graphics API policy](docs/libcrtgfx_api_policy.md)
- [Media API policy](docs/libcrtmedia_api_policy.md)
- [Sysroot porting](docs/sysroot_ports.md)
- [Current status](STATUS.md), [work queue](TODO.md), and [history](HISTORY.md)

## License And Provenance

Project-owned code and imported upstream families retain their applicable
license and provenance records. Bionic/OpenBSD imports are tracked under
`third_party/`; external runtime, graphics, media, shell, and porting sources
have project-owned recipe or import metadata next to their integration.
