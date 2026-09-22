# CRT v0.4.0-preview.1

**First public developer preview of the CRT Graphics/Media SDK stage
(`04-gfx-media`).** Pre-1.0: interfaces may change, and this is not a
production-ready release.

CRT is a Bionic-compatible cross-platform C runtime and Platform Adaptation
Layer (PAL). You rebuild Linux/BSD/Android-style native C/C++ source with your
own Clang/LLD toolchain against a CRT sysroot, and get native Linux, Windows,
and macOS executables. No VM, no container, and no compiler bundled in the SDK.

Demo (Windows, the `media-player` example decoding a clip with FFmpeg into a
native window):

https://github.com/user-attachments/assets/a60ff1ae-c260-4aaa-a140-8095ac95f7c0

## What is in this preview

`04-gfx-media` is the current public SDK milestone. The SDK stages are
cumulative, so the `04-gfx-media` archive already contains everything below it.

| Stage | Adds |
| --- | --- |
| `01-c` | libc, libm, libdl, startup objects, shell, mksh, toybox, awk, make |
| `02-cxx` | imported libc++, libc++abi, and libunwind |
| `03-gfx-simple` | native window, keyboard and mouse input, software framebuffer |
| `04-gfx-media` | Skia CPU and GPU rendering, GPU presentation, FreeType text, FFmpeg software media, native audio output, opt-in hardware H.264 decode |

Bundled third-party payload in `04-gfx-media`: Skia m148, FreeType 2.14.3, and
a narrow LGPL FFmpeg 8.1.2 build (MOV/MP4/M4A, WAV, MP3 demux; H.264, AAC, MP3,
and PCM software decode). Each SDK's `manifest.json` lists these, plus the
external prerequisites CRT does not bundle.

Packaged examples: `gfx-simple`, `gfx-gpu`, `gfx-skia`, and `media-player`,
each with its own `CMakeLists.txt` that rebuilds it against the SDK, plus
ready-made programs under `examples\bin`.

