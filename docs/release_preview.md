# Developer Preview Release

This document defines what a CRT developer-preview release contains, how it is
verified, and how a maintainer produces it. **No release has been published
yet**: `README.md` links here only for the contract, and nothing below claims
that a download exists. For what works today, see the README and
[`STATUS.md`](../STATUS.md).

The first preview is tentatively `v0.4.0-preview.1`, positioned as the first
public developer preview of the CRT Graphics/Media SDK stage (`04-gfx-media`).
The JavaScript stage (`05-js`) is a `libcrtjs` skeleton and is not part of the
preview.

## What a release contains

Each supported host (Linux, Windows, macOS) contributes one set of assets. The
release is the union of the three sets.

| Asset | Purpose |
| --- | --- |
| `crt-<tag>-<os>-<arch>-01-c` | libc/libm/libdl, startup, shell, mksh, toybox, awk, make |
| `crt-<tag>-<os>-<arch>-02-cxx` | `01-c` plus libc++/libc++abi/libunwind |
| `crt-<tag>-<os>-<arch>-03-gfx-simple` | `02-cxx` plus window, keyboard/mouse, software framebuffer |
| `crt-<tag>-<os>-<arch>-04-gfx-media` | `03-gfx-simple` plus Skia CPU/GPU, GPU presentation, FreeType, FFmpeg |
| `crt-<tag>-<os>-04-gfx-media-source.tar.xz` and the `02-cxx`/`03-gfx-simple` equivalents | Stage-source assets that the packaged stage recipes download (pinned by size and SHA-256) |
| `SHA256SUMS-<os>-<arch>` | Checksums of every asset that host produced |
| `release-manifest-<os>-<arch>.json` | Machine-readable list: version, source commit, per-asset size and SHA-256 |

SDK archives are `.zip` on Windows and `.tar.xz` on Linux and macOS, and each
extracts to a single `<stage>/` directory. The stages are **cumulative**: the
`04-gfx-media` archive already contains `01-c` through `03-gfx-simple`, so most
users need only that one. The smaller stages exist for products that stop at C,
C++, or the simple framebuffer. Measured on Windows/x64, the development
archives for `01-c`, `02-cxx`, and `03-gfx-simple` are about 114, 122, and
123 MB.

Every SDK ships a `manifest.json` that records its target, the external
prerequisites it does not bundle, and any third-party payload it redistributes.
No distribution bundles a compiler or linker.

## Host requirements

CRT does not bundle a toolchain. Build machines need Git, CMake 3.25 or newer,
Ninja, Python 3, Clang, and LLD. What has actually been exercised:

| | Windows 11 x64 | Linux | macOS |
| --- | --- | --- | --- |
| Verified with | Windows 11 Pro 26100; LLVM/Clang 22.1.8 (the CI pin); CMake 4.4.3; Ninja 1.13.2; Python 3.11.9; Windows SDK 10.0.28000.0 import libraries | Ubuntu 24.04 aarch64 (the acceptance VM, kernel 6.8); Ubuntu 26.04 x86_64 under WSL2 with Clang 21.1.8 and CMake 4.2.3 | Apple Silicon; macOS 27 or newer is the acceptance target |
| Also needed | Developer Mode enabled for building and porting (CRT uses real symbolic links) | For `03-gfx-simple` and up: a reachable Wayland compositor with `xdg-shell` and `libwayland-client.so.0`; for `04-gfx-media`: the Vulkan loader and a Vulkan ICD; on aarch64, `libatomic.so.1` for `02-cxx` and up | Xcode Command Line Tools; the system frameworks (AppKit, Metal, VideoToolbox, ...) |
| GPU | A D3D12-capable display driver for `04-gfx-media` | Physical-GPU Linux/Vulkan has not been run; the acceptance host uses a virtio-gpu/lavapipe device | Metal |

