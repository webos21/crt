# WebKit CRT Port: Upstream Mapping

**Status: Web Tranches 0-2 closed; Tranche 3 is active through B4, with B5 first
render next (2026-10-12).** This file maps upstream WebKit and WPE components to
their `PlatformCRT`/CRT counterparts, and records every place the CRT port
deliberately differs from upstream and why. Acceptance and tranche order are in
[`crtweb_acceptance.md`](../acceptance/crtweb_acceptance.md); the pin and its
provenance are in
[`libcrtweb/third_party/webkit/README.md`](../../libcrtweb/third_party/webkit/README.md).

The reference is **WPE WebKit 2.54.0**. What is below comes from its release
notes and announcement plus an inventory of the verified archive (file names and
license files), the Tranche 1 JavaScriptCore builds, and the Tranche 2 native build
and source reading across WebCore, WebKit, WPEPlatform and the GLib process launcher. A row marked
*verify* is a hypothesis to confirm against the extracted source in the tranche
named, not a commitment.

## Mapping table

| Upstream area | WPE 2.54 reference | CRT target | Decision / notes |
| --- | --- | --- | --- |
| Embedding API | WPEPlatform: stable, on by default; WebKit selects a platform itself, with no separate backend library (WPEBackend-fdo is not needed). The libwpe-based API is deprecated. Tranche 2 proved the current API path: create/connect `wpe_display_headless_new()`, construct the `WebKitWebView` with that display, then resize its WPE toplevel | **Two things, kept apart.** (a) *Linux prototype boundary:* a WPEPlatform platform implementation, only to reach a first green external-surface proof on Linux (Tranche 3A). (b) *The product:* `PlatformCRT`, a new WebKit port (`OptionsCRT.cmake`, `PlatformCRT.cmake`, `platform/crt`, `UIProcess/crt`, `WebProcess/crt`, `NetworkProcess/crt`) that does not depend on WPEPlatform or GLib being present on Windows/macOS | **Decided 2026-10-03:** WPEPlatform is the Linux reference/prototype boundary, not the cross-platform architecture. Even under WPEPlatform the WPE port keeps `GioUnix`, libsoup, the GLib IPC backend and Unix process entry points (`Source/WebKit/PlatformWPE.cmake`), so carrying it to Windows/macOS would mean porting WPE, which is not the goal. Build with `ENABLE_WPE_LEGACY_API=OFF` for the reference. *Verified* in Tranche 3A (2026-10-10): a platform lives outside the WebKit tree as a GIO module on the `wpe-platform-display` extension point (`libcrtweb/platform/wpe-crt`), no WebKit patch |
| Platform windowing / input | WPEPlatform views and events | `crtui` external surface plus CRT input | The WebView is a `crtui` external-surface producer (accepted in `05-ui`) |
| Graphics output | Skia compositor in the web process (deferred display lists, damage-aware); Cairo removed; threaded tile painting mandatory. The built-in headless platform exposes SHM/DMA-BUF buffer types, and the accepted visible snapshot is a BGRA8888 `WebKitImage` | `crtgfx` external surface: shared memory first, then a GPU buffer | Upstream 2.54 notes DMA-BUF buffers selected by `WPE_BUFFER_FORMAT`. Sharing one Skia context with CRT is not an initial goal. **Confirmed:** the archive bundles its own Skia, milestone **154**, and ANGLE under `Source/ThirdParty`; CRT's Skia is m148. They stay separate copies (the web process owns WebKit's); *verify* in Tranche 3/9 that no symbol of either leaks into the other's process |
| Process launch | `WPEProcessManager`/`WPEProcessLaunchOptions` exist but are compiled **only for Android** (`Source/WebKit/WPEPlatform/CMakeLists.txt`: `if (ANDROID)`); the WPEPlatform docs say child-process launch is otherwise internal to WPE WebKit and that Android is the exception. On Linux/WPE the launcher is WebKit's own `ProcessLauncher`/`AuxiliaryProcess` with the Unix IPC backend | WebKit `ProcessLauncher` + `AuxiliaryProcess` platform code for `PlatformCRT`, on CRT PAL process, pipe and shared-memory primitives | **Corrected 2026-10-03; implemented at B4:** `WPEProcessManager` is reference material, not CRT's generic process solution. `ProcessLauncherCRT.cpp` launches the Web and Network processes through the CRT PAL; Linux/x86_64 process/termination acceptance is green, with Windows/macOS replay still open. |
| IPC | WebKit's own IPC between UI, web, network and GPU processes. The pinned 2.54.0 tarball has `Platform/IPC/{unix,glib,darwin,android}` backends only, and **no Windows backend and no `PlatformWin.cmake`** | Reuse WebKit's Unix IPC message/connection implementation over CRT PAL sockets and descriptor transport where that contract is available; add a CRT transport only where a host replay proves it necessary | **B4 result:** Linux/x86_64 reuses `Platform/IPC/unix` unchanged over CRT `socketpair()`/descriptor semantics and passes process IPC. Windows/macOS transport is deliberately undecided until Tranches 6/7 exercise B4; it is not yet valid to claim a separate `Platform/IPC/crt` backend. |
| Build | CMake with Ninja only (the Makefile generator is unsupported). The native WPE baseline additionally confirmed GLib/GioUnix, libsoup 3, ICU, fontconfig/FreeType, HarfBuzz, GStreamer, image codecs, EGL/epoxy and libdrm as reference dependencies | CRT sysroot toolchain driven by Ninja | Matches the CRT default generator. Release process binaries use compiled-in install paths, so the reference tool uses a private CMake install prefix rather than relying on `WEBKIT_EXEC_PATH` |
| Network | libsoup | CRT networking | Keep upstream first; substitute in Tranche 8 |
| Media | GStreamer | `crtmedia` | Native WPE build confirmed GStreamer is part of the supported Video/WebCodecs configuration. Keep upstream first; substitute in Tranche 8 |
| Fonts / text | fontconfig / FreeType / HarfBuzz | CRT-controlled FreeType/font layer | Confirmed by the native WPE configure/build; substitution remains later port work |
| JavaScriptCore | JSCOnly / WPE build (`Source/cmake/OptionsJSCOnly.cmake`: needs Threads and **ICU >= 70.1** `data i18n uc`; default `EVENT_LOOP_TYPE=Generic`, so no GLib; Ruby, Perl and Python as build-host tools; gperf only once WebCore is enabled) | CRT libc/libc++ build against the installed SDK, interpreter first (JIT off), then JIT | Tranche 1. ICU and gperf are CRT ports (`porting/recipes/icu.json`, `gperf.json`), not host libraries, so the `06-web` SDK stays self-contained; Ruby, Perl and Python are build-host tools. Surfaces threads, TLS, executable memory and signal gaps in the lower runtime |
| Event loop | GLib main loop (WPE); `Generic` for JSCOnly | Generic run loop for `PlatformCRT` | B3/B4 follow the PlayStation-shaped non-GLib port. Linux/x86_64 process launch and IPC pass with the generic run loop; host replays remain part of Tranches 6/7. |

