# CRT

CRT is a Bionic-compatible cross-platform C runtime and Platform Adaptation
Layer (PAL). It lets Linux/BSD/Android-style native libraries and applications
be rebuilt from the same source as native Linux, Windows, and macOS
executables.

The primary target is a Linux-kernel embedded device: set-top boxes,
Raspberry-Pi-class consoles, industrial HMIs, and automotive IVI systems.
Native desktop builds provide a fast development and debugging loop and can
also be shipped as desktop applications.

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

## Staged Runtime

CRT is built and verified as cumulative distributions:

| Stage | Includes |
| --- | --- |
| `01-c` | libc/libm/libdl, startup, shell, mksh, toybox, awk, make |
| `02-cxx` | `01-c` + libc++/libc++abi/libunwind |
| `03-gfx-simple` | `02-cxx` + window, keyboard/mouse, software framebuffer |
| `04-gfx-media` | `03-gfx-simple` + Skia CPU/GPU, Vulkan/D3D12/Metal, FFmpeg |
| `05-js` | `04-gfx-media` + QuickJS bindings |

Simple Graphics deliberately excludes Skia CPU raster/text and Skia GPU. Its
drawing surface is the CPU-writable framebuffer in `crtgfx/window.h`.

The normal repository build currently proves that every later binary package
inherits the preceding installed `dist` tree. A separate source-stage chain is
being added for the stronger boundary: `01-c` fetches a SHA-256-pinned CRT
GitHub Release source asset and builds/tests `02-cxx`; `02-cxx` then builds
`03-gfx-simple`, followed by `04-gfx-media` against the installed Simple
Graphics result. Full details, artifact layout, package naming, and acceptance
rules are in
[`docs/distribution.md`](docs/distribution.md).

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
source-stage bootstrap commands described above will separately verify that a
freshly extracted predecessor SDK can produce the next stage without reading
repository headers, libraries, or build outputs.

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
libcrtjs/         QuickJS integration
porting/recipes/  upstream porting test recipes
tools/            wrappers, rootfs/dist builders, porting automation
tests/            unit, ABI, PAL, and integration tests
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