Real upstream software rebuilt through the CRT sysroot with its own
`configure`/`make` (or GN) flow: zlib, libpng, SQLite, bzip2, xz, PCRE2,
mbedTLS, libcurl (real HTTP and HTTPS round trips), libffi, expat, FreeType,
FFmpeg, and Skia. Recipes and per-host results are in
[`docs/porting_status.md`](https://github.com/webos21/crt/blob/v0.4.0-preview.1/docs/porting_status.md).

## What has been verified

State on each host, not a promise. "Partial" means a gap stated below.

| Capability | Linux | Windows | macOS |
| --- | --- | --- | --- |
| libc / libm / libdl, libc++ runtime, pthread, sockets, mmap, TLS | Verified (libdl: partial) | Verified | Verified |
| Native window and input | Verified (Wayland) | Verified (Win32) | Verified (Cocoa) |
| Skia CPU raster and text | Verified | Verified | Verified |
| Skia GPU (Ganesh) presentation | Verified (Vulkan, physical GPU) | Verified (D3D12) | Verified (Metal) |
| FFmpeg software media | Verified | Verified | Verified |
| Native audio output | Partial | Verified (WASAPI) | Verified (CoreAudio) |
| Hardware H.264 decode, frames delivered to the CPU | Verified (VA-API, physical GPU) | Verified (D3D11VA) | Verified (VideoToolbox) |

The last full `ctest` runs were 149/149 on Windows/x64, 121/121 on
Linux/x86_64 under WSL2 (two TTY-dependent termios tests excluded from that
non-interactive run), 132/132 on a native Linux/x86_64 desktop with
`CRTMEDIA_ENABLE_FFMPEG=ON` and VA-API enabled, and 132/132 on macOS/arm64. The
Linux/aarch64 results come from its recorded acceptance runs. Per-host details
and dates are in
[`STATUS.md`](https://github.com/webos21/crt/blob/v0.4.0-preview.1/STATUS.md).

For this release specifically (Windows/x64): the SDKs were built with
`CRT_RELEASE_TAG=v0.4.0-preview.1` from commit `fd01d7c`. The `04-gfx-media` SDK
was built from the extracted `03-gfx-simple` archive with empty caches in about
50 minutes, and passed its 8 stage tests, rebuilding and running the installed
`gfx-gpu`, `gfx-skia` (D3D12, with resize and pixel checks), and `media-player`
examples, and `verify_dist.py`. The published archive was then extracted into a
path containing a space, its prebuilt demos ran, and the quick start below was
run from it.

For the macOS/arm64 assets, which were added after the release was first
published: they are built from commit **`972d913`**, not from the tag commit
`fd01d7c` (see the note under Downloads for why). The SDKs were built with
`CRT_RELEASE_TAG=v0.4.0-preview.1` from a fresh clone on macOS 26.6.2 with
Apple clang 21.0.0. The `04-gfx-media` SDK was built from the extracted
`03-gfx-simple` archive with empty caches in about 7.5 minutes, and passed its 8
stage tests, rebuilding and running the installed `gfx-gpu`, `gfx-skia` (Metal,
with resize and pixel checks), and `media-player` examples, and
`verify_dist.py`. The archive was then extracted into a path containing a space,
its prebuilt `media-player` and Metal demos ran, and the macOS quick start below
was run from it.

For the Linux/x86_64 assets, added the same day as the Linux VA-API hardware-
decode tranche closed: they are built from commit **`a90bf10`**, not from the
tag commit `fd01d7c` (see the note under Downloads for why). The SDKs were
built with `CRT_RELEASE_TAG=v0.4.0-preview.1` from a fresh clone on a native
(non-VM, non-WSL) Ubuntu desktop with a physical Intel GPU, Clang 21.1.8, and
CMake 4.2.3. The `04-gfx-media` SDK was built from the extracted
`03-gfx-simple` archive with empty caches in about 14 minutes (FreeType and
FFmpeg about 11 minutes, Skia about 3), and passed its 8 stage tests,
rebuilding and running the installed `gfx-gpu`, `gfx-skia` (Vulkan, with resize
and pixel checks), and `media-player` examples, and `verify_dist.py`. Both
`03-gfx-simple` and `04-gfx-media` archives were then extracted into a path
containing a space: `03-gfx-simple`'s ready-made and freshly rebuilt
`crtgfx_window_demo`/`crtgfx_window_example` both presented 60 frames,
`04-gfx-media`'s prebuilt `crtmedia_player_demo` presented 25 frames (software
decode -- this demo does not request hardware decode, so it decodes in
software on every host; the real hardware-decode evidence is
`crtmedia_hw_decode_test`, not this demo), and its prebuilt
`crtgfx_skia_gpu_window_demo` presented 5 real-Vulkan frames with a pixel
check passing, and the Linux quick start below was run from it.

## Downloads

This release attaches **Windows/x64, macOS/arm64, and Linux/x86_64 assets**.

### Windows/x64

| Asset | Size | SHA-256 |
| --- | --- | --- |
| `crt-v0.4.0-preview.1-windows-x86_64-04-gfx-media.zip` | 160.8 MB | `66db007480300efbf69c45bacb53e2c019afafafbb802a0ccda26952a4893e6c` |
| `crt-v0.4.0-preview.1-windows-x86_64-03-gfx-simple.zip` | 122.9 MB | `aa3b8b150e3bb2f3069564fb85a747c13e93fbbd0cca503263cb2e740c4fd874` |
| `crt-v0.4.0-preview.1-windows-x86_64-02-cxx.zip` | 122.6 MB | `bdf345814a9141bb608496fe181bc55047e3af3818c9e724125317253757be65` |
| `crt-v0.4.0-preview.1-windows-x86_64-01-c.zip` | 113.7 MB | `abd63b55e91fca2eaf0506ac9a75d7d691f6ad7e353bbd47cee51d35cacde64a` |
| `crt-v0.4.0-preview.1-windows-04-gfx-media-source.tar.xz` | 18.2 MB | `15e34bae5d8d86982a92b09a505311426f123238a64b9157209b7f40ea400c24` |
| `crt-v0.4.0-preview.1-windows-03-gfx-simple-source.tar.xz` | 0.1 MB | `c2bc57e28a698acbf5a093bf46c9dbd4200f92691b189e84a1d0037d529e0ce2` |
| `crt-v0.4.0-preview.1-windows-02-cxx-source.tar.xz` | 8.2 MB | `f7a79cbec9a7c2684182f26286098cb66e9230748fc1676c223ef554bc87b38e` |

Most people need only the `04-gfx-media` archive. The smaller stages are for
products that stop at C, C++, or the simple framebuffer. The `*-source.tar.xz`
files are the stage-source assets that the packaged stage recipes download
(pinned by size and SHA-256); you do not need them to use an SDK.
`SHA256SUMS-windows-x86_64` lists every checksum above, and
`release-manifest-windows-x86_64.json` records the version, source commit, and
per-asset size and SHA-256.

### macOS/arm64 (Apple Silicon)

| Asset | Size | SHA-256 |
| --- | --- | --- |
| `crt-v0.4.0-preview.1-macos-aarch64-04-gfx-media.tar.xz` | 14.1 MB | `802422edfbb3b6798b6d10ec85089b4e9b173e3da9042f52942bdfdf274b14bd` |
| `crt-v0.4.0-preview.1-macos-aarch64-03-gfx-simple.tar.xz` | 4.9 MB | `f1ec8058a5c93f838186a27d95eea863d8c372e0a8edb621112da894070d313b` |
| `crt-v0.4.0-preview.1-macos-aarch64-02-cxx.tar.xz` | 4.9 MB | `09c2dab341d6ae9a7ca0e1fe3e895d22479dc2157ad2a1e4a223d5127ce5ea1c` |
| `crt-v0.4.0-preview.1-macos-aarch64-01-c.tar.xz` | 1.8 MB | `83876183f0aa24fec1e451144b0b53510741d5f6c42188ea41f71aee64a06c2e` |
| `crt-v0.4.0-preview.1-macos-04-gfx-media-source.tar.xz` | 18.2 MB | `4bf38b0d864358cce6a8e7d6d0551fe8ce7546dfd4e34faf37aa0188a966fb41` |
| `crt-v0.4.0-preview.1-macos-03-gfx-simple-source.tar.xz` | 0.1 MB | `1cc81949ef052fabcd4a44c5a6fdf8c2b949bd47d093faa9745aefff897ec9aa` |
| `crt-v0.4.0-preview.1-macos-02-cxx-source.tar.xz` | 8.0 MB | `1ebb577af42a60c6408eeaacae6b3dba93244bd254441c5e4d82a86bc0d2ec59` |

The same guidance applies: most people need only the `04-gfx-media` archive, and
the `*-source.tar.xz` files are the stage-source assets the packaged recipes
download. `SHA256SUMS-macos-aarch64` lists every checksum above, and
`release-manifest-macos-aarch64.json` records the version, source commit, and
per-asset size and SHA-256.

**The macOS assets were built from a later commit than the tag.** The tag
`v0.4.0-preview.1` points at `fd01d7c`, which is what the Windows assets record.
The macOS manifest records `972d913`, which is later than the tag (the two fixes
below plus documentation commits), and the tag was not moved. Building the isolated `04-gfx-media` stage on macOS from `fd01d7c` itself
fails: the stage project did not link `CoreFoundation` (so `libcrtmedia.dylib`
would not link), and, once that was fixed, the shared `libcrtgfx.dylib` passed a
clock id that real macOS rejects, which made the `gfx-skia` example hang after a
window resize. Both are fixed in `972d913`, and both fixes touch macOS-specific code only, so
the Windows assets are unaffected. If you build macOS from source, use `972d913` or later
(`main`), not the tag.

### Linux/x86_64

| Asset | Size | SHA-256 |
| --- | --- | --- |
| `crt-v0.4.0-preview.1-linux-x86_64-04-gfx-media.tar.xz` | 21.1 MB | `d3996b48172f837d7d605f27fe5763d605f1b1de75ef3103ea0facf7e523278c` |
| `crt-v0.4.0-preview.1-linux-x86_64-03-gfx-simple.tar.xz` | 8.9 MB | `0c40a35f7420720b523643ba658a49582b8cb7d271f4ef05338ddb924a4fed19` |
| `crt-v0.4.0-preview.1-linux-x86_64-02-cxx.tar.xz` | 8.4 MB | `41da2f4414e4028a43396e17bd55b192c2e13fce63aff6fdec48eed72aee8ca5` |
| `crt-v0.4.0-preview.1-linux-x86_64-01-c.tar.xz` | 4.2 MB | `be63d528078b11fd59a9de17bfc52bb21f37a07a2c0c9a992b9dce5d5be46e6f` |
| `crt-v0.4.0-preview.1-linux-04-gfx-media-source.tar.xz` | 19.1 MB | `d2975ede38e0d59554107c880d4df266fb24200a09abc9448c1273b3297adcba` |
| `crt-v0.4.0-preview.1-linux-03-gfx-simple-source.tar.xz` | 0.8 MB | `d784ccf2c4dd170ff35818d434c1ed2522581754dd1bd0b1bd9e4fcbfdf3cd14` |
| `crt-v0.4.0-preview.1-linux-02-cxx-source.tar.xz` | 8.5 MB | `8d7320f4be79bd131e7e8b71bcb579174e7e9952b4d142eba5ea5470d24ac4ff` |

The same guidance applies: most people need only the `04-gfx-media` archive, and
the `*-source.tar.xz` files are the stage-source assets the packaged recipes
download. `SHA256SUMS-linux-x86_64` lists every checksum above, and
`release-manifest-linux-x86_64.json` records the version, source commit, and
per-asset size and SHA-256.

**The Linux assets were built from a later commit than the tag.** The tag
`v0.4.0-preview.1` points at `fd01d7c`, well before the Linux VA-API hardware-
decode tranche. The Linux manifest records `a90bf10`. Building the isolated
`04-gfx-media` stage from `fd01d7c` itself would not exercise VA-API at all
(the recipe change lands later) and hits a real, previously-latent bug this
exact release run found and fixed the same day: `examples/gfx-simple/
CMakeLists.txt` never linked a real host `libwayland-client`, because its own
comment incorrectly claimed native Wayland support only starts at
`04-gfx-media`. Rebuilding the packaged `gfx-simple` example from a genuinely
extracted `03-gfx-simple` archive -- this quick start had never actually been
run from a downloaded Linux archive before -- surfaced it immediately. Fixed
in `a90bf10`, Linux-only, so the Windows and macOS assets are unaffected. If
you build Linux from source, use `a90bf10` or later (`main`), not the tag.

## Quick start (Windows)

Check the download, then extract it (a path containing a space works):

```powershell
(Get-FileHash crt-v0.4.0-preview.1-windows-x86_64-04-gfx-media.zip -Algorithm SHA256).Hash
```

Compare it with the matching line in `SHA256SUMS-windows-x86_64`. Then run a
ready-made program from a shell where you called `activate.cmd`:

```bat
call 04-gfx-media\activate.cmd
04-gfx-media\examples\bin\crtmedia_player_demo.exe 04-gfx-media\examples\media-player\test_video.mp4 30
```

It plays the bundled clip in a native window and prints
`crtmedia_player_demo: presented=30`. Without a frame limit the demo plays until
you close its window.

To rebuild a packaged example against the SDK, point CRT at your own compiler
first. This exact sequence was run against the extracted `04-gfx-media` archive
(it builds in a few seconds and presents 60 frames); the other example folders
have their own `CMakeLists.txt`:

```bat
set "CRT_CC=C:\Program Files\LLVM\bin\clang.exe"
set "CRT_CXX=C:\Program Files\LLVM\bin\clang++.exe"
set "CRT_AR=C:\Program Files\LLVM\bin\llvm-ar.exe"
set "CRT_RANLIB=C:\Program Files\LLVM\bin\llvm-ranlib.exe"
set "CRT_WINDOWS_SDK_LIBPATH=C:\Program Files (x86)\Windows Kits\10\Lib\<sdk-version>\um\x64"
call 04-gfx-media\activate.cmd
cmake -S 04-gfx-media\examples\gfx-simple -B build -G Ninja "-DCMAKE_TOOLCHAIN_FILE=%CD%\04-gfx-media\crt-toolchain.cmake"
cmake --build build
build\crtgfx_window_example.exe 60
```

Activation puts the SDK's `bin\` directory on `PATH`. Windows has no RPATH, so a
rebuilt program that imports the SDK's DLLs needs that; without it the program
fails to start with `STATUS_DLL_NOT_FOUND` (`0xC0000135`).

## Quick start (macOS)

Check the download, then extract it (a path containing a space works):

```sh
shasum -a 256 -c --ignore-missing SHA256SUMS-macos-aarch64
tar -xJf crt-v0.4.0-preview.1-macos-aarch64-04-gfx-media.tar.xz
```

Then run a ready-made program:

```sh
04-gfx-media/examples/bin/crtmedia_player_demo 04-gfx-media/examples/media-player/test_video.mp4 30
```

It plays the bundled clip in a native window and prints
`crtmedia_player_demo: presented=30`. Without a frame limit the demo plays until
you close its window.

To rebuild a packaged example against the SDK, point CRT at your own compiler
first. This sequence was run against the extracted `04-gfx-media` archive (it
builds in a few seconds and presents 60 frames); the other example folders have
their own `CMakeLists.txt`:

```sh
export CRT_CC=/usr/bin/clang CRT_CXX=/usr/bin/clang++
export CRT_AR="$(xcrun -f ar)" CRT_RANLIB="$(xcrun -f ranlib)"
. ./04-gfx-media/activate.sh
cmake -S 04-gfx-media/examples/gfx-simple -B build -G Ninja \
  "-DCMAKE_TOOLCHAIN_FILE=$PWD/04-gfx-media/crt-toolchain.cmake"
cmake --build build
./build/crtgfx_window_example 60
```

A rebuilt program finds the SDK's shared libraries through the RPATH that
`crt-toolchain.cmake` sets. The prebuilt programs and the archive are **not
signed or notarized**. They ran from the extraction directory on the development
machine; a download that carries the macOS quarantine flag has not been tried,
and if macOS refuses to run a program you can clear the flag with
`xattr -dr com.apple.quarantine 04-gfx-media`.

## Quick start (Linux)

Check the download, then extract it (a path containing a space works):

```sh
sha256sum -c --ignore-missing SHA256SUMS-linux-x86_64
tar -xJf crt-v0.4.0-preview.1-linux-x86_64-04-gfx-media.tar.xz
```

Then run a ready-made program:

```sh
04-gfx-media/examples/bin/crtmedia_player_demo 04-gfx-media/examples/media-player/test_video.mp4 30
```

It plays the bundled clip in a native window and prints
`crtmedia_player_demo: presented=30`. Without a frame limit the demo plays until
you close its window. This demo always decodes in software: it does not opt
into `CRTMEDIA_FORMAT_KEY_PREFER_HARDWARE_DECODE`, so real VA-API hardware
decode is not exercised here even though this SDK's FFmpeg build supports it
-- see `crtmedia_hw_decode_test` for that evidence.

To rebuild a packaged example against the SDK, point CRT at your own compiler
first. This sequence was run against the extracted `04-gfx-media` archive (it
builds in a few seconds and presents 60 frames); the other example folders
have their own `CMakeLists.txt`:

```sh
export CRT_CC=/usr/bin/clang CRT_CXX=/usr/bin/clang++
export CRT_AR="$(dirname "$(readlink -f "$(command -v clang)")")/llvm-ar"
export CRT_RANLIB="$(dirname "$(readlink -f "$(command -v clang)")")/llvm-ranlib"
. ./04-gfx-media/activate.sh
cmake -S 04-gfx-media/examples/gfx-simple -B build -G Ninja \
  "-DCMAKE_TOOLCHAIN_FILE=$PWD/04-gfx-media/crt-toolchain.cmake"
cmake --build build
./build/crtgfx_window_example 60
```

Use `llvm-ar`/`llvm-ranlib`, not the plain `ar`/`ranlib` a Debian/Ubuntu host's
`binutils` package installs: a real, confirmed IFUNC-relink segfault in
Binutils 2.46 crashes `ar`/`ranlib`/`nm` on this project's own `libm.so` (see
[`HISTORY.md`](https://github.com/webos21/crt/blob/main/HISTORY.md)'s
2026-09-22 entry) -- LLVM's own tools have no such relink behavior. A rebuilt
program finds the SDK's shared libraries, including the real host
`libwayland-client.so`/`libva.so`/`libva-drm.so` this stage links against,
through the RPATH/`-rpath-link` that `crt-toolchain.cmake` and the packaged
example's own `CMakeLists.txt` set.

## Requirements

CRT does not bundle a toolchain. Build machines need Git, CMake 3.25 or newer,
Ninja, Python 3, Clang, and LLD. Verified on Windows 11 Pro with LLVM/Clang
22.1.8, CMake 4.4.3, Ninja 1.13.2, Python 3.11.9, and Windows SDK 10.0.28000.0
import libraries. Developer Mode must be enabled for building and porting (CRT
uses real symbolic links), and `04-gfx-media` needs a D3D12-capable display
driver. On macOS the assets were built and run on Apple Silicon with macOS
26.6.2, Xcode's Apple clang 21.0.0, CMake 4.4.3, Ninja 1.13.2, and Python 3.14;
`04-gfx-media` needs Metal. On Linux the assets were built and run on a native
Ubuntu desktop with Clang 21.1.8, CMake 4.2.3, Ninja 1.13.2, and Python 3.14,
with a physical Intel UHD Graphics 630 GPU (Mesa's Vulkan ICD and `iHD` VA-API
driver); `04-gfx-media` needs a Vulkan-capable GPU and, for hardware decode, a
VA-API driver (falls back to software otherwise). Package lists for Linux and
macOS are in the
[README](https://github.com/webos21/crt/blob/v0.4.0-preview.1/README.md#prerequisites).

## Known limitations

- **Hardware decode is verified on all three platforms, but only zero-copy is
  still missing.** Windows (D3D11VA), macOS (VideoToolbox), and Linux (VA-API,
  a physical Intel GPU) all deliver real hardware frames. Software decode is
  always the default and the fallback, and a hardware request falls back
  cleanly wherever hardware is unavailable. Decoded surfaces are not shared
  with the GPU (no zero-copy) yet on any platform.
- **The main Linux graphics acceptance host is still a VM.** The Linux/aarch64
  acceptance host (used for the cross-host `03-gfx-simple -> 04-gfx-media`
  stage-build baseline) uses a virtio-gpu/lavapipe Vulkan device. Real
  physical-GPU Linux evidence now exists separately (the x86_64 desktop used
  for VA-API and for these release assets), but is not yet the recorded
  baseline for that broader stage-build acceptance. Linux needs a reachable
  Wayland compositor with `xdg-shell`.
- **The macOS and Linux assets come from a later commit than the tag**
  (`972d913` and `a90bf10`, not `fd01d7c`) and the macOS ones are unsigned; see
  Downloads. Building macOS or Linux from the tag itself does not work as well
  as building from `main`.
- **Not tested on a clean machine.** The Windows, macOS, and Linux archives
  were extracted and run from a fresh path on the development machines, not on
  a machine that never had CRT's build environment.
- **No encode, capture, or streaming layer.** The FFmpeg build is decode and demux
  only.
- **`05-js` is not part of this preview.** It is a `libcrtjs` skeleton; QuickJS,
  the event loop, and JavaScript bindings are roadmap work.
- Interfaces are pre-1.0 and may change. CRT aims for Bionic-compatible source
  portability, not full POSIX or glibc compatibility, and not binary
  compatibility.

## Next

Uploading the macOS and Linux assets and pasting these updated notes into the
GitHub release body, macOS signing and notarization, verification on a clean
machine, a fresh-clone rebuild of the Windows set, zero-copy hardware decode
to GPU textures, and the `05-js` JavaScript stage. Open work is tracked in
[`TODO.md`](https://github.com/webos21/crt/blob/main/TODO.md).

Full documentation: [README](https://github.com/webos21/crt/blob/v0.4.0-preview.1/README.md),
[FAQ](https://github.com/webos21/crt/blob/v0.4.0-preview.1/docs/faq.md).
Project-owned code and imported upstream families keep their applicable license
and provenance records; see `LICENSE.md` in every SDK and the README's
[License And Provenance](https://github.com/webos21/crt/blob/v0.4.0-preview.1/README.md#license-and-provenance)
section.
