# CRT Build Stages And Distributions

## Purpose

CRT is a Bionic-compatible cross-platform C runtime and PAL for rebuilding the
same application source as native Linux, Windows, and macOS executables. Its
primary deployment target is a Linux-kernel embedded device such as an HMI,
set-top box, game console, or IVI system; desktop builds are also the native
development and debugging environment.

CRT distributions never bundle LLVM, Clang, LLD, GCC, or a vendor compiler.
An embedded SDK normally supplies its own compiler, binutils, target triple,
and device libraries. A CRT distribution supplies the sysroot, startup
objects, runtimes, wrappers, CMake integration, and a manifest describing the
external toolchain contract.

## Cumulative Stages

| Stage | Content | Intended use |
| --- | --- | --- |
| `01-c` | libc, libm, libdl, startup objects, shell, mksh, toybox, awk, make | C applications and C libraries |
| `02-cxx` | `01-c` plus libc++, libc++abi, and libunwind | C++ applications and libraries |
| `03-gfx-simple` | `02-cxx` plus one window, keyboard/mouse input, and a CPU-writable software framebuffer | Industrial HMI and simple native UI |
| `04-gfx-media` | `03-gfx-simple` plus the GPU API, Skia CPU/GPU rendering, Vulkan/D3D12/Metal presentation, and FFmpeg media | accelerated UI and playback |
| `05-js` | `04-gfx-media` plus QuickJS and CRT bindings | JavaScript application runtime |

Each binary stage is cumulative and independently consumable. The repository
build currently creates each later directory by copying the preceding
installed stage and overlaying the new component. That proves artifact
inheritance, but it is not by itself proof that the new component can be
rebuilt without repository headers or libraries. The separate source-stage
chain below supplies that stronger boundary.

Each stage is also a self-contained development sysroot for its declared
capabilities. If a stage exposes an external library through its public API,
link interface, or runtime dependency, the distribution must install that
library's required public headers under `include/` and its redistributable
link/runtime artifacts under `lib/` and, on Windows where appropriate,
`bin/`. This includes static archives and import libraries needed for linking
as well as the corresponding `.so`, `.dylib`, or `.dll` files needed to run.
The rule applies transitively: an application must not need an undeclared
developer package merely because CRT linked one of its stage libraries to an
external dependency.

OS-owned libraries and frameworks that are part of the documented minimum
target OS, and device-specific GPU/video drivers such as Vulkan ICDs or VA-API
drivers, remain target prerequisites rather than copied CRT payloads. Every
such exception must be explicit in `manifest.json`; source/version/license
metadata must accompany every third-party artifact that CRT does redistribute.
Package construction and acceptance must fail when a manifest-declared header,
link artifact, runtime library, or notice is absent, or when a binary dependency
scan finds an undeclared non-system `.so`, `.dylib`, or `.dll` dependency.

Simple Graphics intentionally excludes Skia CPU raster and text, Skia GPU,
and the public `crtgfx/gpu.h` surface. Its drawing contract is the mapped
software framebuffer in `crtgfx/window.h`. The Windows implementation may use
the native compositor internally to present that CPU buffer; this does not
make Skia or the advanced GPU API part of the Simple Graphics contract.

## Build Targets

Configure once with a host preset. The default workflow only builds and tests
the C stage:

```sh
cmake --workflow --preset <os>-host-ninja-debug
```

Build, test, and package a specific layer with:

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

The more focused `crt-gfx-build/test` and `crt-media-build/test` targets are
also available. A `*-dist` target builds all preceding stages automatically.
Advanced graphics packaging reflects the configured capabilities; enable and
build the imported libc++, Skia, Vulkan/Metal/D3D12, and FFmpeg paths before
claiming those optional entries in a release manifest.

Outputs are placed under:

```text
out/<preset>/dist/01-c/
out/<preset>/dist/02-cxx/
out/<preset>/dist/03-gfx-simple/
out/<preset>/dist/04-gfx-media/
out/<preset>/dist/05-js/
```

Windows packages are emitted as `.zip`; Linux and macOS packages as
`.tar.xz`. Every directory contains `manifest.json`, `VERSION`, activation
scripts, wrappers, and `crt-toolchain.cmake` in addition to the cumulative
CRT and required redistributable dependency headers and libraries.