## CRT port files (to be filled as the source is read)

The product is a new WebKit port, so each upstream WPE file group gets one
disposition -- **reuse** (platform-neutral, built as is), **wrap** (kept, with a
CRT adapter), **replace** (a `crt` counterpart), or **defer** -- recorded here when
the tranche that touches it reads the source. Not yet decided means blank.

| Upstream reference | CRT counterpart | Disposition |
| --- | --- | --- |
| `Source/cmake/OptionsWPE.cmake` / `PlatformWPE.cmake` | `OptionsCRT.cmake` / `PlatformCRT.cmake` | replace |
| `Source/cmake/OptionsJSCOnly.cmake` | used as is for Tranche 1 | reuse |
| `Source/WebKit/UIProcess/wpe`, `WPEPlatform` | `UIProcess/crt`; WPEPlatform only as the Linux prototype boundary | replace (prototype: wrap) |
| `Source/WebKit/WebProcess/wpe` | `WebProcess/crt` | replace |
| `Source/WebKit/NetworkProcess/soup` | upstream first; CRT backend in Tranche 8 | defer |
| `Source/WebKit/Platform/IPC/{unix,glib}` | Unix IPC over CRT PAL first; a `Platform/IPC/crt` replacement only if a host replay requires it | reuse on Linux; verify on Windows/macOS |
| `GPUProcess` (ON by default for WPE) | `PlatformCRT` path | replace |
| Skia compositor (`Shared/skia`, m154) | kept as WebKit's private copy | reuse |

## Source layers

