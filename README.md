# CRT

**Build embedded Linux applications on the desktop. Run them natively everywhere.**

CRT is a native cross-platform application runtime for **Linux, Windows, and macOS**.

It provides a common C/C++ runtime and platform layer, then builds upward into native windows, GPU graphics, media, networking, application UI, and eventually a WebKit-based Web runtime.

**Same source model. Native executables. Native GPUs. No VM, container, or translation layer.**

The primary target is a Linux-kernel embedded product: set-top boxes, Raspberry-Pi-class consoles, industrial HMIs, smart displays, and automotive IVI systems. Windows and macOS are first-class native development and execution hosts, not emulation environments.

> CRT is pre-1.0 developer software under active development.  
> The accepted product runtime now reaches **`05-ui`**: `crtui` application widgets, layout, input, external-surface composition, and a GPU-composed `MediaView`, verified on all three hosts. The current upper-runtime work is **`06-web`**, a WebKit CRT Port with JavaScriptCore, WebCore, WebKit, `PlatformCRT`, and `WebView`. Its bring-up has closed JavaScriptCore acceptance on all four required host/architecture pairs and the pristine native WPE reference on Linux/x86_64; `PlatformCRT` and the public `crtweb` API are the next product work.

[Roadmap](docs/runtime_roadmap.md) ·
[Current Status](STATUS.md) ·
[Work Queue](TODO.md) ·
[History](HISTORY.md) ·
[Releases](https://github.com/webos21/crt/releases)

---

## What CRT Is Becoming

CRT started as a Bionic-compatible C/C++ portability runtime.

That foundation now supports a broader goal:

> **One native application stack for embedded Linux, Windows, and macOS.**

```text
                         Application
                              |
                            crtui
                              |
              +---------------+---------------+
              |               |               |
           Widgets         MediaView        WebView
              |               |               |
            LVGL           crtmedia         crtweb
                              |               |
                            FFmpeg          WebKit
              |               |               |
              +---------------+---------------+
                              |
                            crtgfx
                              |
                             Skia
                              |
                    +---------+---------+
                    |         |         |
                  Vulkan    D3D12     Metal
                    |         |         |
                    +---------+---------+
                              |
                     CRT C/C++ Runtime
                              |
                  Bionic-compatible PAL
                              |
                    +---------+---------+
                    |         |         |
                  Linux     Windows    macOS
```

Today, the lower and middle layers are already working across all three hosts:

- Bionic-shaped libc/libm/libdl and imported libc++ runtime
- native windows and input
- Skia CPU and GPU rendering
- Vulkan, D3D12, and Metal presentation
- software and hardware media decode
- zero-copy or measured GPU-copy decoded-texture interop
- camera capture and hardware/software encode
- HTTP/HTTPS streaming with bounded buffering and reconnect
- `crtui` application widgets with CRT-owned layout, styling, input and focus
- external-surface composition, including GPU video (`MediaView`) composed in the final present

The `crtweb` product runtime remains in progress and is not claimed as
implemented. Its JavaScriptCore and native Linux WPE reference baselines are
verified bring-up evidence, not a packaged `06-web` SDK.

---

## Why CRT?

Embedded software often begins on Linux, but development, debugging, simulation, and companion applications frequently need to run elsewhere.

Traditionally that means maintaining different platform ports, running a VM or container, or adopting a large application framework that owns the entire stack.

CRT takes a different approach.

### Native everywhere

Applications are rebuilt from the same source as ordinary native executables.

```text
Linux source model
       |
       +----> native Linux executable
       |
       +----> native Windows executable
       |
       +----> native macOS executable
```

There is no Linux VM on Windows, no container requirement, and no binary translation layer.

### The portability work lives below the application

Files, sockets, threads, TLS, memory mapping, process primitives, dynamic loading, startup, C++, graphics, media, and networking differ significantly between operating systems.

CRT absorbs those differences behind a Bionic-shaped runtime and a small host PAL so applications and upstream libraries do not each solve them again.

### Embedded Linux remains the center of gravity

Windows and macOS are not the product definition.

They are first-class native hosts that make it possible to develop, test, debug, demonstrate, and sometimes ship the same application code away from the embedded target.

### Rebuild, do not emulate

CRT is source portability, not binary compatibility.

You compile with Clang/LLVM against a CRT sysroot and produce a native executable for the target host.

CRT is **not**:

- Android APK compatibility
- glibc binary compatibility
- a VM or container
- Wine/WSL-style binary execution
- an Electron clone
- a complete Qt/GTK replacement
- production-ready `1.0`

For comparisons with other approaches, see the [FAQ](docs/faq.md).

---

## What You Can Build Today

The currently accepted runtime is already suitable for native graphics/media/network experiments and embedded-style application development.

Examples include:

- video and streaming clients
- media dashboards
- camera/capture applications
- HMI and control-panel prototypes
- native GPU visualization
- STB/TV-style interfaces
- cross-platform desktop development frontends for embedded products

`crtui` extends that base into a real application-facing widget layer.

The active `06-web` stage will add an embedded Web runtime rather than making Web technology a prerequisite for the native stack.

---

## Current Roadmap

```text
01-c
  C runtime / PAL
      |
02-cxx
  C++ runtime
      |
03-gfx-simple
  window / input / software framebuffer
      |
04-gfx-media
  Skia / GPU / media / networking
      |
05-ui                         accepted on all three hosts
  crtui / LVGL / widgets
  external-surface composition
  MediaView
      |
06-web                        <-- current upper-runtime work (PlatformCRT prototype next)
  JavaScriptCore
  WebCore
  WebKit
  PlatformCRT
  crtweb / WebView
```

Major completed upper-runtime milestones:

```text
Hardware Decode
      |
Zero-copy Decoded Textures
      |
Encode & Capture
      |
Networking & Streaming
      |
Application UI
      |
Web Runtime          <-- current
```

WebRTC, WebGPU, EME/DRM, and other large Web capabilities are intentionally consumer-driven follow-ups rather than prerequisites for the WebKit bring-up.

See [docs/runtime_roadmap.md](docs/runtime_roadmap.md) for the dependency order and stage policy.

---

## Where It Stands

### `04-gfx-media`: accepted cross-host foundation

The graphics/media/networking foundation is verified on Linux, Windows, and macOS.

It includes:

- native window/input
- Skia CPU raster and text
- live Skia GPU presentation
- Vulkan / D3D12 / Metal
- FFmpeg software decode
- H.264 hardware decode
- decoded GPU texture interop
- capture and encode
- mux/decode-back validation
- progressive HTTP input
- fragmented-MP4 HTTP output
- bounded back-pressure
- reconnect without unsafe byte-stream splicing
- HTTPS with explicit trust policy

### `05-ui`: accepted cross-host

`crtui` is a CRT-owned application API.

LVGL is an implementation dependency, not part of the public CRT ABI.

Accepted on Linux, Windows, and macOS:

- frozen `crtui` public contract and a headless model
- LVGL 9.6.0 software rendering through a CRT display adapter
- pointer, keyboard, and committed-text input; spatial focus navigation; resize
- CRT-owned layout (free, row, column, stack, scroll), a CRT-neutral style set,
  and the v1 widgets
- private-LVGL boundary: only `crtui_*` is exported and no LVGL header or file
  ships in the SDK
- the producer-neutral external-surface scene, composed by `crtgfx` in the final
  present
- `MediaView`: hardware-decoded GPU video composed in that present without an
  LVGL framebuffer copy (zero-copy on Linux/macOS, measured GPU-copy on Windows)
- the isolated `04-gfx-media -> 05-ui` source-stage build and the installed
  `ui-basic` consumer

Interactive real-device input is not automated; the input evidence is scripted.
`tools/prepare_release_assets.py` does not yet know `05-ui`, so it is not a
release asset.

See [docs/crtui_acceptance.md](docs/crtui_acceptance.md).

### `06-web`: current work

The Web stage is built around a **WebKit CRT Port**, not around porting WPE unchanged to every OS.

The intended architecture is:

```text
JavaScriptCore
WebCore
WebKit
     |
 PlatformCRT
     |
 +---+---------+-------------+
 |             |             |
CRT PAL      crtgfx       crtmedia
 |             |             |
 +--------- CRT Network -----+
```

WPE WebKit is the Linux reference port during bring-up. The final target is one CRT-facing Web runtime and `WebView` API across Linux, Windows, and macOS.

Tranches 0-2 are closed: the WPE WebKit 2.54.0 provenance is frozen;
JavaScriptCore passes interpreter, Baseline/DFG/FTL JIT, WebAssembly, and
sampling-profiler acceptance on Linux/x86_64, Linux/aarch64, macOS/arm64, and
Windows/x64; and pristine native WPE renders the accepted local HTML fixture
through its built-in headless backend on Linux/x86_64. These are bring-up and
reference baselines. No `PlatformCRT`, public `crtweb` API, WebView, or `06-web`
distribution exists yet.

See:

- [docs/crtweb_acceptance.md](docs/crtweb_acceptance.md)
- [docs/crtweb_porting.md](docs/crtweb_porting.md)

---

## Demo

Current recorded demo: Windows 11/x64 `media-player` example.

https://github.com/user-attachments/assets/a60ff1ae-c260-4aaa-a140-8095ac95f7c0

The existing clip demonstrates native process/window creation and FFmpeg playback from an isolated `04-gfx-media` SDK.

It predates `crtui` and therefore is **not** the final application-runtime showcase.

The next representative demo is intended to combine, from the same source:

```text
native window
+ crtui widgets
+ GPU presentation
+ streaming media
+ MediaView
+ input / focus / resize
```

on Linux, Windows, and macOS.

That is the point where CRT's application-runtime direction becomes visible in a single application rather than as separate subsystem tests.

---

## Capability Snapshot

Each cell describes evidence on that host, not a future promise.

- **Verified** — implementation plus automated or recorded acceptance passed.
- **Partial** — implemented and exercised, with a documented limitation.
- **In progress** — current work, not yet fully accepted.
- **Planned** — not implemented.

| Capability | Linux | Windows | macOS | Notes |
| --- | --- | --- | --- | --- |
| libc / libm | Verified | Verified | Verified | Bionic-shaped public surface; full runtime tests. |
| libdl | Partial | Verified | Verified | Linux real-library loading through a CRT-owned loader remains deferred. |
| libc++ / libc++abi / libunwind | Verified | Verified | Verified | Static/shared smoke, RTTI, exceptions. |
| pthread | Verified | Verified | Verified | Mutex, condvar, barrier, attributes, TLS, process-shared coverage. |
| Sockets / DNS | Verified | Verified | Verified | Real HTTP/HTTPS through CRT sockets + libcurl/mbedTLS. Resolver remains IPv4/UDP A-record only. |
| mmap / TLS | Verified | Verified | Verified | Runtime memory mapping and thread-local storage tests. |
| Native window / input | Verified | Verified | Verified | Wayland, Win32, Cocoa; keyboard, pointer, resize, DPI. |
| Skia CPU raster / text | Verified | Verified | Verified | Skia m148 + FreeType. |
| Skia GPU presentation | Verified | Verified | Verified | Vulkan / D3D12 / Metal with pixel and resize checks. |
| FFmpeg software media | Verified | Verified | Verified | Narrow LGPL configuration. |
| Native audio output | Partial | Verified | Verified | ALSA/PulseAudio, WASAPI, CoreAudio; Linux real-device behavior is environment-dependent. |
| Hardware H.264 decode | Verified | Verified | Verified | VA-API, D3D11VA, VideoToolbox. |
| Decoded GPU texture interop | Verified | Verified | Verified | Direct zero-copy on Linux/macOS; measured no-CPU-readback GPU-copy fallback on Windows. |
| Video encode / capture | Verified | Verified | Verified | V4L2/VA-API, Media Foundation, AVFoundation/VideoToolbox. |
| Network streaming | Verified | Verified | Verified | Bounded HTTP/HTTPS input/output, reconnect, lifecycle and package acceptance. |
| Application UI (`crtui`) | Verified | Verified | Verified | Contract, input, private-LVGL wrapper, widgets, external surfaces, MediaView and the isolated `05-ui` package (Tranches 0-7) accepted on all hosts. |
| Web runtime (`crtweb`) | In progress | In progress | In progress | WebKit CRT Port (`06-web`). JavaScriptCore Tranche 1 is accepted on Linux/x86_64, Linux/aarch64, macOS/arm64, and Windows/x64; the pristine WPE reference Tranche 2 is accepted on Linux/x86_64. `PlatformCRT`, the public API, WebView, and packaging remain. |

The authoritative detailed evidence lives in [STATUS.md](STATUS.md), [HISTORY.md](HISTORY.md), and the subsystem acceptance documents.

---

## Native Graphics

CRT does not hide every host behind a software renderer.

The advanced graphics stage uses each platform's native GPU API:

```text
Linux       -> Vulkan
Windows     -> D3D12
macOS       -> Metal
                 |
                 v
                Skia
```

The same public CRT graphics layer drives each backend.

Acceptance includes live presentation, pixel comparison, and resize behavior rather than compile-only validation.

---

## Hardware Media

Hardware H.264 decode is accepted on all three host families:

| Host | Decoder |
| --- | --- |
| Linux/x86_64 | VA-API |
| Windows/x64 | D3D11VA |
| macOS/arm64 | VideoToolbox |

The CPU-resident frame path remains available and software decode remains the fallback.

An additive GPU-frame path also exists:

```text
Linux
VA-API -> dma-buf/Vulkan -> Skia
             direct zero-copy

macOS
VideoToolbox -> CVPixelBuffer/Metal -> Skia
             direct zero-copy

Windows
D3D11VA -> GPU-copy interop -> D3D12 -> Skia
             no CPU readback
```

See:

- [Hardware decode acceptance](docs/crtmedia_hardware_decode_acceptance.md)
- [Zero-copy decode acceptance](docs/crtmedia_zero_copy_decode_acceptance.md)
- [Encode and capture acceptance](docs/crtmedia_encode_capture_acceptance.md)
- [Networking acceptance](docs/crtmedia_networking_acceptance.md)

---

## Portability Proof

CRT's portability claim is tested by rebuilding real upstream software against the CRT sysroot and then running it.

Recipes are pinned and checksum-verified under [`porting/recipes/`](porting/recipes/).

| Upstream | Version | Linux / Windows / macOS | Evidence |
| --- | --- | --- | --- |
| zlib | 1.3.1 | pass | Static/shared compress/decompress round trip. |
| libpng | 1.6.57 | pass | Static/shared image create/write/destroy paths. |
| SQLite | 3.53.4 | pass | Amalgamation build and recipe/link smoke. |
| bzip2 | 1.0.8 | pass | Static/shared round trip. |
| xz / liblzma | 5.8.3 | pass | Maximum-preset round trip with CRC64. |
| PCRE2 | 10.47 | pass | 8-bit regex matching. |
| mbedTLS | 3.6.7 | pass | SHA-256 and AES-128-CBC known-answer/round-trip tests. |
| curl | 8.21.0 | pass | Real HTTP and HTTPS over CRT sockets/DNS. |
| FreeType | 2.14.3 | pass | Glyph rasterization; also used by Skia text. |
| FFmpeg | 8.1.2 | pass | Demux/decode/playback plus platform hardware decode. |
| Skia | m148 | pass | CPU text/raster and native GPU presentation. |

libffi and expat also pass on all three hosts.

The rule is important:

> Do not patch upstream source merely to hide a missing CRT API.

When an upstream library expects Linux/Bionic behavior, CRT implements that behavior at the runtime/PAL boundary whenever practical.

Per-port details are in [docs/porting_status.md](docs/porting_status.md).

To reproduce a recipe:

```sh
cmake --build --preset <preset> --target port-test-<name>
cmake --build --preset <preset> --target port-test-recipes
```

---

## Staged Runtime

CRT is built as cumulative SDK stages.

| Stage | Surface |
| --- | --- |
| `01-c` | libc/libm/libdl, startup, shell, mksh, Toybox, awk, make |
| `02-cxx` | `01-c` + libc++ / libc++abi / libunwind |
| `03-gfx-simple` | `02-cxx` + native window/input/software framebuffer |
| `04-gfx-media` | `03-gfx-simple` + Skia CPU/GPU, Vulkan/D3D12/Metal, FFmpeg, media/networking |
| `05-ui` | `04-gfx-media` + `crtui`, LVGL-backed widgets, application composition |
| `06-web` | in progress (Tranches 0-2 closed; Tranche 3 next): `05-ui` + JavaScriptCore/WebCore/WebKit, `PlatformCRT`, `crtweb`/WebView |

A later stage is expected to contain everything from the preceding stage.

The stronger release gate is a predecessor-only source-stage build:

```text
installed previous SDK
        |
        v
fresh next-stage source
        |
        v
build + test + package
```

This prevents a repository build tree from silently satisfying undeclared dependencies.

See [docs/distribution.md](docs/distribution.md).

---

## Build

CRT uses CMake presets.

The default workflow builds/tests the C stage:

```sh
cmake --workflow --preset linux-host-ninja-debug
cmake --workflow --preset macos-host-ninja-debug
cmake --workflow --preset windows-host-ninja-debug
```

Focused stage targets use the corresponding build preset:

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

cmake --build --preset <preset> --target crt-ui-build
cmake --build --preset <preset> --target crt-ui-test
cmake --build --preset <preset> --target crt-ui-dist
```

Outputs are cumulative SDK trees and archives under:

```text
out/<preset>/dist/
```

The `crt-ui-*` targets need the LVGL renderer: run the `crtui-lvgl-fetch` target
once and configure with `-DCRTUI_ENABLE_LVGL=ON`. Without it `crt-ui-dist` fails
at `verify_dist.py`, because the installed `ui-basic` sample it must package is
only built then. The Skia/`MediaView` companions and their tests also need a
Skia/FFmpeg-enabled tree. The in-tree `crt-gfx-media-dist` keeps Skia and FFmpeg
OFF; the release-grade `04-gfx-media` and `05-ui` SDKs come from the isolated
stage builds (`tools/crt-stage-build.py`, see
[docs/release_preview.md](docs/release_preview.md)).

---

## Prerequisites

All development hosts need:

- Git
- CMake 3.25+
- Ninja
- Python 3
- Clang/LLVM
- LLD
- suitable archive tools

These are build-machine prerequisites, not files bundled into a CRT distribution.

### Linux

For Debian/Ubuntu, a representative base setup is:

```sh
sudo apt install git cmake ninja-build python3 clang lld compiler-rt
```

Simple Graphics also needs a running Wayland compositor and the Wayland/xkbcommon runtime.

Advanced graphics typically adds:

```sh
sudo apt install libvulkan-dev libwayland-dev mesa-vulkan-drivers vulkan-tools
```

A vendor Vulkan ICD may replace Mesa.

The Linux FFmpeg/VA-API path also requires the appropriate VA libraries, headers, and vendor driver on the host.

Embedded images should satisfy these capabilities through the board/vendor platform rather than blindly copying desktop package lists.

### Windows 11

Install:

- Git
- CMake
- Ninja
- Python
- LLVM for Windows
- Windows 10/11 SDK

Run CMake from an environment where the SDK import libraries are discoverable.

Windows **Developer Mode must be enabled** for supported CRT development because CRT uses real filesystem symbolic links for the Bionic/POSIX `symlink()` model and for GNU-style source/build/install flows.

### macOS

Install:

- Xcode Command Line Tools
- CMake
- Ninja
- Python 3

Metal and required system frameworks come from the platform SDK.

---

## Using A Distribution

Select a stage and provide an external toolchain:

```sh
export CRT_CC=/path/to/clang
export CRT_CXX=/path/to/clang++
export CRT_AR=/path/to/llvm-ar
export CRT_RANLIB=/path/to/llvm-ranlib

. out/<preset>/dist/02-cxx/activate.sh
```

On Windows:

```bat
call out\<preset>\dist\02-cxx\activate.cmd
```

CMake consumers can use the packaged:

```text
crt-toolchain.cmake
```

CRT distributions do **not** bundle a compiler or linker.

An embedded vendor toolchain remains authoritative when required and can consume the packaged CRT sysroot directly.

The packaged wrappers enforce the runtime/sysroot boundary, while porting helpers, recipes, tests, and stage-specific examples are shipped with the relevant SDK.

---

## Toolchain Policy

The primary development toolchain is:

```text
Clang / LLVM
LLD
compiler-rt
```

The C++ runtime is imported and built as part of CRT's staged runtime.

On Windows, CRT uses the Bionic/Itanium ABI lane and project-built libunwind. C++ exceptions use DWARF CFI rather than depending on the native Windows SEH C++ ABI.

See [docs/cxx_runtime.md](docs/cxx_runtime.md).

---

## Porting Philosophy

CRT aims for **Bionic-compatible source portability**, not generic POSIX completeness.

Why Bionic-shaped?

- it is the libc surface used by Android
- it already models Linux-kernel APIs expected by a large body of native software
- its upstream code base is suitable as a portability reference
- it provides a concrete API/ABI target rather than an undefined "Unix-like" compatibility goal

Host adaptation stays below that public surface.

```text
upstream native source
        |
   Bionic-shaped API
        |
       CRT
        |
 +------+------+------+
 |             |      |
Linux       Windows  macOS
```

A CRT port should prefer implementing the missing runtime behavior over adding source patches to every upstream consumer.

---

## Application UI

`crtui` is deliberately **not the LVGL API**.

Applications target CRT-owned types and behavior:

```text
Application
    |
  crtui
    |
  LVGL
    |
 crtgfx
```

This keeps LVGL private and leaves room for CRT-native media/external-surface widgets.

The v1 widget set is intentionally small and application-oriented:

- window/screen
- container
- row/column
- text
- button
- image
- slider
- progress
- switch
- checkbox
- list/scroll view
- text input

The external surface is the architecture boundary that keeps GPU-backed content out of the LVGL CPU framebuffer. It is accepted for video (`MediaView`); a Web view is the next producer:

```text
                 crtui scene
                     |
        +------------+------------+
        |            |            |
      Widgets     MediaView    future WebView
        |            |            |
      LVGL        crtmedia      WebKit
        |            |            |
        +------------+------------+
                     |
                   crtgfx
```

Video stays GPU-backed instead of being copied into an LVGL CPU framebuffer, and the same contract is meant to carry Web content.

See [docs/crtui_acceptance.md](docs/crtui_acceptance.md).

---

## Web Runtime

The in-progress Web layer is a **WebKit CRT Port**.

The goal is not to expose WPE or WebKit types directly to applications.

Applications will eventually use a CRT-owned surface such as:

```text
crtweb runtime
crtweb view
crtui WebView
```

while WebKit is integrated through `PlatformCRT`.

The WebKit work is sequenced as follows (the first three items are closed):

1. pinned WebKit/JSC provenance and license policy
2. JavaScriptCore/JSCOnly bring-up
3. Linux WPE reference baseline
4. `PlatformCRT` graphics/input integration
5. `libcrtweb` and WebView
6. WebKit multi-process lifecycle
7. Windows/x64
8. macOS/arm64
9. gradual replacement of reference network/media backends with CRT subsystems
10. packaging, security, lifecycle, and license closure

QuickJS is no longer a runtime stage prerequisite.

WebRTC is also intentionally deferred until an actual Web/application consumer requires it.

See:

- [docs/crtweb_acceptance.md](docs/crtweb_acceptance.md)
- [docs/crtweb_porting.md](docs/crtweb_porting.md)

---

## Repository Layout

```text
include/          Bionic-compatible public headers
libc/             libc, PAL, startup, OS/architecture backends
libm/             math runtime
libdl/            dynamic-loading API and host backends
libstdc++/        bootstrap ABI shim and imported libc++ integration
shell/            tiny shell, mksh, Toybox, awk
libcrtgfx/        window/input/framebuffer and Skia/GPU integration
libcrtmedia/      media, capture/encode, streaming and FFmpeg integration
libcrtui/         application-facing UI API and LVGL adapter
libcrtweb/        Web Runtime: WebKit pin and provenance only so far (planned port)
porting/recipes/  pinned upstream porting recipes
tools/            wrappers, builders, packaging and porting automation
libc/tests/       libc/PAL/shell/ABI/integration tests
libstdc++/tests/  C++ ABI-boundary tests
libcrtgfx/tests/  window/input/software/GPU/Skia tests
libcrtmedia/tests/ media/codec/audio/capture/network tests
docs/             design, policy, roadmap and acceptance records
```

`libcrtweb/` holds the pinned WPE WebKit reference, its provenance and the JavaScriptCore acceptance tests; the `crtweb` API, `PlatformCRT` and the `06-web` source stage are planned and should not be read as existing implementation.

---

## Documentation

Start here:

- [Runtime roadmap](docs/runtime_roadmap.md)
- [Current status](STATUS.md)
- [Current work queue](TODO.md)
- [History](HISTORY.md)
- [FAQ](docs/faq.md)
- [Project meaning](docs/project_meanings.md)

Runtime/application layers:

- [crtui acceptance](docs/crtui_acceptance.md)
- [crtweb acceptance](docs/crtweb_acceptance.md)
- [crtweb porting plan](docs/crtweb_porting.md)
- [Graphics API policy](docs/libcrtgfx_api_policy.md)
- [Media API policy](docs/libcrtmedia_api_policy.md)
- [Hardware decode acceptance](docs/crtmedia_hardware_decode_acceptance.md)
- [Zero-copy decode acceptance](docs/crtmedia_zero_copy_decode_acceptance.md)
- [Encode and capture acceptance](docs/crtmedia_encode_capture_acceptance.md)
- [Networking acceptance](docs/crtmedia_networking_acceptance.md)

Build/distribution:

- [Distribution stages](docs/distribution.md)
- [Developer preview release contract](docs/release_preview.md)
- [Sysroot porting](docs/sysroot_ports.md)
- [C++ runtime](docs/cxx_runtime.md)
- [Porting status](docs/porting_status.md)

---

## Project Status

CRT is a developer-preview project.

The important distinction is between what works today and where the architecture is heading.

### Verified today

```text
C/C++ runtime
native windows/input
Skia CPU/GPU
Vulkan / D3D12 / Metal
software + hardware media
zero-copy/GPU interop
capture + encode
network streaming
crtui widgets, layout, input and MediaView
isolated 05-ui source-stage build
```

### In progress

```text
WebKit CRT Port (06-web):
Tranches 0-2 closed;
JavaScriptCore accepted through interpreter, Baseline/DFG/FTL,
WebAssembly, and sampling profiler on all four required host pairs;
pristine native WPE headless reference accepted on Linux/x86_64
```

### Planned

```text
PlatformCRT graphics/input prototype
crtweb / WebView
multi-process cross-host replay and 06-web packaging
```

Claims in this README are intentionally evidence-based.

If this file and the detailed status records disagree, [STATUS.md](STATUS.md) and the corresponding acceptance document are authoritative.

---

## License And Provenance

CRT contains project-owned code plus imported and externally built upstream components.

Each component retains its applicable license and provenance requirements.

Bionic/OpenBSD-derived imports are tracked under `third_party/`. External runtime, graphics, media, UI, shell, and porting dependencies have project-owned recipe/import metadata alongside their integration.

The active WebKit stage keeps WebKit/JSC/WebCore provenance and license obligations explicit and separate from CRT-owned platform adapter code.

---

## In One Sentence

> **CRT is a native application runtime for embedded Linux products that lets the same C/C++ application stack run directly on Linux, Windows, and macOS — from the runtime and GPU up through media, networking, UI, and, next, the Web.**
