# Developer Preview Release

This document defines what a CRT developer-preview release contains, how it is
verified, and how a maintainer produces it. The first preview,
[`v0.4.0-preview.1`](https://github.com/webos21/crt/releases/tag/v0.4.0-preview.1), was published
on 2026-09-21 as a GitHub pre-release, and now carries **Windows/x64,
macOS/arm64, and Linux/x86_64 assets** (all three built and checksummed
2026-09-21/22, uploaded since; re-confirmed 2026-09-22 via the release API --
27 files, release body byte-identical to
[`release_notes_v0.4.0-preview.1.md`](release_notes_v0.4.0-preview.1.md)) --
see "What has been verified so far" below for each host's own build. For what
works today, see the README and [`STATUS.md`](../STATUS.md).

That preview is positioned as the first public developer preview of the CRT
Graphics/Media SDK stage (`04-gfx-media`).
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
| Verified with | Windows 11 Pro 26100; LLVM/Clang 22.1.8 (the CI pin); CMake 4.4.3; Ninja 1.13.2; Python 3.11.9; Windows SDK 10.0.28000.0 import libraries | Ubuntu 24.04 aarch64 (the acceptance VM, kernel 6.8); Ubuntu 26.04 x86_64 under WSL2 with Clang 21.1.8 and CMake 4.2.3; a native (non-VM, non-WSL) Ubuntu x86_64 desktop with Clang 21.1.8 and CMake 4.2.3, used for the release assets and VA-API evidence below | Apple Silicon; macOS 27 or newer is the acceptance target |
| Also needed | Developer Mode enabled for building and porting (CRT uses real symbolic links) | For `03-gfx-simple` and up: a reachable Wayland compositor with `xdg-shell` and `libwayland-client.so.0`; for `04-gfx-media`: the Vulkan loader, a Vulkan ICD, `pkg-config`, and (for VA-API hardware decode) `libva-dev`/`libva2`/`libva-drm2` plus a VA driver; on aarch64, `libatomic.so.1` for `02-cxx` and up | Xcode Command Line Tools; the system frameworks (AppKit, Metal, VideoToolbox, ...) |
| GPU | A D3D12-capable display driver for `04-gfx-media` | The aarch64 acceptance VM uses a virtio-gpu/lavapipe device; a physical Intel UHD 630 (Mesa Vulkan ICD, `iHD` VA-API driver) is now also verified separately (release assets and VA-API hardware decode, 2026-09-22) | Metal |

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

Note that applies to every host below: `v0.4.0-preview.1`'s packaged
`crtmedia_player_demo`/`examples/media-player` does not request hardware
decode -- it always decodes in software, on Windows, macOS, and Linux alike,
regardless of what that host's FFmpeg build supports. The "presents N frames"
results quoted per host below are software-decode evidence, not
hardware-decode evidence; the real hardware-decode evidence is each host's
own `crtmedia_hw_decode_test` `RESULT` line (`docs/
crtmedia_hardware_decode_acceptance.md`). This demo requests hardware decode
by default starting after this release (`HISTORY.md`, 2026-09-22); see
`TODO.md`'s "Next release hardening" for re-verifying that on macOS and
Linux.

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
- The published GitHub release, checked through the public API (nothing was
  downloaded except two small files and a 1 KB range): it is not a draft, it is
  marked as a pre-release, its tag is `v0.4.0-preview.1`, its body equals
  [`release_notes_v0.4.0-preview.1.md`](release_notes_v0.4.0-preview.1.md), and
  all nine files are attached -- the seven archives and stage-source assets
  (size and SHA-256 equal to `SHA256SUMS-windows-x86_64`), that checksum file, and
  the release manifest (both byte-identical to the local copies). The
  `download` URL a packaged recipe uses now resolves. The asset contents were not
  re-downloaded and re-verified from GitHub.
- 23 unit tests for the tooling, and the existing distribution tests.

Verified on macOS/arm64 (2026-09-21, macOS 26.6.2 on Apple Silicon, Apple clang
21.0.0, CMake 4.4.3, Ninja 1.13.2), built from a **fresh clone**, empty work and
download directories, with the release tag `v0.4.0-preview.1`:

- The published tag itself does **not** build a macOS `04-gfx-media`. The first
  fresh-clone attempt from `fd01d7c` failed to link `libcrtmedia.dylib`
  (undefined CoreFoundation symbols from FFmpeg's VideoToolbox objects): the
  isolated stage project keeps its own copy of the macOS framework list, and
  `CoreFoundation` had been added only to `libcrtmedia/CMakeLists.txt`. The
  second attempt got past that and then hung its `gfx-skia` example after the
  scripted resize: the shared `libcrtgfx.dylib` binds `clock_gettime` to real
  libSystem, where this project's clock id is invalid, so the event pump waited
  on a ~25-day deadline. The in-tree build passed both, because it links the
  static libc, which is why the in-tree `ctest` never showed either. Both are
  fixed (`32ab384`, `972d913`); the first now has a test that fails when the two
  framework lists drift, and the second is gated by the isolated stage's own
  `gfx-skia` run (5 frames, resize, pixel check, 60 s timeout).
- The macOS assets therefore record commit `972d913`, **not** the tag commit
  `fd01d7c`; the two differ by those two fixes and by documentation. Anyone
  reading the macOS manifest against the tag must know that.
- Clone of `972d913` (`working_tree_dirty: false`): `01-c` to `03-gfx-simple`
  in about 3 minutes, then the isolated option-ON `04-gfx-media` from the
  extracted release-tagged `03-gfx-simple` archive in 7.5 minutes (FreeType and
  FFmpeg 5.7 minutes at `-j4`, Skia 59 seconds, everything else about 20
  seconds; Windows needed 50). Its 8 stage tests, the rebuilt `gfx-gpu`,
  `gfx-skia` and `media-player` examples, `verify_dist.py`, and atomic
  publication pass, and `prepare_release_assets.py` accepted the set: four
  `.tar.xz` SDK archives (1.8, 4.9, 4.9 and 14 MB), three stage-source assets,
  `SHA256SUMS-macos-aarch64` (`shasum -c` passes for all seven) and
  `release-manifest-macos-aarch64.json`.
- The extracted `04-gfx-media` archive, in a path containing a space, is
  `v0.4.0-preview.1`, declares FreeType, FFmpeg and Skia, and its prebuilt
  programs run: `crtmedia_player_demo` presents 30 frames and
  `crtgfx_skia_gpu_window_demo` presents 5 on Metal with the resize and pixel
  checks passing. From the same extraction, following the quick start,
  `gfx-simple` rebuilds and presents 60 frames and `media-player` rebuilds and
  presents 30. This is the first macOS link of the `media-player` example.
- Nothing was uploaded at the time of this build; the set was attached to the
  release afterward (see "Not yet done" below).
  Limits: the `.tar.xz` files were run from the extraction location on the
  development machine (not a machine that never had CRT's environment), the
  binaries are not signed or notarized, and only the local checksums were
  checked, not a download.

Verified on Linux/x86_64 (2026-09-22, a native -- non-VM, non-WSL -- Ubuntu
desktop with a physical Intel UHD Graphics 630, Clang 21.1.8, CMake 4.2.3,
Ninja 1.13.2), built from a **fresh clone**, empty work and download
directories, with the release tag `v0.4.0-preview.1`:

- The published tag itself does **not** build a release-ready Linux
  `04-gfx-media`: it predates the same day's Linux VA-API hardware-decode
  tranche entirely, and a fresh-clone rebuild from it hits a real,
  previously-latent bug this exact release run found: `examples/gfx-simple/
  CMakeLists.txt` never linked a real host `libwayland-client`, because its
  own comment incorrectly claimed native Wayland support only starts at
  `04-gfx-media` -- `libcrtgfx/CMakeLists.txt`'s real gate compiles
  `window_wayland_native.c` whenever a Vulkan loader is found at configure
  time (a host capability, not a stage choice), which any normal
  Advanced-Graphics-capable Linux host already has. Rebuilding the packaged
  `03-gfx-simple` `gfx-simple` example from a genuinely extracted archive --
  this exact quick start had never actually been run from a downloaded Linux
  archive before -- surfaced it immediately. Fixed in `a90bf10` with the same
  `NO_CMAKE_FIND_ROOT_PATH` + `dpkg-architecture` multiarch `find_file()`
  fallback `examples/gfx-gpu/CMakeLists.txt` already established for its own
  Vulkan/Wayland needs.
- The Linux assets therefore record commit `a90bf10`, **not** the tag commit
  `fd01d7c`; the two differ by the whole VA-API tranche plus this fix and
  documentation. Anyone reading the Linux manifest against the tag must know
  that.
- Clone of `a90bf10` (`working_tree_dirty: false`): `01-c` to `03-gfx-simple`
  in about 3 minutes (including the one-time libc++/native-Wayland
  bootstrap), then the isolated option-ON `04-gfx-media` from the extracted
  release-tagged `03-gfx-simple` archive in about 14.5 minutes (FreeType and
  FFmpeg about 11 minutes at `-j12`, Skia about 3 minutes, everything else
  under a minute). Its 8 stage tests, the rebuilt `gfx-gpu`, `gfx-skia`
  (Vulkan, `resize_frame=2 pixel_check=pass post_resize_present=pass`), and
  `media-player` examples, `verify_dist.py`, and atomic publication all pass,
  and `prepare_release_assets.py` accepted the set: four `.tar.xz` SDK
  archives (4.2, 8.4, 8.9, and 21.1 MB), three stage-source assets,
  `SHA256SUMS-linux-x86_64` (`sha256sum -c` passes for all seven) and
  `release-manifest-linux-x86_64.json`.
- The extracted `04-gfx-media` archive, in a path containing a space, is
  `v0.4.0-preview.1`, declares FreeType, FFmpeg (with VA-API hwaccels) and
  Skia, and its prebuilt programs run: `crtmedia_player_demo` presents 25
  frames -- in software, like every host: this demo never opts into
  `CRTMEDIA_FORMAT_KEY_PREFER_HARDWARE_DECODE`, so it always decodes in
  software regardless of what the SDK's FFmpeg build supports (corrected
  2026-09-22, `HISTORY.md`; the real VA-API hardware-decode evidence is
  `crtmedia_hw_decode_test`'s own `RESULT` line, recorded separately) -- and
  `crtgfx_skia_gpu_window_demo` presents 5 frames on Vulkan (physical GPU)
  with the resize and pixel checks passing. From the same extraction and the
  extracted `03-gfx-simple` archive, following the quick start, both the
  ready-made and freshly rebuilt `crtgfx_window_demo`/`crtgfx_window_example`
  present 60 frames, and `media-player` rebuilds and presents 25. This is the
  first native (non-VM, non-WSL) Linux hardware-decode and physical-GPU
  Vulkan/Skia evidence recorded for a release build.
- Nothing was uploaded at the time of this build; the set was attached to the
  release afterward (see "Not yet done" below).
  Limits: the `.tar.xz` files were run from the extraction location on the
  development machine (not a machine that never had CRT's environment), and
  only the local checksums were checked, not a download.

Done since the builds above: both the macOS (nine files) and Linux (seven
files) sets were uploaded to the release, and the updated, three-platform
`release_notes_v0.4.0-preview.1.md` was pasted into the GitHub release body --
2026-09-22 re-confirmed both via the release API (27 assets across all three
hosts; the live body is byte-identical to the file in this repo).

Not yet done:

- A fresh-clone rebuild of the Windows set. It is not needed for consistency:
  the tag `v0.4.0-preview.1` points at `fd01d7c`, the commit the assets record.
  It would only remove the "existing build directory was reused" limit above.
- A dedicated clean-machine test is not planned here (`HISTORY.md`,
  2026-09-23): the checks above ran from a clean extraction path on a
  development machine, not a genuinely clean one, on all three hosts, but
  this project is not acquiring a dedicated clean device to close that gap.
  Real coverage now comes from whoever downloads the release after the
  public launch; watch for and act on their issue reports instead.
- Re-running the hardware-decode-by-default packaged `crtmedia_player_demo`
  (`HISTORY.md`, 2026-09-22) on macOS and Linux -- this release's own
  `crtmedia_player_demo` predates that change and still only decodes in
  software on every host, which the release notes already say; the change
  itself is scoped to whatever CRT publishes next (`TODO.md`'s "Next release
  hardening").

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
Hardware H.264 decode: verified on all three hosts -- macOS (VideoToolbox),
Windows (D3D11VA), and Linux (VA-API, physical GPU).
Checksums: SHA256SUMS-<os>-<arch>.
```