| Source | Role |
| --- | --- |
| WPE WebKit 2.54.0 release tarball (`recipe.json`) | *Reference*: the WPE baseline, JSCOnly bring-up, the Linux 3A prototype. Has no Windows port |
| Full WebKit commit `73f39d84...` (the signed tag's target; pinned 2026-10-10 as `product_source`: commit, tree id and listing digest, `tools/fetch_webkit_commit.py`; licensing: `license-scan.json`, `tools/check_webkit_patch_manifest.py`) | *PlatformCRT product source*: the Mac and Win ports as references, and the tree `PlatformCRT` lives in |

## Rules

- Before the first carried WebKit patch: a per-file license/provenance scan and a patch
  manifest that separates new CRT-owned platform files from modified upstream files.

- Upstream source is not patched to hide a CRT/PAL deficiency; a missing
  Bionic-compatible surface is fixed in CRT (`../AGENTS.md`).
- Each deviation from upstream is a row here with a removal condition, the same
  way port-specific workarounds are documented for `porting/recipes/`.
- No WebKit/WPE/GLib type crosses the `libcrtweb` public API.
- A pin bump (including security updates) changes `libcrtweb/third_party/webkit/recipe.json`
  and re-runs the stage gates; see Tranche 0 in `crtweb_acceptance.md`.

## Current carried patches

`libcrtweb/patches/manifest.json` is authoritative for target selection, exact source hashes,
reasons and removal conditions. The current patches describe target ABI/object-format traits
that WebKit 2.54.0 selects through `OS()` rather than CRT/PAL deficiencies:

- `0001` keeps Darwin's real `int64_t` identity while the macOS target uses the Linux-shaped
  source persona.
- `0002` selects Mach-O assembler spelling for that macOS persona.
- `0003` selects COFF assembler/calling-convention spelling for the Windows persona.
- `0004` reserves x18 for the Darwin arm64 ABI even though WTF sees `OS(LINUX)`. It was required
  by the macOS FTL/B3 replay: the Linux register table allocated x18 and generated code faulted
  after a runtime call. `WTF_CRT_DARWIN_ARM64_ABI` is emitted only for macOS/arm64, so
  Linux/aarch64 retains WebKit's Linux table.
- `0005` registers `CRT` in `ALL_PORTS` (a port that is not upstream has to be registered).
- `0007` widens nine `PLATFORM(PLAYSTATION)` sites in WebCore to `(PLATFORM(PLAYSTATION) ||
  PLATFORM(CRT))`: PlatformCRT compiles the PlayStation port's files for the same roles (the
  accessibility wrapper class, `RenderTheme`/`PlatformRenderTheme`, ChromeClient accessibility
  notifications, SQLite extras, ICU searcher). Defining `WTF_PLATFORM_PLAYSTATION` as well was
  rejected: it changes WTF's `PlatformEnable`/`PlatformHave` and 28 WTF sites for every module.
- `0008` makes `Page::filterRenderingModes()` skip `Settings::hardwareAccelerationEnabled()` for
  `PLATFORM(CRT)`: that preference exists only for GTK/WPEPlatform, and the CRT port has no
  hardware-acceleration setting (CPU painting until Tranche 9).

- `0010` widens the 16 files of Source/WebKit that select per-port members and behaviour with
  `PLATFORM(PLAYSTATION)` (process paths in the launch options, libwpe event classes, the socket
  monitor of `IPC::Connection`, website-data directories) to `|| PLATFORM(CRT)`.
- `0011` takes the PlayStation-shaped C API headers (`WKViewRef`, `WKAPICastPlayStation.h`) under
  `BUILDING_CRT__`.
- `0012` makes the framework header copies in the build tree symlinks on hosts that can create them
  (clang's `#pragma once` does not deduplicate copies; GCC does, which is why Linux/GCC ports do not
  meet it).
- `0013` selects `DrawingAreaCRT` in `DrawingArea::create`.
- `0014` spells the definition of `WebPasteboardProxy::readStringFromPasteboard` with the declaration's `uint64_t` (the same
  type as `size_t` on LP64 Linux, a different one on Darwin). Removal: upstream fixes the definition.
- `0015` converts `std::filesystem::temp_directory_path()` to `std::string` explicitly in `WTF/FileSystem.cpp`: with
  `-D_LIBCPP_WIN32API` (needed on Windows so WebKit and the imported libc++ agree on `path::value_type`) the path's
  native string is `std::wstring`. Removal: upstream uses `.string()` or the Windows port is built as `PORT=Win`.