Package lists for each host are in the README's
[Prerequisites](../README.md#prerequisites). Device-specific GPU and video
drivers are target prerequisites listed in each `manifest.json`, never copied
into an SDK.

## Quick start

Download one SDK archive and its `SHA256SUMS-<os>-<arch>` file, then check the
download.

Linux and macOS:

```sh
sha256sum -c --ignore-missing SHA256SUMS-<os>-<arch>
```

Windows (PowerShell):

```powershell
(Get-FileHash crt-<tag>-windows-x86_64-03-gfx-simple.zip -Algorithm SHA256).Hash
```

Compare that value with the matching line in `SHA256SUMS-windows-x86_64`.

Extract, point CRT at your own compiler, activate, and rebuild a packaged
example. The commands below were run on Windows against a real
`03-gfx-simple` archive extracted into a path containing a space:

```bat
set "CRT_CC=C:\Program Files\LLVM\bin\clang.exe"
set "CRT_CXX=C:\Program Files\LLVM\bin\clang++.exe"
set "CRT_AR=C:\Program Files\LLVM\bin\llvm-ar.exe"
set "CRT_RANLIB=C:\Program Files\LLVM\bin\llvm-ranlib.exe"
set "CRT_WINDOWS_SDK_LIBPATH=C:\Program Files (x86)\Windows Kits\10\Lib\<sdk-version>\um\x64"
call 03-gfx-simple\activate.cmd
cmake -S 03-gfx-simple\examples\gfx-simple -B build -G Ninja "-DCMAKE_TOOLCHAIN_FILE=%CD%\03-gfx-simple\crt-toolchain.cmake"
cmake --build build
build\crtgfx_window_example.exe 60
```

The last program opens a native window, presents 60 frames, and prints
`crtgfx_window_demo: presented=60`. Run it from the shell where you called
`activate.cmd`: activation puts the SDK's `bin\` directory on `PATH`, and a
rebuilt program imports its runtime DLLs (such as `libcrtgfx.dll`) from there,
because Windows has no RPATH. Without activation the program fails to start
with `STATUS_DLL_NOT_FOUND` (`0xC0000135`); the ready-made programs under
`examples\bin\` link the CRT statically and run either way.

On Linux and macOS the same flow uses `. ./activate.sh` after exporting
`CRT_CC`, `CRT_CXX`, `CRT_AR`, and `CRT_RANLIB`, as shown in
[`examples/README.md`](../examples/README.md). That path has not yet been run
from a downloaded release archive.

The packaged examples are `gfx-simple` (from `03-gfx-simple`) plus `gfx-gpu`,
`gfx-skia`, and `media-player` (`04-gfx-media`). `gfx-simple` is the smallest
build-and-run example. `media-player` plays a bundled clip through FFmpeg into a
native window and, like the others, is present only in the option-ON
`04-gfx-media` a release ships. Give it a frame limit for a bounded run, for
example `examples\bin\crtmedia_player_demo.exe examples\media-player\test_video.mp4 30`
(prints `crtmedia_player_demo: presented=30`).

## Producing the assets (maintainers)

Run these on each host from a fresh clone of the release commit. The tree must
be clean; the tooling refuses a dirty tree for anything but a rehearsal. The
tag is baked in once, through `CRT_RELEASE_TAG`, so SDK `VERSION` files, archive
names, and the download URLs inside the packaged stage recipes all agree.

1. Configure with the tag:

   ```sh
   cmake --preset <os>-host-ninja-debug -DCRT_RELEASE_TAG=v0.4.0-preview.1
   ```

2. Build the `01-c` to `03-gfx-simple` SDKs, their archives, and the stage-source
   assets they reference:

   ```sh
   cmake --build --preset <os>-host-ninja-debug --target crt-gfx-simple-dist
   ```

3. Build the release-grade `04-gfx-media`. The ordinary `crt-gfx-media-dist`
   target keeps Skia and FFmpeg OFF and is **not** releasable. Use the isolated
   option-ON stage build from the packaged `03-gfx-simple` SDK, with the local
   `--asset` override because the release is not published yet (the asset is
   still checked against the recipe). It needs network access and took about
   50 minutes on Windows/x64 with 12 logical CPUs (FreeType and FFmpeg about 46
   minutes at `-j4`, Skia about 3 minutes, everything else under a minute):

   ```sh
   python out/<preset>/dist/03-gfx-simple/tools/crt-stage-build.py \
     --sdk-root out/<preset>/dist/03-gfx-simple \
     --recipe out/<preset>/dist/03-gfx-simple/stages/recipes/04-gfx-media.json \
     --asset out/<preset>/stage-sources/crt-<tag>-<os>-04-gfx-media-source.tar.xz \
     --work-root <work> --output <out>/04-gfx-media
   ```

4. Collect, verify, and checksum:

   ```sh
   python tools/prepare_release_assets.py \
     --dist-root out/<preset>/dist --version v0.4.0-preview.1 \
     --sdk 04-gfx-media=<out>/04-gfx-media --output-dir <release-dir>
   ```

   `tools/prepare_release_assets.py` copies bytes unchanged. It fails unless the
   tree is clean, each SDK's `VERSION` equals the tag, `verify_dist.py` passes on
   the **extracted archive**, `04-gfx-media` really contains Skia, FreeType, and
   FFmpeg, and every packaged recipe points at this release and at a stage-source
   asset with the recorded size and SHA-256. Add `--dry-run` to run the checks
   only.

5. Upload every host's `<release-dir>` contents to one GitHub release. Uploading
   and publishing are manual, deliberate steps; no tool here does either.

## What has been verified so far

Verified on Windows/x64 (2026-09-21):

- An existing `03-gfx-simple` archive extracted into a path with a space passes
  `verify_dist.py`; the packaged `gfx-simple` example rebuilds from its
  `crt-toolchain.cmake` in about five seconds and both the ready-made and the
  rebuilt programs present 60 frames.
- The release tooling end to end on real data: a release-tagged `01-c` archive
  plus a release-tagged `02-cxx` stage-source asset are collected, verified, and
  checksummed (`sha256sum -c` passes); a recipe or asset that does not match the
  tag is rejected; the ordinary default-OFF `04-gfx-media` is rejected.
- The isolated option-ON `04-gfx-media` (built with the `development` tag from
  the packaged `03-gfx-simple` SDK) passes every phase: 8 stage tests, rebuilding
  and running the installed `gfx-gpu`, `gfx-skia`, and `media-player` examples,
  `verify_dist.py` (which now requires the `media-player` source, clip, project,
  and prebuilt binary), and atomic publication. It is a 523 MB SDK (about
  159 MB as a zip) that declares FreeType, FFmpeg, and Skia, and the FFmpeg it
  ships contains the D3D11VA hwaccels. `prepare_release_assets.py` rejects it
  only because its `VERSION` and recipe URLs use the `development` tag. That
  run first failed twice on stale packaging (three private headers missing from
  the stage-source list, and the per-OS backend definitions not passed to two
  direct Skia consumers); both are fixed and
  `tools/test_stage_source_closure.py` now fails if a stage-source list misses a
  private header its sources include.
- A complete Windows/x64 asset set built with the **release tag**
  `v0.4.0-preview.1` from source commit `fd01d7c`, and accepted by
  `prepare_release_assets.py` (dry run and real run): the `01-c`, `02-cxx`, and
  `03-gfx-simple` archives, the isolated option-ON `04-gfx-media` archive (about
  161 MB), the three stage-source assets, `SHA256SUMS-windows-x86_64` (`sha256sum -c`
  passes for all seven files), and `release-manifest-windows-x86_64.json`
  (`working_tree_dirty: false`). The `04-gfx-media` build started from the
  extracted release-tagged `03-gfx-simple` archive, with empty work and download
  directories, and took 50.4 minutes: FreeType and FFmpeg 44.7 minutes at `-j4`,
  Skia 3.3 minutes, everything else under 40 seconds; its 8 stage tests, the
  rebuilt `gfx-gpu`, `gfx-skia`, and `media-player` examples, `verify_dist.py`,
  and atomic publication all pass. The published 04 archive was then extracted
  into a path containing a space and run: `VERSION` is the tag, the prebuilt
  `crtmedia_player_demo.exe` presents 20 frames, and the prebuilt
  `crtgfx_skia_gpu_window_demo.exe` presents 5 frames on D3D12 with the resize
  and pixel checks passing.
  Limits: the `01-c` to `03-gfx-simple` runtime was **not** rebuilt from a fresh
  clone -- the existing development build directory was reconfigured with the tag
  and only its packaging re-ran -- and the assets record commit `fd01d7c`, so
  the release tag must point at that commit, or the set must be rebuilt from the
  tagged commit (the recipes embed the source commit).
- 23 unit tests for the tooling, and the existing distribution tests.

Not yet done, and required before a release can be published:

- A fresh-clone build of the Windows set from the commit that will actually be
  tagged (see the limits above).
- Any Linux or macOS assets (a release-tagged build there, including the first
  Linux/macOS link of the `media-player` example), and any release-archive test
  on those hosts.
- A test on a machine that has never had CRT's build environment. The Windows
  check above was a clean extraction path on a development machine, not a clean
  machine.
- Creating and publishing the GitHub release, which needs an explicit decision.

## Release notes template

The notes drafted for `v0.4.0-preview.1` are in
[`release_notes_v0.4.0-preview.1.md`](release_notes_v0.4.0-preview.1.md); it is
written to be pasted as the GitHub release body. The short template for later
previews:

```text
CRT v0.4.0-preview.1: first public developer preview of the CRT Graphics/Media
SDK stage (04-gfx-media). Pre-1.0; interfaces may change. 05-js is roadmap work
and is not part of this preview.

Verified: <per-host table from README "What Already Works">
Hardware H.264 decode: verified on macOS (VideoToolbox) and Windows (D3D11VA);
Linux VA-API is not yet verified.
Checksums: SHA256SUMS-<os>-<arch>.
```