Each directory also carries the CRT mksh/toybox shell environment, optional
Python porting drivers, recipes/tests/shims, and the examples appropriate to
that stage. Python is an orchestration convenience and is declared in the
manifest when required; configure and make commands themselves run through
the packaged CRT mksh. MSYS and Git Bash are not distribution prerequisites.

Archive names carry version, OS, architecture, and stage, for example
`crt-development-windows-x86_64-01-c.zip` and
`crt-development-linux-aarch64-04-gfx-media.tar.xz`. A release replaces the
`development` token with the project release version.

## Source-stage bootstrap chain

The CRT GitHub repository is the meta-toolchain: it produces both the binary
SDK archives above and immutable source assets for the individual upper
stages. A released binary SDK does not clone the live repository or build
against an arbitrary branch. Instead, it contains a catalog of stage recipes
that records, at minimum:

- the input and output stage IDs;
- the target OS, which must match the preceding SDK;
- an immutable CRT GitHub Release asset URL;
- the source commit represented by that asset;
- the archive byte size and SHA-256 digest;
- the required preceding binary stage and external tools;
- configure/build/install/test entry points and expected installed artifacts.

The first supported transitions are `01-c -> 02-cxx`, then
`02-cxx -> 03-gfx-simple -> 04-gfx-media`. Thus `01-c` can fetch the pinned
libc++/libc++abi/libunwind source package and build `02-cxx`; `02-cxx` can
fetch the Simple Graphics and advanced Graphics/Media packages and build them
in dependency order. The `04-gfx-media -> 05-js` transition follows the same
model after these transitions are stable.

The SHA-256 is over the exact release asset bytes, not merely a Git ref. Git
commit IDs remain provenance metadata, while the digest is the download
integrity and reproducibility boundary. A development build may generate a
local source asset and matching recipe, but a published recipe must never use
a mutable branch URL or an unfilled digest.

`tools/create_stage_source.py` produces the `02-cxx` and `03-gfx-simple`
assets. The former packages the already-fetched pinned
libc++/libc++abi/libunwind trees; the latter packages the selected OS window
backend and, on Linux, the pinned libxkbcommon source port. Both carry only the
CRT recipes, standalone build files, wrappers, tests, and source files needed
for that target. Asset names include the target OS, and schema-v2 recipes bind
that OS to both the predecessor SDK and the archive metadata. Archive entry
ordering, timestamps, owners, and modes are normalized before SHA-256
calculation. The producer refuses a dirty meta-toolchain tree for release
output unless local testing explicitly opts in.

Every SDK carries `tools/crt-stage-build.py`. Given an input SDK and a
published recipe, it checks the input stage, byte size, SHA-256, embedded CRT
commit, archive path safety, and build entry point before executing anything.
The `02-cxx` entry point copies `01-c` to a new output, builds the imported C++
runtime against that copy, overlays the installed headers/libraries, runs the
static/shared imported-libc++ smoke, and verifies the resulting distribution.
The `03-gfx-simple` entry point similarly builds the window-only standalone
CMake project against `02-cxx`; it deliberately excludes GPU/native-Wayland
Vulkan coupling, runs window and synthetic-input tests, installs and rebuilds
the external example, and verifies the cumulative SDK. On Linux it first
builds libxkbcommon from the pinned source asset and carries its headers,
static library, license, and recipe provenance in the result. These files are
listed under `redistributed_dependencies` in `manifest.json`; distribution
verification rejects a Linux `03-gfx-simple` SDK whose xkbcommon declaration
or declared payload is incomplete. This is the first implemented port-library
instance of the transitive dependency rule, not an exemption for future
graphics/media ports.
Release packaging may add generated recipes under `stages/recipes/` with
`create_dist.py --stage-recipe <recipe>`; development packages do not claim a
downloadable transition until such a fully pinned recipe exists.

## External Toolchain Contract

The activation scripts and wrappers accept these externally supplied values:

