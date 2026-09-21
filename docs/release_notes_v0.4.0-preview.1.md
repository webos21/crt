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
| Skia GPU (Ganesh) presentation | Partial (Vulkan) | Verified (D3D12) | Verified (Metal) |
| FFmpeg software media | Verified | Verified | Verified |
| Native audio output | Partial | Verified (WASAPI) | Verified (CoreAudio) |
| Hardware H.264 decode, frames delivered to the CPU | Not verified | Verified (D3D11VA) | Verified (VideoToolbox) |

The last full `ctest` runs were 149/149 on Windows/x64, 121/121 on
Linux/x86_64 under WSL2 (two TTY-dependent termios tests excluded from that
non-interactive run), and 132/132 on macOS/arm64. The Linux/aarch64 results come
from its recorded acceptance runs. Per-host details and dates are in
[`STATUS.md`](https://github.com/webos21/crt/blob/v0.4.0-preview.1/STATUS.md).

For this release specifically (Windows/x64): the SDKs were built with
`CRT_RELEASE_TAG=v0.4.0-preview.1` from commit `fd01d7c`. The `04-gfx-media` SDK
was built from the extracted `03-gfx-simple` archive with empty caches in about
50 minutes, and passed its 8 stage tests, rebuilding and running the installed
`gfx-gpu`, `gfx-skia` (D3D12, with resize and pixel checks), and `media-player`
examples, and `verify_dist.py`. The published archive was then extracted into a
path containing a space, its prebuilt demos ran, and the quick start below was
run from it.

## Downloads

This release attaches **Windows/x64 assets only**.

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

**Linux and macOS:** no prebuilt archives are attached to this release. Build
them from this tag with the instructions in the
[README](https://github.com/webos21/crt/blob/v0.4.0-preview.1/README.md#build).

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

## Requirements

CRT does not bundle a toolchain. Build machines need Git, CMake 3.25 or newer,
Ninja, Python 3, Clang, and LLD. Verified on Windows 11 Pro with LLVM/Clang
22.1.8, CMake 4.4.3, Ninja 1.13.2, Python 3.11.9, and Windows SDK 10.0.28000.0
import libraries. Developer Mode must be enabled for building and porting (CRT
uses real symbolic links), and `04-gfx-media` needs a D3D12-capable display
driver. Package lists for Linux and macOS are in the
[README](https://github.com/webos21/crt/blob/v0.4.0-preview.1/README.md#prerequisites).

## Known limitations

- **Hardware decode is not cross-platform yet.** It is verified on Windows
  (D3D11VA, on a physical Intel GPU) and macOS (VideoToolbox). Linux VA-API is
  not verified: the aarch64 VM's Mesa driver exposes no H.264 decode entrypoint,
  and under WSL2 on an Intel GPU a real decode deadlocks inside Intel's own WSL
  video driver even with a plain FFmpeg. Software decode is always the default
  and the fallback, and on Linux a hardware request falls back cleanly.
  Decoded surfaces are not shared with the GPU (no zero-copy) yet.
- **Linux graphics evidence comes from a VM.** The Linux/aarch64 acceptance host
  uses a virtio-gpu/lavapipe Vulkan device; Vulkan on a physical GPU has not been
  run. Linux needs a reachable Wayland compositor with `xdg-shell`.
- **No prebuilt Linux or macOS archives** in this release, and the
  `media-player` example has been linked in the isolated stage only on Windows.
- **Not tested on a clean machine.** The Windows archive was extracted and run
  from a fresh path on the development machine, not on a machine that never had
  CRT's build environment.
- **No encode, capture, or streaming layer.** The FFmpeg build is decode and demux
  only.
- **`05-js` is not part of this preview.** It is a `libcrtjs` skeleton; QuickJS,
  the event loop, and JavaScript bindings are roadmap work.
- Interfaces are pre-1.0 and may change. CRT aims for Bionic-compatible source
  portability, not full POSIX or glibc compatibility, and not binary
  compatibility.

## Next

Linux and macOS release archives, verification on a clean machine, Linux VA-API
evidence on a native host, zero-copy hardware decode to GPU textures, and the
`05-js` JavaScript stage. Open work is tracked in
[`TODO.md`](https://github.com/webos21/crt/blob/main/TODO.md).

Full documentation: [README](https://github.com/webos21/crt/blob/v0.4.0-preview.1/README.md),
[FAQ](https://github.com/webos21/crt/blob/v0.4.0-preview.1/docs/faq.md).
Project-owned code and imported upstream families keep their applicable license
and provenance records; see `LICENSE.md` in every SDK and the README's
[License And Provenance](https://github.com/webos21/crt/blob/v0.4.0-preview.1/README.md#license-and-provenance)
section.