- `0016` exposes the UI-side backing store's size (`DrawingAreaProxyCoordinatedGraphics::
  backingStoreSizeForCRT`), so an embedder does not copy a stale picture after a resize.
- `0017` implements `WebPage::handleEditingKeyboardEvent` for PlayStation-shaped ports (caret
  movement, selection, deletion, newline/tab, character insertion), which upstream leaves a stub.
- `0019` bounds the PlayStation socket monitor's `select()` in `Platform/IPC/unix/ConnectionUnix.cpp` at 100 ms: on macOS a
  `close()` in another thread does not wake a blocked `select()`, and the monitor (which owns a `Connection` reference)
  would otherwise live forever. Removal: an interruptible event-loop source replaces the monitor.
- `0018` clears the removed node's `m_left`/`m_right` `CheckedPtr`s in `WTF::RedBlackTree::remove`; without it the generic
  RunLoop's `ScheduledTask` destructor decrements a destroyed neighbour's count (WebProcessProxy teardown crash, B7).
  Removal: upstream fixes `remove()`.
- `0020` makes `WebPage::platformUserAgent` (PlayStation) return the page's own user agent, so the embedder's
  application name reaches `navigator.userAgent` (stacked on 0017, same file). Removal: upstream takes it into account.

## WebKit files of PlatformCRT (milestone B4)

`libcrtweb/port/Source/WebKit/PlatformCRT.cmake` follows `PlatformPlayStation.cmake`. CRT-owned files:
`UIProcess/Launcher/crt/ProcessLauncherCRT.cpp`, `WebProcess/InjectedBundle/crt/InjectedBundleCRT.cpp`,
`WebProcess/WebPage/crt/DrawingAreaCRT.{h,cpp}` (B5: CPU painting through `DrawingAreaProxy::Update`),
`UIProcess/API/C/crt/WKViewCRT.{h,cpp}` (`WKViewCRTCopyPixels`, `WKViewCRTSetSize`, `WKViewCRTHandlePointer/Wheel/Key/Text`), `Platform/crt/WPELoaderCRT.cpp`, and in WebCore
`platform/crt/UserAgentCRT.cpp`. The Web and Network process executables are upstream's Unix entry
points (`WebProcess/EntryPoint/unix`), linked against `libWebKit.so`. Taken from PlayStation unchanged:
`Platform/IPC/unix`, the libwpe event files, `UIProcess/playstation` (page client, view, preferences,
process pool), `WebProcess/playstation`, `Shared/unix/AuxiliaryProcessMain`, the C API. Not taken:
the PlayStation SDK launcher, bundle loader and `dlopen()` entry points, and the stale compositing
files (`LayerTreeHostPlayStation`, `ThreadedCompositorPlayStation`, `AcceleratedSurfacePlayStation`).

## WebCore files of PlatformCRT (milestone B3)

`libcrtweb/port/Source/WebCore/PlatformCRT.cmake` and `.../PAL/pal/PlatformCRT.cmake` follow
`PlatformPlayStation.cmake`. Upstream files used unchanged (they are plain stubs or generic code):

| Role | Upstream file used | Note |
| --- | --- | --- |
| Accessibility | `accessibility/playstation/*` | stubs; widened by `0007` |
| Resource usage | `page/playstation/*` | stubs |
| Network state | `platform/network/playstation/NetworkStateNotifierPlayStation.cpp` | |
| MIME types, screen, theme, scrollbar theme | `platform/playstation/*`, `rendering/playstation/*` | |
| System fonts | `platform/graphics/playstation/SystemFontDatabasePlayStation.cpp` | returns the standard family; real data comes from fontconfig |
| Keyboard keys, pasteboard, editor | `platform/libwpe/*`, `editing/libwpe/*` | through the static `libwpe` port |
| EGL/GL context, display | `platform/graphics/egl/*`, `PlatformDisplay.cpp` | compiled and linked against the CRT "no GPU" libraries; no context is created |

Not taken from PlayStation: `UserAgentPlayStation.cpp` (PlayStation's user-agent string), the
curl TLS-handle file (certificate store API of the PlayStation SDK), `PlatformDisplayLibWPE` and
`GLContextLibWPE` (the WPE renderer backend): CRT-owned replacements arrive with B4/B5 (user agent,
CA bundle, display). The WebCore side of key events is libwpe's; the WebKit side is B6.

Findings that differ from the plan: the PAL directory is `Source/WebCore/PAL/pal`; upstream's
PlayStation port is not compile-clean with Skia at this commit (`PlatformDisplaySkia.cpp`,
`Page.cpp`), so PlatformCRT is the first non-GTK/WPE Skia port to build.