- `CRT_CC`, `CRT_CXX`: C and C++ compiler executables;
- `CRT_AR`, `CRT_RANLIB`: archive tools;
- `CRT_WINDOWS_SDK_LIBPATH`: Windows SDK `um/x64` or `um/arm64` import-library
  directory needed by the freestanding Windows link.

For a packaged distribution, source `activate.sh` or call `activate.cmd`, then
build the application normally. The wrappers select the installed CRT sysroot
and its freestanding/default-runtime policy. `crt-toolchain.cmake` is provided
for CMake consumers and selects those wrappers. An embedded vendor toolchain
that cannot use the Clang-compatible wrappers remains authoritative and must
consume the packaged headers, startup objects, and libraries directly.
On Windows, `activate.cmd` derives `CRT_WINDOWS_SDK_LIBPATH` from
`WindowsSdkDir` and `WindowsSDKLibVersion` when run from a Developer Command
Prompt; otherwise set it explicitly.

Clang configuration files can provide GCC-spec-like defaults for a particular
driver name, but they are a convenience layer rather than the distribution
boundary: their discovery rules depend on the compiler installation and the
selected target triple. CRT therefore keeps explicit wrappers/toolchain files
and records the chosen compiler/triple/options in the manifest.

## Distribution Acceptance

A release stage is complete only when acceptance tests run outside the source
and build trees using the packaged directory. At minimum they must verify:

1. compile, link, and run a C or C++ sample as appropriate;
2. static and shared runtime linkage;
3. a path containing spaces;
4. one CMake consumer and one configure/make consumer where applicable;
5. the next stage fetched from its pinned source asset, SHA-256 verified,
   built and tested using only the preceding extracted stage;
6. no access to repository headers, libraries, or build outputs during that
   isolated stage build;
7. packaged examples rebuilt and run at the appropriate stage;
8. no accidental absolute source/build paths in installed files;
9. no compiler or linker executable in the archive;
10. every declared external dependency's headers, link artifacts, and runtime
    libraries are present, and its provenance/license metadata is recorded;
11. a binary dependency scan finds no undeclared non-system `.so`, `.dylib`,
    or `.dll` dependency;
12. OS-owned or device-driver prerequisites excluded from the archive are
    explicitly named in the manifest;
13. the manifest's OS, architecture, stage, compiler inputs, and option set.

## Linux Host And Device Capabilities

Package names below are examples, not CRT dependencies baked into an embedded
image.

For a Debian/Ubuntu development host, the common build tools are `git`,
`cmake`, `ninja-build`, `python3`, `clang`, `lld`, and `compiler-rt`. The
compiler packages are host prerequisites and are never copied into CRT.

Simple Graphics needs a reachable Wayland compositor and the Wayland runtime;
keyboard mapping also needs xkbcommon data. The software surface is `wl_shm`
based and does not require Vulkan, Mesa, or libva.

Advanced Graphics on a Debian/Ubuntu host normally uses:

- build: `libvulkan-dev` and Wayland development files;
- runtime loader: `libvulkan1`;
- one Vulkan ICD: for example `mesa-vulkan-drivers`, or the GPU vendor's ICD;
- diagnostics: optional `vulkan-tools`.

Fedora uses `vulkan-loader-devel`; Arch uses `vulkan-icd-loader` plus a
separate device driver package. Mesa is one implementation, not an ABI
requirement. An embedded product should use the board vendor's loader/ICD and
Wayland WSI, or a future CRT DRM/KMS backend when that backend exists.

The current FFmpeg recipe does not enable Linux VA-API, so libva is not yet a
hard dependency. When the VA-API hardware-decode path is enabled, a typical
Debian/Ubuntu split is `libva-dev` for builds, `libva2` and `libva-drm2` at
runtime, `vainfo` for diagnostics, and a GPU-specific VA driver. Fedora uses
`libva-devel`; Arch uses `libva` and `libva-utils`. A successful `vainfo` is a
device-image acceptance check, not evidence that CRT should bundle the driver.

## Linker Scope

The `linker/` directory remains reserved for the long-term CRT-owned dynamic
loader. Implementing or distributing that loader is outside the current
roadmap. The active goal is source-rebuild portability and native executables,
not unmodified glibc binaries, APKs, containers, or a bundled compiler suite.
