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

The exact evidence, per-port results, and open work are in
[`STATUS.md`](STATUS.md), [`docs/porting_status.md`](docs/porting_status.md),
and [`TODO.md`](TODO.md). To try it from source, see
[Prerequisites](#prerequisites), [Build](#build), and
[Using A Distribution](#using-a-distribution).

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

- [Project meaning](docs/project_meanings.md)
- [Stack and toolchain policy](docs/project_stacks.md)
- [Distribution stages](docs/distribution.md)
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
